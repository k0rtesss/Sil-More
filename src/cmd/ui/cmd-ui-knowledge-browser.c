/* File: cmd-ui-knowledge-browser.c */
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
#include "platform-audio.h"
#include "platform-input.h"
#include "log/log.h"
#include "cmd-ui-knowledge.h"
#include "ui/ui-semantic-scene.h"

/*
 * Description of each object group.
 */
static cptr object_group_text[] = {
    "Herbs",
    "Potions",
    "Rings",
    "Amulets",
    "Staves",
    "Horns",
    "Swords",
    "Axes & Polearms",
    "Blunt Weapons",
    "Diggers",
    "Bows",
    "Light Sources",
    "Soft Armour",
    "Mail",
    "Shields",
    "Cloaks",
    "Gloves",
    "Helms",
    "Crowns",
    "Boots",
    "Chests",
    NULL
};

/*
 * TVALs of items in each group.
 */
static byte object_group_tval[] = {
    TV_FOOD,
    TV_POTION,
    TV_RING,
    TV_AMULET,
    TV_STAFF,
    TV_HORN,
    TV_SWORD,
    TV_POLEARM,
    TV_HAFTED,
    TV_DIGGING,
    TV_BOW,
    TV_LIGHT,
    TV_SOFT_ARMOR,
    TV_MAIL,
    TV_SHIELD,
    TV_CLOAK,
    TV_GLOVES,
    TV_HELM,
    TV_CROWN,
    TV_BOOTS,
    TV_CHEST,
    0
};

/*
 * Build a list of object indexes in the given group. Return the number of
 * objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
    int i;
    int j;
    int k;
    int object_cnt = 0;
    byte group_tval = object_group_tval[grp_cur];

    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        k = 0;

        if (!k_ptr->name)
            continue;

        for (j = 0; j < 4; j++)
            k += k_ptr->chance[j];

        if (!k)
            continue;

        if (!k_ptr->everseen)
            continue;

        if (k_ptr->tval == group_tval)
        {
            if (object_idx)
            {
                object_idx[object_cnt].type = OBJ_NORMAL;
                object_idx[object_cnt].idx = i;
            }

            object_cnt++;
        }
    }

    for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];

        if (!e_ptr->name)
            continue;

        if (!e_ptr->everseen)
            continue;

        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            if (e_ptr->tval[j] == group_tval)
            {
                if (object_idx)
                {
                    object_idx[object_cnt].type = OBJ_SPECIAL;
                    object_idx[object_cnt].idx = -1;
                    object_idx[object_cnt].e_idx = i;
                    object_idx[object_cnt].tval = group_tval;
                    object_idx[object_cnt].sval = -1;
                }
                object_cnt++;
                break;
            }
        }
    }

    if (object_idx)
        object_idx[object_cnt].type = OBJ_NONE;

    return object_cnt;
}

/*
 * Build a list of artefact indexes in the given group. Return the number of
 * eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
    int i;
    int object_cnt = 0;
    bool* okay;
    bool know_all = cheat_know;
    byte group_tval = object_group_tval[grp_cur];

    okay = mem_alloc_array(z_info->art_max, bool);

    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        bool revealed = (a_ptr->seen & ART_SEEN_REVEALED) != 0;

        okay[i] = false;

        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        if (!know_all && !p_ptr->wizard && !a_ptr->found_num && !revealed)
            continue;

        if (!know_all && !revealed && !a_ptr->cur_num)
            continue;

        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1)
            || (i == ART_MORGOTH_2))
        {
            continue;
        }

        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;

        okay[i] = true;
    }

    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        if (!okay[i])
            continue;

        if (a_ptr->tval == group_tval)
            object_idx[object_cnt++] = i;
    }

    object_idx[object_cnt] = 0;

    mem_free_null(okay);

    return object_cnt;
}

/*
 * Hack -- Create a "forged" artefact.
 */
static bool prepare_fake_artefact(object_type* o_ptr, byte name1)
{
    s16b i;
    artefact_type* a_ptr = &a_info[name1];

    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    if (!i)
        return false;

    object_prep(o_ptr, i);

    o_ptr->name1 = name1;
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
    memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));

    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    object_known(o_ptr);
    o_ptr->ident |= IDENT_SPOIL;

    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= IDENT_CURSED;

    return true;
}

/*
 * Describe fake artefact.
 */
void desc_art_fake(int a_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    object_wipe(i_ptr);

    prepare_fake_artefact(i_ptr, a_idx);

    handle_stuff();

    object_info_screen(i_ptr);
}

/*
 * Display known artefacts.
 */
void do_cmd_knowledge_artefacts(void)
{
    log_debug("Player opened artifacts knowledge screen");
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_ARTEFACTS);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] = {
    "Uniques",
    "Bats & Birds",
    "Canines",
    "Young Dragons",
    "Great Dragons",
    "Felines",
    "Giants",
    "Horrors",
    "Insects",
    "Young Spiders",
    "Spiders",
    "Nameless Things",
    "Orcs",
    "Raukar",
    "Serpents",
    "Ancient Serpents",
    "Trolls",
    "Vampires",
    "Valar",
    "Creeping Shadows",
    "Wights and Wraiths",
    "Plants",
    "People",
    NULL
};

/*
 * Symbols of monsters in each group.
 */
static cptr monster_group_char[] = {
    (char*)-1L,
    "b",
    "C",
    "d",
    "D",
    "f",
    "G",
    "H",
    "I",
    "m",
    "M",
    "N",
    "o",
    "R",
    "s",
    "S",
    "T",
    "v",
    "V",
    "w",
    "W",
    "&",
    "@",
    NULL
};

/*
 * Build a list of monster indexes in the given group.
 */
static int collect_monsters(int grp_cur, monster_list_entry* mon_idx, int mode)
{
    int i;
    int mon_count = 0;
    cptr group_char = monster_group_char[grp_cur];
    bool grp_unique = (monster_group_char[grp_cur] == (char*)-1L);

    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];
        bool unique = (r_ptr->flags1 & RF1_UNIQUE) != 0;

        if (!r_ptr->name)
            continue;

        if (grp_unique && !unique)
            continue;

        if (!(mode & 0x02) && !cheat_know && !know_monster_info
            && !l_ptr->tsights)
        {
            continue;
        }

        if (r_ptr->level > 25)
            continue;

        if (grp_unique || strchr(group_char, r_ptr->d_char))
        {
            mon_idx[mon_count++].r_idx = i;

            if (mode & 0x01)
                break;
        }
    }

    mon_idx[mon_count].r_idx = 0;

    return mon_count;
}

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_MONSTERS);
}

/*
 * Add a pval so the object descriptions do not look strange.
 */
void apply_magic_fake(object_type* o_ptr)
{
    s16b old_pval = o_ptr->pval;

    switch (o_ptr->tval)
    {
    case TV_DIGGING:
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;

    case TV_RING:
        switch (o_ptr->sval)
        {
        case SV_RING_STR:
        case SV_RING_DEX:
        case SV_RING_SECRETS:
        case SV_RING_ERED_LUIN:
        case SV_RING_LAIQUENDI:
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;

        case SV_RING_ACCURACY:
            if (o_ptr->att < 1)
                o_ptr->att = 1;
            break;

        case SV_RING_EVASION:
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;
            break;
        }
        break;

    case TV_AMULET:
        switch (o_ptr->sval)
        {
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        case SV_AMULET_BLESSED_REALM:
        case SV_AMULET_VIGILANT_EYE:
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;

        case SV_AMULET_PROTECTION:
            if (o_ptr->pd < 1)
                o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;
        }
        break;

    case TV_LIGHT:
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
            o_ptr->timeout = 0;
            break;
        }
        break;

    case TV_STAFF:
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;
    }

    {
        int pval_delta = (int)o_ptr->pval - (int)old_pval;

        if (pval_delta != 0)
        {
            object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr),
                pval_delta);
        }
    }
}

/*
 * Describe fake object.
 */
void knowledge_desc_obj_fake(int k_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    object_wipe(i_ptr);

    object_prep(i_ptr, k_idx);

    apply_magic_fake(i_ptr);

    i_ptr->ident |= IDENT_KNOWN;

    handle_stuff();

    object_info_screen(i_ptr);
}

static void knowledge_clamp_group_state(int* column, int* grp_cur, int* grp_top,
    int grp_cnt, int* entry_cur, int* entry_top, int entry_cnt, int per_page)
{
    if (grp_cnt <= 0)
    {
        *column = 0;
        *grp_cur = 0;
        *grp_top = 0;
        *entry_cur = 0;
        *entry_top = 0;
        return;
    }

    if (*grp_cur >= grp_cnt)
        *grp_cur = grp_cnt - 1;
    if (*grp_cur < 0)
        *grp_cur = 0;
    if (*grp_top > *grp_cur)
        *grp_top = *grp_cur;
    if (*grp_cur >= *grp_top + per_page)
        *grp_top = *grp_cur - per_page + 1;
    if (*grp_top < 0)
        *grp_top = 0;

    if (entry_cnt <= 0)
    {
        *column = 0;
        *entry_cur = 0;
        *entry_top = 0;
    }
    else
    {
        if (*entry_cur >= entry_cnt)
            *entry_cur = entry_cnt - 1;
        if (*entry_cur < 0)
            *entry_cur = 0;
        if (*entry_top > *entry_cur)
            *entry_top = *entry_cur;
        if (*entry_cur >= *entry_top + per_page)
            *entry_top = *entry_cur - per_page + 1;
        if (*entry_top < 0)
            *entry_top = 0;
    }

    if (*column < 0)
        *column = 0;
    if (*column > 1)
        *column = 1;
    if (entry_cnt <= 0)
        *column = 0;
}

static void knowledge_clamp_list_state(int* cur, int* top, int count,
    int per_page)
{
    if (count <= 0)
    {
        *cur = 0;
        *top = 0;
        return;
    }

    if (*cur >= count)
        *cur = count - 1;
    if (*cur < 0)
        *cur = 0;
    if (*top > *cur)
        *top = *cur;
    if (*cur >= *top + per_page)
        *top = *cur - per_page + 1;
    if (*top < 0)
        *top = 0;
}

static void knowledge_monster_summary(char* buf, size_t buflen, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(buf, buflen, "Total creatures slain: %u.",
            (unsigned)slay_count);
    }
    else
    {
        strnfmt(buf, buflen, "Known uniques: %u, slain uniques: %u.",
            (unsigned)known_uniques, (unsigned)dead_uniques);
    }
}

static int knowledge_collect_curses(int curse_idx[])
{
    int id;
    int count = 0;

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (CURSE_SEEN(id))
            curse_idx[count++] = id;
    }

    return count;
}

static cptr knowledge_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0)
        raw += 8;

    return raw;
}

static cptr knowledge_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name)
    {
        cptr raw = cu_name + cu_info[idx].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;

        return raw;
    }

    return knowledge_curse_display_name(idx);
}

static bool knowledge_scene_add_rich_paragraph(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, cptr text)
{
    if (!scene || !panel || !text || !text[0])
        return true;

    return app_ui_panel_begin_rich_paragraph(scene, panel)
        && app_ui_panel_add_rich_text(scene, panel, attr, text);
}

static app_ui_panel* knowledge_begin_curse_detail_scene(app_ui_scene* scene,
    cptr cname, cptr subtitle)
{
    return ui_semantic_scene_begin_plain(scene, APP_UI_SCENE_FLAG_DIM_BACKDROP,
        APP_UI_LAYER_MODAL, TERM_L_RED, cname, TERM_L_GREEN, subtitle,
        TERM_L_BLUE, 720, 1180);
}

static bool knowledge_present_curse_detail_scene(app_ui_scene* scene,
    app_ui_panel* panel, bool steamdeck, cptr accept_label)
{
    if (!scene || !panel)
        return false;

    if (steamdeck)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            accept_label, "Continue");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Any", "Continue");
    }

    return ui_semantic_scene_present_and_wait_key(scene, true, false,
        APP_WAIT_REASON_NONE, NULL);
}

static bool knowledge_show_curse_detail_ui(int curse_id)
{
    curse_type* c;
    cptr cname;
    cptr cdesc;
    cptr cpower;
    cptr bname;
    cptr bdesc;
    cptr bpower;
    bool steamdeck = steamdeck_controls_active();
    bool has_blessing_text;
    bool has_blessing_effect;
    bool has_blessing_info;
    bool show_blessing_name;
    char accept_label[16] = "";
    char effect_line[256];
    char blessing_line[256];
    char subtitle[APP_UI_TEXT_MAX];
    app_ui_scene scene;
    app_ui_panel* panel = NULL;

    if (curse_id < 0 || curse_id >= z_info->cu_max)
        return false;

    c = &cu_info[curse_id];
    cname = cu_name + c->name;
    cdesc = cu_text + c->text;
    cpower = cu_text + c->power;
    bname = knowledge_blessing_display_name(curse_id);
    bdesc = c->blessing_text ? (cu_text + c->blessing_text) : "";
    bpower = c->blessing_power ? (cu_text + c->blessing_power) : "";
    has_blessing_text = bdesc && *bdesc;
    has_blessing_effect = bpower && *bpower;
    has_blessing_info = has_blessing_text || has_blessing_effect
        || (c->blessing_name != 0);
    show_blessing_name = has_blessing_info && bname && bname[0]
        && strcmp(bname, cname) != 0;

    if (steamdeck)
    {
        controller_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    subtitle[0] = '\0';
    if (show_blessing_name)
        strnfmt(subtitle, sizeof(subtitle), "Blessing: %s", bname);

    panel = knowledge_begin_curse_detail_scene(&scene, cname, subtitle);
    if (!panel)
        return false;

    if (cdesc && cdesc[0]
        && !knowledge_scene_add_rich_paragraph(&scene, panel, TERM_WHITE, cdesc))
    {
        return false;
    }

    strnfmt(effect_line, sizeof(effect_line), "Effect: %s",
        (*cpower) ? cpower : "[no additional effect listed]");
    if (!knowledge_scene_add_rich_paragraph(&scene, panel, TERM_RED, effect_line))
        return false;

    if (has_blessing_info)
    {
        if (has_blessing_text
            && !knowledge_scene_add_rich_paragraph(&scene, panel, TERM_WHITE,
                bdesc))
        {
            return false;
        }

        strnfmt(blessing_line, sizeof(blessing_line), "Blessing effect: %s",
            has_blessing_effect ? bpower : "[no additional effect listed]");
        if (!knowledge_scene_add_rich_paragraph(&scene, panel, TERM_L_GREEN,
                blessing_line))
        {
            return false;
        }
    }

    return knowledge_present_curse_detail_scene(&scene, panel, steamdeck,
        accept_label);
}

static void knowledge_scene_add_footer_actions(app_ui_panel* panel,
    bool has_groups, bool can_recall)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "e", "Prev page");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
        "i", "Next page");
    if (has_groups)
    {
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4/6", "Group");
    }
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, can_recall,
        "r", "Recall");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Back");
}

static void knowledge_scene_add_group_detail_lines(app_ui_panel* panel,
    int grp_idx[], cptr group_text[], int grp_cnt, int grp_cur, int grp_top)
{
    int start;
    int i;

    if (!panel || !grp_idx || !group_text || grp_cnt <= 0)
        return;

    panel->flags |= APP_UI_PANEL_FLAG_DETAIL_LEADING;
    app_ui_panel_set_detail_title(panel, TERM_SLATE, "Groups");
    start = grp_top;
    if (start < 0)
        start = 0;
    if (start >= grp_cnt)
        start = grp_cnt - 1;
    if (start < 0)
        start = 0;
    if ((start + (int)APP_UI_DETAIL_LINE_MAX) > grp_cnt)
        start = MAX(0, grp_cnt - (int)APP_UI_DETAIL_LINE_MAX);

    for (i = start;
        i < grp_cnt && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX; i++)
    {
        char buf[APP_UI_TEXT_MAX];
        bool selected = (i == grp_cur);
        byte attr = selected ? TERM_L_BLUE : TERM_WHITE;

        strnfmt(buf, sizeof(buf), "%c %s", selected ? '>' : ' ',
            group_text[grp_idx[i]]);
        (void)app_ui_panel_add_detail_line(panel, attr, buf);
    }
}

static void knowledge_scene_append_artefact_rows(app_ui_panel* panel,
    int artefact_idx[], int artefact_cnt, int artefact_cur)
{
    int i;

    if (!panel || !artefact_idx || artefact_cnt <= 0)
        return;

    for (i = 0; i < artefact_cnt; i++)
    {
        object_type object_type_body;
        object_type* o_ptr = &object_type_body;
        artefact_type* a_ptr;
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        object_wipe(o_ptr);
        if (!prepare_fake_artefact(o_ptr, (byte)artefact_idx[i]))
            continue;

        a_ptr = &a_info[artefact_idx[i]];
        object_desc(label, sizeof(label), o_ptr, true, 0);
        meta[0] = '\0';
        if (cheat_know)
        {
            strnfmt(meta, sizeof(meta), "#%d L%d R%d", artefact_idx[i],
                a_ptr->level, a_ptr->rarity);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE, TERM_SLATE,
                object_attr(o_ptr), object_char(o_ptr), true,
                i == artefact_cur, "", label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_object_rows(app_ui_panel* panel,
    object_list_entry object_idx[], int object_cnt, int object_cur)
{
    int i;

    if (!panel || !object_idx || object_cnt <= 0)
        return;

    for (i = 0; i < object_cnt; i++)
    {
        object_list_entry* obj = &object_idx[i];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte attr = TERM_WHITE;
        byte icon_attr = 0;
        char icon_char = '\0';

        label[0] = '\0';
        meta[0] = '\0';

        switch (obj->type)
        {
        case OBJ_NORMAL:
        {
            object_kind* k_ptr = &k_info[obj->idx];

            attr = k_ptr->aware ? TERM_WHITE : TERM_SLATE;
            strip_name(label, obj->idx);
            if (k_ptr->aware)
            {
                object_type object_type_body;
                object_type* o_ptr = &object_type_body;

                object_wipe(o_ptr);
                object_prep(o_ptr, obj->idx);
                o_ptr->ident |= IDENT_KNOWN;
                icon_attr = object_attr(o_ptr);
                icon_char = object_char(o_ptr);
            }
            if (cheat_know)
                strnfmt(meta, sizeof(meta), "#%d", obj->idx);
            break;
        }

        case OBJ_SPECIAL:
        {
            ego_item_type* e_ptr = &e_info[obj->e_idx];

            attr = e_ptr->aware ? TERM_WHITE : TERM_SLATE;
            if (obj->sval == -1)
            {
                strnfmt(label, sizeof(label), "%s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char base_name[80];

                base_name[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval)
                        && (k_info[j].sval == obj->sval))
                    {
                        strip_name(base_name, j);
                        break;
                    }
                }

                strnfmt(label, sizeof(label), "%s %s", base_name,
                    &e_name[e_ptr->name]);
            }
            if (cheat_know)
                SDL_strlcpy(meta, "ego", sizeof(meta));
            break;
        }

        case OBJ_NONE:
        default:
            continue;
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, TERM_SLATE,
                icon_attr, icon_char, true, i == object_cur, "", label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_monster_rows(app_ui_panel* panel,
    monster_list_entry mon_idx[], int monster_count, int mon_cur)
{
    int i;

    if (!panel || !mon_idx || monster_count <= 0)
        return;

    for (i = 0; i < monster_count; i++)
    {
        int r_idx = mon_idx[i].r_idx;
        monster_race* r_ptr = &r_info[r_idx];
        monster_lore* l_ptr = &l_list[r_idx];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte meta_attr = TERM_SLATE;

        monster_desc_race(label, sizeof(label), r_idx);
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            SDL_strlcpy(meta, (r_ptr->max_num == 0) ? "dead" : "alive",
                sizeof(meta));
            meta_attr = (r_ptr->max_num == 0) ? TERM_L_RED : TERM_L_GREEN;
        }
        else
        {
            strnfmt(meta, sizeof(meta), "%d", l_ptr->pkills);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE, meta_attr,
                r_ptr->x_attr, r_ptr->x_char, true, i == mon_cur, "",
                label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_curse_rows(app_ui_panel* panel,
    int curse_idx[], int curse_cnt, int curse_cur)
{
    int i;

    if (!panel || !curse_idx || curse_cnt <= 0)
        return;

    for (i = 0; i < curse_cnt; i++)
    {
        char meta[APP_UI_META_MAX];
        cptr blessing = knowledge_blessing_display_name(curse_idx[i]);

        meta[0] = '\0';
        if (blessing && blessing[0]
            && strcmp(blessing, knowledge_curse_display_name(curse_idx[i])) != 0)
        {
            SDL_strlcpy(meta, blessing, sizeof(meta));
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_L_RED,
                meta[0] ? TERM_L_GREEN : TERM_SLATE, 0, '\0', true,
                i == curse_cur, "", knowledge_curse_display_name(curse_idx[i]),
                meta))
        {
            break;
        }
    }
}

static void knowledge_scene_add_curse_detail(app_ui_panel* panel, int curse_id)
{
    curse_type* curse;
    cptr blessing;
    cptr power;
    char buf[APP_UI_TEXT_MAX];

    if (!panel || curse_id < 0)
        return;

    curse = &cu_info[curse_id];
    blessing = knowledge_blessing_display_name(curse_id);
    power = cu_text + curse->power;

    app_ui_panel_set_detail_title(panel, TERM_L_RED,
        knowledge_curse_display_name(curse_id));
    if (blessing && blessing[0]
        && strcmp(blessing, knowledge_curse_display_name(curse_id)) != 0)
    {
        strnfmt(buf, sizeof(buf), "Blessing: %s", blessing);
        (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, buf);
    }

    strnfmt(buf, sizeof(buf), "Effect: %s",
        (power && power[0]) ? power : "[no additional effect listed]");
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
}

static bool knowledge_build_artefact_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    int artefact_idx[], int artefact_cnt, int artefact_top, int artefact_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (artefact_cnt > 0)
    {
        strnfmt(status, sizeof(status), "%d artefact%s in %s.", artefact_cnt,
            (artefact_cnt == 1) ? "" : "s", object_group_text[grp_idx[grp_cur]]);
    }
    else
    {
        SDL_strlcpy(status, "No known artefacts yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Artefacts");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Artefact");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, object_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_artefact_rows(panel, artefact_idx, artefact_cnt,
        artefact_cur);
    app_ui_panel_set_row_offset(panel, (s16b)artefact_top);
    knowledge_scene_add_footer_actions(panel, true, artefact_cnt > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_object_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    object_list_entry object_idx[], int object_cnt, int object_top,
    int object_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (object_cnt > 0)
    {
        object_list_entry* obj = &object_idx[object_cur];

        if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
        {
            strnfmt(status, sizeof(status),
                "%d object%s in %s. Recall available.", object_cnt,
                (object_cnt == 1) ? "" : "s", object_group_text[grp_idx[grp_cur]]);
        }
        else
        {
            strnfmt(status, sizeof(status),
                "%d object%s in %s. Recall works for identified base items.",
                object_cnt, (object_cnt == 1) ? "" : "s",
                object_group_text[grp_idx[grp_cur]]);
        }
    }
    else
    {
        SDL_strlcpy(status, "No known objects yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Objects");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Object");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, object_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_object_rows(panel, object_idx, object_cnt,
        object_cur);
    app_ui_panel_set_row_offset(panel, (s16b)object_top);
    knowledge_scene_add_footer_actions(panel, true,
        (object_cnt > 0) && (object_idx[object_cur].type == OBJ_NORMAL)
        && k_info[object_idx[object_cur].idx].aware);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_monster_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    monster_list_entry mon_idx[], int monster_count, int mon_top, int mon_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (monster_count > 0)
        knowledge_monster_summary(status, sizeof(status), grp_idx[grp_cur]);
    else
        SDL_strlcpy(status, "No known monsters in this group yet.",
            sizeof(status));

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Monsters");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Monster");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, monster_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_monster_rows(panel, mon_idx, monster_count, mon_cur);
    app_ui_panel_set_row_offset(panel, (s16b)mon_top);
    knowledge_scene_add_footer_actions(panel, true, monster_count > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_curse_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int curse_idx[], int curse_cnt, int curse_top,
    int curse_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (curse_cnt > 0)
    {
        curse_type* c = &cu_info[curse_idx[curse_cur]];
        cptr power = cu_text + c->power;

        strnfmt(status, sizeof(status), "Effect: %s",
            (power && power[0]) ? power : "[no additional effect listed]");
    }
    else
    {
        SDL_strlcpy(status, "No known curses yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Curses");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Known curses");
    knowledge_scene_append_curse_rows(panel, curse_idx, curse_cnt, curse_cur);
    if (curse_cnt > 0)
        knowledge_scene_add_curse_detail(panel, curse_idx[curse_cur]);
    app_ui_panel_set_row_offset(panel, (s16b)curse_top);
    knowledge_scene_add_footer_actions(panel, false, curse_cnt > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

void do_cmd_knowledge_browser_page(int page)
{
    ui_information_scene_scope info_scope;
    int i;
    int artefact_grp_idx[100];
    int object_grp_idx[100];
    int monster_grp_idx[100];
    int* artefact_idx = NULL;
    object_list_entry* object_idx = NULL;
    monster_list_entry* mon_idx = NULL;
    int* curse_idx = NULL;
    int artefact_grp_cnt = 0;
    int object_grp_cnt = 0;
    int monster_grp_cnt = 0;
    int curse_cnt = 0;
    int artefact_old = -1;
    int object_old = -1;
    int monster_old = -1;
    knowledge_browser_state state = { 0 };
    bool done = false;

    page = knowledge_normalize_page(page);
    knowledge_set_last_page(page);

    FILE_TYPE(FILE_TYPE_TEXT);

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge browser",
            "Known lore browser unavailable."))
    {
        return;
    }

    artefact_idx = mem_alloc_array(z_info->art_max, int);
    object_idx = mem_alloc_array(z_info->k_max + z_info->e_max + 1,
        object_list_entry);
    mon_idx = mem_alloc_array(z_info->r_max, monster_list_entry);
    curse_idx = mem_alloc_array(z_info->cu_max, int);

    for (i = 0; object_group_text[i] != NULL; i++)
    {
        if (collect_artefacts(i, artefact_idx))
            artefact_grp_idx[artefact_grp_cnt++] = i;
        if (collect_objects(i, NULL))
            object_grp_idx[object_grp_cnt++] = i;
    }

    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        if ((monster_group_char[i] == (char*)-1L)
            || collect_monsters(i, mon_idx, 0x01))
        {
            monster_grp_idx[monster_grp_cnt++] = i;
        }
    }

    curse_cnt = knowledge_collect_curses(curse_idx);

    if (p_ptr && p_ptr->playing)
        platform_music_play_menu_theme();

    while (!done)
    {
        int ch;

        switch (page)
        {
        case KNOWLEDGE_PAGE_ARTEFACTS:
        {
            int artefact_cnt = 0;
            int selected_artefact = -1;

            if (artefact_grp_cnt > 0)
            {
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            }
            knowledge_clamp_group_state(&state.column[page],
                &state.group_cur[page], &state.group_top[page],
                artefact_grp_cnt, &state.entry_cur[page], &state.entry_top[page],
                artefact_cnt, KNOWLEDGE_BROWSER_ROWS);
            if (artefact_grp_cnt > 0)
            {
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            }

            if (artefact_cnt > 0)
                selected_artefact = artefact_idx[state.entry_cur[page]];

            if (selected_artefact != artefact_old)
            {
                handle_stuff();
                artefact_old = selected_artefact;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_artefact_browser_scene(&scene, page,
                            state.tabs_focus, artefact_grp_idx,
                            artefact_grp_cnt, state.group_cur[page],
                            state.group_top[page], artefact_idx, artefact_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge artefact browser",
                        "Artefact knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                    &state.tabs_focus,
                    (artefact_grp_cnt <= 0) || ((state.column[page] == 0)
                        ? (state.group_cur[page] == 0)
                        : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (artefact_cnt > 0)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    desc_art_fake(artefact_idx[state.entry_cur[page]]);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                knowledge_browser_cursor_with_rows((char)ch,
                    &state.column[page], &state.group_cur[page],
                    artefact_grp_cnt, &state.entry_cur[page], artefact_cnt,
                    KNOWLEDGE_BROWSER_ROWS);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_OBJECTS:
        {
            int object_cnt = 0;
            int tracked_kind = 0;

            if (object_grp_cnt > 0)
            {
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            }
            knowledge_clamp_group_state(&state.column[page],
                &state.group_cur[page], &state.group_top[page],
                object_grp_cnt, &state.entry_cur[page], &state.entry_top[page],
                object_cnt, KNOWLEDGE_BROWSER_ROWS);
            if (object_grp_cnt > 0)
            {
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            }

            if ((object_cnt > 0)
                && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL))
            {
                tracked_kind = object_idx[state.entry_cur[page]].idx;
            }

            if (tracked_kind != object_old)
            {
                object_kind_track(tracked_kind);
                handle_stuff();
                object_old = tracked_kind;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_object_browser_scene(&scene, page,
                            state.tabs_focus, object_grp_idx, object_grp_cnt,
                            state.group_cur[page], state.group_top[page],
                            object_idx, object_cnt, state.entry_top[page],
                            state.entry_cur[page]),
                        &scene, "knowledge object browser",
                        "Object knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                    &state.tabs_focus,
                    (object_grp_cnt <= 0) || ((state.column[page] == 0)
                        ? (state.group_cur[page] == 0)
                        : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if ((object_cnt > 0)
                    && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL)
                    && k_info[object_idx[state.entry_cur[page]].idx].aware)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    knowledge_desc_obj_fake(object_idx[state.entry_cur[page]].idx);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                knowledge_browser_cursor_with_rows((char)ch,
                    &state.column[page], &state.group_cur[page],
                    object_grp_cnt, &state.entry_cur[page], object_cnt,
                    KNOWLEDGE_BROWSER_ROWS);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_MONSTERS:
        {
            int monster_cnt = 0;
            int selected_r_idx = 0;

            if (monster_grp_cnt > 0)
            {
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            }
            knowledge_clamp_group_state(&state.column[page],
                &state.group_cur[page], &state.group_top[page],
                monster_grp_cnt, &state.entry_cur[page], &state.entry_top[page],
                monster_cnt, KNOWLEDGE_BROWSER_ROWS);
            if (monster_grp_cnt > 0)
            {
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            }

            if (monster_cnt > 0)
                selected_r_idx = mon_idx[state.entry_cur[page]].r_idx;

            if (selected_r_idx != monster_old)
            {
                monster_race_track(selected_r_idx);
                handle_stuff();
                monster_old = selected_r_idx;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_monster_browser_scene(&scene, page,
                            state.tabs_focus, monster_grp_idx,
                            monster_grp_cnt, state.group_cur[page],
                            state.group_top[page], mon_idx, monster_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge monster browser",
                        "Monster knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                    &state.tabs_focus,
                    (monster_grp_cnt <= 0) || ((state.column[page] == 0)
                        ? (state.group_cur[page] == 0)
                        : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (monster_cnt > 0)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    if (!ui_information_scene_show_monster_recall(
                            mon_idx[state.entry_cur[page]].r_idx, NULL, NULL,
                            false, NULL))
                    {
                        bell("Monster recall screen unavailable.");
                    }
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                knowledge_browser_cursor_with_rows((char)ch,
                    &state.column[page], &state.group_cur[page],
                    monster_grp_cnt, &state.entry_cur[page], monster_cnt,
                    KNOWLEDGE_BROWSER_ROWS);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_CURSES:
        default:
        {
            knowledge_clamp_list_state(&state.entry_cur[page],
                &state.entry_top[page], curse_cnt, KNOWLEDGE_BROWSER_ROWS);
            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_curse_browser_scene(&scene, page,
                            state.tabs_focus, curse_idx, curse_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge curse browser",
                        "Curse knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                    &state.tabs_focus,
                    (curse_cnt <= 0) || (state.entry_cur[page] == 0)))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (curse_cnt > 0)
                {
                    if (!knowledge_show_curse_detail_ui(
                            curse_idx[state.entry_cur[page]]))
                    {
                        bell("Curse detail unavailable.");
                        msg_print("Curse detail unavailable.");
                    }
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
            {
                int d = target_dir(ch);

                if (curse_cnt <= 0)
                {
                    state.entry_cur[page] = 0;
                    break;
                }

                if (!d)
                    break;

                if (ddx[d] && ddy[d])
                    state.entry_cur[page] += ddy[d] * KNOWLEDGE_BROWSER_ROWS;
                else if (ddy[d])
                    state.entry_cur[page] += ddy[d];

                if (state.entry_cur[page] < 0)
                    state.entry_cur[page] = 0;
                if (state.entry_cur[page] >= curse_cnt)
                    state.entry_cur[page] = curse_cnt - 1;
                break;
            }
            }
            break;
        }
        }
    }

cleanup:
    mem_free_null(curse_idx);
    mem_free_null(mon_idx);
    mem_free_null(object_idx);
    mem_free_null(artefact_idx);

    if (info_scope.active)
        ui_information_scene_leave(&info_scope);
    if (p_ptr && p_ptr->playing)
        platform_music_stop_main();
}

/*
 * Display known objects.
 */
void do_cmd_knowledge_objects(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_OBJECTS);
}
