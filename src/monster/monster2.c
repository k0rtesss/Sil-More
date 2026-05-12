/* File: monster/monster2.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "app/app-session.h"
#include "cmd/monster/cmd-monster.h"
#include "log/log.h"
#include "metarun.h"
#include "monster/monster.h"
#include "player/player-abilities.h"

static void listen_hint_handle_monster_removed(int m_idx);
static void listen_hint_set(int m_idx);
static void listen_hint_clear_monster(int m_idx);

/*
 * Return another race for a monster to polymorph into.  -LM-
 *
 * Perform a modified version of "get_mon_num()", with exact minimum and
 * maximum depths and preferred monster types.
 */
s16b poly_r_idx(const monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    s16b base_idx = m_ptr->r_idx;

    alloc_entry* table = alloc_race_table;

    int i, min_lev, max_lev, r_idx;
    long total, value;

    /* Source monster's level and symbol */
    int r_lev = r_ptr->level;
    char d_char = r_ptr->d_char;

    /* Hack -- Uniques never polymorph */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        return (base_idx);
    }

    /* Allowable level of new monster */
    min_lev = (MAX(1, r_lev - 1 - r_lev / 5));
    max_lev = (MIN(MAX_DEPTH, r_lev + 1 + r_lev / 5));

    /* Reset sum */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Assume no probability */
        table[i].prob3 = 0;

        /* Ignore illegal monsters - only those that don't get generated. */
        if (!table[i].prob1)
            continue;

        /* Not below the minimum base depth */
        if (table[i].level < min_lev)
            continue;

        /* Not above the maximum base depth */
        if (table[i].level > max_lev)
            continue;

        /* Get the monster index */
        r_idx = table[i].index;

        /* We're polymorphing -- we don't want the same monster */
        if (r_idx == base_idx)
            continue;

        /* Get the actual race */
        r_ptr = &r_info[r_idx];

        /* Hack -- No uniques */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            continue;

        /* Accept */
        table[i].prob3 = table[i].prob2;

        /* Bias against monsters far from initial monster's depth */
        if (table[i].level < (min_lev + r_lev) / 2)
            table[i].prob3 /= 4;
        if (table[i].level > (max_lev + r_lev) / 2)
            table[i].prob3 /= 4;

        /* Bias against monsters not of the same symbol */
        if (r_ptr->d_char != d_char)
            table[i].prob3 /= 4;

        /* Sum up probabilities */
        total += table[i].prob3;
    }

    /* No legal monsters */
    if (total == 0)
    {
        return (base_idx);
    }

    /* Pick a monster */
    value = rand_int(total);

    /* Find the monster */
    for (i = 0; i < alloc_race_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* Result */
    return (table[i].index);
}

/*
 * Delete a monster by index.
 *
 * When a monster is deleted, all of its objects are deleted.
 */
void delete_monster_idx(int i)
{
    int x, y;

    monster_type* m_ptr = &mon_list[i];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    s16b this_o_idx, next_o_idx = 0;

    /* Get location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Hack -- Reduce the racial counter */
    r_ptr->cur_num--;

    /* Hack -- count the number of "reproducers" */
    if (r_ptr->flags2 & (RF2_MULTIPLY))
        num_repro--;

    /* Hack -- remove target monster */
    if (p_ptr->target_who == i)
        target_set_monster(0);

    /* Hack -- remove tracked monster */
    if (p_ptr->health_who == i)
        health_track(0);

    /* Monster is gone */
    cave_m_idx[y][x] = 0;
    song_disguise_handle_monster_removed(i);
    song_duels_handle_monster_removed(i);
    if (i > 0 && i < MAX_MONSTERS)
        listen_hint_handle_monster_removed(i);

    /* Delete objects */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Hack -- efficiency */
        o_ptr->held_m_idx = 0;

        /* Delete the object */
        delete_object_idx(this_o_idx);
    }

    /* Wipe the Monster */
    memset(m_ptr, 0, sizeof(monster_type));

    /* Count monsters */
    mon_cnt--;

    /* Visual update */
    dungeon_mark_map_for_redraw();
}

static byte listen_hint[MAX_MONSTERS];
static bool listen_hint_has_data = false;

static void listen_hint_refresh_presence(void)
{
    int i;

    listen_hint_has_data = false;
    for (i = 1; i < MAX_MONSTERS; i++)
    {
        if (listen_hint[i])
        {
            listen_hint_has_data = true;
            return;
        }
    }
}

static void listen_hint_handle_monster_removed(int m_idx)
{
    if (m_idx <= 0 || m_idx >= MAX_MONSTERS || !listen_hint[m_idx])
        return;

    listen_hint[m_idx] = 0;
    listen_hint_refresh_presence();
}

static void listen_hint_set(int m_idx)
{
    if (m_idx <= 0 || m_idx >= MAX_MONSTERS)
        return;

    listen_hint[m_idx] = 1;
    listen_hint_has_data = true;
}

static void listen_hint_clear_monster(int m_idx)
{
    if (m_idx <= 0 || m_idx >= MAX_MONSTERS || !listen_hint[m_idx])
        return;

    listen_hint[m_idx] = 0;
    listen_hint_refresh_presence();
}

void listen_hint_new_player_turn(void)
{
    if (!listen_hint_has_data)
        return;

    memset(listen_hint, 0, sizeof(listen_hint));
    listen_hint_has_data = false;
    p_ptr->redraw |= (PR_MAP);
}

bool listen_hint_overlay(int m_idx, byte* a, char* c)
{
    int base;
    byte k;
    monster_type* m_ptr;

    if (!listen_hint_has_data)
        return false;
    if (m_idx <= 0 || m_idx >= MAX_MONSTERS || !listen_hint[m_idx])
        return false;
    if (!a || !c)
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || m_ptr->ml)
        return false;

    if (graphics_are_ascii())
    {
        base = 0x30;
        k = TERM_SLATE;
        *a = misc_to_attr[base + k];
        *c = misc_to_char[base + k];
    }
    else
    {
        *a = misc_to_attr[ICON_UNKNOWN_ENEMY];
        *c = misc_to_char[ICON_UNKNOWN_ENEMY];
    }

    return true;
}

/*
 * Return the monster's base protection sides after permanent reductions.
 */
int monster_base_armour_sides(const monster_type* m_ptr)
{
    const monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int base = r_ptr->ps;

    if (base <= 0)
        return 0;

    if (m_ptr->armor_ps_reduction >= base)
        return 0;

    return base - m_ptr->armor_ps_reduction;
}

int monster_song_hp_loss(const monster_type* m_ptr)
{
    return (int)m_ptr->song_hp_loss_lo
        | ((int)m_ptr->song_hp_loss_hi << 8);
}

void monster_add_song_hp_loss(monster_type* m_ptr, int amount)
{
    if (amount <= 0)
        return;

    int total = monster_song_hp_loss(m_ptr) + amount;
    if (total > 0xFFFF)
        total = 0xFFFF;

    m_ptr->song_hp_loss_lo = (byte)(total & 0xFF);
    m_ptr->song_hp_loss_hi = (byte)((total >> 8) & 0xFF);
}

/*
 * Delete the monster, if any, at a given location
 */
void delete_monster(int y, int x)
{
    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Delete the monster (if any) */
    if (cave_m_idx[y][x] > 0)
        delete_monster_idx(cave_m_idx[y][x]);
}

/*
 * Move a monster from index i1 to index i2 in the monster list
 */
static void compact_monsters_aux(int i1, int i2)
{
    int y, x;

    monster_type* m_ptr;

    s16b this_o_idx, next_o_idx = 0;

    /* Do nothing */
    if (i1 == i2)
        return;

    /* Old monster */
    m_ptr = &mon_list[i1];

    /* Location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Update the cave */
    cave_m_idx[y][x] = i2;

    /* Repair objects being carried by monster */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Reset monster pointer */
        o_ptr->held_m_idx = i2;
    }

    /* Hack -- Update the target */
    if (p_ptr->target_who == i1)
        p_ptr->target_who = i2;

    /* Hack -- Update the health bar */
    if (p_ptr->health_who == i1)
        p_ptr->health_who = i2;

    /* Hack -- move monster */
    memcpy(&mon_list[i2], &mon_list[i1], sizeof(monster_type));

    /* Hack -- wipe hole */
    memset(&mon_list[i1], 0, sizeof(monster_type));
}

/*
 * Compact and Reorder the monster list
 *
 * This function can be very dangerous, use with caution!
 *
 * When compacting monsters, we first delete far away monsters without
 * objects, starting with those of lowest level.  Then nearby monsters and
 * monsters with objects get compacted, then unique monsters. -LM-
 *
 * After "compacting" (if needed), we "reorder" the monsters into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */

void compact_monsters(int size)
{
    int i, j, cnt;

    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia -- refuse to wipe too many monsters at one time */
    if (size > MAX_MONSTERS / 2)
        size = MAX_MONSTERS / 2;

    /* Compact */
    if (size)
    {
        s16b* mon_lev;
        s16b* mon_index;

        /* Allocate the "mon_lev and mon_index" arrays */
        mon_lev = mem_alloc_array(mon_max, s16b);
        mon_index = mem_alloc_array(mon_max, s16b);

        /* Message */
        msg_print("Compacting monsters...");

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        /* Scan the monster list */
        for (i = 1; i < mon_max; i++)
        {
            m_ptr = &mon_list[i];
            r_ptr = &r_info[m_ptr->r_idx];

            /* Dead monsters have minimal level (but are counted!) */
            if (!m_ptr->r_idx)
                mon_lev[i] = -1L;

            /* Get the monster level */
            else
            {
                mon_lev[i] = r_ptr->level;

                /* Uniques are protected */
                if (r_ptr->flags1 & (RF1_UNIQUE))
                    mon_lev[i] += MAX_DEPTH * 2;

                /* Nearby monsters are protected */
                else if ((character_dungeon) && (m_ptr->cdis < MAX_SIGHT))
                    mon_lev[i] += MAX_DEPTH;

                /* Monsters with objects are protected */
                else if (m_ptr->hold_o_idx)
                    mon_lev[i] += MAX_DEPTH;
            }

            /* Save this monster index */
            mon_index[i] = i;
        }

        /* Sort all the monsters by (adjusted) level */
        for (i = 0; i < mon_max - 1; i++)
        {
            for (j = 0; j < mon_max - 1; j++)
            {
                int j1 = j;
                int j2 = j + 1;

                /* Bubble sort - ascending values */
                if (mon_lev[j1] > mon_lev[j2])
                {
                    s16b tmp_lev = mon_lev[j1];
                    u16b tmp_index = mon_index[j1];

                    mon_lev[j1] = mon_lev[j2];
                    mon_index[j1] = mon_index[j2];

                    mon_lev[j2] = tmp_lev;
                    mon_index[j2] = tmp_index;
                }
            }
        }

        /* Delete monsters until we've reached our quota */
        for (cnt = 0, i = 0; i < mon_max; i++)
        {
            /* We've deleted enough monsters */
            if (cnt >= size)
                break;

            /* Get this monster, using our saved index */
            m_ptr = &mon_list[mon_index[i]];

            /* "And another one bites the dust" */
            cnt++;

            /* No need to delete dead monsters again */
            if (!m_ptr->r_idx)
                continue;

            /* Delete the monster */
            delete_monster_idx(mon_index[i]);
        }

        /* Free the "mon_lev and mon_index" arrays */
        mem_free_null(mon_lev);
        mem_free_null(mon_index);
    }

    /* Excise dead monsters (backwards!) */
    for (i = mon_max - 1; i >= 1; i--)
    {
        /* Get the i'th monster */
        monster_type* hole_m_ptr = &mon_list[i];

        /* Skip real monsters */
        if (hole_m_ptr->r_idx)
            continue;

        /* Move last monster into open hole */
        compact_monsters_aux(mon_max - 1, i);

        /* Compress "mon_max" */
        mon_max--;
    }
}

/*
 * Delete/Remove all the monsters when the player leaves the level
 *
 * This is an efficient method of simulating multiple calls to the
 * "delete_monster()" function, with no visual effects.
 */
void wipe_mon_list(void)
{
    int i;

    /* Delete all the monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Hack -- Reduce the racial counter */
        r_ptr->cur_num--;

        /* Monster is gone */
        cave_m_idx[m_ptr->fy][m_ptr->fx] = 0;

        /* Wipe the Monster */
        memset(m_ptr, 0, sizeof(monster_type));
    }

    /* Reset "mon_max" */
    mon_max = 1;

    /* Reset "mon_cnt" */
    mon_cnt = 0;

    /* Hack -- reset "reproducer" count */
    num_repro = 0;

    /* Hack -- no more target */
    target_set_monster(0);

    /* Hack -- no more tracking */
    health_track(0);

    /* Hack -- make sure there is no player ghost */
    bones_selector = 0;
}

/*
 * Get and return the index of a "free" monster.
 *
 * This routine should almost never fail, but it *can* happen.
 */
static s16b mon_pop(void)
{
    int i;

    /* Normal allocation */
    if (mon_max < MAX_MONSTERS)
    {
        /* Get the next hole */
        i = mon_max;

        /* Expand the array */
        mon_max++;

        /* Count monsters */
        mon_cnt++;

        /* Return the index */
        return (i);
    }

    /* Recycle dead monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr;

        /* Get the monster */
        m_ptr = &mon_list[i];

        /* Skip live monsters */
        if (m_ptr->r_idx)
            continue;

        /* Count monsters */
        mon_cnt++;

        /* Use this monster */
        return (i);
    }

    /* Warn the player (except during dungeon creation) */
    if (character_dungeon)
        msg_print("Too many monsters!");

    /* Try not to crash */
    return (0);
}



bool build_monlist_subwindow_ui_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    u16b* race_counts;
    int idx;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_SLATE;
    app_ui_panel_set_widths(panel, 420, 900);
    app_ui_panel_set_title(panel, TERM_WHITE, "Monsters");

    if (p_ptr->image)
        return app_ui_panel_add_body_line(panel, TERM_L_WHITE,
            "What you see is not to be believed.");

    race_counts = mem_alloc_array(z_info->r_max, u16b);
    if (!race_counts)
        return false;

    for (idx = 1; idx < mon_max; idx++)
    {
        monster_type* m_ptr = &mon_list[idx];

        if (!m_ptr->ml)
            continue;

        race_counts[m_ptr->r_idx]++;
    }

    for (idx = 1; idx < mon_max && panel->row_count < APP_UI_ROW_MAX; idx++)
    {
        monster_type* m_ptr = &mon_list[idx];
        monster_race* r_ptr;
        char meta[APP_UI_META_MAX];
        cptr m_name;

        if (!m_ptr->ml)
            continue;
        if (!race_counts[m_ptr->r_idx])
            continue;

        r_ptr = &r_info[m_ptr->r_idx];
        strnfmt(meta, sizeof(meta), "%3d", race_counts[m_ptr->r_idx]);
        race_counts[m_ptr->r_idx] = 0;
        m_name = r_name + r_ptr->name;

        if (!app_ui_panel_add_row_ex(panel, (s16b)panel->row_count,
                TERM_WHITE, TERM_SLATE, monster_attr(r_ptr),
                monster_char(r_ptr), true, false, "", m_name, meta))
        {
            mem_free_null(race_counts);
            return false;
        }
    }

    if (panel->row_count == 0
        && !app_ui_panel_add_body_line(panel, TERM_SLATE, "No visible monsters."))
    {
        mem_free_null(race_counts);
        return false;
    }

    mem_free_null(race_counts);
    return true;
}

/*
 * Build a string describing a monster in some way.
 *
 * We can correctly describe monsters based on their visibility.
 * We can force all monsters to be treated as visible or invisible.
 * We can build nominatives, objectives, possessives, or reflexives.
 * We can selectively pronominalize hidden, visible, or all monsters.
 * We can use definite or indefinite descriptions for hidden monsters.
 * We can use definite or indefinite descriptions for visible monsters.
 *
 * Pronominalization involves the gender whenever possible and allowed,
 * so that by cleverly requesting pronominalization / visibility, you
 * can get messages like "You hit someone.  She screams in agony!".
 *
 * Reflexives are acquired by requesting Objective plus Possessive.
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 * Note that the "possessive" for certain unique monsters will look
 * really silly, as in "Morgoth, Lord of Darkness's".  We should
 * perhaps add a flag to "remove" any "descriptives" in the name.
 *
 * Note that "offscreen" monsters will get a special "(offscreen)"
 * notation in their name if they are visible but offscreen.  This
 * may look silly with possessives, as in "the rat's (offscreen)".
 * Perhaps the "offscreen" descriptor should be abbreviated.
 *
 * Mode Flags:
 *   0x01 --> Objective (or Reflexive)
 *   0x02 --> Possessive (or Reflexive)
 *   0x04 --> Use indefinites for hidden monsters ("something")
 *   0x08 --> Use indefinites for visible monsters ("a kobold")
 *   0x10 --> Pronominalize hidden monsters
 *   0x20 --> Pronominalize visible monsters
 *   0x40 --> Assume the monster is hidden
 *   0x80 --> Assume the monster is visible
 *
 * Useful Modes:
 *   0x00 --> Full nominative name ("the kobold") or "it"
 *   0x04 --> Full nominative name ("the kobold") or "something"
 *   0x80 --> Banishment resistance name ("the kobold")
 *   0x88 --> Killing name ("a kobold")
 *   0x22 --> Possessive, genderized if visable ("his") or "its"
 *   0x23 --> Reflexive, genderized if visable ("himself") or "itself"
 */


/*
 * Take note that the given monster just dropped some treasure
 *
 * Note that learning the "CHEST/GOOD"/"GREAT" flags gives information
 * about the treasure (even when the monster is killed for the first
 * time, such as uniques, and the treasure has not been examined yet).
 *
 * This "indirect" method is used to prevent the player from learning
 * exactly how much treasure a monster can drop from observing only
 * a single example of a drop.  This method actually observes how many
 * items are dropped, and remembers that information to be
 * described later by the monster recall code.
 */
void lore_treasure(int m_idx, int num_item)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    /* Note the number of things dropped */
    if (num_item > l_ptr->drop_item)
        l_ptr->drop_item = num_item;

    /* Hack -- memorize the chest/good/great/superb/artefact flags */
    if (r_ptr->flags1 & (RF1_DROP_CHEST))
        l_ptr->flags1 |= (RF1_DROP_CHEST);
    if (r_ptr->flags1 & (RF1_DROP_GOOD))
        l_ptr->flags1 |= (RF1_DROP_GOOD);
    if (r_ptr->flags1 & (RF1_DROP_GREAT))
        l_ptr->flags1 |= (RF1_DROP_GREAT);
    if (r_ptr->flags2 & (RF2_DROP_SUPERB))
        l_ptr->flags2 |= (RF2_DROP_SUPERB);
    if (r_ptr->flags3 & (RF3_DROP_ARTEFACT))
        l_ptr->flags3 |= (RF3_DROP_ARTEFACT);

    /* Update monster recall window */
    if (p_ptr->monster_race_idx == m_ptr->r_idx)
    {
        /* Window stuff */
        p_ptr->window |= (PW_MONSTER);
    }
}

/*
 *  Calculates a skill score for a monster
 */
int monster_skill(monster_type* m_ptr, int skill_type)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int skill = 0;

    switch (skill_type)
    {
    case S_MEL:
        msg_debug("Can't determine the monster's Melee score.");
        break;
    case S_ARC:
        msg_debug("Can't determine the monster's Archery score.");
        break;
    case S_EVN:
        msg_debug("Can't determine the monster's Evasion score.");
        break;
    case S_STL:
        skill = r_ptr->stl;
        skill -= m_ptr->song_stealth_penalty;
        skill += 2 * curse_flag_delta_cur(CUR_MON_STL);   /* +/-2 Stl per stack */
        break;
    case S_PER:
        skill = r_ptr->per;
        skill += 2 * curse_flag_delta_cur(CUR_MON_PER);   /* +/-2 Per per stack */
        break;
    case S_WIL:
        skill = r_ptr->wil;
        skill -= m_ptr->song_will_penalty;
        skill += 2 * curse_flag_delta_cur(CUR_MON_WIL);   /* +/-2 Wil per stack */
        break;
    case S_SMT:
        msg_debug("Can't determine the monster's Smithing score.");
        break;
    case S_SNG:
        msg_debug("Can't determine the monster's Song score.");
        break;

    default:
        msg_debug("Asked for an invalid monster skill.");
        break;
    }

    // penalise stunning
    if (m_ptr->stunned)
        skill -= 2;

    // Song of Challenge debuff - applies while singing or for some time after
    // NOTE: Challenge now reduces monster Will (S_WIL) in addition to Stealth
    if (p_ptr->song_challenge_effect > 0 && (skill_type == S_STL || skill_type == S_WIL))
    {
        // Calculate the full penalty and max duration based on current song skill
        int song_skill = ability_bonus(S_SNG, SNG_CHALLENGE);
    int full_penalty = song_skill / 5;
    if (full_penalty < 1) full_penalty = 1;
        
        // Calculate max duration: 15 turns at skill 20, formula: (skill * 3) / 4
        int max_duration = (song_skill * 3) / 4;
        if (max_duration < 3) max_duration = 3;
        
        // Scale the penalty based on remaining duration
        int penalty = (full_penalty * p_ptr->song_challenge_effect) / max_duration;
        if (penalty < 1 && p_ptr->song_challenge_effect > 0) penalty = 1;
        
        if (penalty > 0)
        {
            int before = skill;
            skill -= penalty;
            log_debug(
                "Song of Challenge penalty applied (r_idx=%d skill=%d -> %d, "
                "delta=%d, effect=%d/%d)",
                (int)m_ptr->r_idx, before, skill, penalty, 
                p_ptr->song_challenge_effect, max_duration);
        }
    }

    // Song of Elbereth debuff - applies while singing or for some time after
    if (p_ptr->song_elbereth_effect > 0 && skill_type == S_WIL)
    {
        // Calculate the full penalty and max duration based on current song skill
        int song_skill = ability_bonus(S_SNG, SNG_ELBERETH);
    int full_penalty = song_skill / 5;
    if (full_penalty < 1) full_penalty = 1;
        
        // Calculate max duration: 15 turns at skill 20, formula: (skill * 3) / 4
        int max_duration = (song_skill * 3) / 4;
        if (max_duration < 3) max_duration = 3;
        
        // Scale the penalty based on remaining duration
        int penalty = (full_penalty * p_ptr->song_elbereth_effect) / max_duration;
        if (penalty < 1 && p_ptr->song_elbereth_effect > 0) penalty = 1;
        
        if (penalty > 0)
        {
            int before = skill;
            skill -= penalty;
            log_debug(
                "Song of Elbereth penalty applied (r_idx=%d skill=%d -> %d, "
                "delta=%d, effect=%d/%d)",
                (int)m_ptr->r_idx, before, skill, penalty,
                p_ptr->song_elbereth_effect, max_duration);
        }
    }

    return (skill);
}

/*
 *  Calculates a Stat score for a monster
 */
int monster_stat(monster_type* m_ptr, int stat_type)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int stat = 0;

    int mhp = m_ptr->maxhp;
    int base = 20;

    switch (stat_type)
    {
    case A_STR:
        stat = (r_ptr->blow[0].dd * 2) + (r_ptr->hdice / 10) - 4;
        ;
        break;
    case A_DEX:
        msg_debug("Can't determine the monster's Dex score.");
        break;
    case A_CON:
        if (mhp < base)
        {
            while (mhp < base)
            {
                stat--;
                base = (base * 10) / 12;
            }
        }
        else if (mhp >= base)
        {
            stat--;
            while (mhp >= base)
            {
                stat++;
                base = (base * 12) / 10;
            }
        }
        // msg_debug("%d => %d.", m_ptr->maxhp, stat); // Sil-y: this seems
        // slightly erroneous for extreme values
        break;
    case A_GRA:
        msg_debug("Can't determine the monster's Gra score.");
        break;

    default:
        msg_debug("Asked for an invalid monster stat.");
        break;
    }

    return (stat);
}

/*
 * Shared sound-based detection logic. Returns true when the check succeeds.
 */
bool detect_monster_noise(monster_type* m_ptr, int skill)
{
    int result;
    int m_idx;
    int y = m_ptr->fy;
    int x = m_ptr->fx;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    int difficulty = flow_dist(FLOW_PLAYER_NOISE, y, x) - m_ptr->noise;

    // reset the monster noise
    m_ptr->noise = 0;

    // must not be visible
    if (m_ptr->ml)
        return false;

    // monster must be able to move
    if (r_ptr->flags1 & (RF1_NEVER_MOVE))
        return false;

    // use monster stealth
    difficulty += monster_skill(m_ptr, S_STL);

    // bonus for awake but unwary monsters (to simulate their lack of care)
    if ((m_ptr->alertness >= ALERTNESS_UNWARY)
        && (m_ptr->alertness < ALERTNESS_ALERT))
        difficulty -= 3;

    // penalty for song of silence
    if (singing(SNG_SILENCE))
        difficulty += ability_bonus(S_SNG, SNG_SILENCE);

    // make the check
    result = skill_check(PLAYER, skill, difficulty, m_ptr);

    // give up if it is a failure
    if (result <= 0)
    {
        dungeon_mark_map_for_redraw();
        return false;
    }

    m_idx = cave_m_idx[y][x];

    // make the monster completely visible if a dramatic success
    if (result > 10)
    {
        listen_hint_clear_monster(m_idx);
        m_ptr->ml = true;
        dungeon_mark_map_for_redraw();
        return true;
    }

    listen_hint_set(m_idx);
    dungeon_mark_map_for_redraw();
    return true;
}

static void listen(monster_type* m_ptr)
{
    // must have the listen skill
    if (!p_ptr->active_ability[S_PER][PER_LISTEN])
        return;

    detect_monster_noise(m_ptr, ability_score(S_PER, PER_LISTEN));
}

/*
 * This function updates the monster record of the given monster
 *
 * This involves extracting the distance to the player (if requested),
 * and then checking for visibility (natural, see-invis,
 * telepathy), updating the monster visibility flag, redrawing (or
 * erasing) the monster when its visibility changes, and taking note
 * of any interesting monster flags (cold-blooded, invisible, etc).
 *
 * Note the new "mflag" field which encodes several monster state flags,
 * including "view" for when the monster is currently in line of sight,
 * and "mark" for when the monster is currently visible via detection.
 *
 * The only monster fields that are changed here are "cdis" (the
 * distance from the player), "ml" (visible to the player), and
 * "mflag" (to maintain the "MFLAG_VIEW" flag).
 *
 * Note the special "update_monsters()" function which can be used to
 * call this function once for every monster.
 *
 * Note the "full" flag which requests that the "cdis" field be updated,
 * this is only needed when the monster (or the player) has moved.
 *
 * Every time a monster moves, we must call this function for that
 * monster, and update the distance, and the visibility.  Every time
 * the player moves, we must call this function for every monster, and
 * update the distance, and the visibility.  Whenever the player "state"
 * changes in certain ways ("blindness", "telepathy",
 * and "see invisible"), we must call this function for every monster,
 * and update the visibility.
 *
 * Routines that change the "illumination" of a grid must also call this
 * function for any monster in that grid, since the "visibility" of some
 * monsters may be based on the illumination of their grid.
 *
 * Note that this function is called once per monster every time the
 * player moves.  When the player is running, this function is one
 * of the primary bottlenecks, along with "update_view()" and the
 * "process_monsters()" code, so efficiency is important.
 *
 * Note the optimized "inline" version of the "distance()" function.
 *
 * A monster is "visible" to the player if (1) it has been detected
 * by the player, (2) it is close to the player and the player has
 * telepathy, or (3) it is close to the player, and in line of sight
 * of the player, and it is "illuminated" by some combination of
 * torch light, or permanent light (invisible monsters
 * are only affected by "light" if the player can see invisible).
 *
 * Monsters which are not on the current panel may be "visible" to
 * the player, and their descriptions will include an "offscreen"
 * reference.  Currently, offscreen monsters cannot be targetted
 * or viewed directly, but old targets will remain set.  XXX XXX
 *
 */
void update_mon(int m_idx, bool full)
{
    monster_type* m_ptr = &mon_list[m_idx];

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    int d;

    /* Current location */
    int fy = m_ptr->fy;
    int fx = m_ptr->fx;

    /* Seen at all */
    bool flag = false;

    /* Seen by vision */
    bool easy = false;

    /* Known because immobile */
    bool immobile_seen = false;

    // unmoving mindless monsters (i.e. molds) can be seen once encountered
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE)) && (r_ptr->flags2 & (RF2_MINDLESS))
        && m_ptr->encountered)
    {
        immobile_seen = true;
    }

    /* Compute distance */
    if (full)
    {
        int py = p_ptr->py;
        int px = p_ptr->px;

        /* Distance components */
        int dy = (py > fy) ? (py - fy) : (fy - py);
        int dx = (px > fx) ? (px - fx) : (fx - px);

        /* Approximate distance */
        d = (dy > dx) ? (dy + (dx >> 1)) : (dx + (dy >> 1));

        /* Restrict distance */
        if (d > 255)
            d = 255;

        /* Save the distance */
        m_ptr->cdis = d;
    }

    /* Extract distance */
    else
    {
        /* Extract the distance */
        d = m_ptr->cdis;
    }

    /* Detected */
    if (m_ptr->mflag & (MFLAG_MARK))
        flag = true;

    // debugging option for seeing all monsters
    if (cheat_monsters)
        flag = true;

    /* Nearby */
    if (d <= MAX_SIGHT)
    {
        /* Basic telepathy */
        if (p_ptr->telepathy > 0)
        {
            /* Mindless, no telepathy */
            if (r_ptr->flags2 & (RF2_MINDLESS))
            {
                /* Memorize flags */
                l_ptr->flags2 |= (RF2_MINDLESS);
            }

            /* Normal mind, allow telepathy */
            else
            {
                /* Detectable */
                flag = true;

                /* Hack -- Memorize mental flags */
                if (r_ptr->flags2 & (RF2_SMART))
                    l_ptr->flags2 |= (RF2_SMART);
                if (r_ptr->flags2 & (RF2_MINDLESS))
                    l_ptr->flags2 |= (RF2_MINDLESS);
            }
        }

        /* Normal line of sight, and not blind */
        if (player_has_los_bold(fy, fx) && !p_ptr->blind)
        {
            bool do_invisible = false;
            int difficulty = monster_skill(m_ptr, S_WIL)
                + (2 * distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx));

            /* Use "illumination" */
            if (player_can_see_bold(fy, fx))
            {
                /* Handle "invisible" monsters */
                if (r_ptr->flags2 & (RF2_INVISIBLE))
                {
                    /* Take note */
                    do_invisible = true;

                    /* See invisible makes things much easier */
                    difficulty -= 10 * p_ptr->see_inv;

                    /* Keen senses */
                    if (p_ptr->active_ability[S_PER][PER_KEEN_SENSES])
                    {
                        // makes things a bit easier
                        difficulty -= 5;
                    }

                    // Sil-x: calling this here seems to cause randseed issues
                    // on reloading games
                    //        i.e. saving then loading will 'see' different
                    //        monsters
                    /* See invisible through perception skill */
                    if (skill_check(
                            PLAYER, p_ptr->skill_use[S_PER], difficulty, m_ptr)
                        > 0)
                    {
                        /* Easy to see */
                        easy = flag = true;
                    }
                }

                /* Handle "normal" monsters */
                else
                {
                    /* Easy to see */
                    easy = flag = true;
                }
            }

            // handle keen senses ability
            else if (seen_by_keen_senses(fy, fx))
            {
                /* Easy to see */
                easy = flag = true;
            }

            /* Visible */
            if (flag)
            {
                /* Memorize flags */
                if (do_invisible)
                    l_ptr->flags2 |= (RF2_INVISIBLE);
            }
        }
    }

    /* The monster is now visible */
    if (flag || immobile_seen)
    {
        // Untarget if this is an out-of-LOS stationary monster
        if (immobile_seen && !flag)
        {
            if (p_ptr->target_who == m_idx)
                target_set_monster(0);
            if (p_ptr->health_who == m_idx)
                health_track(0);
        }

        /* It was previously unseen */
        if (!m_ptr->ml)
        {
            /* Mark as visible */
            m_ptr->ml = true;

            /* Track monster visibility for Niena mercy quest */
            if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE && m_ptr->r_idx != R_IDX_NIENA) {
                p_ptr->niena_monsters_seen++;
                log_trace("Niena quest: Monster seen (total seen=%d, killed=%d)", 
                         p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            }

            /* Draw the monster */
            dungeon_mark_map_for_redraw();

            /* Update health bar as needed */
            if (p_ptr->health_who == m_idx)
                p_ptr->redraw |= (PR_HEALTHBAR);

            /* Disturb on visibility change */
            disturb(0, 0);

            /* Window stuff */
            p_ptr->window |= PW_MONLIST;

            // identify see invisible items
            if ((r_ptr->flags2 & (RF2_INVISIBLE)) && (p_ptr->see_inv > 0))
                ident_see_invisible(m_ptr);
        }
    }

    /* The monster is not visible */
    else
    {
        /* It was previously seen */
        if (m_ptr->ml)
        {
            /* Mark as not visible */
            m_ptr->ml = false;

            /* Erase the monster */
            dungeon_mark_map_for_redraw();

            /* Update health bar as needed */
            if (p_ptr->health_who == m_idx)
                p_ptr->redraw |= (PR_HEALTHBAR);

            /* Disturb on visibility change */
            // disturb(0, 0);

            /* Window stuff */
            p_ptr->window |= PW_MONLIST;
        }
    }

    /* The monster is now easily visible */
    if (easy)
    {
        /* Change */
        if (!(m_ptr->mflag & (MFLAG_VIEW)))
        {
            /* Mark as easily visible */
            m_ptr->mflag |= (MFLAG_VIEW);

            /* Disturb on appearance */
            disturb(0, 0);
        }
    }

    /* The monster is not easily visible */
    else
    {
        /* Change */
        if (m_ptr->mflag & (MFLAG_VIEW))
        {
            /* Mark as not easily visible */
            m_ptr->mflag &= ~(MFLAG_VIEW);

            /* Disturb on disappearance */
            // disturb(1, 0);
        }
    }

    // Ensure repeated calls within the same turn remain deterministic by seeding
    // the RNG from the current turn, then restoring the saved state afterwards.
    {
        u64b saved_state = Rand_state_export();
        u64b temp_seed = ((u64b)playerturn + 1) * 15485863ULL;
        Rand_state_import(temp_seed);
        listen(m_ptr);
        Rand_state_import(saved_state);
    }

    // Check ecounters with monsters (must be visible and in line of sight)
    if (m_ptr->ml && !m_ptr->encountered
        && player_has_los_bold(m_ptr->fy, m_ptr->fx)
        && (l_ptr->psights < MAX_SHORT))
    {
        int new_exp = adjusted_mon_exp(r_ptr, false);
        bool first_unique_sighting =
            (r_ptr->flags1 & RF1_UNIQUE) && (l_ptr->psights == 0);

        // gain experience for encounter
        gain_exp(new_exp);
        p_ptr->encounter_exp += new_exp;

        // update stats
        m_ptr->encountered = true;
        l_ptr->psights++;
        if (l_ptr->tsights < MAX_SHORT)
            l_ptr->tsights++;

        // If the player encounters a Unique for the first time, write a note.
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            char note2[120];
            char real_name[120];

            /* Get the monster's real name for the notes file */
            monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);

            /* Write note */
            SDL_strlcpy(
                note2, format("Encountered %s", real_name), sizeof(note2));

            do_cmd_note(note2, p_ptr->depth);

            if (first_unique_sighting)
                gain_knowledge_points(1, "A unique foe enters your lore.");
        }

        // if it was a wraith, possibly realise you are haunted
        if ((r_ptr->flags3 & (RF3_UNDEAD))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL)))
        {
            ident_haunted();
        }
    }
}

/*
 * This function simply updates all the (non-dead) monsters (see above).
 */
void update_monsters(bool full)
{
    int i;

    /* Update each (live) monster */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Update the monster */
        update_mon(i, full);
    }
}

/*
 * Make a monster carry an object
 */
s16b monster_carry(int m_idx, object_type* j_ptr)
{
    s16b o_idx;

    s16b this_o_idx, next_o_idx = 0;

    monster_type* m_ptr = &mon_list[m_idx];

    /* Scan objects already being held for combination */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Check for combination */
        if (object_similar(o_ptr, j_ptr))
        {
            /* Combine the items */
            object_absorb(o_ptr, j_ptr);

            if (j_ptr->number == 0)
            {
                /* Result */
                return (this_o_idx);
            }
        }
    }

    /* Make an object */
    o_idx = o_pop();

    /* Success */
    if (o_idx)
    {
        object_type* o_ptr;

        /* Get new object */
        o_ptr = &o_list[o_idx];

        /* Copy object */
        object_copy(o_ptr, j_ptr);

        /* Forget mark */
        o_ptr->marked = false;

        /* Forget location */
        o_ptr->iy = o_ptr->ix = 0;

        /* Link the object to the monster */
        o_ptr->held_m_idx = m_idx;

        /* Link the object to the pile */
        o_ptr->next_o_idx = m_ptr->hold_o_idx;

        /* Link the monster to the object */
        m_ptr->hold_o_idx = o_idx;
    }

    /* Result */
    return (o_idx);
}

/*
 * Check if the monster in the given location needs to fall down a chasm
 */
static void m_fall_in_chasm(int fy, int fx)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    int dice;
    int dam;

    // paranoia
    if (cave_m_idx[fy][fx] <= 0)
        return;

    m_ptr = &mon_list[cave_m_idx[fy][fx]];
    r_ptr = &r_info[m_ptr->r_idx];

    if ((cave_feat[fy][fx] == FEAT_CHASM) && !(r_ptr->flags2 & (RF2_FLYING)))
    {
        // Get the monster name
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // message for visible monsters
        if (m_ptr->ml)
        {
            // Dump a message
            if (m_ptr->morale < -200)
                msg_format("%^s leaps into the abyss!", m_name);
            else
                msg_format("%^s topples into the abyss!", m_name);
        }

        // pause so that the monster will be displayed in the chasm before it
        // disappears
        message_flush();

        // determine the falling damage
        if (p_ptr->depth >= MORGOTH_DEPTH - 1)
            dice = 3; // only fall one floor in this case
        else
            dice = 6;

        // roll the damage dice
        dam = damroll(dice, 4);

        // update combat rolls if visible
        if (m_ptr->ml)
        {
            // Store information for the combat rolls window
            combat_roll_special_char = (&f_info[cave_feat[fy][fx]])->d_char;
            combat_roll_special_attr = (&f_info[cave_feat[fy][fx]])->d_attr;

            update_combat_rolls1b(NULL, m_ptr, true);
            update_combat_rolls2(dice, 4, dam, -1, -1, 0, 0, GF_HURT, false);
        }

        // kill monsters which cannot survive the damage
        if (m_ptr->hp <= dam)
        {
            // kill the monster, gain experience etc
            monster_death(cave_m_idx[fy][fx]);

            // delete the monster
            delete_monster_idx(cave_m_idx[fy][fx]);
        }

        // otherwise the monster survives! (mainly relevant for uniques)
        else
        {
            // just delete the monster
            delete_monster(m_ptr->fy, m_ptr->fx);
        }
    }
}

/*
 * Print a message saying what is underfoot.
 */
static void describe_floor_object(void)
{
    object_type* o_ptr;
    char o_name[80];
    char smith_buf[20];

    // generate the object's name
    o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

    smith_buf[0] = '\0';
    if (op_ptr->opt[OPT_show_smithing_difficulty_look]
        && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);
        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
    }

    // skip 'nothings'
    if (!o_ptr->k_idx)
    {
        // do nothing
    }

    // skip notes
    else if (o_ptr->tval == TV_NOTE)
    {
        // do nothing
    }

    // skip fired/thrown items
    else if (o_ptr->pickup)
    {
        // do nothing
    }

    // arms and armour show weight
    else if (((wield_slot(o_ptr) >= INVEN_WIELD)
                 && (wield_slot(o_ptr) <= INVEN_STAFF))
        || (wield_slot(o_ptr) == INVEN_HORN)
        || ((wield_slot(o_ptr) >= INVEN_BODY)
            && (wield_slot(o_ptr) <= INVEN_FEET)))
    {
        int wgt = o_ptr->weight * o_ptr->number;
        if (!p_ptr->blind)
            msg_format("You see %s %d.%1d lb%s.", o_name, wgt / 10, wgt % 10,
                smith_buf);
        else
            msg_format("Your feet strike against %s.", o_name);

        /* Disturb */
        disturb(0, 0);
    }

    // other things just show description
    else
    {
        if (!p_ptr->blind)
            msg_format("You see %s%s.", o_name, smith_buf);
        else
            msg_format("Your feet strike against %s.", o_name);

        /* Disturb */
        disturb(0, 0);
    }

    // special explanation the first time you step over the crown
    if ((o_ptr->name1 == ART_MORGOTH_3) && !(p_ptr->crown_hint))
    {
        if (hjkl_movement)
        {
            msg_print("To attempt to prise a Silmaril from the crown, use the "
                      "'destroy' "
                      "command ('Ctrl-k').");
        }
        else
        {
            msg_print("To attempt to prise a Silmaril from the crown, use the "
                      "'destroy' "
                      "command (which is 'k' by default).");
        }
        p_ptr->crown_hint = true;
    }
}

/*
 * Swap the players/monsters (if any) at two locations XXX XXX XXX
 *
 * Note that this assumes the monster at y1-x1 is actively moving to y2-x2
 */
static bool player_environment_bonus_state_changed(int old_y, int old_x,
    int new_y, int new_x)
{
    return level_partition_big_cave_type_for_point(old_y, old_x)
        != level_partition_big_cave_type_for_point(new_y, new_x);
}

void monster_swap(int y1, int x1, int y2, int x2)
{
    int m1 = cave_m_idx[y1][x1];
    int m2 = cave_m_idx[y2][x2];

    int y, x;

    monster_type* m_ptr = NULL; // default to soother compiler warnings
    monster_race* r_ptr = NULL; // default to soother compiler warnings
    monster_lore* l_ptr = NULL; // default to soother compiler warnings

    char m_name[80];

    bool monster1 = false;

    /* Monster 1 */
    if (m1 > 0)
    {
        monster1 = true;
        m_ptr = &mon_list[m1];

        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        // (skip_next_turn is there to stop you getting opportunist attacks afer
        // knocking someone back)
        if (!singing(SNG_DISGUISE) && m_ptr->ml && !m_ptr->skip_next_turn
            && !p_ptr->truce
            && !p_ptr->confused && !p_ptr->afraid && !p_ptr->entranced
            && (p_ptr->stun <= 100))
        {
            if (!forgo_attacking_unwary
                || (m_ptr->alertness >= ALERTNESS_ALERT))
            {
                if (p_ptr->active_ability[S_MEL][MEL_ZONE_OF_CONTROL])
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1)
                        && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1))
                    {
                        if (valorous_oath_auto_attack_safety
                            && chosen_oath(OATH_VALOROUS)
                            && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
                            && (m_ptr->stance == STANCE_FLEEING))
                        {
                            msg_format("%^s moves through your zone of control, but you hold back.", m_name);
                        }
                        else
                        {
                            msg_format("%^s moves through your zone of control.", m_name);
                            py_attack_aux(y1, x1, ATT_ZONE_OF_CONTROL);
                        }
                    }
                }
                if (p_ptr->active_ability[S_STL][STL_OPPORTUNIST])
                {
                    if ((distance(y1, x1, p_ptr->py, p_ptr->px) == 1)
                        && (distance(y2, x2, p_ptr->py, p_ptr->px) > 1))
                    {
                        if (valorous_oath_auto_attack_safety
                            && chosen_oath(OATH_VALOROUS)
                            && !oath_invalid(OATH_VALOROUS) && m_ptr->ml
                            && (m_ptr->stance == STANCE_FLEEING))
                        {
                            msg_format("%^s moves away from you, but you hold back.", m_name);
                        }
                        else
                        {
                            msg_format("%^s moves away from you.", m_name);
                            py_attack_aux(y1, x1, ATT_OPPORTUNIST);
                        }
                    }
                }
            }
        }
        if (m_ptr->hp <= 0)
            return;

        // abort the monster swap if the monster has been moved by the free
        // attack
        if (cave_m_idx[y1][x1] != m1)
            return;

        /* Move monster */
        m_ptr->fy = y2;
        m_ptr->fx = x2;

        if ((r_info[m_ptr->r_idx].flags3 & RF3_SPECIAL_VAULT_ONLY)
            && (((cave_info[y1][x1] & CAVE_G_VAULT) == 0)
                || ((cave_info[y2][x2] & CAVE_G_VAULT) == 0)))
        {
            log_trace(
                "SPECIAL_VAULT_ONLY move: monster='%s' r_idx=%d m_idx=%d depth=%d from=(%d,%d) to=(%d,%d) from_g_vault=%d to_g_vault=%d from_icky=%d to_icky=%d from_morgoth_tunnel=%d to_morgoth_tunnel=%d",
                m_name, m_ptr->r_idx, m1, p_ptr->depth, y1, x1, y2, x2,
                (cave_info[y1][x1] & CAVE_G_VAULT) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_G_VAULT) ? 1 : 0,
                (cave_info[y1][x1] & CAVE_ICKY) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_ICKY) ? 1 : 0,
                (cave_info[y1][x1] & CAVE_MORGOTH_TUNNEL) ? 1 : 0,
                (cave_info[y2][x2] & CAVE_MORGOTH_TUNNEL) ? 1 : 0);
        }

        // makes noise when moving
        if (m_ptr->noise == 0)
            m_ptr->noise = 5;

        /* Update monster */
        (void)update_mon(m1, true);
    }

    /* Player 1 */
    else if (m1 < 0)
    {
        bool bonus_state_changed =
            player_environment_bonus_state_changed(y1, x1, y2, x2);
        bool should_log_environment = bonus_state_changed
            || level_partition_big_cave_type_for_point(y1, x1) != BIG_CAVE_NONE
            || level_partition_big_cave_type_for_point(y2, x2) != BIG_CAVE_NONE
            || ((cave_info[y1][x1] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0)
            || ((cave_info[y2][x2] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0);

        if (should_log_environment)
            log_partition_debug_for_point("monster_swap.old", y1, x1);

        // deal with monsters with Opportunist or Zone of Control
        for (y = p_ptr->py - 1; y <= p_ptr->py + 1; y++)
        {
            for (x = p_ptr->px - 1; x <= p_ptr->px + 1; x++)
            {
                if (cave_m_idx[y][x] > 0)
                {
                    m_ptr = &mon_list[cave_m_idx[y][x]];
                    r_ptr = &r_info[m_ptr->r_idx];
                    l_ptr = &l_list[m_ptr->r_idx];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    if (!singing(SNG_DISGUISE)
                        && (m_ptr->alertness >= ALERTNESS_ALERT)
                        && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING)
                        && !m_ptr->skip_next_turn && !m_ptr->skip_this_turn)
                    {
                        // Opportunist
                        if ((r_ptr->flags2 & (RF2_OPPORTUNIST))
                            && (distance(m_ptr->fy, m_ptr->fx, y2, x2) > 1))
                        {
                            msg_format(
                                "%^s attacks you as you step away.", m_name);
                            make_attack_normal(m_ptr);

                            // remember that the monster can do this
                            if (m_ptr->ml)
                                l_ptr->flags2 |= (RF2_OPPORTUNIST);
                        }

                        // Zone of Control
                        if ((r_ptr->flags2 & (RF2_ZONE_OF_CONTROL))
                            && (distance(m_ptr->fy, m_ptr->fx, y2, x2) == 1))
                        {
                            msg_format("You move through %s's zone of control.",
                                m_name);
                            make_attack_normal(m_ptr);

                            // remember that the monster can do this
                            if (m_ptr->ml)
                                l_ptr->flags2 |= (RF2_ZONE_OF_CONTROL);
                        }
                    }
                }
            }
        }
        if (p_ptr->chp <= 0)
            return;

        /* Move player */
        p_ptr->py = y2;
        p_ptr->px = x2;

        /* Update the panel */
        p_ptr->update |= (PU_PANEL);

        /* Update the visuals (and monster distances) */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        if (should_log_environment)
            log_partition_debug_for_point("monster_swap.new", y2, x2);

        if (bonus_state_changed)
        {
            p_ptr->update |= (PU_BONUS);
            update_stuff();
        }
    }

    /* Monster 2 */
    if (m2 > 0)
    {
        m_ptr = &mon_list[m2];

        /* Move monster */
        m_ptr->fy = y1;
        m_ptr->fx = x1;

        // makes noise when moving
        if (m_ptr->noise == 0)
            m_ptr->noise = 5;

        /* Update monster */
        (void)update_mon(m2, true);

        // reset its previous movement to stop it charging etc.
        m_ptr->previous_action[0] = ACTION_MISC;
    }

    /* Player 2 */
    else if (m2 < 0)
    {
        /* Move player */
        p_ptr->py = y1;
        p_ptr->px = x1;

        /* Update the panel */
        p_ptr->update |= (PU_PANEL);

        /* Update the visuals (and monster distances) */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Update grids */
    cave_m_idx[y1][x1] = m2;
    cave_m_idx[y2][x2] = m1;

    /* Redraw */
    dungeon_mark_map_for_redraw();
    dungeon_mark_map_for_redraw();

    {
        app_session* session = app_session_current();
        u32b invalidation_mask = APP_SNAPSHOT_INVALIDATE_MAP;
        bool moved_target = false;

        if ((m1 > 0) && (p_ptr->target_who == m1))
        {
            p_ptr->target_row = y2;
            p_ptr->target_col = x2;
            moved_target = true;
        }
        if ((m2 > 0) && (p_ptr->target_who == m2))
        {
            p_ptr->target_row = y1;
            p_ptr->target_col = x1;
            moved_target = true;
        }

        if ((m1 < 0) || (m2 < 0) || moved_target)
        {
            invalidation_mask |= APP_SNAPSHOT_INVALIDATE_CURSOR
                | APP_SNAPSHOT_INVALIDATE_TARGET;
        }
        if ((m1 < 0) || (m2 < 0))
        {
            invalidation_mask |= APP_SNAPSHOT_INVALIDATE_STATUS;
        }

        if (session)
        {
            if (moved_target && hilite_target && target_sighted())
            {
                app_session_note_cursor_relative(session, p_ptr->target_row,
                    p_ptr->target_col);
            }

            if (m1 != 0)
            {
                app_session_note_animation(session,
                    APP_ANIMATION_HINT_ACTOR_MOVED,
                    (m1 < 0) ? APP_DUNGEON_PLAYER_SUBJECT : m1,
                    APP_PACK_COORD(y1, x1), APP_PACK_COORD(y2, x2), m2,
                    invalidation_mask);
            }

            if (m2 != 0)
            {
                app_session_note_animation(session,
                    APP_ANIMATION_HINT_ACTOR_MOVED,
                    (m2 < 0) ? APP_DUNGEON_PLAYER_SUBJECT : m2,
                    APP_PACK_COORD(y2, x2), APP_PACK_COORD(y1, x1), m1,
                    invalidation_mask);
            }
        }
    }

    // deal with set polearm attacks
    if (p_ptr->active_ability[S_MEL][MEL_POLEARMS] && monster1 && m_ptr->ml)
    {
        object_type* o_ptr = &inventory[INVEN_WIELD];
        u32b f1, f2, f3;

        object_flags(o_ptr, &f1, &f2, &f3);

        if (!forgo_attacking_unwary || (m_ptr->alertness >= ALERTNESS_ALERT))
        {
            if ((distance(y1, x1, p_ptr->py, p_ptr->px) > 1)
                && (distance(y2, x2, p_ptr->py, p_ptr->px) == 1)
                && !p_ptr->truce && !p_ptr->confused && !p_ptr->afraid
                && (f3 & (TR3_POLEARM)) && p_ptr->focused)
            {
                char o_name[80];

                /* Get the basic name of the object */
                object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

                if (valorous_oath_auto_attack_safety && chosen_oath(OATH_VALOROUS)
                    && !oath_invalid(OATH_VALOROUS)
                    && (m_ptr->stance == STANCE_FLEEING))
                {
                    msg_format("%^s comes into reach of your %s, but you hold back.", m_name, o_name);
                }
                else
                {
                    msg_format("%^s comes into reach of your %s.", m_name, o_name);
                    py_attack_aux(y2, x2, ATT_POLEARM);
                }
            }
        }
    }

    // deal with falling down chasms
    if (m1 > 0)
        m_fall_in_chasm(y2, x2);
    if (m2 > 0)
        m_fall_in_chasm(y1, x1);

    // describe object you are standing on if any
    if ((m1 < 0) || (m2 < 0))
    {
        describe_floor_object();
    }
}

/*
 * Place the player in the dungeon XXX XXX
 */
s16b player_place(int y, int x)
{
    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] != 0)
        return (0);

    /* Save player location */
    p_ptr->py = y;
    p_ptr->px = x;

    /* Mark cave grid */
    cave_m_idx[y][x] = -1;
    if (cave_feat[y][x] == FEAT_RUBBLE)
        cave_feat[y][x] = FEAT_FLOOR;

    /* Success */
    return (-1);
}

/*
 * Place a copy of a monster in the dungeon XXX XXX
 */
s16b monster_place(int y, int x, monster_type* n_ptr)
{
    s16b m_idx;

    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] != 0)
        return (0);

    /* Get a new record */
    m_idx = mon_pop();

    /* Oops */
    if (m_idx)
    {
        /* Make a new monster */
        cave_m_idx[y][x] = m_idx;

        /* Get the new monster */
        m_ptr = &mon_list[m_idx];

        /* Copy the monster XXX */
        memcpy(m_ptr, n_ptr, sizeof(monster_type));

        /* Location */
        m_ptr->fy = y;
        m_ptr->fx = x;

        /* Update the monster */
        update_mon(m_idx, true);

        /* Get the new race */
        r_ptr = &r_info[m_ptr->r_idx];

        /* Hack -- Notice new multi-hued monsters */
        if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            shimmer_monsters = true;

        /* Hack -- Count the number of "reproducers" */
        if (r_ptr->flags2 & (RF2_MULTIPLY))
            num_repro++;

        /* Count racial occurances */
        r_ptr->cur_num++;
    }

    /* Result */
    return (m_idx);
}

/*calculate the monster_speed of a monster at a given location*/
void calc_monster_speed(int y, int x)
{
    int speed;

    /*point to the monster at the given location & the monster race*/
    monster_type* m_ptr;
    monster_race* r_ptr;

    /* Paranoia XXX XXX */
    if (cave_m_idx[y][x] <= 0)
        return;

    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];

    /* Get the monster base speed */
    speed = r_ptr->speed;

    /*factor in the hasting and slowing counters*/
    if (m_ptr->hasted)
        speed += 1;
    if (m_ptr->slowed)
        speed -= 1;

    if (speed < 1)
        speed = 1;

    /*set the speed and return*/
    m_ptr->mspeed = speed;

    return;
}

bool monster_race_is_vala(int r_idx)
{
    monster_race* r_ptr;

    if (!z_info || !r_info)
        return false;
    if ((r_idx <= 0) || (r_idx >= z_info->r_max))
        return false;

    /* Morgoth has bespoke alertness/sleep mechanics tied to the Iron Crown. */
    if (r_idx == R_IDX_MORGOTH)
        return false;

    r_ptr = &r_info[r_idx];
    if (!r_ptr->name)
        return false;

    return (r_ptr->d_char == 'V');
}

bool monster_clear_vala_state(monster_type* m_ptr)
{
    bool speed_changed;

    if (!m_ptr || !monster_race_is_vala(m_ptr->r_idx))
        return false;

    speed_changed = (m_ptr->hasted != 0) || (m_ptr->slowed != 0);

    m_ptr->alertness = ALERTNESS_ALERT;
    m_ptr->stunned = 0;
    m_ptr->confused = 0;
    m_ptr->slowed = 0;
    m_ptr->hasted = 0;
    m_ptr->skip_next_turn = false;
    m_ptr->skip_this_turn = false;
    m_ptr->stance = STANCE_CONFIDENT;

    return speed_changed;
}

void set_monster_haste(s16b m_idx, s16b counter, bool message)
{
    /*get the monster at the given location*/
    monster_type* m_ptr = &mon_list[m_idx];

    bool recalc = false;

    char m_name[80];

    if (monster_race_is_vala(m_ptr->r_idx))
    {
        if (monster_clear_vala_state(m_ptr))
            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        return;
    }

    /* Get monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /*see if we need to recalculate speed*/
    if (m_ptr->hasted)
    {
        /*monster is no longer hasted and speed needs to be recalculated*/
        if (counter == 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s slows down.", m_name);
        }
    }
    else
    {
        /*monster is now hasted and speed needs to be recalculated*/
        if (counter > 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s starts moving faster.", m_name);
        }
    }

    /*update the counter*/
    m_ptr->hasted = counter;

    /*re-calculate speed if necessary*/
    if (recalc)
        calc_monster_speed(m_ptr->fy, m_ptr->fx);

    return;
}

void set_monster_slow(s16b m_idx, s16b counter, bool message)
{
    /*get the monster at the given location*/
    monster_type* m_ptr = &mon_list[m_idx];

    bool recalc = false;

    char m_name[80];

    if (monster_race_is_vala(m_ptr->r_idx))
    {
        if (monster_clear_vala_state(m_ptr))
            calc_monster_speed(m_ptr->fy, m_ptr->fx);
        return;
    }

    /* Get monster name*/
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /*see if we need to recalculate speed*/
    if (m_ptr->slowed)
    {
        /*monster is no longer slowed and speed needs to be recalculated*/
        if (counter == 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s speeds up.", m_name);
        }
    }
    else
    {
        /*monster is now slowed and speed needs to be recalculated*/
        if (counter > 0)
        {
            recalc = true;

            /*give a message*/
            if (message)
                msg_format("%^s starts moving slower.", m_name);
        }
    }

    /*update the counter*/
    m_ptr->slowed = counter;

    /*re-calculate speed if necessary*/
    if (recalc)
        calc_monster_speed(m_ptr->fy, m_ptr->fx);

    return;
}



/*
 * Let the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 */
bool reproduce_monster(int old_m_idx, int new_r_idx)
{
    monster_type* old_m_ptr = &mon_list[old_m_idx];
    monster_race* new_r_ptr = &r_info[new_r_idx];

    int i, y, x;

    bool result = false;

    u16b grid[8];
    int grids = 0;

    /* Scan the adjacent floor grids */
    for (i = 0; i < 8; i++)
    {
        y = old_m_ptr->fy + ddy_ddd[i];
        x = old_m_ptr->fx + ddx_ddd[i];

        /* Must be fully in bounds */
        if (!in_bounds_fully(y, x))
            continue;

        /* This grid is OK for this monster (should monsters be able to dig?) */
        if (cave_exist_mon(new_r_ptr, y, x, false, false))
        {
            /* Save this grid */
            grid[grids++] = GRID(y, x);
        }
    }

    /* No grids available */
    if (!grids)
        return (false);

    /* Pick a grid at random */
    i = rand_int(grids);

    /* Get the coordinates */
    y = GRID_Y(grid[i]);
    x = GRID_X(grid[i]);

    /* Create a new monster (awake, no groups) */
    result = place_monster_aux(y, x, new_r_idx, false, false);

    /* Result */
    return (result);
}

/*
 * Dump a message describing a monster's reaction to damage.
 *
 * Historically, this function gave a description (visual or auditory) of
 * a monster's reaction in order to give you an idea of their health level.
 *
 * Now it only gives a message if the monster is unseen, and the primary
 * purpose is to show that there is indeed a monster in the dark corridor
 * getting hurt.
 *
 * Note that while the monsters 'cry out', it doesn't wake any monsters or
 * anything, as the idea is that it makes no more noise than regular melee
 * combat. It is just that in melee combat, we wouldn't want to spam up the
 * screen with messages about noises.
 */
