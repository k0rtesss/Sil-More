/* File: spell/spell-utility.c */
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

/*
 * Utility spells: healing, curses, identification, self-knowledge, and hooks.
 * Split from spells2.c for better code organization.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "app/app-session.h"
#include "log/log.h"
#include "metarun.h"
#include "player/identification.h"
#include "object/object-ui-identify.h"
#include "supplies.h"
#include "spell/spell-utility.h"
#include "ui/ui-information-scene.h"

// Function declarations
void analyze_weapon_properties(int* count, char s[][200], char t[][200], bool good[], 
                               bool identify[], int slot, const char* weapon_name);
void identify_revealed_items(bool identify[]);

#define TR1 0
#define TR2 1
#define TR3 2
#define RF1 3
#define RF2 4
#define RF3 5
#define RF4 6
#define RHF 7
#define VLT 8
#define CUR 9
#define UNQ 10
#define MAX_FLAG_SETS 11

enum
{
    SELF_KNOWLEDGE_PAGE_ENTRY_MAX = 8,
    RECHARGE_TARGET_PAGE_SIZE = 10
};

// Flags with descriptions
flag_name info_flags_desc[] = { 
{"Will Affinity is at 3, and never affected by curses", UNQ, UNQ_EARENDIL}, 
{ "Artifacts take only 1 charge of forge, easier to make fire and light items", UNQ, UNQ_SMT_FEANOR },
{ "Majesty ability is 1.5x effective", UNQ, UNQ_WIL_FIN }, 
{ "Song of Staying is twice effective", UNQ, UNQ_SNG_FIN },
{ "Song of Lorien is 1.5x effective", UNQ, UNQ_SNG_LUT }, 
{ "Horns are twice effective", UNQ, UNQ_WIL_TUOR },
{ "Song of Threshold and Staff of Warding are twice effective", UNQ, UNQ_SNG_MEL }, 
{ "Can create very sharp items, easier to create sharp and accurate items", UNQ, UNQ_SMT_TELCHAR },
{ "Using 3 forge charges can create mithril items without mithril", UNQ, UNQ_SMT_GAMIL }, 
{ "All rings cost 30% less to create and ring slots are treated as major slots", UNQ, UNQ_SMT_CELEBRIMBOR },
{ "Song of Slaying is twice effective", UNQ, UNQ_SNG_HURIN },
{ "Song of Mastery is 1.75x effective", UNQ, UNQ_SNG_THINGOL }, 
{ "Starts with all stealth skills", UNQ, UNQ_MIM },
{ "Melee abilities are twice effective, better at one-handed combat", UNQ, UNQ_MEL_MAEDHROS },
{ "Will abilities are twice effective, can break fate-cursed items", UNQ, UNQ_WIL_TURIN },
{ "Song of Disguise checks add your Perception skill", UNQ, UNQ_SNG_TURGON },
{ "Song skill is not reduced for woven minor themes", UNQ, UNQ_WOVEN_MASTER },
{ "If you die story death counter is not increased", RHF, RHF_GIFTERU }, 
{ "Deppending on the number of Silmarils retrieved there is a chance to murder your kin", RHF, RHF_KINSLAYER },
{ "You get more complex curses", RHF, RHF_CURSE }, 
{ "Can steal a Silmaril in the end", RHF, RHF_TREACHERY },
{ "Decreased ability price", RHF, RHF_FREE }, 
{ "Encounter more dangerous creatures", RHF, RHF_MOR_CURSE },
{ "Kheled-zaram gives +30 bonus to identification", RHF, RHF_KHELED_ZARAM }
};

const size_t info_flags_desc_n = sizeof(info_flags_desc) / sizeof(info_flags_desc[0]);

/*
 * Increase player's hit points by the given percentage of maximum, notice
 * effects
 */
bool hp_player(int x, bool percent, bool message)
{
    int points;

    if (percent)
        points = (p_ptr->mhp * x) / 100;
    else
        points = x;

    /* Healing needed */
    if ((p_ptr->chp < p_ptr->mhp) && (points > 0))
    {
        /* Gain hitpoints */
        p_ptr->chp += points;

        /* Enforce maximum */
        if (p_ptr->chp >= p_ptr->mhp)
        {
            p_ptr->chp = p_ptr->mhp;
            p_ptr->chp_frac = 0;
        }

        /* Redraw */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);

        app_session_note_animation(app_session_current(),
            APP_ANIMATION_HINT_DAMAGE, APP_DUNGEON_PLAYER_SUBJECT, -points,
            p_ptr->chp, 0,
            APP_SNAPSHOT_INVALIDATE_STATUS | APP_SNAPSHOT_INVALIDATE_MAP);

        if (message)
        {
            /* Heal 0-4 */
            if (points < 5)
            {
                msg_print("You feel a little better.");
            }

            /* Heal 5-10 */
            else if (points < 10)
            {
                msg_print("You feel better.");
            }

            /* Heal 10-25 */
            else if (points < 25)
            {
                msg_print("You feel much better.");
            }

            /* Heal 35+ */
            else
            {
                msg_print("You feel very good.");
            }
        }

        /* Notice */
        return (true);
    }

    /* Ignore */
    return (false);
}

/*
 * Leave a "glyph of warding" which prevents monster movement
 */
void warding_glyph(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    /* XXX XXX XXX */
    if (!cave_clean_bold(py, px))
    {
        msg_print("The object resists the spell.");
        return;
    }

    /* Create a glyph */
    cave_set_feat(py, px, FEAT_GLYPH);
}

/*
 * Array of stat "descriptions"
 */
static cptr desc_stat_pos[] = { "strong", "dextrous", "healthy", "attuned" };

/*
 * Array of stat "descriptions"
 */
static cptr desc_stat_neg[] = { "weak", "awkward", "sickly", "drained" };

/*
 * Lose a "point"
 */
bool do_dec_stat(int stat, monster_type* m_ptr)
{
    bool resistance = false; // default to soothe compiler warnings

    /* Turin character resistance check first */
    if (turin_resist_bad_effect())
        return (true);

    /* Get the "sustain" */
    switch (stat)
    {
    case A_STR:
        resistance = p_ptr->sustain_str;
        break;
    case A_DEX:
        resistance = p_ptr->sustain_dex;
        break;
    case A_CON:
        resistance = p_ptr->sustain_con;
        break;
    case A_GRA:
        resistance = p_ptr->sustain_gra;
        break;
    }

    /* Saving throw */
    if (saving_throw(m_ptr, resistance))
    {
        /* Message */
        msg_format(
            "You feel %s for a moment, but it passes.", desc_stat_neg[stat]);

        // possibly identify relevant items
        switch (stat)
        {
        case A_STR:
            ident_resist(TR2_SUST_STR);
            break;
        case A_DEX:
            ident_resist(TR2_SUST_DEX);
            break;
        case A_CON:
            ident_resist(TR2_SUST_CON);
            break;
        case A_GRA:
            ident_resist(TR2_SUST_GRA);
            break;
        }

        /* Notice effect */
        return (true);
    }

    /* Attempt to reduce the stat */
    if (dec_stat(stat, 1, false))
    {
        /* Message */
        msg_format("You feel %s.", desc_stat_neg[stat]);

        /* Notice effect */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Restore lost "points" in a stat
 */
bool do_res_stat(int stat, int points)
{
    /* Attempt to increase */
    if (res_stat(stat, points))
    {
        /* Message */
        msg_format("You feel less %s.", desc_stat_neg[stat]);

        /* Notice */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Gain a "point" in a stat
 */
bool do_inc_stat(int stat)
{
    bool res;

    /* Restore stat */
    res = res_stat(stat, 20);

    /* Attempt to increase */
    if (inc_stat(stat))
    {
        /* Message */
        msg_format("You feel %s!", desc_stat_pos[stat]);

        /* Notice */
        return (true);
    }

    /* Restoration worked */
    if (res)
    {
        /* Message */
        msg_format("You feel less %s.", desc_stat_neg[stat]);

        /* Notice */
        return (true);
    }

    /* Nothing obvious */
    return (false);
}

/*
 * Identify everything being carried.
 */
void identify_pack(void)
{
    int i;

    /* Simply identify and know every item */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Aware and Known */
        object_aware(o_ptr);
        object_known(o_ptr);
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

/*
 * Hack -- Removes curse from an object.
 */
void uncurse_object(object_type* o_ptr)
{
    /* Uncurse it */
    o_ptr->ident &= ~(IDENT_CURSED);
    o_ptr->ident |= IDENT_UNCURSED;

    /* Remove special inscription, if any */
    if (o_ptr->discount >= INSCRIP_NULL)
        o_ptr->discount = 0;

    /* The object has been "sensed" */
    o_ptr->ident |= (IDENT_SENSE);

    /* Newly compatible stacks should collapse on the next inventory pass. */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    if ((o_ptr >= inventory) && (o_ptr < inventory + INVEN_TOTAL))
    {
        int slot = (int)(o_ptr - inventory);

        if ((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2))
            p_ptr->redraw |= (PR_QUIVER);
        else if (slot == INVEN_LITE)
            p_ptr->redraw |= (PR_LIGHT);
    }
}

/*
 * Removes curses from items in inventory.
 *
 * Note that Items bound by the Oath of Feanor (TR3_PERMA_CURSE)
 * can NEVER be uncursed by normal means - only the holy light
 * of items with the BREAKS_PERMA_CURSE flag can break such an oath.
 *
 * Note that if "all" is false, then Items which are
 * "Heavy-Cursed" (Mormegil, Calris, and Weapons of Morgul)
 * will not be uncursed.
 */
static int remove_curse_aux(bool star_curse)
{
    int i, cnt = 0;

    /* Attempt to uncurse items being worn */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b f1, f2, f3;

        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Uncursed already */
        if (!cursed_p(o_ptr))
            continue;

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        /* Heavily Cursed Items need a special spell */
        if (!star_curse && (f3 & (TR3_HEAVY_CURSE)))
            continue;

        /* Items bound by the Oath of Feanor can only be freed by a Silmaril */
        if (f3 & (TR3_PERMA_CURSE))
            continue;

        /* Uncurse the object */
        uncurse_object(o_ptr);

        /* Recalculate the bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Window stuff */
        p_ptr->window |= (PW_EQUIP);

        /* Count the uncursings */
        cnt++;
    }

    /* Return "something uncursed" */
    return (cnt);
}

/*
 * Remove most curses
 */
bool remove_curse(bool star_curse) { return (remove_curse_aux(star_curse)); }

static void append_resist_name(char* buf, size_t buf_len, const char* name)
{
    if (buf[0] != '\0')
        SDL_strlcat(buf, ", ", buf_len);
    SDL_strlcat(buf, name, buf_len);
}

static void append_resist_entry(char* buf, size_t buf_len, const char* name,
    int tier)
{
    if (tier <= 1) {
        append_resist_name(buf, buf_len, name);
        return;
    }

    char labeled[32];
    strnfmt(labeled, sizeof(labeled), "%s (x%d)", name, tier);
    append_resist_name(buf, buf_len, labeled);
}

static byte resist_color(const char* name)
{
    char base[32];
    size_t i = 0;

    while (name[i] && name[i] != ' ' && name[i] != '(' && i < sizeof(base) - 1) {
        base[i] = name[i];
        i++;
    }
    base[i] = '\0';

    if (streq(base, "fire"))
        return TERM_L_RED;
    if (streq(base, "cold"))
        return TERM_L_BLUE;
    if (streq(base, "poison"))
        return TERM_GREEN;
    if (streq(base, "bleeding"))
        return TERM_RED;
    if (streq(base, "fear"))
        return TERM_VIOLET;
    if (streq(base, "blindness"))
        return TERM_L_DARK;
    if (streq(base, "confusion"))
        return TERM_VIOLET;
    if (streq(base, "stunning"))
        return TERM_ORANGE;
    if (streq(base, "hallucination"))
        return TERM_VIOLET;

    return TERM_WHITE;
}

static bool self_knowledge_append_rich_span(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, cptr text)
{
    char buf[APP_UI_TEXT_MAX];
    size_t len;

    if (!scene || !panel || !text || !text[0])
        return true;

    len = strlen(text);
    while (len > 0)
    {
        size_t chunk_len = len;

        if (chunk_len >= sizeof(buf))
            chunk_len = sizeof(buf) - 1u;
        memcpy(buf, text, chunk_len);
        buf[chunk_len] = '\0';
        if (!app_ui_panel_add_rich_text(scene, panel, attr, buf))
            return false;
        text += chunk_len;
        len -= chunk_len;
    }

    return true;
}

static bool self_knowledge_scene_add_resist_paragraph(app_ui_scene* scene,
    app_ui_panel* panel, cptr text)
{
    const char* prefix_resist = "You resist ";
    const char* prefix_vuln = "You are vulnerable to ";
    const char* prefix_none = "You do not resist ";
    const char* prefix = NULL;
    const char* list;
    const char* p;

    if (!scene || !panel || !text || !text[0])
        return true;

    if (strncmp(text, prefix_resist, strlen(prefix_resist)) == 0)
        prefix = prefix_resist;
    else if (strncmp(text, prefix_vuln, strlen(prefix_vuln)) == 0)
        prefix = prefix_vuln;
    else if (strncmp(text, prefix_none, strlen(prefix_none)) == 0)
        prefix = prefix_none;

    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;
    if (!prefix)
        return self_knowledge_append_rich_span(scene, panel, TERM_WHITE, text);

    if (!self_knowledge_append_rich_span(scene, panel, TERM_WHITE, prefix))
        return false;

    list = text + strlen(prefix);
    p = list;
    while (*p)
    {
        const char* comma = strstr(p, ", ");
        size_t len = comma ? (size_t)(comma - p) : strlen(p);

        if (len > 0)
        {
            char token[64];

            if (len >= sizeof(token))
                len = sizeof(token) - 1u;
            memcpy(token, p, len);
            token[len] = '\0';
            if (!self_knowledge_append_rich_span(scene, panel,
                    resist_color(token), token))
            {
                return false;
            }
        }

        if (!comma)
            break;
        if (!self_knowledge_append_rich_span(scene, panel, TERM_WHITE, ", "))
            return false;
        p = comma + 2;
    }

    return true;
}

static bool self_knowledge_scene_add_entry(app_ui_scene* scene,
    app_ui_panel* panel, cptr main_text, cptr detail_text, bool good)
{
    if (!scene || !panel || !main_text || !main_text[0])
        return true;

    if (!detail_text || !detail_text[0])
        return self_knowledge_scene_add_resist_paragraph(scene, panel,
            main_text);

    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;
    if (!self_knowledge_append_rich_span(scene, panel, TERM_WHITE, main_text))
        return false;
    if (!self_knowledge_append_rich_span(scene, panel, TERM_WHITE, " "))
        return false;
    return self_knowledge_append_rich_span(scene, panel,
        good ? TERM_GREEN : TERM_L_RED, detail_text);
}

static bool self_knowledge_build_ui_scene(app_ui_scene* scene, char s[][200],
    char t[][200], bool good[], int count, int start, int* out_next)
{
    app_ui_panel* panel;
    int next = start;
    int page_entries = 0;
    char subtitle[APP_UI_TEXT_MAX];

    if (out_next)
        *out_next = start;
    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1440);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Your Attributes");

    if (count <= 0 || start >= count)
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel)
            || !app_ui_panel_add_rich_text(scene, panel, TERM_WHITE,
                "You discern nothing unusual about yourself."))
        {
            return false;
        }
        if (out_next)
            *out_next = count;
    }
    else
    {
        while (next < count && page_entries < SELF_KNOWLEDGE_PAGE_ENTRY_MAX)
        {
            if (!self_knowledge_scene_add_entry(scene, panel, s[next], t[next],
                    good[next]))
            {
                return false;
            }

            next++;
            page_entries++;
        }

        if (next == start)
        {
            if (!self_knowledge_scene_add_entry(scene, panel, s[next], t[next],
                    good[next]))
            {
                return false;
            }
            next++;
        }

        strnfmt(subtitle, sizeof(subtitle), "Entries %d-%d of %d", start + 1,
            next, count);
        app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
        if (out_next)
            *out_next = next;
    }

    if (next < count)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Any", "Next");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Close");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Any", "Close");
    }

    return true;
}

static bool display_attributes_ui(char s[][200], char t[][200], bool good[],
    int count)
{
    ui_information_scene_scope scope;
    int start = 0;

    if (!ui_information_scene_enter(&scope))
        return false;

    while (true)
    {
        app_ui_scene scene;
        int next = start;
        int key;

        if (!self_knowledge_build_ui_scene(&scene, s, t, good, count, start,
                &next)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == ESCAPE || next <= start || next >= count)
            break;
        start = next;
    }

    ui_information_scene_leave(&scope);
    return true;
}

/*
 * Hack -- acquire self knowledge
 *
 * List various information about the player and/or his current equipment.
 *
 * Use the "roff()" routines, perhaps.  XXX XXX XXX
 *
 * Use the "show_file()" method, perhaps.  XXX XXX XXX
 *
 * This function uses page wrapping and column management to ensure content 
 * stays within screen bounds. Long descriptions wrap to the next line.
 */
void self_knowledge(void)
{
    int i = 0, j, k;
    u32b f1 = 0L, f2 = 0L, f3 = 0L;
    object_type* o_ptr;
    
    char s[100][200];
    char t[100][200];
    bool good[100];
    bool identify[INVEN_TOTAL];
    
    int light = 0, mel = 0, arc = 0, stl = 0, medic = 0;

    if (p_ptr->update)
        update_stuff();

    if (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px)
        != BIG_CAVE_NONE
        || ((cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0))
    {
        log_partition_debug_for_point("self_knowledge", p_ptr->py, p_ptr->px);
        log_debug(
            "self_knowledge: base_fire=%d base_cold=%d base_pois=%d fear=%d stun=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d effective_fire=%d effective_cold=%d effective_pois=%d",
            p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
            p_ptr->resist_fear, p_ptr->resist_stun, p_ptr->oppose_fire,
            p_ptr->oppose_cold, p_ptr->oppose_pois, resist_fire(),
            resist_cold(), resist_pois());
    }

    // Initialize arrays
    for (j = 0; j < 100; j++) {
        s[j][0] = '\0';
        t[j][0] = '\0';
        good[j] = true;
    }
    
    for (j = 0; j < INVEN_TOTAL; j++) {
        identify[j] = false;
    }

    // Get item flags from equipment
    for (k = INVEN_WIELD; k < INVEN_TOTAL; k++) {
        u32b t1, t2, t3, t4;
        o_ptr = &inventory[k];

        if (!o_ptr->k_idx) continue;

        object_flags4(o_ptr, &t1, &t2, &t3, &t4);

        {
            bool is_quiver1 = (k == INVEN_QUIVER1);
            bool is_quiver2 = (k == INVEN_QUIVER2);
            bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, t3);

            if (is_quiver1)
                continue;
            if (is_quiver2 && !is_throwing_item)
                continue;
        }
        f1 |= t1; f2 |= t2; f3 |= t3;

        if (t2 & TR2_LIGHT) light++;
        if (t2 & TR2_DARKNESS) light--;
        if (t4 & TR4_UNLIGHT) light--;
        if (t1 & TR1_MEL) mel += o_ptr->skill_bonus[S_MEL];
        if (t1 & TR1_ARC) arc += o_ptr->skill_bonus[S_ARC];
        if (t1 & TR1_STL) stl += o_ptr->skill_bonus[S_STL];
        if (t3 & TR3_MEDIC) medic++;
    }

    // Add curse information
    int active_ids[64], n_active = 0;
    
    for (int id = 0; id < (int)z_info->cu_max && id < 64; id++) {
        if (CURSE_GET(id) > 0) active_ids[n_active++] = id;
    }
    
    // Add race/character trait information
    u32b rhf_bits = p_info[p_ptr->prace].flags | c_info[p_ptr->pcharacter].flags;
    u32b unq_bits = c_info[p_ptr->pcharacter].flags_u;
    int cand[64], n = 0;
    
    for (size_t idx = 0; idx < info_flags_desc_n && n < 64; idx++) {
        const flag_name *d = &info_flags_desc[idx];
        if ((d->set == RHF && (rhf_bits & d->flag)) || 
            (d->set == UNQ && (unq_bits & d->flag))) {
            cand[n++] = (int)idx;
        }
    }
    
    // Show either curse or flag information, not both
    bool show_curse = (n_active > 0) && one_in_(6);
    bool show_flag = (n > 0) && one_in_(6);
    
    if (show_curse) {
        int pick = active_ids[rand_int(n_active)];
        curse_type *c = &cu_info[pick];
        cptr cname = cu_name + c->name;
        cptr cdesc = cu_text + c->text;
        cptr cpower = cu_text + c->power;
        
        strnfmt(s[i], 200, "A shadow upon you: %s", cname);
        strnfmt(t[i], 200, "%s  %s", cdesc, cpower);
        good[i] = false;
        i++;
        CURSE_SEEN_SET(pick);
    }
    if (show_flag) {
        const flag_name *d = &info_flags_desc[cand[rand_int(n)]];
        strnfmt(s[i], 200, "You sense a hidden trait.");
        strnfmt(t[i], 200, "%s", d->name);
        good[i] = true;
        i++;
    }

    // Equipment-based traits
    if (f2 & TR2_TRAITOR) {
        strnfmt(s[i], 80, "You feel doom hastening toward you");
        strnfmt(t[i], 80, "(you will be betrayed)");
        good[i] = false; i++;
    }
    
    if (f3 & TR3_CHEAT_DEATH) {
        strnfmt(s[i], 80, "You are protected from serious harm");
        strnfmt(t[i], 80, "(you will survive a killing blow)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_AVOID_TRAPS) {
        strnfmt(s[i], 80, "Your feet do not trigger traps");
        strnfmt(t[i], 80, "(does not protect from webs, roosts and pits)");
        good[i] = true; i++;
    }
    
    if (medic > 0) {
        strnfmt(s[i], 80, "You gain extra health from healing items");
        strnfmt(t[i], 80, "(%d%%)", 33 * medic);
        good[i] = true; i++;
    }
    
    if (f3 & TR3_STAND_FAST) {
        strnfmt(s[i], 80, "You stand fast against your foes");
        strnfmt(t[i], 80, "(you cannot be moved by enemy abilities)");
        good[i] = true; i++;
    }

    if (p_ptr->see_inv > 0) {
        strnfmt(s[i], 80, "You can see invisible creatures");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    if (p_ptr->free_act > 0) {
        strnfmt(s[i], 80, "You move freely");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    if (p_ptr->regenerate > 0) {
        strnfmt(s[i], 80, "You regenerate quickly");
        t[i][0] = '\0';
        good[i] = true; i++;
    }

    {
        char resist_buf[200];
        char no_resist_buf[200];
        char vuln_buf[200];
        int res;

        resist_buf[0] = '\0';
        no_resist_buf[0] = '\0';
        vuln_buf[0] = '\0';

        res = resist_fire();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "fire",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "fire", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "fire");

        res = resist_cold();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "cold",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "cold", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "cold");

        res = resist_pois();
        if (res > 1) {
            append_resist_entry(resist_buf, sizeof(resist_buf), "poison",
                res - 1);
        }
        else if (res < 1) {
            int tier = (-res) - 1;
            if (tier < 1)
                tier = 1;
            append_resist_entry(vuln_buf, sizeof(vuln_buf), "poison", tier);
        }
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "poison");

        res = p_ptr->resist_bleed;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "bleeding");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "bleeding");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "bleeding");

        res = p_ptr->resist_fear;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "fear");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "fear");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "fear");

        res = p_ptr->resist_blind;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "blindness");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "blindness");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "blindness");

        res = p_ptr->resist_confu;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "confusion");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "confusion");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "confusion");

        res = p_ptr->resist_stun;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "stunning");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "stunning");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "stunning");

        res = p_ptr->resist_hallu;
        if (res > 0)
            append_resist_name(resist_buf, sizeof(resist_buf), "hallucination");
        else if (res < 0)
            append_resist_name(vuln_buf, sizeof(vuln_buf), "hallucination");
        else
            append_resist_name(no_resist_buf, sizeof(no_resist_buf), "hallucination");

        if (resist_buf[0] != '\0') {
            strnfmt(s[i], 200, "You resist %s", resist_buf);
            t[i][0] = '\0';
            good[i] = true; i++;
        }

        if (vuln_buf[0] != '\0') {
            strnfmt(s[i], 200, "You are vulnerable to %s", vuln_buf);
            t[i][0] = '\0';
            good[i] = false; i++;
        }

        if (no_resist_buf[0] != '\0') {
            strnfmt(s[i], 200, "You do not resist %s", no_resist_buf);
            t[i][0] = '\0';
            good[i] = false; i++;
        }
    }

    // Player state information
    if (p_ptr->pspeed < 2) {
        strnfmt(s[i], 80, "You are moving slowly");
        strnfmt(t[i], 80, "(speed %d)", p_ptr->pspeed);
        good[i] = false; i++;
    } else if (p_ptr->pspeed > 2) {
        strnfmt(s[i], 80, "You are moving quickly");
        strnfmt(t[i], 80, "(speed %d)", p_ptr->pspeed);
        good[i] = true; i++;
    }
    
    if (p_ptr->stealth_mode) {
        strnfmt(s[i], 80, "You are moving carefully");
        strnfmt(t[i], 80, "(+5 Stealth)");
        good[i] = true; i++;
    }

    // Hunger effects
    if (p_ptr->hunger < 0) {
        strnfmt(s[i], 80, "You grow hungry %sslowly", (p_ptr->hunger < -1) ? "very " : "");
        strnfmt(t[i], 80, "(1/%d the normal rate)", int_exp(3, -p_ptr->hunger));
        good[i] = true; i++;
    } else if (p_ptr->hunger > 0) {
        strnfmt(s[i], 80, "You burn with a%s unnatural hunger", (p_ptr->hunger > 1) ? " most" : "n");
        strnfmt(t[i], 80, "(%d times the normal rate)", int_exp(3, p_ptr->hunger));
        good[i] = false; i++;
    }

    // Status effects
    struct { bool condition; const char* text; const char* detail; bool is_good; } status_effects[] = {
        {p_ptr->blind, "You cannot see", "", false},
        {p_ptr->image, "You are hallucinating", "", false},
        {p_ptr->confused, "You are confused", "", false},
        {p_ptr->afraid, "You are terrified", "", false},
        {p_ptr->cut, "You are bleeding", "", false},
        {p_ptr->poisoned, "You are poisoned", "", false},
        {p_ptr->rage, "You are in a dark rage", "", false},
        {0, NULL, NULL, false} // Sentinel
    };
    
    for (int idx = 0; status_effects[idx].text; idx++) {
        if (status_effects[idx].condition) {
            strnfmt(s[i], 80, "%s", status_effects[idx].text);
            strnfmt(t[i], 80, "%s", status_effects[idx].detail);
            good[i] = status_effects[idx].is_good;
            i++;
        }
    }
    
    // Stun with special handling
    if (p_ptr->stun) {
        strnfmt(s[i], 80, "You are %sstunned", (p_ptr->stun <= 50) ? "heavily " : "");
        strnfmt(t[i], 80, "(-%d to all skills)", (p_ptr->stun <= 50) ? 2 : 4);
        good[i] = false; i++;
    }

    // Temporary stat boosts
    struct { bool condition; const char* text; const char* detail; } temp_stats[] = {
        {p_ptr->tmp_str, "You feel stronger", "(+3 Strength)"},
        {p_ptr->tmp_dex, "You feel more agile", "(+3 Dexterity)"},
        {p_ptr->tmp_con, "You feel more resilient", "(+3 Constitution)"},
        {p_ptr->tmp_gra, "You feel more attuned to the world", "(+3 Grace)"},
        {p_ptr->tmp_per, "Your perceptions are heightened", "(+10 Perception)"},
        {0, NULL, NULL} // Sentinel
    };
    
    for (int idx = 0; temp_stats[idx].text; idx++) {
        if (temp_stats[idx].condition) {
            strnfmt(s[i], 80, "%s", temp_stats[idx].text);
            strnfmt(t[i], 80, "%s", temp_stats[idx].detail);
            good[i] = true;
            i++;
        }
    }

    // Add equipment stat modifiers
    const char* equip_stat_names[] = {
        "Strength", "Dexterity", "Constitution", "Grace"
    };
    for (int stat = 0; stat < 4; stat++) {
        if (p_ptr->stat_equip_mod[stat] != 0) {
            strnfmt(s[i], 80, "Your %s is affected by your equipment", 
                    (stat == A_STR) ? "strength" : (stat == A_DEX) ? "dexterity" : 
                    (stat == A_CON) ? "constitution" : "grace");
            strnfmt(t[i], 80, "(%+d %s)", p_ptr->stat_equip_mod[stat],
                equip_stat_names[stat]);
            good[i] = (p_ptr->stat_equip_mod[stat] > 0);
            i++;
        }
    }

    // Add skill modifiers
    if (mel != 0) {
        strnfmt(s[i], 80, "Your melee is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Melee)", mel);
        good[i] = (mel > 0); i++;
    }
    if (arc != 0) {
        strnfmt(s[i], 80, "Your archery is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Archery)", arc);
        good[i] = (arc > 0); i++;
    }
    if (stl != 0) {
        strnfmt(s[i], 80, "Your stealth is affected by your equipment");
        strnfmt(t[i], 80, "(%+d Stealth)", stl);
        good[i] = (stl > 0); i++;
    }

    // Light effects
    if (light > 0) {
        strnfmt(s[i], 80, "Your equipment glows with an inner light");
        strnfmt(t[i], 80, "(%+d radius)", light);
        good[i] = true; i++;
    } else if (light < 0) {
        strnfmt(s[i], 80, "Your equipment radiates an unnatural darkness");
        strnfmt(t[i], 80, "(%+d radius)", light);
        good[i] = false; i++;
    }

    // Analyze weapons and equipment for special properties
    analyze_weapon_properties(&i, s, t, good, identify, INVEN_WIELD, "weapon");
    if (p_ptr->mds2 > 0) {
        analyze_weapon_properties(&i, s, t, good, identify, INVEN_ARM, "off-hand weapon");
    }
    analyze_weapon_properties(&i, s, t, good, identify, INVEN_BOW, "bow");

    // Add abilities from equipment
    for (j = 0; j < S_MAX; j++) {
        for (k = 0; k < ABILITIES_MAX; k++) {
            if (p_ptr->have_ability[j][k] && !p_ptr->innate_ability[j][k]) {
                strnfmt(s[i], 80, "Your equipment grants you the ability: %s",
                        b_name + (&b_info[ability_index(j, k)])->name);
                t[i][0] = '\0'; // No detail text
                good[i] = true;
                i++;
            }
        }
    }

    // Display the information
    if (!display_attributes_ui(s, t, good, i))
        log_warn("self knowledge: semantic presentation required");
    
    // Identify items that revealed information
    identify_revealed_items(identify);
}

// Helper function to analyze weapon properties
void analyze_weapon_properties(int* count, char s[][200], char t[][200], bool good[], 
                              bool identify[], int slot, const char* weapon_name)
{
    object_type* o_ptr = &inventory[slot];
    if (!o_ptr->k_idx) return;
    
    u32b f1, f2, f3, f4;
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    int i = *count;
    
    // Special attack bonuses
    if (f1 & TR1_SHARPNESS) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s cuts easily through armour", weapon_name);
        strnfmt(t[i], 200, "(ignore 50%% of protection)");
        good[i] = true; i++;
    }
    
    if (f1 & TR1_SHARPNESS2) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s cuts exceptionally easily through armour", weapon_name);
        strnfmt(t[i], 200, "(ignore 100%% of protection)");
        good[i] = true; i++;
    }
    
    if (f1 & TR1_VAMPIRIC) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s drains life from your enemies", weapon_name);
        strnfmt(t[i], 200, "(+7 health per kill)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_ACCURATE) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s %s", weapon_name, 
                (slot == INVEN_BOW) ? "fires with unerring precision" : "is unusually well balanced");
        strnfmt(t[i], 200, "(reroll missed attacks)");
        good[i] = true; i++;
    }
    
    if (f3 & TR3_CUMBERSOME) {
        identify[slot] = true;
        strnfmt(s[i], 200, "Your %s is cumbersome", weapon_name);
        strnfmt(t[i], 200, "(no critical hits)");
        good[i] = false; i++;
    }

    // Brand effects
    const char* brand_names[] = {"shocks", "burns", "freezes", "poisons"};
    u32b brand_flags[] = {TR1_BRAND_ELEC, TR1_BRAND_FIRE, TR1_BRAND_COLD, TR1_BRAND_POIS};
    
    for (int b = 0; b < 4; b++) {
        if (f1 & brand_flags[b]) {
            identify[slot] = true;
            strnfmt(s[i], 200, "Your %s %s your foes", weapon_name, brand_names[b]);
            strnfmt(t[i], 200, "(+1 damage die)");
            good[i] = true; i++;
        }
    }

    // Slay effects
    typedef struct {
        u32b flag;
        int flagset;
        const char* name;
        bool use_effective;
    } slay_attr_t;

    const slay_attr_t slays[] = {
        { TR1_SLAY_ORC, 1, "orcs", false },
        { TR1_SLAY_TROLL, 1, "trolls", false },
        { TR1_SLAY_WOLF, 1, "wolves", false },
        { TR1_SLAY_SPIDER, 1, "spiders", false },
        { TR1_SLAY_RAUKO, 1, "raukar", false },
        { TR1_SLAY_DRAGON, 1, "dragons", false },
        { TR1_SLAY_UNDEAD, 1, "the undead", true },
        { TR4_SLAY_SERPENT, 4, "serpents", false },
        { TR4_SLAY_VAMPIRE, 4, "vampires", false },
        { TR4_SLAY_HORROR, 4, "horrors", true },
        { TR4_SLAY_CAT, 4, "cats", false },
        { TR4_SLAY_GIANT, 4, "giants", false },
    };

    for (size_t sl = 0; sl < (sizeof(slays) / sizeof(slays[0])); sl++) {
        u32b flags = (slays[sl].flagset == 4) ? f4 : f1;
        if (flags & slays[sl].flag) {
            identify[slot] = true;
            strnfmt(s[i], 200, "Your %s is especially %s against %s", weapon_name,
                    slays[sl].use_effective ? "effective" : "deadly", slays[sl].name);
            strnfmt(t[i], 200, "(+1 damage die)");
            good[i] = true; i++;
        }
    }

    if (f1 & TR1_SLAY_MAN_OR_ELF) {
        identify[slot] = true;
        strnfmt(s[i], 80, "Your %s is especially effective against men", weapon_name);
        strnfmt(t[i], 80, "(+1 damage die)");
        good[i] = true; i++;
        strnfmt(s[i], 80, "Your %s is especially effective against elves", weapon_name);
        strnfmt(t[i], 80, "(+1 damage die)");
        good[i] = true; i++;
    }
    
    *count = i;
}

// Helper function to identify revealed items
void identify_revealed_items(bool identify[])
{
    for (int i = 0; i < INVEN_TOTAL; i++) {
        if (identify[i]) {
            object_type* o_ptr = &inventory[i];
            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
                continue;
            }

            if (!object_known_p(o_ptr))
            {
                char o_short_name[80], o_full_name[80];

                object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);
                ident(o_ptr);
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format(
                    "You realize that your %s is %s.", o_short_name, o_full_name);
            }
        }
    }
}


/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_digger(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);

    if ((f1 & (TR1_TUNNEL)) && (o_ptr->pval > 0))
    {
        return (true);
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify "weapon"
 */
bool item_tester_hook_ided_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    case TV_BOW:
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_ided_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return (false);
    }
    }

    return (false);
}

/*
 * Hook to specify "armour"
 */
bool item_tester_hook_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_BOOTS:
    case TV_GLOVES:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify non-herb food
 */
bool item_tester_hook_non_herb_food(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_FOOD) && (o_ptr->pval > 300))
        return (true);

    return (false);
}

/*
 * Hook to specify light with fuel or that does not need fuel
 */
bool item_tester_hook_light_with_fuel(const object_type* o_ptr)
{
    if (o_ptr->tval != TV_LIGHT)
        return (false);

    if (o_ptr->timeout < 1 && fuelable_light_p(o_ptr))
        return (false);

    return (true);
}

/*
 * Hook to specify "enchantable amulet"
 */
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr)
{
    if ((o_ptr->tval == TV_AMULET) && (o_ptr->pval > 0))
        return (true);

    return (false);
}

/*
 * Identify an object chosen from the unified unidentified list.
 * Returns true if an item was identified.
 */
bool ident_spell(bool include_floor)
{
    int item;
    object_type* o_ptr;

    if (!display_unified_identify_menu(include_floor, &item, &o_ptr))
        return false;

    (void)do_ident_item(item, o_ptr);

    return true;
}

/*
 * Hook for "get_item()".  Determine if something is rechargable.
 */
bool item_tester_hook_recharge(const object_type* o_ptr)
{
    /* Recharge staffs */
    if (o_ptr->tval == TV_STAFF)
        return (true);

    /* Nope */
    return (false);
}

typedef struct recharge_target_entry
{
    int item;
    object_type* o_ptr;
} recharge_target_entry;

enum
{
    MAX_RECHARGE_TARGETS =
        INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK
};

static int recharge_collect_targets(recharge_target_entry entries[],
    int max_entries)
{
    int count = 0;
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

    if (!entries || max_entries <= 0)
        return 0;

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    for (int i = 0; i < INVEN_PACK && count < max_entries; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = i;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    for (int i = 0; i < floor_num && count < max_entries; i++)
    {
        int o_idx = floor_list[i];
        object_type* o_ptr = &o_list[o_idx];

        if (!item_tester_hook_recharge(o_ptr))
            continue;

        entries[count].item = 0 - o_idx;
        entries[count].o_ptr = o_ptr;
        count++;
    }

    return count;
}

static bool recharge_build_target_ui_scene(app_ui_scene* scene,
    const recharge_target_entry entries[], int count, int current, int top,
    int page_size)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    char prompt[APP_UI_TEXT_MAX];
    int visible_count;

    if (!scene || !entries || count <= 0 || current < 0 || current >= count)
        return false;

    visible_count = count - top;
    if (visible_count > page_size)
        visible_count = page_size;
    if (visible_count < 1)
        visible_count = 1;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 760, 1220);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Recharge which staff?");
    strnfmt(subtitle, sizeof(subtitle), "%d rechargeable target%s",
        count, (count == 1) ? "" : "s");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);

    if (count > page_size)
    {
        strnfmt(prompt, sizeof(prompt), "Showing %d-%d of %d", top + 1,
            top + visible_count, count);
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, prompt);
    }
    else
    {
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
            "Choose a target to receive the recharge.");
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "a-z", "Pick");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Cancel");

    for (int i = 0; i < visible_count; i++)
    {
        const recharge_target_entry* entry = &entries[top + i];
        object_type* o_ptr = entry->o_ptr;
        char key[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        char desc[80];
        bool highlighted = (top + i == current);
        byte attr;

        if (i < 26)
            strnfmt(key, sizeof(key), "%c", I2A(i));
        else
            key[0] = '\0';

        if (entry->item >= 0)
        {
            object_desc(desc, sizeof(desc), o_ptr, true, 3);
            strnfmt(label, sizeof(label), "%s: %s", mention_use(entry->item),
                desc);
            strnfmt(meta, sizeof(meta), "%c", index_to_label(entry->item));
        }
        else
        {
            object_desc_floor(desc, sizeof(desc), o_ptr, true, 3);
            strnfmt(label, sizeof(label), "On floor: %s", desc);
            SDL_strlcpy(meta, "-)", sizeof(meta));
        }

        attr = highlighted ? TERM_L_BLUE : object_display_color(o_ptr,
            object_default_text_color(o_ptr));

        if (!app_ui_panel_add_row_ex(panel, (s16b)entry->item, attr, attr,
                object_attr(o_ptr), object_char(o_ptr), true, highlighted, key,
                label, meta))
        {
            return false;
        }
    }

    {
        const recharge_target_entry* selected = &entries[current];
        char detail[APP_UI_TEXT_MAX];
        char desc[80];
        object_type* o_ptr = selected->o_ptr;
        int charges = MAX(o_ptr->pval, 0);

        if (selected->item >= 0)
            object_desc(desc, sizeof(desc), o_ptr, true, 3);
        else
            object_desc_floor(desc, sizeof(desc), o_ptr, true, 3);

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, desc);
        if (selected->item >= 0)
        {
            strnfmt(detail, sizeof(detail), "Location: %s",
                mention_use(selected->item));
        }
        else
        {
            SDL_strlcpy(detail, "Location: floor", sizeof(detail));
        }
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);

        strnfmt(detail, sizeof(detail), "Stored charges: %d", charges);
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, detail);
    }

    return true;
}

static bool recharge_choose_target_ui(const recharge_target_entry entries[],
    int count, int* out_item)
{
    ui_information_scene_scope scope;
    int current = 0;
    int top = 0;
    const int page_size = RECHARGE_TARGET_PAGE_SIZE;

    if (!entries || count <= 0 || !out_item)
        return false;
    if (!ui_information_scene_enter(&scope))
        return false;

    while (true)
    {
        app_ui_scene scene;
        int visible_count;
        int key;

        if (current < top)
            top = current;
        if (current >= top + page_size)
            top = current - page_size + 1;

        visible_count = count - top;
        if (visible_count > page_size)
            visible_count = page_size;

        if (!recharge_build_target_ui_scene(&scene, entries, count, current,
                top, page_size)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return false;
        }

        key = ui_information_scene_wait_key();
        switch (key)
        {
        case ESCAPE:
            ui_information_scene_leave(&scope);
            return false;

        case '\r':
        case '\n':
        case ' ':
#ifdef KC_ENTER
        case KC_ENTER:
#endif
            *out_item = entries[current].item;
            ui_information_scene_leave(&scope);
            return true;

        case '8':
        case 'k':
        case 'K':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            current = (current > 0) ? current - 1 : count - 1;
            break;

        case '2':
        case 'j':
        case 'J':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            current = (current + 1 < count) ? current + 1 : 0;
            break;

        default:
        {
            int pick;

            if (!isalpha((unsigned char)key))
                break;

            pick = A2I((char)tolower((unsigned char)key));
            if (pick >= 0 && pick < visible_count)
            {
                *out_item = entries[top + pick].item;
                ui_information_scene_leave(&scope);
                return true;
            }

            break;
        }
        }
    }
}

static bool recharge_choose_target(const recharge_target_entry entries[],
    int count, int* out_item)
{
    if (!entries || count <= 0 || !out_item)
        return false;

    return recharge_choose_target_ui(entries, count, out_item);
}

/*
 * Recharge a staff from the pack, equipment, or on the floor.
 *
 * Mage -- Recharge I --> recharge(5)
 * Mage -- Recharge II --> recharge(40)
 * Mage -- Recharge III --> recharge(100)
 *
 * Priest -- Recharge --> recharge(15)
 *
 * Scroll of recharging --> recharge(60)
 *
 * recharge(20) = 1/6 failure for empty 10th level wand
 * recharge(60) = 1/10 failure for empty 10th level wand
 *
 * It is harder to recharge high level, and highly charged wands.
 *
 * XXX XXX XXX Beware of "sliding index errors".
 *
 * Should probably not "destroy" over-charged items, unless we
 * "replace" them by, say, a broken stick or some such.  The only
 * reason this is okay is because "scrolls of recharging" appear
 * BEFORE all staves/wands/rods in the inventory.  Note that the
 * new "auto_sort_pack" option would correctly handle replacing
 * the "broken" wand with any other item (i.e. a broken stick).
 *
 */
bool recharge(int num)
{
    int item;
    int target_count;

    object_type* o_ptr;
    recharge_target_entry targets[MAX_RECHARGE_TARGETS];

    target_count = recharge_collect_targets(targets, N_ELEMENTS(targets));
    if (target_count <= 0)
    {
        msg_print("You have nothing to recharge.");
        return (false);
    }

    if (!recharge_choose_target(targets, target_count, &item))
        return (false);

    /* Get the item (in the pack) */
    if (item >= 0)
        o_ptr = &inventory[item];

    /* Get the item (on the floor) */
    else
        o_ptr = &o_list[0 - item];

    /* Attempt to Recharge a staff, or handle failure to recharge . */
    if (o_ptr->tval == TV_STAFF)
    {
        if (o_ptr->sval == SV_STAFF_RECHARGING
            && p_ptr->active_ability[S_WIL][WIL_CHANNELING])
        {
            num /= 2;
        }

        /* Recharge the staff. */
        o_ptr->pval += num;

        if (object_aware_p(o_ptr) && (o_ptr->ident & (IDENT_EMPTY)))
        {
            object_aware(o_ptr);
            object_known(o_ptr);
        }

        /* Hack -- we no longer think the item is empty */
        o_ptr->ident &= ~(IDENT_EMPTY);
    }

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Something was done */
    return (true);
}


/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ided_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (object_known_p(o_ptr))
            return (true);
        else
            return false;
    }
    }

    return (false);
}

/*
 * Hook to specify "arrows"
 */
bool item_tester_hook_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Hook to specify ordinary arrows
 */
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    {
        if (o_ptr->name1 || object_has_ego(o_ptr) || o_ptr->att > 0)
            return false;
        return true;
    }
    }

    return false;
}

/*
 * Identifies all objects in the equipment, inventory, and supplies.
 */
void identify_pack_contents(void)
{
    int item;
    object_type* o_ptr;

    /* Identify equipment */
    for (item = INVEN_WIELD; item < INVEN_TOTAL; item++)
    {
        /* Get the object */
        o_ptr = &inventory[item];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        (void)do_ident_item(item, o_ptr);
    }

    /* Identify inventory */
    for (item = 0; item < INVEN_WIELD; item++)
    {
        while (true)
        {
            /* Get the object */
            o_ptr = &inventory[item];

            /* Ignore empty objects */
            if (!o_ptr->k_idx)
                break;

            /* Ignore known objects */
            if (object_known_p(o_ptr))
                break;

            /* Identify it */
            (void)do_ident_item(item, o_ptr);
            break;
        }
    }

    /* Identify supplies */
    int supply_count = supplies_entry_count();
    for (int supply_idx = 0; supply_idx < supply_count; supply_idx++)
    {
        o_ptr = supplies_entry_at(supply_idx);
        if (!o_ptr || !o_ptr->k_idx)
            continue;
        if (object_known_p(o_ptr))
            continue;

        (void)do_ident_item(SUPPLIES_INDEX + supply_idx, o_ptr);
    }
}

/* Mass-identify handler */
bool mass_identify(int rad)
{
    /* Direct the ball to the player */
    target_set_location(p_ptr->py, p_ptr->px);

    /* Cast the ball spell */
    fire_ball(GF_IDENTIFY, 5, 0, 0, -1, rad);

    /* Identify equipment, inventory, and supplies */
    identify_pack_contents();

    /* This spell always works */
    return (true);
}

/*
 * Execute some common code of the identify spells.
 * "item" is used to print the slot occupied by an object in equip/inven.
 * ANY negative value assigned to "item" can be used for specifying an object
 * on the floor (they don't have a slot, example: the code used to handle
 * GF_IDENTIFY in project_o).
 * The return value is retained for compatibility and is always neutral.
 */
int do_ident_item(int item, object_type* o_ptr)
{
    char o_name[80];

    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Description */
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Describe */
    if (item >= SUPPLIES_INDEX)
    {
        int supply_index = item - SUPPLIES_INDEX;
        msg_format("In your supplies: %s.", o_name);
        supplies_refresh_entry(supply_index);
    }
    else if (item >= INVEN_WIELD)
    {
        msg_format(
            "%^s: %s (%c).", describe_use(item), o_name, index_to_label(item));
    }
    else if (item >= 0)
    {
        msg_format("In your pack: %s (%c).", o_name, index_to_label(item));
    }
    else
    {
        msg_format("On the ground: %s.", o_name);
    }

    return 0;
}
