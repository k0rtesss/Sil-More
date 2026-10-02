#!/usr/bin/env python3
"""Exercise cave atmospheres through the real engine and save readers.

This is an isolated C harness.  It links the current build objects, uses only
temporary edit/data paths, and keeps all save streams in SDL memory.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, TESTS as SCENT_TESTS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/cave-atmospheres"


def fixture_block(name):
    return fixture_function(name)


WRITER = r'''
#include "fs/save.c"
#include <assert.h>

size_t fixture_write_partition_meta(const partition_meta_save* source,
    byte* buffer, size_t capacity)
{
    level_partition_meta_set(source);
    fff = SDL_IOFromMem(buffer, capacity); assert(fff);
    xor_byte = 0; v_stamp = x_stamp = save_byte_offset = 0; write_error = false;
    save_write_partition_meta();
    size_t meta_size = (size_t)SDL_TellIO(fff);
    assert(!write_error && meta_size == 1 + 3 * 2
        + PARTITION_META_MAX * 3);
    save_wr_u32b(0xA1B2C3D4U);
    size_t length = (size_t)SDL_TellIO(fff);
    assert(!write_error && length == meta_size + 4);
    SDL_CloseIO(fff); fff = NULL;
    return length;
}
'''


READER = r'''
#include "fs/load.c"
#include <assert.h>

int fixture_read_partition_meta(const byte* buffer, size_t length, int extra,
    u32b* sentinel, size_t* consumed, u32b* decoded)
{
    fff = SDL_IOFromConstMem(buffer, length); assert(fff);
    xor_byte = 0; v_check = x_check = load_byte_offset = 0;
    sf_major = VERSION_MAJOR; sf_minor = VERSION_MINOR;
    sf_patch = extra < 0 ? VERSION_PATCH : 8;
    sf_extra = extra < 0 ? VERSION_EXTRA : extra;
    savefile_has_partition_meta = savefile_version_at_least(0, 9, 1, 7);
    savefile_has_partition_meta_types = savefile_version_at_least(0, 9, 1, 9);
    int result = load_read_partition_meta();
    *sentinel = 0;
    if (!result)
    {
        load_rd_u32b(sentinel);
        if (*sentinel != 0xA1B2C3D4U) result = -2;
    }
    *consumed = (size_t)SDL_TellIO(fff);
    *decoded = load_byte_offset;
    SDL_CloseIO(fff); fff = NULL;
    return result;
}
'''


TESTS = r'''
#include "cave/cave-atmosphere.h"
#include "cave/cave-environment.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-flood.h"
#include "cave/cave-water-flow.h"
#include "level-generation/level-generation-internal.h"
#include "player/player-upkeep-internal.h"
#include "supplies.h"
#include "init.h"
#include <assert.h>

extern cptr sdl_depth_menu_partition_label(void);
extern int light_up_to(int base_radius, object_type* o_ptr);
size_t fixture_write_partition_meta(const partition_meta_save*, byte*, size_t);
int fixture_read_partition_meta(const byte*, size_t, int, u32b*, size_t*, u32b*);

static byte encoded[512], plain[512], modified[512];

static void decode_stream(const byte* source, byte* target, size_t size)
{
    byte previous = 0;
    for (size_t i = 0; i < size; ++i)
    {
        byte current = source[i];
        target[i] = current ^ previous;
        previous = current;
    }
}

static void encode_stream(const byte* source, byte* target, size_t size)
{
    byte previous = 0;
    for (size_t i = 0; i < size; ++i)
    {
        previous ^= source[i];
        target[i] = previous;
    }
}

static void atmosphere_map(void)
{
    fresh_map();
    p_ptr->cur_map_hgt = 20;
    p_ptr->cur_map_wid = 30;
    p_ptr->depth = 10;
    p_ptr->py = 5;
    p_ptr->px = 5;
    p_ptr->is_dead = false;
    p_ptr->restoring = false;
    character_generated = true;
    character_dungeon = true;
    memset(inventory, 0, sizeof(inventory));
    memset(cave_o_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_o_idx));
    memset(cave_m_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_m_idx));

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            bool border = !y || !x || y == p_ptr->cur_map_hgt - 1
                || x == p_ptr->cur_map_wid - 1;
            cave_feat[y][x] = border ? FEAT_WALL_PERM : FEAT_FLOOR;
            cave_info[y][x] = border ? CAVE_WALL : 0;
            cave_natural[y][x] = 0;
            cave_m_idx[y][x] = 0;
        }

    /* One column partition per atmosphere candidate. */
    partition_meta_save meta = {0};
    meta.grid_rows = 1;
    meta.grid_cols = meta.partition_count = 3;
    meta.modes[0] = QUAD_MODE_CAVEY;
    meta.modes[1] = QUAD_MODE_ROOMY;
    meta.modes[2] = QUAD_MODE_RUINED;
    meta.atmospheres[0] = CAVE_ATMOSPHERE_HUSHED;
    meta.atmospheres[1] = CAVE_ATMOSPHERE_ECHOING;
    meta.atmospheres[2] = CAVE_ATMOSPHERE_DRAUGHTY;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            int pi = x < 10 ? 0 : (x < 20 ? 1 : 2);
            if (pi == 0)
                cave_natural[y][x] = 1;
            else
                cave_info[y][x] |= CAVE_ROOM;
        }
    level_partition_meta_set(&meta);
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    playerturn = 5432;
}

static void set_atmospheres(byte a0, byte a1, byte a2)
{
    partition_meta_save meta = {0};
    meta.grid_rows = 1;
    meta.grid_cols = meta.partition_count = 3;
    meta.modes[0] = QUAD_MODE_CAVEY;
    meta.modes[1] = QUAD_MODE_ROOMY;
    meta.modes[2] = QUAD_MODE_RUINED;
    meta.atmospheres[0] = a0;
    meta.atmospheres[1] = a1;
    meta.atmospheres[2] = a2;
    level_partition_meta_set(&meta);
}

static void put_at(int x)
{
    memset(cave_m_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_m_idx));
    p_ptr->py = 5;
    p_ptr->px = x;
    cave_m_idx[5][x] = -1;
}

static int light_kind(int sval)
{
    for (int i = 1; i < z_info->k_max; ++i)
        if (k_info[i].tval == TV_LIGHT && k_info[i].sval == sval)
            return i;
    return 0;
}

static void equip_jewel(void)
{
    int k_idx = light_kind(SV_LIGHT_LESSER_JEWEL);
    assert(k_idx);
    memset(&inventory[INVEN_LITE], 0, sizeof(inventory[INVEN_LITE]));
    inventory[INVEN_LITE].k_idx = k_idx;
    inventory[INVEN_LITE].tval = TV_LIGHT;
    inventory[INVEN_LITE].sval = SV_LIGHT_LESSER_JEWEL;
    inventory[INVEN_LITE].number = 1;
}

static void configure_tree_player(void)
{
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
    memset(p_ptr->have_ability, 0, sizeof(p_ptr->have_ability));
    memset(p_ptr->innate_ability, 0, sizeof(p_ptr->innate_ability));
    p_ptr->skill_base[S_STL] = 20;
    p_ptr->skill_base[S_SNG] = 5;
    p_ptr->have_ability[S_SNG][SNG_TREES] = true;
    p_ptr->innate_ability[S_SNG][SNG_TREES] = true;
    p_ptr->active_ability[S_SNG][SNG_TREES] = true;
    p_ptr->song1 = SNG_TREES;
    p_ptr->song2 = SNG_NOTHING;
    equip_jewel();
}

static void test_candidates_and_labels(void)
{
    atmosphere_map();
    assert(cave_atmosphere_at(5, 5) == CAVE_ATMOSPHERE_HUSHED);
    assert(cave_atmosphere_at(5, 15) == CAVE_ATMOSPHERE_ECHOING);
    assert(cave_atmosphere_at(5, 25) == CAVE_ATMOSPHERE_DRAUGHTY);

    cave_natural[5][5] = 0;
    assert(cave_atmosphere_at(5, 5) == CAVE_ATMOSPHERE_NONE);
    cave_natural[5][5] = 1;
    cave_info[5][15] &= ~CAVE_ROOM;
    assert(cave_atmosphere_at(5, 15) == CAVE_ATMOSPHERE_NONE);
    cave_info[5][15] |= CAVE_ROOM;
    cave_info[5][25] |= CAVE_ICKY;
    assert(cave_atmosphere_at(5, 25) == CAVE_ATMOSPHERE_NONE);
    cave_info[5][25] &= ~CAVE_ICKY;
    cave_info[5][25] |= CAVE_G_VAULT;
    assert(cave_atmosphere_at(5, 25) == CAVE_ATMOSPHERE_NONE);
    cave_info[5][25] &= ~CAVE_G_VAULT;

    put_at(5);
    assert(!strcmp(cave_atmosphere_name(CAVE_ATMOSPHERE_HUSHED), "Hushed"));
    assert(strstr(cave_atmosphere_description(CAVE_ATMOSPHERE_HUSHED), "Stealth +3"));
    assert(!strcmp(sdl_depth_menu_partition_label(), "Caves \xC2\xB7 Hushed"));
    put_at(15);
    assert(!strcmp(sdl_depth_menu_partition_label(), "Room \xC2\xB7 Echoing"));
    put_at(25);
    assert(!strcmp(sdl_depth_menu_partition_label(), "Ruin - Draughty area"));
    puts("Atmosphere candidates, protected cells, descriptions and SDL labels PASS.");
}

static void test_generation(void)
{
    bool seen_none = false, seen_hushed = false;
    bool seen_echoing = false, seen_draughty = false;
    atmosphere_map();
    for (u64b seed = 1; seed <= 600; ++seed)
    {
        set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
            CAVE_ATMOSPHERE_NONE);
        Rand_state_init(seed);
        cave_atmosphere_generate();
        byte got[PARTITION_META_MAX] = {0};
        cave_atmosphere_get_partitions(got);
        if (!got[0] && !got[1] && !got[2]) seen_none = true;
        if (got[0] == CAVE_ATMOSPHERE_HUSHED) seen_hushed = true;
        if (got[1] == CAVE_ATMOSPHERE_ECHOING) seen_echoing = true;
        if (got[2] == CAVE_ATMOSPHERE_DRAUGHTY) seen_draughty = true;
        assert(got[0] == CAVE_ATMOSPHERE_NONE || got[0] == CAVE_ATMOSPHERE_HUSHED);
        assert(got[1] == CAVE_ATMOSPHERE_NONE || got[1] == CAVE_ATMOSPHERE_ECHOING);
        assert(got[2] == CAVE_ATMOSPHERE_NONE || got[2] == CAVE_ATMOSPHERE_DRAUGHTY);
    }
    assert(seen_none && seen_hushed && seen_echoing && seen_draughty);

    p_ptr->depth = MORGOTH_DEPTH;
    set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
        CAVE_ATMOSPHERE_NONE);
    Rand_state_init(7);
    cave_atmosphere_generate();
    byte got[PARTITION_META_MAX] = {0};
    cave_atmosphere_get_partitions(got);
    assert(!got[0] && !got[1] && !got[2]);
    p_ptr->depth = UTUMNO_DEPTH;
    bool utumno_seen = false;
    for (u64b seed = 1; seed <= 600 && !utumno_seen; ++seed)
    {
        set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
            CAVE_ATMOSPHERE_NONE);
        Rand_state_init(seed);
        cave_atmosphere_generate();
        cave_atmosphere_get_partitions(got);
        utumno_seen = got[0] || got[1] || got[2];
    }
    assert(utumno_seen);
    puts("Atmosphere generation: terrain-weighted normal/variant outcomes, compatible partition kinds, Morgoth exclusion PASS.");
}

static void fill_partition_context(int pi, int style, int feature, int flow)
{
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (level_partition_index_for_point(y, x) == pi)
            {
                cave_color[y][x] = COLOR_STYLE_BASE + style;
                cave_feat[y][x] = feature;
                cave_water_flow_set(y, x, flow);
            }
}

static void test_terrain_weights(void)
{
    byte chances[PARTITION_META_MAX], saved[PARTITION_META_MAX], after[PARTITION_META_MAX];
    atmosphere_map();
    cave_atmosphere_get_partitions(saved);
    u64b rng = Rand_state_export();
    cave_atmosphere_generation_chances(chances);
    assert(chances[0] == 15 && chances[1] == 35 && chances[2] == 15);
    assert(Rand_state_export() == rng);
    cave_atmosphere_get_partitions(after);
    assert(!memcmp(saved, after, sizeof(saved)));

    /* Production style-levels.txt is loaded even when style.raw is cached. */
    const int soft_styles[] = {5, 6, 7, 42, 43, 44};
    for (unsigned i = 0; i < N_ELEMENTS(soft_styles); i++) {
        fill_partition_context(0, soft_styles[i], FEAT_FLOOR, CAVE_WATER_FLOW_CALM);
        cave_atmosphere_generation_chances(chances); assert(chances[0] == 50);
    }
    fill_partition_context(0, 62, FEAT_FLOOR, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 60);
    fill_partition_context(0, 62, FEAT_ICE, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 5);

    atmosphere_map();
    fill_partition_context(1, 42, FEAT_FLOOR, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 10);
    fill_partition_context(1, 62, FEAT_FLOOR, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 5);
    fill_partition_context(1, 62, FEAT_ICE, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 50);
    fill_partition_context(1, 0, FEAT_WATER, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 30);
    fill_partition_context(1, 0, FEAT_WATER, CAVE_WATER_FLOW_EAST);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 15);
    fill_partition_context(1, 0, FEAT_POISON, CAVE_WATER_FLOW_EAST);
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 15);

    atmosphere_map();
    fill_partition_context(0, 0, FEAT_WATER, CAVE_WATER_FLOW_EAST);
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 5);
    fill_partition_context(0, 0, FEAT_POISON, CAVE_WATER_FLOW_EAST);
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 5);

    atmosphere_map();
    fill_partition_context(2, 0, FEAT_BRIDGE_LAVA_H, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[2] == 45);
    fill_partition_context(2, 0, FEAT_BRIDGE_CHASM_H, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[2] == 45);
    fill_partition_context(2, 0, FEAT_BROKEN, CAVE_WATER_FLOW_CALM);
    cave_atmosphere_generation_chances(chances); assert(chances[2] == 25);
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 20; x < p_ptr->cur_map_wid - 1; x++)
            cave_feat[y][x] = (x % 2) ? FEAT_BRIDGE_LAVA_H : FEAT_BRIDGE_CHASM_H;
    cave_atmosphere_generation_chances(chances); assert(chances[2] == 65);

    /* A single soft tile cannot dictate a whole area's roll. */
    atmosphere_map();
    cave_color[1][1] = COLOR_STYLE_BASE + 42;
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 15);
    for (int y = 1; y <= 9; y++)
        for (int x = 1; x < 10; x++) cave_color[y][x] = COLOR_STYLE_BASE + 42;
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 33);

    /* Only one candidate room cell: isolate radius, walls and protected cells. */
    atmosphere_map();
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 10; x < 20; x++) cave_info[y][x] &= ~CAVE_ROOM;
    cave_info[5][15] |= CAVE_ROOM;
    cave_feat[5][17] = FEAT_LAVA;
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 20);
    cave_feat[5][16] = FEAT_WALL_EXTRA;
    cave_info[5][16] |= CAVE_WALL;
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 35);
    cave_feat[5][16] = FEAT_FLOOR; cave_info[5][16] &= ~CAVE_WALL;
    cave_info[5][17] |= CAVE_ICKY;
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 35);
    cave_info[5][17] &= ~CAVE_ICKY; cave_feat[5][17] = FEAT_FLOOR;
    cave_feat[5][18] = FEAT_LAVA;
    cave_atmosphere_generation_chances(chances); assert(chances[1] == 35);

    /* Explicit material metadata, not name/atlas inference; strict parsing. */
    atmosphere_map(); fill_partition_context(0, 0, FEAT_FLOOR, 0);
    char soft[] = "A:0:SOFT # test";
    assert(!parse_style_levels(soft, NULL));
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 50);
    const char* invalid[] = {"A:-1:SOFT", "A:64:SNOW", "A:0:UNKNOWN", "A:0:SOFT junk", "A:0:"};
    for (unsigned i = 0; i < N_ELEMENTS(invalid); i++) {
        char line[80]; SDL_strlcpy(line, invalid[i], sizeof(line));
        assert(parse_style_levels(line, NULL));
    }
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 50);
    char stone[] = "A:0:STONE";
    assert(!parse_style_levels(stone, NULL));
    cave_atmosphere_generation_chances(chances); assert(chances[0] == 15);
    puts("Terrain weights: soft/snow/ice, flowing water/acid, lava/chasm bridges, broken doors, whole-area coverage and wall/vault isolation PASS.");
}

static void test_bonus_and_movement(void)
{
    atmosphere_map();
    configure_tree_player();
    /* The tree song gives a light threshold at an ordinary 5 Song skill;
     * retain the dynamic check in case ability data changes its score. */
    int threshold = -1;
    for (int base = 0; base <= 40; ++base)
    {
        p_ptr->skill_base[S_SNG] = base;
        put_at(5); calc_bonuses(); calc_torch();
        int hush_light = p_ptr->cur_light;
        put_at(15); calc_bonuses(); calc_torch();
        if (p_ptr->cur_light > hush_light) { threshold = base; break; }
    }
    assert(threshold >= 0);
    p_ptr->skill_base[S_SNG] = threshold;
    set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
        CAVE_ATMOSPHERE_NONE);
    put_at(5); calc_bonuses(); calc_torch();
    int normal_stl = p_ptr->skill_use[S_STL];
    int normal_sng = p_ptr->skill_use[S_SNG];
    set_atmospheres(CAVE_ATMOSPHERE_HUSHED, CAVE_ATMOSPHERE_ECHOING,
        CAVE_ATMOSPHERE_DRAUGHTY);
    put_at(5); calc_bonuses(); calc_torch();
    int base_stl = p_ptr->skill_base[S_STL];
    int base_sng = p_ptr->skill_base[S_SNG];
    int hush_stl = p_ptr->skill_use[S_STL];
    int hush_sng = p_ptr->skill_use[S_SNG];
    int hush_light = p_ptr->cur_light;
    bool purchased_tree = p_ptr->active_ability[S_SNG][SNG_TREES];
    assert(hush_stl == normal_stl + 3);
    assert(hush_sng == normal_sng - 3);
    assert(purchased_tree);

    /* A normal player swap must recalculate Song before the torch. */
    monster_swap(5, 5, 5, 15);
    assert(p_ptr->px == 15 && p_ptr->py == 5);
    assert(p_ptr->skill_use[S_STL] == normal_stl - 3);
    assert(p_ptr->skill_use[S_SNG] == normal_sng + 3);
    assert(p_ptr->cur_light > hush_light);
    assert(p_ptr->active_ability[S_SNG][SNG_TREES] == purchased_tree);
    assert(p_ptr->skill_base[S_STL] == base_stl
        && p_ptr->skill_base[S_SNG] == base_sng);
    int echo_light = p_ptr->cur_light;

    /* Leaving Echo through the same path restores the threshold immediately. */
    monster_swap(5, 15, 5, 5);
    assert(p_ptr->px == 5 && p_ptr->py == 5);
    assert(p_ptr->skill_use[S_STL] == hush_stl);
    assert(p_ptr->skill_use[S_SNG] == hush_sng);
    assert(p_ptr->cur_light == hush_light);
    assert(echo_light > hush_light);

    /* Forced movement (monster into player) uses the destination atmosphere. */
    put_at(15);
    calc_bonuses(); calc_torch();
    memset(&mon_list[1], 0, sizeof(mon_list[1]));
    mon_list[1].r_idx = 1;
    mon_list[1].fy = 5; mon_list[1].fx = 5;
    mon_list[1].hp = mon_list[1].maxhp = 10;
    mon_list[1].ml = false;
    mon_max = 2;
    cave_m_idx[5][5] = 1;
    cave_m_idx[5][15] = -1;
    monster_swap(5, 5, 5, 15);
    assert(p_ptr->px == 5 && p_ptr->py == 5);
    assert(cave_m_idx[5][5] == -1 && cave_m_idx[5][15] == 1);
    assert(p_ptr->skill_use[S_STL] == hush_stl);
    assert(p_ptr->skill_use[S_SNG] == hush_sng);
    assert(p_ptr->cur_light == hush_light);
    puts("Full bonuses preserve base/purchased Song, and normal/forced swaps refresh skills and Trees light PASS.");
}

static void test_lights_and_gust(void)
{
    atmosphere_map();
    put_at(25);
    object_type torch = {0}, lantern = {0}, jewel = {0};
    torch.tval = TV_LIGHT; torch.sval = SV_LIGHT_TORCH; torch.timeout = 10000;
    lantern.tval = TV_LIGHT; lantern.sval = SV_LIGHT_LANTERN; lantern.timeout = 10000;
    jewel.tval = TV_LIGHT; jewel.sval = SV_LIGHT_LESSER_JEWEL;

    Rand_state_init(0x12345678);
    u64b rng = Rand_state_export();
    int p0 = cave_atmosphere_flame_penalty(5, 25, false);
    int p1 = cave_atmosphere_flame_penalty(5, 25, false);
    assert(p0 == p1 && Rand_state_export() == rng);
    assert(!strcmp(sdl_depth_menu_partition_label(), "Ruin - Draughty area"));
    assert(Rand_state_export() == rng);
    assert(light_up_to(RADIUS_TORCH, &torch) >= 0
        && light_up_to(RADIUS_TORCH, &torch) <= RADIUS_TORCH);
    assert(light_up_to(2, &lantern) >= 1 && light_up_to(2, &lantern) <= 2);
    assert(light_up_to(2, &jewel) == 2);

    int torch_hits = 0, lantern_hits = 0, previous_gust = -3;
    for (int t = 0; t < 6000; ++t)
    {
        playerturn = t;
        int gust = cave_atmosphere_flame_penalty(5, 25, false);
        torch_hits += gust;
        if (gust) { assert(t - previous_gust >= 3); previous_gust = t; }
        lantern_hits += cave_atmosphere_flame_penalty(5, 25, true);
    }
    assert(torch_hits > lantern_hits && lantern_hits > 0);
    int dark_torches = 0, dim_lanterns = 0;
    for (int t = 0; t < 6000; ++t)
    {
        playerturn = t;
        int torch_radius = light_up_to(RADIUS_TORCH, &torch);
        int lamp_radius = light_up_to(RADIUS_LANTERN, &lantern);
        assert(torch_radius >= 0 && torch_radius <= 1);
        assert(lamp_radius >= 1 && lamp_radius <= 2);
        dark_torches += torch_radius == 0;
        dim_lanterns += lamp_radius == 1;
    }
    assert(dark_torches == torch_hits && dim_lanterns == lantern_hits);
    assert(dark_torches < 6000 && dim_lanterns < 6000);
    /* Exercise calc_torch with a full-fuel lantern and compare a permanent
     * jewel across the same draught transition. */
    int lantern_k = light_kind(SV_LIGHT_LANTERN);
    assert(lantern_k);
    memset(&inventory[INVEN_LITE], 0, sizeof(inventory[INVEN_LITE]));
    inventory[INVEN_LITE].k_idx = lantern_k;
    inventory[INVEN_LITE].tval = TV_LIGHT;
    inventory[INVEN_LITE].sval = SV_LIGHT_LANTERN;
    inventory[INVEN_LITE].number = 1;
    player_light_set_fuel(&inventory[INVEN_LITE], 10000);
    assert(player_light_has_fuel(&inventory[INVEN_LITE]));
    calc_torch();
    assert(p_ptr->cur_light >= 1 && p_ptr->cur_light <= 2);
    equip_jewel();
    set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
        CAVE_ATMOSPHERE_NONE);
    calc_torch();
    int jewel_normal = p_ptr->cur_light;
    set_atmospheres(CAVE_ATMOSPHERE_NONE, CAVE_ATMOSPHERE_NONE,
        CAVE_ATMOSPHERE_DRAUGHTY);
    calc_torch();
    assert(p_ptr->cur_light == jewel_normal);
    puts("Draughty gusts: deterministic one-turn phases, no RNG/menu rerolls, bounded fuel radius, lamp frequency and jewel immunity PASS.");
}

static void compare_meta(const partition_meta_save* a, const partition_meta_save* b,
    bool compare_atmospheres)
{
    assert(a->grid_rows == b->grid_rows && a->grid_cols == b->grid_cols
        && a->partition_count == b->partition_count);
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        assert(a->modes[i] == b->modes[i]);
        assert(a->big_cave_types[i] == b->big_cave_types[i]);
        if (compare_atmospheres) assert(a->atmospheres[i] == b->atmospheres[i]);
        else assert(b->atmospheres[i] == CAVE_ATMOSPHERE_NONE);
    }
}

static void test_save_load(void)
{
    atmosphere_map();
    partition_meta_save source = {0};
    source.grid_rows = 1; source.grid_cols = source.partition_count = 3;
    source.modes[0] = QUAD_MODE_CAVEY;
    source.modes[1] = QUAD_MODE_ROOMY;
    source.modes[2] = QUAD_MODE_RUINED;
    source.atmospheres[0] = CAVE_ATMOSPHERE_HUSHED;
    source.atmospheres[1] = CAVE_ATMOSPHERE_ECHOING;
    source.atmospheres[2] = CAVE_ATMOSPHERE_DRAUGHTY;
    playerturn = 24680;
    put_at(25);
    int saved_gust = cave_atmosphere_flame_penalty(5, 25, true);
    size_t length = fixture_write_partition_meta(&source, encoded, sizeof(encoded));
    assert(length == 86);

    u32b sentinel, decoded; size_t consumed;
    assert(!fixture_read_partition_meta(encoded, length, -1,
        &sentinel, &consumed, &decoded));
    assert(sentinel == 0xA1B2C3D4U && consumed == length && decoded == length);
    partition_meta_save restored = {0};
    level_partition_meta_get(&restored);
    compare_meta(&source, &restored, true);
    assert(cave_atmosphere_flame_penalty(5, 25, true) == saved_gust);

    /* Version 30 has exactly the historical 57-byte partition block; its
     * sentinel remains aligned and the newly added atmosphere lane is empty. */
    decode_stream(encoded, plain, length);
    memmove(plain + 57, plain + 82, 4);
    size_t old_length = 61;
    encode_stream(plain, modified, old_length);
    set_atmospheres(CAVE_ATMOSPHERE_HUSHED, CAVE_ATMOSPHERE_ECHOING,
        CAVE_ATMOSPHERE_DRAUGHTY);
    assert(!fixture_read_partition_meta(modified, old_length, 30,
        &sentinel, &consumed, &decoded));
    assert(sentinel == 0xA1B2C3D4U && consumed == old_length && decoded == old_length);
    level_partition_meta_get(&restored);
    compare_meta(&source, &restored, false);
    assert(restored.atmospheres[0] == CAVE_ATMOSPHERE_NONE
        && restored.atmospheres[1] == CAVE_ATMOSPHERE_NONE
        && restored.atmospheres[2] == CAVE_ATMOSPHERE_NONE);
    assert(cave_atmosphere_at(5, 25) == CAVE_ATMOSPHERE_NONE);

    /* Every missing byte in the new tail must fail before a following marker
     * can be mistaken for atmosphere state. */
    for (size_t cut = 57; cut < 82; ++cut)
    {
        set_atmospheres(CAVE_ATMOSPHERE_HUSHED, CAVE_ATMOSPHERE_ECHOING,
            CAVE_ATMOSPHERE_DRAUGHTY);
        assert(fixture_read_partition_meta(encoded, cut, -1,
            &sentinel, &consumed, &decoded) != 0);
    }
    decode_stream(encoded, plain, length);
    plain[57 + 1] = CAVE_ATMOSPHERE_MAX;
    encode_stream(plain, modified, length);
    set_atmospheres(CAVE_ATMOSPHERE_HUSHED, CAVE_ATMOSPHERE_ECHOING,
        CAVE_ATMOSPHERE_DRAUGHTY);
    assert(fixture_read_partition_meta(modified, length, -1,
        &sentinel, &consumed, &decoded) != 0);
    puts("Partition atmosphere save: 82-byte current block, v30 alignment/defaults, every tail truncation and invalid kind rejection PASS.");
}

static void test_atmospheres(void)
{
    test_terrain_weights();
    test_candidates_and_labels();
    test_generation();
    test_bonus_and_movement();
    test_lights_and_gust();
    test_save_load();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    harness = prefix
    harness += '#include "cave/cave-fixtures.h"\n'
    harness += '#include "cave/cave-environment.h"\n'
    harness += '#include "cave/cave-flood.h"\n'
    harness += '#include "cave/cave-water-flow.h"\n'
    harness += '#include "monster/monster-ai.h"\n'
    harness += '#include "monster/monster-senses.h"\n'
    harness += fixture_block("terminal_extra") + "\n"
    harness += fixture_block("reset_map") + "\n"
    fresh_start = SCENT_TESTS.index("static void fresh_map(void)")
    fresh_end = SCENT_TESTS.index("\n}", fresh_start) + 2
    harness += SCENT_TESTS[fresh_start:fresh_end] + "\n"
    harness += TESTS + "\n" + init
    harness += "    style_info = (style_type*)style_head.info_ptr; style_name = style_head.name_ptr;\n"
    harness += "    test_atmospheres();\n"
    harness += '    puts("Cave atmosphere engine integration: PASS.");\n'
    harness += "    SDL_Quit(); return 0;\n}\n"

    sources = []
    for name, content in (("check.c", harness), ("writer.c", WRITER), ("reader.c", READER)):
        source = OUT / name
        source.write_text(content, encoding="utf-8")
        sources.append(str(source))

    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/fs/save.c.obj", "/src/fs/load.c.obj")
    objects = [path for path in objects if not path.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + path + '"' for path in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", *sources,
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        for startup in ("fresh", "cached"):
            result = subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                                    cwd=data, env=env, text=True, capture_output=True, timeout=120)
            (OUT / (startup + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            print(startup + " startup:", result.stdout, sep="\n", end="")
            if result.returncode:
                print(result.stderr[-4000:])
                result.check_returncode()


if __name__ == "__main__":
    main()
