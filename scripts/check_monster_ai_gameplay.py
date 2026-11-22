#!/usr/bin/env python3
"""Play isolated monster encounters through the real offscreen SDL interface.

Build standard first. This links the existing game objects, prepares a disposable
encounter, then calls the production dungeon loop. Every player action thereafter
arrives through GameControl's SDL event queue. A read-only test observer records
the monsters at input boundaries; no decisions, attacks, or turns are mocked.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
from typing import Callable
import uuid

import check_game_control as adapter

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/monster-ai-gameplay"
sys.path.insert(0, str(ROOT / "tools"))
from game_control import GameControl, ControlError


FIXTURE = r'''
#include "cJSON.h"
#include "birth/birth.h"
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include "monster/monster-social.h"
#include "monster/monster-routine.h"
#include "monster/monster-tactics.h"
#include "monster/monster-abilities.h"
#include "cave/cave-environment.h"
#include "cave/cave-events.h"
#include "tutorial/tutorial.h"
void dungeon(void);
extern bool current_build_vault_exact_token;

/* Observer-only identities follow live array compaction; index is not an ID. */
static unsigned fixture_actor_ids[MAX_MONSTERS], fixture_next_actor_id;
static unsigned fixture_actor_id(const monster_type* m)
{
    int i = (int)(m - mon_list);
    if (!fixture_actor_ids[i]) fixture_actor_ids[i] = ++fixture_next_actor_id;
    return fixture_actor_ids[i];
}
void __real_monster_social_remap(int, int);
void __wrap_monster_social_remap(int from, int to)
{
    if (from > 0 && from < MAX_MONSTERS && from != to)
    {
        if (to > 0 && to < MAX_MONSTERS)
            fixture_actor_ids[to] = fixture_actor_ids[from];
        fixture_actor_ids[from] = 0;
    }
    __real_monster_social_remap(from, to);
}

static int number(const cJSON* object, const char* key, int fallback)
{
    const cJSON* n = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(n) ? n->valueint : fallback;
}

static cJSON* fixture_read(const char* name)
{
    char path[1400];
    strnfmt(path, sizeof(path), "%s/%s", SDL_getenv("CONTROL_TEST_PROFILE"), name);
    size_t length;
    char* bytes = SDL_LoadFile(path, &length);
    assert(bytes);
    cJSON* json = cJSON_ParseWithLength(bytes, length);
    SDL_free(bytes);
    assert(json);
    return json;
}

static void action_event(const char* kind, const monster_type* m, int detail,
    int old_y, int old_x, bool melee, bool skipped)
{
    char path[1400];
    strnfmt(path, sizeof(path), "%s/actions.jsonl", SDL_getenv("CONTROL_TEST_PROFILE"));
    FILE* file = fopen(path, "ab");
    assert(file);
    fprintf(file, "{\"kind\":\"%s\",\"turn\":%ld,\"player_turn\":%ld,"
        "\"race\":%d,\"index\":%d,\"actor_id\":%u,\"detail\":%d,\"y\":%d,\"x\":%d,"
        "\"old_y\":%d,\"old_x\":%d,\"melee\":%d,\"skipped\":%d,"
        "\"hp\":%d,\"mana\":%d,\"energy\":%d,\"speed\":%d,\"alertness\":%d,"
        "\"poison\":%d,\"concentration\":%d,\"recovery\":%d,\"vengeance\":%d,"
        "\"min_range\":%d,\"best_range\":%d,\"sees_player\":%d}\n",
        kind, (long)turn, (long)playerturn, m->r_idx, (int)(m - mon_list), fixture_actor_id(m), detail,
        m->fy, m->fx, old_y, old_x, melee, skipped, m->hp, m->mana, m->energy,
        m->mspeed, m->alertness, m->poisoned, m->consecutive_attacks, m->smite_recovery, m->vengeance,
        m->min_range, m->best_range, monster_ai_can_see_player(m));
    assert(fclose(file) == 0);
}

void __real_monster_abilities_end_action(monster_type*, int, int, bool);
void __wrap_monster_abilities_end_action(monster_type* m, int y, int x, bool skipped)
{
    bool melee = m->ability_melee;
    __real_monster_abilities_end_action(m, y, x, skipped);
    action_event("action", m, m->previous_action[0], y, x, melee, skipped);
}

bool __real_make_attack_ranged(monster_type*, int);
bool __wrap_make_attack_ranged(monster_type* m, int attack)
{
    bool committed = __real_make_attack_ranged(m, attack);
    if (committed) action_event("ranged", m, attack, m->fy, m->fx, false, false);
    return committed;
}

bool __real_make_attack_normal(monster_type*);
bool __wrap_make_attack_normal(monster_type* m)
{
    action_event("melee_begin", m, 0, m->fy, m->fx, true, false);
    bool attacked = __real_make_attack_normal(m);
    if (attacked) action_event("melee", m, 0, m->fy, m->fx, true, false);
    return attacked;
}

bool __real_make_attack_reaction(monster_type*);
bool __wrap_make_attack_reaction(monster_type* m)
{
    bool attacked = __real_make_attack_reaction(m);
    if (attacked) action_event("reaction", m, 0, m->fy, m->fx, true, false);
    return attacked;
}

/* Instrument only the linked test executable, never the shipped interface. */
static void fixture_snapshot(bool waiting)
{
    if (waiting && character_dungeon && p_ptr) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "turn", turn);
        cJSON_AddNumberToObject(root, "player_turn", playerturn);
        /* Observe the same native choice rows displayed in the PNG. Some
         * terrain interactions are question menus, not yes/no prompts. */
        if (g_question_menu.active && !g_question_menu.context_hint) {
            cJSON* menu = cJSON_AddObjectToObject(root, "question_menu");
            cJSON_AddStringToObject(menu, "title", g_question_menu.title);
            cJSON* choices = cJSON_AddArrayToObject(menu, "choices");
            for (int i = 0; i < g_question_menu.count; ++i) {
                const sdl_question_menu_entry_state* row = &g_question_menu.entries[i];
                if (row->choice < 0 || row->text_attr == TERM_L_DARK || !row->letter[0]) continue;
                cJSON* choice = cJSON_CreateObject();
                char key[2] = { row->letter[0], 0 };
                cJSON_AddStringToObject(choice, "key", key);
                cJSON_AddStringToObject(choice, "label", row->text);
                cJSON_AddItemToArray(choices, choice);
            }
        }
        cJSON* player = cJSON_AddObjectToObject(root, "player");
        cJSON_AddNumberToObject(player, "y", p_ptr->py);
        cJSON_AddNumberToObject(player, "x", p_ptr->px);
        cJSON_AddNumberToObject(player, "hp", p_ptr->chp);
        cJSON_AddNumberToObject(player, "max_hp", p_ptr->mhp);
        cJSON_AddNumberToObject(player, "poison", p_ptr->poisoned);
        cJSON_AddNumberToObject(player, "speed", p_ptr->pspeed);
        cJSON_AddNumberToObject(player, "light", p_ptr->cur_light);
        cJSON_AddNumberToObject(player, "tile_light", cave_light[p_ptr->py][p_ptr->px]);
        cJSON_AddNumberToObject(player, "song", p_ptr->song1);
        cJSON_AddNumberToObject(player, "melee", p_ptr->skill_use[S_MEL]);
        cJSON_AddNumberToObject(player, "evasion", p_ptr->skill_use[S_EVN]);
        cJSON_AddNumberToObject(player, "stealth", p_ptr->skill_use[S_STL]);
        cJSON* monsters = cJSON_AddArrayToObject(root, "monsters");
        for (int i = 1; i < mon_max; i++) {
            const monster_type* m = &mon_list[i];
            if (!m->r_idx) continue;
            const monster_race* r = &r_info[m->r_idx];
            cJSON* o = cJSON_CreateObject();
            cJSON_AddItemToArray(monsters, o);
            cJSON_AddNumberToObject(o, "index", i);
            cJSON_AddNumberToObject(o, "actor_id", fixture_actor_id(m));
            cJSON_AddNumberToObject(o, "race", m->r_idx);
            cJSON_AddStringToObject(o, "name", r_name + r->name);
            cJSON_AddNumberToObject(o, "y", m->fy);
            cJSON_AddNumberToObject(o, "x", m->fx);
            cJSON_AddNumberToObject(o, "hp", m->hp);
            cJSON_AddNumberToObject(o, "max_hp", m->maxhp);
            cJSON_AddNumberToObject(o, "mana", m->mana);
            cJSON_AddNumberToObject(o, "energy", m->energy);
            cJSON_AddNumberToObject(o, "speed", m->mspeed);
            cJSON_AddNumberToObject(o, "min_range", m->min_range);
            cJSON_AddNumberToObject(o, "best_range", m->best_range);
            cJSON_AddNumberToObject(o, "noise_distance", flow_dist(FLOW_PLAYER_NOISE, m->fy, m->fx));
            cJSON_AddNumberToObject(o, "blocking", monster_blocking_bonus_dice_preview(m));
            cJSON_AddNumberToObject(o, "dodging", monster_dodging_bonus_preview(m));
            cJSON_AddNumberToObject(o, "alertness", m->alertness);
            cJSON_AddNumberToObject(o, "stance", m->stance);
            cJSON_AddNumberToObject(o, "morale", m->morale);
            cJSON_AddNumberToObject(o, "poison", m->poisoned);
            cJSON_AddNumberToObject(o, "recovery", m->smite_recovery);
            cJSON_AddNumberToObject(o, "vengeance", m->vengeance);
            cJSON_AddNumberToObject(o, "concentration", m->consecutive_attacks);
            cJSON_AddNumberToObject(o, "action", m->previous_action[0]);
            cJSON_AddNumberToObject(o, "feat", cave_feat[m->fy][m->fx]);
            cJSON_AddNumberToObject(o, "pending_hazard", cave_environment_pending_hazard(m->fy, m->fx));
            cJSON_AddNumberToObject(o, "song", m->song);
            cJSON_AddNumberToObject(o, "sense", m->ai.sense.kind);
            cJSON_AddNumberToObject(o, "sense_y", m->ai.sense.y);
            cJSON_AddNumberToObject(o, "sense_x", m->ai.sense.x);
            cJSON_AddNumberToObject(o, "sense_age", m->ai.sense.stale_decisions);
            cJSON_AddNumberToObject(o, "perception", r->per);
            cJSON_AddNumberToObject(o, "light_emission", r->light);
            cJSON_AddNumberToObject(o, "tile_light", cave_light[m->fy][m->fx]);
            cJSON_AddNumberToObject(o, "shot_projection", projectable(m->fy, m->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK));
            cJSON_AddNumberToObject(o, "search", m->ai.sense.search_decisions);
            cJSON_AddNumberToObject(o, "cast_reserve", m->ai.cast_reserve);
            cJSON_AddNumberToObject(o, "commander", m->squad.commander);
            cJSON_AddNumberToObject(o, "role", m->squad.role);
            cJSON_AddNumberToObject(o, "job", m->squad.job);
            cJSON_AddNumberToObject(o, "plan", m->squad.plan);
            cJSON_AddNumberToObject(o, "order_y", m->squad.y);
            cJSON_AddNumberToObject(o, "order_x", m->squad.x);
            cJSON_AddNumberToObject(o, "patrol", m->routine.style);
            cJSON_AddNumberToObject(o, "patrol_count", m->routine.count);
            cJSON_AddNumberToObject(o, "territory", m->routine.territory);
            cJSON_AddNumberToObject(o, "world_task", m->world.task);
            cJSON_AddNumberToObject(o, "supplies", m->world.supplies);
            cJSON_AddNumberToObject(o, "social", m->social_state);
            cJSON_AddNumberToObject(o, "rival", m->social_rival);
            cJSON_AddBoolToObject(o, "sees_player", monster_ai_can_see_player(m));
            cJSON_AddBoolToObject(o, "visual_perception_bonus", monster_has_visual_perception_bonus(m));
            cJSON_AddBoolToObject(o, "visible", m->ml);
            cJSON* knowledge = cJSON_AddArrayToObject(o, "knowledge");
            for (int f = 0; f < MON_AI_FEATURE_COUNT; f++)
                cJSON_AddItemToArray(knowledge, cJSON_CreateNumber(monster_ai_confidence(m, f)));
        }
        /* Terrain also detects real doors, webs, and bridge transformations. */
        cJSON* grid = cJSON_AddArrayToObject(root, "grid");
        for (int y = 0; y < p_ptr->cur_map_hgt; y++) {
            cJSON* row = cJSON_CreateArray();
            cJSON_AddItemToArray(grid, row);
            for (int x = 0; x < p_ptr->cur_map_wid; x++)
                cJSON_AddItemToArray(row, cJSON_CreateNumber(cave_feat[y][x]));
        }
        cJSON* bridges = cJSON_AddArrayToObject(root, "bridges");
        for (int y = 0; y < p_ptr->cur_map_hgt; y++)
            for (int x = 0; x < p_ptr->cur_map_wid; x++) {
                const environment_cell* cell = cave_environment_cell_at(y, x);
                if (!(cell->flags & ENV_BRIDGE)) continue;
                cJSON* bridge = cJSON_CreateObject();
                cJSON_AddItemToArray(bridges, bridge);
                cJSON_AddNumberToObject(bridge, "y", y);
                cJSON_AddNumberToObject(bridge, "x", x);
                cJSON_AddNumberToObject(bridge, "integrity", cell->integrity);
                cJSON_AddNumberToObject(bridge, "work", cell->work);
            }
        cJSON* light = cJSON_AddArrayToObject(root, "light_probes");
        const int probes[][2] = {{20,36},{20,37},{20,38},{20,39},{20,40},{19,37}};
        for (int i = 0; i < (int)N_ELEMENTS(probes); i++)
            cJSON_AddItemToArray(light, cJSON_CreateNumber(cave_light[probes[i][0]][probes[i][1]]));
        char path[1400];
        strnfmt(path, sizeof(path), "%s/telemetry.json", SDL_getenv("CONTROL_TEST_PROFILE"));
        char* bytes = cJSON_PrintUnformatted(root);
        FILE* file = fopen(path, "wb");
        assert(file && bytes);
        assert(fwrite(bytes, 1, strlen(bytes), file) == strlen(bytes));
        assert(fclose(file) == 0);
        cJSON_free(bytes);
        cJSON_Delete(root);
    }
}

bool __real_sdl_control_poll(bool waiting);
bool __wrap_sdl_control_poll(bool waiting)
{
    fixture_snapshot(waiting);
    return __real_sdl_control_poll(waiting);
}

static void fixture_prepare(void)
{
    cJSON* spec = fixture_read("scenario.json");
    player_wipe();
    flavor_init();
    Rand_state_init(number(spec, "seed", 20261112));
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    SDL_strlcpy(op_ptr->full_name, "AI Playtest", sizeof(op_ptr->full_name));
    process_player_name(true);
    /* Use the game's presentation setting; AI turns and RNG are unchanged. */
    op_ptr->delay_factor = number(spec, "animation_delay", 0);
    p_ptr->playing = p_ptr->wizard = p_ptr->restoring = true;
    p_ptr->noscore = 0x000a;
    p_ptr->game_type = number(spec, "game_type", 0);
    p_ptr->depth = p_ptr->max_depth = 10;
    p_ptr->cur_map_hgt = 40;
    p_ptr->cur_map_wid = 64;
    p_ptr->py = number(spec, "player_y", 20);
    p_ptr->px = number(spec, "player_x", 40);
    p_ptr->energy = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->image = number(spec, "hallucination", 0);
    p_ptr->blind = number(spec, "blindness", 0);
    for (int a = 0; a < A_MAX; a++) p_ptr->stat_base[a] = 3;
    /* Long encounters without changing monster stats or granting invulnerability.
     * This is correctness testing, not a claim about normal-character balance. */
    p_ptr->stat_base[A_CON] = number(spec, "constitution", 25);
    p_ptr->skill_base[S_MEL] = number(spec, "melee", 5);
    p_ptr->skill_base[S_EVN] = number(spec, "evasion", 10);
    p_ptr->skill_base[S_STL] = number(spec, "stealth", 0);
    p_ptr->skill_base[S_WIL] = number(spec, "will", 5);
    p_ptr->skill_base[S_SNG] = number(spec, "song_skill", 12);
    p_ptr->skill_base[S_PER] = 12;
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    if (!number(spec, "unarmoured", 0)) {
        object_prep(&inventory[INVEN_ARM], lookup_kind(TV_SHIELD, SV_ROUND_SHIELD));
        object_prep(&inventory[INVEN_BODY], lookup_kind(TV_MAIL, SV_MAIL_CORSLET));
    }
    object_prep(&inventory[INVEN_LITE], lookup_kind(TV_LIGHT, SV_LIGHT_TORCH));
    inventory[INVEN_LITE].timeout = 10000;
    inventory[INVEN_WIELD].name2 = number(spec, "weapon_ego", 0);
    inventory[INVEN_ARM].name2 = number(spec, "shield_ego", 0);
    u32b resistance = number(spec, "fire_resistance", 0) ? TR2_RES_FIRE
        : number(spec, "cold_resistance", 0) ? TR2_RES_COLD
        : number(spec, "poison_resistance", 0) ? TR2_RES_POIS : 0;
    if (resistance) {
        bool found = false;
        for (int i = 1; i < z_info->k_max; i++)
            if (k_info[i].tval == TV_RING && (k_info[i].flags2 & resistance)) {
                object_prep(&inventory[INVEN_LEFT], i); found = true; break;
            }
        assert(found);
    }
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        if (inventory[i].k_idx) {
            inventory[i].ident |= IDENT_KNOWN;
            p_ptr->total_weight += inventory[i].weight;
            p_ptr->equip_cnt++;
        }
    cJSON* abilities = cJSON_GetObjectItemCaseSensitive(spec, "abilities");
    cJSON* entry;
    cJSON_ArrayForEach(entry, abilities) {
        int skill = number(entry, "skill", -1), ability = number(entry, "ability", -1);
        assert(skill >= 0 && skill < S_MAX && ability >= 0 && ability < ABILITIES_MAX);
        p_ptr->innate_ability[skill][ability] = true;
        p_ptr->have_ability[skill][ability] = p_ptr->active_ability[skill][ability] = true;
    }
    int song = number(spec, "song", SNG_NOTHING);
    if (song < SNG_MAX) {
        p_ptr->innate_ability[S_SNG][song] = true;
        p_ptr->have_ability[S_SNG][song] = p_ptr->active_ability[S_SNG][song] = true;
        p_ptr->song1 = song;
    }
    character_generated = character_dungeon = true;
    character_icky = character_xtra = 0;
    turn = playerturn = 100;
    tutorial_set_enabled(false);
    set_sdl_gameplay_tutorial_mode(TUTORIAL_MODE_DISABLED);
    sdl_config_set_default_movement_bindings(&config, SDL_MOVEMENT_PRESET_CLASSIC_SIL);
    sdl_config_mark_keyboard_preset_prompt_seen();
    sdl_config_mark_mouse_tutorial_seen();
    sdl_config_mark_touch_tutorial_seen();
    sdl_config_mark_character_wheel_tutorial_seen();
    use_sound = false;
    visual_recognition = number(spec, "recognition", 1);
    op_ptr->level_entry_narrative_mode = 0;
    set_sdl_show_context_square_popups(false);
    screen_set_startup_supporting_panes_hidden(false);
    screen_set_startup_touch_pane_hidden(false);
    sdl_welcome_screen_hide();
    cave_environment_reset();
    cave_events_reset();
    monster_social_reset();
    p_ptr->truce = number(spec, "truce", 0);
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++) {
            bool border = !y || !x || y == p_ptr->cur_map_hgt - 1 || x == p_ptr->cur_map_wid - 1;
            cave_set_feat(y, x, border ? FEAT_WALL_PERM : FEAT_FLOOR);
            cave_info[y][x] |= CAVE_MARK;
            if (number(spec, "glow", 1)) cave_info[y][x] |= CAVE_GLOW;
        }
    cJSON* tiles = cJSON_GetObjectItemCaseSensitive(spec, "tiles");
    cJSON_ArrayForEach(entry, tiles) {
        int y1 = number(entry, "y1", 1), x1 = number(entry, "x1", 1);
        int y2 = number(entry, "y2", y1), x2 = number(entry, "x2", x1);
        assert(in_bounds(y1, x1) && in_bounds(y2, x2));
        for (int y = y1; y <= y2; y++) for (int x = x1; x <= x2; x++)
            cave_set_feat(y, x, number(entry, "feat", FEAT_FLOOR));
    }
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    cJSON* monsters = cJSON_GetObjectItemCaseSensitive(spec, "monsters");
    cJSON_ArrayForEach(entry, monsters) {
        int race = number(entry, "race", 41), y = number(entry, "y", 20), x = number(entry, "x", 32);
        /* The encounter loader may place authored vault-only guardians. Their
         * normal placement restriction remains enabled during gameplay. */
        int occupied_feat = cave_feat[y][x];
        if (number(entry, "exposed", 0)) cave_set_feat(y, x, FEAT_FLOOR);
        current_build_vault_exact_token = true;
        bool placed = place_monster_one(y, x, race, false, true, NULL);
        current_build_vault_exact_token = false;
        if (number(entry, "exposed", 0)) cave_set_feat(y, x, occupied_feat);
        if (!placed) {
            fprintf(stderr, "Fixture cannot place race %d at (%d,%d), feature %d\n", race, y, x, occupied_feat);
            exit(2);
        }
        monster_type* m = &mon_list[cave_m_idx[y][x]];
        m->alertness = number(entry, "alertness", ALERTNESS_ALERT);
        m->stance = number(entry, "stance", STANCE_CONFIDENT);
        m->tmp_morale = number(entry, "morale", 100);
        m->energy = number(entry, "energy", 0);
        m->skip_next_turn = m->skip_this_turn = false;
        m->poisoned = number(entry, "poison", 0);
        m->hp = MAX(1, m->maxhp * number(entry, "hp_percent", 100) / 100);
        m->smite_recovery = number(entry, "recovery", 0);
        m->confused = number(entry, "confused", 0);
        m->stunned = number(entry, "stunned", 0);
        m->mana = number(entry, "mana", m->mana);
        if (m->smite_recovery) m->skip_next_turn = true;
        if (number(entry, "heard", 0))
            monster_senses_hear(m, p_ptr->py, p_ptr->px);
        if (number(entry, "group", 0)) monster_social_set_group(m, number(entry, "group", 0));
        if (number(entry, "patrol", 0)) {
            m->routine.home_y = y; m->routine.home_x = x;
            assert(monster_routine_plan(m));
        }
    }
    cJSON* relations = cJSON_GetObjectItemCaseSensitive(spec, "relations");
    cJSON_ArrayForEach(entry, relations)
        assert(monster_group_set_relation(number(entry, "a", 0),
            number(entry, "b", 0), number(entry, "relation", 1)));
    if (number(spec, "environment", 0)) {
        cave_environment_seed();
        cave_environment_set_speed(0);
    }
    cJSON* cells = cJSON_GetObjectItemCaseSensitive(spec, "environment_cells");
    cJSON_ArrayForEach(entry, cells) {
        int y = number(entry, "y", 20), x = number(entry, "x", 13);
        environment_cell cell = *cave_environment_cell_at(y, x);
        cell.flags = number(entry, "flags", cell.flags);
        cell.pending_feat = number(entry, "pending", cell.pending_feat);
        cell.due = number(entry, "due", 10000);
        cell.bridge_feat = number(entry, "bridge", cell.bridge_feat);
        cell.underlay = number(entry, "underlay", cell.underlay);
        cell.material = number(entry, "material", cell.material);
        cell.integrity = number(entry, "integrity", cell.integrity);
        assert(cave_environment_restore_cell(y, x, cell));
    }
    cJSON* events = cJSON_GetObjectItemCaseSensitive(spec, "events");
    cJSON_ArrayForEach(entry, events)
        cave_event_emit(number(entry, "kind", CAVE_EVENT_COLLAPSE),
            number(entry, "y", 20), number(entry, "x", 15), number(entry, "volume", 25));
    cJSON_Delete(spec);
    p_ptr->update = PU_BONUS | PU_TORCH | PU_HP | PU_MANA | PU_UPDATE_VIEW | PU_MONSTERS;
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    p_ptr->csp = p_ptr->msp;
    p_ptr->energy = 100;
    p_ptr->redraw = PR_BASIC | PR_EXTRA | PR_MAP;
    Term_flush();
    inkey_next_set(NULL);
    inkey_xtra = false;
}
'''


def build(extra_wraps: tuple[str, ...] = ()) -> tuple[Path, dict]:
    OUT.mkdir(parents=True, exist_ok=True)
    build_dir = OUT / ("build-" + uuid.uuid4().hex[:8])
    build_dir.mkdir()
    source = build_dir / "fixture.c"
    harness = adapter.HARNESS.replace("int main(int argc, char** argv)", FIXTURE + "\nint main(int argc, char** argv)")
    harness = harness.replace('    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {',
        '    if (SDL_getenv("AI_PLAYTEST")) {\n'
        '        fixture_prepare(); dungeon(); fixture_snapshot(true); sdl_control_shutdown(); return 0;\n'
        '    }\n    if (SDL_getenv("CONTROL_TEST_INTERACTIVE")) {')
    source.write_text(harness, encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = build_dir / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith("/src/main.c.obj")))
    env = os.environ.copy()
    env.pop("SIL_MORE_CONTROL_DIR", None)
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"), AI_PLAYTEST="1")
    exe = build_dir / "fixture.exe"
    provenance = hashlib.sha256((BUILD / "sil-more.exe").read_bytes())
    provenance.update(Path(__file__).read_bytes())
    for template in sorted((ROOT / "lib/edit").glob("*.txt")):
        provenance.update(template.name.encode())
        provenance.update(template.read_bytes())
    env["AI_PLAYTEST_SIGNATURE"] = provenance.hexdigest()
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    *("-Wl,--wrap=" + name for name in ("sdl_control_poll",
                      "monster_abilities_end_action", "make_attack_ranged", "make_attack_normal",
                      "make_attack_reaction", "monster_social_remap", *extra_wraps)),
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    return exe, env


def telemetry(profile: Path) -> dict:
    for _ in range(20):
        try:
            return json.loads((profile / "telemetry.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            time.sleep(.01)
    raise AssertionError(f"No stable telemetry in {profile}")


def start(exe: Path, env: dict, name: str, spec: dict) -> tuple[Path, subprocess.Popen, GameControl]:
    profile = OUT / f"{name}-{uuid.uuid4().hex[:8]}"
    profile.mkdir()
    # The SDL config path is profile/user, so no real preferences are loaded.
    (profile / "scenario.json").write_text(json.dumps(spec, indent=2))
    control = GameControl(profile / "control", timeout=30)
    process = subprocess.Popen([str(exe), "--windowed", "--control-dir", str(control.directory)],
        cwd=ROOT, env=dict(env, CONTROL_TEST_PROFILE=str(profile)),
        stdout=(profile / "stdout.txt").open("w"), stderr=(profile / "stderr.txt").open("w"),
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"Fixture exited {process.returncode}: {profile}")
        if (profile / "telemetry.json").exists() and (control.directory / "session.json").exists():
            response = control.observe(capture=False)
            if response["state"]["dungeon"] and response["state"]["waiting_for_input"]:
                (profile / "session.json").write_text(json.dumps({"pid": process.pid, "profile": str(profile)}, indent=2))
                return profile, process, control
        time.sleep(.05)
    process.terminate()
    process.wait(timeout=5)
    raise AssertionError(f"No dungeon input boundary: {profile}")


def brief(state: dict) -> dict:
    return {k: v for k, v in state.items() if k != "grid"}


def validate(profile: Path, spec: dict, frames: list[dict]) -> dict:
    events = [json.loads(line) for line in (profile / "actions.jsonl").read_text().splitlines()]
    actions = [e for e in events if e["kind"] == "action"]
    assert actions, f"No scheduled monster actions: {profile}"
    monsters = [m for frame in frames for m in frame["monsters"]]
    final_by_id = {m["index"]: m for m in frames[-1]["monsters"]}
    def distance_from_player(frame, m):
        dy = abs(m["y"] - frame["player"]["y"])
        dx = abs(m["x"] - frame["player"]["x"])
        return max(dy, dx) + min(dy, dx) // 2
    checks = ["SDL input advances the production dungeon loop", "monster actions are scheduled",
              "actors occupy distinct squares"]
    for check in spec.get("checks", []):
        if check == "pursuit":
            assert any(e["kind"] == "melee" for e in events), f"No melee engagement: {profile}"
        elif check == "ranged":
            assert any(e["kind"] == "ranged" for e in events), f"No ranged action: {profile}"
        elif check == "squad":
            assert any(m["commander"] and m["role"] == 2 for m in monsters), f"No squad members: {profile}"
            for frame in frames:
                orders = [(m["order_y"], m["order_x"]) for m in frame["monsters"]
                          if m["commander"] and (m["order_y"] or m["order_x"])]
                assert len(orders) == len(set(orders)), f"Overlapping squad orders: {profile}"
            assert any(sum(distance_from_player(f, m) == 1 for m in f["monsters"]) >= 2
                       for f in frames), f"Squad never coordinates melee contact: {profile}"
            leaders = [m["index"] for m in frames[0]["monsters"] if m["race"] == 409]
            if leaders:
                for frame in frames:
                    leader = next(m for m in frame["monsters"] if m["index"] == leaders[0])
                    # Archers outside six-square contact may elect their own
                    # leader. A four-square interior also allows for both
                    # actors moving after this scheduler pass planned orders.
                    if not leader["sees_player"] or leader["stance"] == 1:
                        continue
                    for m in frame["monsters"]:
                        dy, dx = abs(m["y"] - leader["y"]), abs(m["x"] - leader["x"])
                        if max(dy, dx) + min(dy, dx) // 2 <= 4:
                            assert not m["commander"] or m["commander"] == leaders[0], f"Nearby viable Fankil loses command: {profile}"
        elif check == "archer_lanes":
            archers = {m["index"] for m in frames[0]["monsters"] if m["race"] in (51, 103)}
            assert archers
            for idx in archers:
                assert any(e["kind"] == "ranged" and e["index"] == idx
                           and e["player_turn"] >= frames[0]["player_turn"] + 14 for e in events), f"Archer stops firing: {profile}"
                blocked = 0
                previous = None
                for f in frames:
                    m = next((m for m in f["monsters"] if m["index"] == idx), None)
                    position = (m["y"], m["x"]) if m else None
                    blocked = blocked + 1 if (m and m["shot_projection"] == 1
                        and m["action"] == 10 and position == previous) else 0
                    assert blocked < 8, f"Archer pinned behind ally: {profile}"
                    previous = position
        elif check == "archer_range":
            sightings = [e for e in actions if e["sees_player"] and not e["skipped"]]
            assert sightings, f"Archer never recognizes player: {profile}"
            assert all(not (e["min_range"] == 1 and e["best_range"] == 1) for e in sightings), f"Cached melee range: {profile}"
            if not spec.get("recognition", 1):
                assert any(m["best_range"] == 8 for m in monsters), f"No shooting range established: {profile}"
            else:
                assert any(2 <= m["best_range"] <= 8 for m in monsters), f"No recognized shooting range: {profile}"
            assert not any(e["kind"] == "melee" for e in events), f"Archer closes for melee: {profile}"
        elif check == "archer_recognition":
            # An archer must not repeatedly retreat beyond recognition and
            # chase its old sighting. A crowded squad may briefly detour out
            # of sight while moving around an ally to a clear firing position.
            archers = {m["index"] for m in frames[0]["monsters"] if m["race"] in (51, 103)}
            assert archers
            for idx in archers:
                actor = [(f, m) for f in frames for m in f["monsters"] if m["index"] == idx]
                acquired = next((i for i, (_, m) in enumerate(actor) if m["sees_player"]), None)
                assert acquired is not None, f"Archer never acquires player: {profile}"
                lost = 0
                allowance = 2 if len(frames[0]["monsters"]) > 1 else 0
                for _, m in actor[acquired:]:
                    lost = 0 if m["sees_player"] else lost + 1
                    assert lost <= allowance, f"Archer abandons visual contact: {profile}"
                assert any(e["kind"] == "ranged" and e["index"] == idx
                           and e["player_turn"] >= frames[0]["player_turn"] + 20
                           for e in events), f"Archer does not sustain fire: {profile}"
        elif check == "concentration":
            assert any(e["concentration"] == 3 for e in actions), f"No concentration chain: {profile}"
            assert all(e["concentration"] <= 3 for e in actions)
            assert all(not e["concentration"] for e in actions
                       if e["skipped"] or not e["melee"] or (e["y"], e["x"]) != (e["old_y"], e["old_x"]))
        elif check == "smite":
            assert any(e["recovery"] == 1 for e in actions), f"No Smite: {profile}"
            assert any(e["recovery"] == 2 and e["skipped"] for e in actions), f"No paid recovery: {profile}"
            for first, second in zip(actions, actions[1:]):
                if first["recovery"] == 1:
                    assert second["recovery"] == 2 and second["skipped"] and not second["melee"]
        elif check == "sprint":
            assert any(e["speed"] >= 4 for e in actions), f"Sprinting never activates: {profile}"
        elif check == "escape":
            assert any((e["y"], e["x"]) != (e["old_y"], e["old_x"]) for e in actions), f"No escape movement: {profile}"
            assert all(not m["pending_hazard"] for m in frames[-1]["monsters"]), f"Still on impending hazard: {profile}"
        elif check == "investigate":
            noise = spec["events"][0]
            assert any(max(abs(m["y"] - noise["y"]), abs(m["x"] - noise["x"])) <= 1
                       for m in monsters), f"Noise origin never investigated: {profile}"
        elif check == "poison_escape":
            for initial in frames[0]["monsters"]:
                idx = initial["index"]
                escape = next(((f, m) for f in frames[1:] for m in f["monsters"]
                               if m["index"] == idx and m["feat"] != 87), None)
                assert escape and escape[1]["hp"] > 0, f"No living escape: {profile}"
                before_exit = [e for e in actions if e["index"] == idx
                               and e["player_turn"] <= escape[0]["player_turn"]]
                assert len(before_exit) <= 2 + bool(initial["recovery"]), f"Escape needlessly delayed: {profile}"
                if idx in final_by_id:
                    assert final_by_id[idx]["hp"] > 0 and final_by_id[idx]["feat"] != 87
                else:
                    # The forced centre placement costs two unavoidable
                    # 8-point doses. A naturally rolled 15-HP orc must still
                    # escape promptly, but cannot survive those pending ticks.
                    assert initial["hp"] <= initial["poison"] + 16, f"Survivable escape victim died: {profile}"
                    assert escape[1]["hp"] <= escape[1]["poison"]
                if initial["recovery"] == 1:
                    first = next(e for e in actions if e["index"] == idx)
                    assert first["skipped"] and first["recovery"] == 2 and not first["melee"]
                    assert first["hp"] < initial["hp"] and first["poison"] > 0
        elif check in ("open_door", "bash_door", "pass_door", "phase_wall"):
            grid = json.loads((profile / "final-state.json").read_text())["grid"]
            assert any(m["x"] > 35 for m in monsters), f"Obstacle never crossed: {profile}"
            if check == "phase_wall":
                assert all(grid[y][35] == 56 for y in range(12, 29)), f"Phaser changed wall: {profile}"
            elif check == "pass_door":
                assert grid[20][35] == 32, f"Pass Door altered door: {profile}"
            else:
                assert grid[20][35] in (4, 5), f"Door stays closed: {profile}"
                if check == "bash_door":
                    assert any("burst" in message for f in frames for message in f.get("messages", [])), f"Door was not bashed: {profile}"
        elif check == "terrain_crossing":
            feat = spec["crossing_feat"]
            for initial in frames[0]["monsters"]:
                idx = initial["index"]
                assert any(m["index"] == idx and m["feat"] == feat for m in monsters), f"Terrain never entered: {profile}"
                assert idx in final_by_id and final_by_id[idx]["hp"] > 0
                assert any(m["index"] == idx and m["x"] > 36 for m in monsters), f"Terrain never crossed: {profile}"
        elif check == "archer_retreat":
            initial = frames[0]["monsters"][0]
            retreat = [f for f in frames[1:] if any(m["index"] == initial["index"]
                       and m["x"] < initial["x"] and m["shot_projection"] == 2 for m in f["monsters"])]
            assert retreat, f"Archer treats its vacated square as a blocker: {profile}"
            assert any(e["kind"] == "ranged" and e["player_turn"] > retreat[0]["player_turn"]
                       for e in events), f"Archer cannot fire after retreat: {profile}"
        elif check == "blocking":
            actor = [m for m in monsters if m["race"] == 82]
            moved = [m for m in actor if m["action"] in (1, 2, 3, 4, 6, 7, 8, 9)]
            assert moved and all(m["blocking"] == 0 for m in moved), f"Moving shield retains Blocking: {profile}"
            first_move = next(i for i, m in enumerate(actor) if m in moved)
            assert any(m["blocking"] > 0 and m["action"] == 10 for m in actor[first_move + 1:]), f"Blocking never resumes after movement: {profile}"
        elif check == "dodging":
            actor = [m for m in monsters if m["race"] == 408]
            assert any(m["dodging"] == 3 for m in actor), f"No moving Dodging: {profile}"
            first_dodge = next(i for i, m in enumerate(actor) if m["dodging"] == 3)
            assert any(m["dodging"] == 0 and m["action"] == 10 for m in actor[first_dodge + 1:]), f"Dodging never resets after moving: {profile}"
        elif check == "truce":
            assert not any(e["kind"] in ("melee", "ranged", "reaction") for e in events), f"Truce attacked player: {profile}"
        elif check == "tunnel":
            grid = json.loads((profile / "final-state.json").read_text())["grid"]
            assert any(grid[y][35] != 56 for y in range(12, 29)), f"Wall never tunnelled: {profile}"
        elif check == "stationary":
            assert all((e["y"], e["x"]) == (e["old_y"], e["old_x"]) for e in actions)
        elif check == "unseen":
            assert all(not m["sees_player"] for m in monsters)
            assert all(not any(m["knowledge"]) for m in monsters), f"Unwitnessed learning: {profile}"
            assert not any(e["kind"] in ("melee", "ranged") for e in events)
        elif check == "patrol":
            assert any(m["patrol"] == 2 and m["patrol_count"] >= 12 for m in monsters)
            assert len({(e["y"], e["x"]) for e in actions}) >= 8, f"Stalled patrol: {profile}"
        elif check == "builder":
            assert any(m["supplies"] < 24 and m["world_task"] == 2 for m in monsters), f"No bridge work: {profile}"
            bridge = next(b for b in frames[-1]["bridges"] if (b["y"], b["x"]) == (20, 13))
            assert bridge["integrity"] == 100 and bridge["work"] == 0, f"Repair incomplete: {profile}"
        elif check == "zone":
            assert any(e["kind"] == "reaction" and e["race"] == 406 for e in events), f"No zone reaction: {profile}"
            assert any("zone of control" in message for f in frames for message in f.get("messages", []))
        elif check == "vengeance":
            initial = next(m["hp"] for m in frames[0]["monsters"] if m["race"] == 253)
            assert any(m["race"] == 253 and m["hp"] < initial for m in monsters)
            blows = [e for e in events if e["kind"] in ("melee_begin", "melee") and e["race"] == 253]
            assert any(a["kind"] == "melee_begin" and a["vengeance"] == 1
                       and b["kind"] == "melee" and b["vengeance"] == 0
                       and a["player_turn"] == b["player_turn"] for a, b in zip(blows, blows[1:])), f"No charge consumed: {profile}"
        elif check == "shadow":
            shadow = [m for m in monsters if m["race"] == 408]
            assert shadow and all(m["light_emission"] == -3 for m in shadow)
            assert len({(m["y"], m["x"]) for m in shadow}) > 1
            assert len({tuple(f["light_probes"]) for f in frames}) > 1, f"Lighting never changes: {profile}"
            assert any(v < 0 for f in frames for v in f["light_probes"]), f"No darkness measured: {profile}"
        elif check == "social":
            assert any(m["social"] for m in monsters) or len(frames[-1]["monsters"]) < len(frames[0]["monsters"])
        elif check == "fire_learning":
            assert any(m["knowledge"][0] > 0 for m in monsters), f"No observed fire resistance: {profile}"
        elif check == "slay_fear":
            assert any(m["knowledge"][32] > 0 for m in monsters), f"No learned weapon fear: {profile}"
        elif check == "song":
            assert sum(f["player"]["song"] == spec["song"] for f in frames[1:]) >= 5, f"Song not sustained: {profile}"
        elif check == "song_spacing":
            # A simultaneous squad order can be the chosen feedback message.
            # Verify the paid retreat and subsequent fire, not its wording.
            initial = {m["index"]: m["noise_distance"] for m in frames[0]["monsters"]}
            assert any(m["noise_distance"] > initial[m["index"]]
                       and m["shot_projection"] == 2
                       and any(e["kind"] == "ranged" and e["index"] == m["index"]
                               and e["player_turn"] > f["player_turn"] for e in events)
                       for f in frames for m in f["monsters"]), f"No paid song retreat followed by fire: {profile}"
        elif check == "song_fear":
            assert any(m["stance"] == 1 and distance_from_player(f, m) >= 8
                       for f in frames for m in f["monsters"]), f"Elbereth provokes no retreat: {profile}"
        elif check == "song_challenge":
            assert any(m["stance"] == 3 and distance_from_player(f, m) == 1
                       for f in frames for m in f["monsters"] if m["race"] == 82), f"Challenge provokes no approach: {profile}"
        elif check == "scent":
            tracking = [(f, m) for f in frames for m in f["monsters"]
                        if m["sense"] == 3 and not m["sees_player"]]
            assert len({(m["y"], m["x"]) for _, m in tracking}) >= 4, f"No unseen trail following: {profile}"
            assert any(m["x"] == 42 and m["y"] > 10 for _, m in tracking), f"Tracker never rounds corner: {profile}"
            assert all((m["sense_y"], m["sense_x"]) != (f["player"]["y"], f["player"]["x"])
                       for f, m in tracking), f"Hidden player position leaked to scent: {profile}"
        elif check in ("player_flanking", "player_retreat", "player_focus"):
            feature = {"player_flanking": 10, "player_retreat": 11, "player_focus": 20}[check]
            assert any(m["knowledge"][feature] > 0 for m in monsters), f"No witnessed {check}: {profile}"
        else:
            raise AssertionError(f"Unknown check {check}")
        checks.append(check)
    return dict(checks=checks, monster_actions=len(actions),
                melee_attacks=sum(e["kind"] == "melee" for e in events),
                reaction_attacks=sum(e["kind"] == "reaction" for e in events),
                ranged_attacks=sum(e["kind"] == "ranged" for e in events),
                moves=sum((e["y"], e["x"]) != (e["old_y"], e["old_x"]) for e in actions))


def run(exe: Path, env: dict, name: str, spec: dict,
        keys: list[str] | Callable[[Path, list[dict]], str | None]) -> dict:
    profile, process, game = start(exe, env, name, spec)
    frames = []
    try:
        game.observe(output=profile / "initial.png")
        frames.append(dict(key="initial", **brief(telemetry(profile))))
        died = False
        policy = keys if callable(keys) else None
        pending_keys = [] if policy else list(keys)
        while pending_keys or policy:
            key = pending_keys.pop(0) if pending_keys else policy(profile, frames)
            if key is None:
                break
            response = game.key(key, capture=False)
            assert response["ok"], response.get("error", "Control request failed")
            assert response["state"]["video_driver"] == "dummy"
            frame = brief(telemetry(profile))
            if response["closed"]:
                assert spec.get("allow_death") and response["state"]["player"]["hp"] <= 0, "Game closed unexpectedly"
                frame["player"].update(response["state"]["player"])
                frame["player_turn"] = response["state"]["player_turn"]
                frame["turn"] = response["state"]["turn"]
                died = True
            frame.update(key=key, messages=response["state"]["messages"],
                         terminal=[x.rstrip() for x in response["state"]["terminal"] if x.strip()])
            frames.append(frame)
            if (spec.get("confirm_acid") and not died
                    and frame.get("question", "").startswith("Step into poisonous acid?")):
                game.observe(output=profile / f"acid-confirmation-{len(frames):03}.png")
                pending_keys.insert(0, "y")
            if spec.get("confirm_traps") and not died:
                for choice in frame.get("question_menu", {}).get("choices", []):
                    if choice["label"] == "Step onto it":
                        game.observe(output=profile / f"trap-confirmation-{len(frames):03}.png")
                        pending_keys.insert(0, choice["key"])
                        break
            if (spec.get("decline_chasms") and not died
                    and frame.get("question", "").startswith("Step into the chasm?")):
                game.observe(output=profile / f"chasm-confirmation-{len(frames):03}.png")
                pending_keys.insert(0, "n")
            if died:
                break
            assert frame["player"]["hp"] > 0, f"Player died: {profile}"
            positions = [(m["y"], m["x"]) for m in frame["monsters"]]
            assert len(positions) == len(set(positions)), f"Monster overlap: {profile}"
            assert (frame["player"]["y"], frame["player"]["x"]) not in positions
        if not died:
            game.observe(output=profile / "final.png")
        final_state = telemetry(profile)
        if died:
            final_state.update(player=frames[-1]["player"], player_turn=frames[-1]["player_turn"], turn=frames[-1]["turn"])
        (profile / "final-state.json").write_text(json.dumps(final_state, indent=2))
        result = dict(name=name, profile=str(profile), actions=len(frames) - 1, died=died,
            seed=spec.get("seed", 20261112), fixture_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
            build_signature=env["AI_PLAYTEST_SIGNATURE"],
            player_turns=frames[-1]["player_turn"] - frames[0]["player_turn"],
            damage=frames[0]["player"]["hp"] - frames[-1]["player"]["hp"],
            initial=frames[0], final=frames[-1])
        assert result["player_turns"] > 0, f"Inputs never reached gameplay: {profile}"
        result.update(validate(profile, spec, frames))
        print(json.dumps({k: v for k, v in result.items() if k not in ("initial", "final")}), flush=True)
        return result
    finally:
        (profile / "trace.json").write_text(json.dumps(frames, indent=2), encoding="utf-8")
        if process.poll() is None:
            try:
                game.key("Ctrl+x", capture=False)
                process.wait(timeout=5)
            except (ControlError, subprocess.TimeoutExpired):
                process.terminate()
                process.wait(timeout=5)


def scenarios() -> list[tuple[str, dict, list[str]]]:
    """Separate behavior assertions from broad species smoke encounters."""
    cases = []

    def add(name, spec, turns=24, keys=None, checks=()):
        cases.append((name, dict(spec, checks=list(checks)), keys or ["KP5"] * turns))

    add("recognized-orc-pursuit", {"monsters": [{"race": 41, "x": 37}]}, 24, checks=["pursuit"])
    for name, race, stun in [("orc-first-sighting", 51, 0), ("easterling-first-sighting", 103, 0),
                              ("easterling-stunned-sighting", 103, 6)]:
        add(name, {"monsters": [{"race": race, "x": 32, "heard": 1, "stunned": stun}]},
            30, checks=["ranged", "archer_range", "archer_recognition"])
    archer_corridor = [
        {"y1": 1, "y2": 38, "x1": 1, "x2": 62, "feat": 63},
        {"y1": 20, "x1": 25, "x2": 50, "feat": 1}]
    add("archer-recognition-boundary", {"tiles": archer_corridor,
        "monsters": [{"race": 51, "x": 37, "heard": 1}]}, 36,
        checks=["ranged", "archer_range", "archer_recognition"])
    add("orc-squad", {"recognition": 0, "monsters": [
        {"race": 91, "x": 31}, {"race": 41, "y": 18, "x": 29},
        {"race": 61, "y": 22, "x": 29}, {"race": 51, "y": 21, "x": 27}]}, 36, checks=["squad", "pursuit", "ranged"])
    add("fankil-squad", {"recognition": 0, "monsters": [
        {"race": 409, "x": 32}, {"race": 82, "y": 18, "x": 30},
        {"race": 82, "y": 22, "x": 30}, {"race": 103, "y": 21, "x": 28},
        {"race": 103, "y": 19, "x": 28}]}, 40, checks=["squad", "pursuit", "ranged", "archer_lanes"])
    # Factory recognition and ordinary morale exercise the conditions of real
    # encounters, including archers guided into contact by a perceptive leader.
    add("orc-squad-recognition", {"monsters": [
        {"race": 91, "x": 35, "heard": 1, "morale": 0},
        {"race": 41, "y": 18, "x": 34, "heard": 1, "morale": 0},
        {"race": 61, "y": 22, "x": 34, "heard": 1, "morale": 0},
        {"race": 51, "y": 21, "x": 33, "heard": 1, "morale": 0}]}, 36,
        checks=["squad", "pursuit", "ranged", "archer_lanes", "archer_recognition"])
    add("fankil-squad-recognition", {"monsters": [
        {"race": 409, "x": 34, "heard": 1, "morale": 0},
        {"race": 82, "y": 18, "x": 33, "heard": 1, "morale": 0},
        {"race": 82, "y": 22, "x": 33, "heard": 1, "morale": 0},
        {"race": 103, "y": 19, "x": 31, "heard": 1, "morale": 0},
        {"race": 103, "y": 21, "x": 31, "heard": 1, "morale": 0}]}, 36,
        checks=["squad", "pursuit", "ranged", "archer_lanes", "archer_recognition"])
    for name, races in [("wolf-pack", [11, 11, 11, 11]), ("spider-pack", [44, 62, 32, 32])]:
        add(name, {"recognition": 0, "monsters": [dict(race=r, y=y, x=x)
            for r, (y, x) in zip(races, [(20, 34), (18, 32), (22, 32), (20, 30)])]}, 26, checks=["squad", "pursuit"])
    add("ringwion-concentration", {"monsters": [{"race": 403, "x": 39}]}, 12, checks=["concentration", "pursuit"])
    add("nambatur-smite", {"monsters": [{"race": 411, "x": 39}]}, 10, checks=["smite", "pursuit"])
    add("langon-sprinting", {"recognition": 0, "monsters": [{"race": 407, "x": 23}]},
        keys=["KP6"] * 8 + ["KP5"] * 8, checks=["sprint"])
    add("angacirca-zone", {"monsters": [{"race": 406, "x": 39}]},
        keys=["KP5"] * 3 + ["KP8", "KP6", "KP2", "KP4"] * 3, checks=["pursuit", "zone"])
    add("duron-mobile-shadow", {"glow": 1, "recognition": 0, "monsters": [{"race": 408, "x": 38}, {"race": 61, "x": 36}]},
        keys=["KP5"] * 4 + ["KP8", "KP6", "KP2", "KP4"] * 3, checks=["shadow", "dodging"])
    add("easterling-blocking", {"recognition": 0, "monsters": [{"race": 82, "x": 36}]},
        20, checks=["pursuit", "blocking"])
    add("morgoth-truce", {"truce": 1, "recognition": 0, "monsters": [
        {"race": 251, "x": 36}, {"race": 41, "x": 34}]}, 12, checks=["truce"])
    wall = {"y1": 1, "y2": 38, "x1": 30, "feat": 63}
    for name, race in [("pending-lava-ground", 41), ("pending-lava-flying", 407)]:
        add(name, {"environment": 1, "player_x": 50, "tiles": [wall],
            "environment_cells": [{"y": 20, "x": 12, "pending": 85}],
            "monsters": [{"race": race, "x": 12, "alertness": -20}]}, 8, checks=["escape", "unseen"])
    for name, race, pending in [("pending-lava-fire-immune", 181, 85),
                                ("pending-chasm-flying", 407, 2)]:
        add(name, {"environment": 1, "player_x": 50, "tiles": [wall],
            "environment_cells": [{"y": 20, "x": 12, "pending": pending}],
            "monsters": [{"race": race, "x": 12, "alertness": -20}]}, 8, checks=["stationary", "unseen"])
    add("bridge-repair", {"environment": 1, "player_x": 50,
        "tiles": [wall, {"y1": 20, "x1": 13, "feat": 88}],
        "environment_cells": [{"y": 20, "x": 13, "flags": 16, "bridge": 88,
            "underlay": 84, "material": 1, "integrity": 40}],
        "monsters": [{"race": 41, "x": 12, "alertness": -10}]}, 20, checks=["builder", "unseen"])
    add("physical-noise-investigation", {"player_x": 50, "tiles": [wall],
        "events": [{"y": 20, "x": 19}], "monsters": [{"race": 82, "x": 12, "alertness": -20}]}, 24, checks=["investigate", "unseen"])
    add("patrol-around-pillar", {"player_x": 50, "tiles": [wall,
        {"y1": 17, "y2": 23, "x1": 14, "x2": 18, "feat": 56}],
        "monsters": [{"race": 41, "x": 12, "alertness": -10, "patrol": 1}]}, 80, checks=["patrol", "unseen"])
    add("hostile-bands", {"player_x": 50, "tiles": [wall],
        "relations": [{"a": 8, "b": 9}], "monsters": [
            {"race": 61, "x": 12, "group": 8, "alertness": -10},
            {"race": 82, "x": 15, "group": 9, "alertness": -10}]}, 60, checks=["social", "unseen"])
    for name, race, extra in [("poison-archer", 51, {}), ("poison-regenerator", 214, {}),
                              ("poison-smite-recovery", 411, {"recovery": 1, "poison": 20})]:
        add(name, {"recognition": 0, "tiles": [{"y1": 19, "y2": 21, "x1": 33, "x2": 35, "feat": 87}],
            "monsters": [dict(race=race, x=34, exposed=1, **extra)]}, 14, checks=["poison_escape"])
    add("fire-dragon-resistance", {"recognition": 0, "fire_resistance": 1,
        "monsters": [{"race": 181, "x": 34}]}, 24, checks=["fire_learning", "ranged"])
    add("doriath-weapon-fear", {"recognition": 0, "weapon_ego": 22, "melee": 15,
        "monsters": [{"race": 211, "x": 39}, {"race": 62, "x": 38, "y": 19}]},
        keys=["KP4", "KP5"] * 7, checks=["slay_fear"])
    # A straight narrow passage makes door capability and legal traversal visible.
    corridor = [{"y1": 1, "y2": 38, "x1": 1, "x2": 62, "feat": 63},
                {"y1": 20, "x1": 25, "x2": 45, "feat": 1},
                {"y1": 20, "x1": 35, "feat": 32}]
    for name, race in [("open-door", 41), ("bash-door", 74), ("pass-door", 407)]:
        tiles = [dict(tile) for tile in corridor]
        if name == "bash-door":
            tiles[-1]["feat"] = 33  # Locked: ordinary opening cannot pass this test.
        actor = {"race": race, "x": 34, "heard": 1}
        if race == 407:
            # Calm heralds keep their authored spy distance; an aggressive
            # herald tests paid passage and pursuit instead of that behavior.
            actor.update(stance=3, morale=1000)
            # Leave enough room beyond the door for his later standoff range.
            tiles[1]["x2"] = 55
        add(name, {"recognition": 0, "player_x": 50 if race == 407 else 40,
            "tiles": tiles, "monsters": [actor]},
            28, checks=["pursuit", name.replace("-", "_")])
    for name, song in [("ordinary", 100), ("lorien", 14), ("mastery", 16)]:
        add(f"corridor-archer-{name}", {"recognition": 0, "song": song, "song_skill": 0,
            "tiles": corridor[:-1], "monsters": [{"race": 103, "x": 35}]}, 24,
            checks=["archer_retreat", "ranged"] + (["song", "song_spacing"] if song != 100 else []))
    for name, race in [("tunnel-wall", 410), ("phase-wall", 167)]:
        add(name, {"recognition": 0, "tiles": [
            {"y1": 12, "y2": 28, "x1": 35, "feat": 56}],
            "monsters": [{"race": race, "x": 34, "heard": 1}]}, 28,
            checks=["pursuit", "tunnel"] if race == 410 else ["pursuit", "phase_wall"])
    for name, feat, race in [("water-melee", 84, 82), ("ice-melee", 86, 403),
                             ("lava-immune", 85, 181), ("poison-immune-flight", 87, 407),
                             ("chasm-flight", 2, 405)]:
        add(name, {"recognition": 0, "allow_death": True, "crossing_feat": feat,
            "tiles": corridor[:-1] + [{"y1": 20, "x1": 34, "x2": 36, "feat": feat}],
            "monsters": [{"race": race, "x": 33, "stance": 3, "morale": 1000}]}, 20,
            checks=["terrain_crossing", "pursuit"])
    for name, song in [("lorien-counterplay", 14), ("elbereth-counterplay", 0),
                       ("challenge-counterplay", 1), ("silence-counterplay", 4),
                       ("disguise-counterplay", 13)]:
        behavior = {14: "song_spacing", 0: "song_fear", 1: "song_challenge"}
        add(name, {"recognition": 0, "song": song, "monsters": [
            {"race": 103, "x": 35}, {"race": 82, "y": 19, "x": 37}]}, 22,
            checks=["song"] + ([behavior[song]] if song in behavior else []))
    add("short-sighted-stationary", {"monsters": [{"race": 12, "x": 36}]}, 14, checks=["stationary"])
    add("grotesque-watched", {"monsters": [{"race": 163, "x": 37}]}, 14, checks=["stationary"])
    add("carcharoth-vengeance", {"allow_death": True, "monsters": [{"race": 253, "x": 39}], "melee": 50},
        keys=["KP4", "KP5"] * 5, checks=["vengeance"])
    add("quiet-player-not-recognized", {"monsters": [{"race": 41, "x": 32}]}, 20, checks=["unseen"])
    add("wolf-follows-real-scent", {"player_y": 10, "player_x": 35, "tiles": [
        {"y1": 1, "y2": 38, "x1": 1, "x2": 62, "feat": 63},
        {"y1": 10, "x1": 8, "x2": 42, "feat": 1},
        {"y1": 10, "y2": 25, "x1": 42, "feat": 1},
        {"y1": 25, "x1": 8, "x2": 42, "feat": 1}],
        "monsters": [{"race": 11, "y": 10, "x": 20, "morale": 1000}]},
        keys=["KP6"] * 7 + ["KP2"] * 15 + ["KP4"] * 18 + ["KP5"] * 20, checks=["scent"])
    add("witness-player-flanking", {"melee": 0, "unarmoured": 1, "abilities": [{"skill": 2, "ability": 6}],
        "monsters": [{"race": 403, "x": 39}]}, keys=["KP8", "KP2"] * 7, checks=["player_flanking"])
    add("witness-player-retreat", {"melee": 0, "abilities": [{"skill": 2, "ability": 9}],
        "monsters": [{"race": 403, "x": 39}]}, keys=["KP6"] * 8 + ["KP5"] * 4, checks=["player_retreat"])
    add("witness-player-focus", {"melee": 0, "monsters": [{"race": 403, "x": 39}]},
        keys=["KP5", "KP4"] * 6, checks=["player_focus"])
    # Exercise all hostile historical uniques and the nine added guardians.
    ids = [54, 76, 84, 85, 95, 105, 115, 117, 125, 126, 135, 145, 146,
           165, 166, 176, 177, 185, 186, 195, 196, 205, 206, 214, 215,
           224, 225, 233, 234, 241, 242, 243, 244, 251, 253, 402, *range(403, 412)]
    for race in ids:
        add(f"species-{race}", {"recognition": 0, "allow_death": True, "monsters": [{"race": race, "x": 36}]}, 10)
    for race in [21, 31, 33, 53, 63, 71, 73, 75, 93, 94, 101, 113, 121, 151, 154, 171, 194, 203]:
        add(f"ordinary-{race}", {"recognition": 0, "monsters": [{"race": race, "x": 36}]}, 14)
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spec", type=Path, help="JSON encounter specification")
    parser.add_argument("--name", default="pursuit")
    parser.add_argument("--keys", default=",".join(["KP5"] * 20))
    parser.add_argument("--all", action="store_true", help="Run the behavioral and species matrix")
    parser.add_argument("--filter", default="", help="Only matrix cases matching this regular expression")
    parser.add_argument("--resume", action="store_true", help="Retain successful cases from the last matrix")
    parser.add_argument("--rerun", default="", help="Recheck retained cases matching this regular expression")
    parser.add_argument("--report", type=Path, default=OUT / "matrix-result.json",
                        help="JSON result file (choose a unique file for concurrent matrix runs)")
    parser.add_argument("--seed", type=int, help="Override the encounter seed")
    args = parser.parse_args()
    exe, env = build()
    if args.all:
        previous = args.report.resolve()
        previous.parent.mkdir(parents=True, exist_ok=True)
        results = json.loads(previous.read_text())["results"] if args.resume and previous.exists() else []
        signature = env["AI_PLAYTEST_SIGNATURE"]
        # Do not inherit passes from a different executable or encounter seed.
        results = [r for r in results if r.get("build_signature") == signature
                   and r.get("seed") == (args.seed if args.seed is not None else 20261112)]
        if args.rerun:
            results = [r for r in results if not re.search(args.rerun, r["name"])]
        failures = []
        completed = {result["name"] for result in results}
        for name, spec, keys in scenarios():
            if args.seed is not None:
                spec = dict(spec, seed=args.seed)
            if name in completed:
                continue
            if args.filter and not re.search(args.filter, name):
                continue
            try:
                results.append(run(exe, env, name, spec, keys))
            except Exception as exc:
                failures.append(dict(name=name, error=str(exc)))
                print(json.dumps(failures[-1]), flush=True)
            previous.write_text(json.dumps(dict(results=results, failures=failures), indent=2), encoding="utf-8")
        print(f"Gameplay matrix: {len(results)} passed, {len(failures)} failed", flush=True)
        if failures:
            raise SystemExit(1)
        return
    spec = json.loads(args.spec.read_text()) if args.spec else {"monsters": [{"race": 41}]}
    if args.seed is not None:
        spec["seed"] = args.seed
    result = run(exe, env, args.name, spec, args.keys.split(","))
    (OUT / "last-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
