/* File: object-info-lore.c */
/*
 * Copyright (c) 2002 Andrew Sidwell, Robert Ruehlmann
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
#include "log/log.h"
#include "object/object-info-internal.h"

typedef struct object_lore_profile
{
    cptr keywords[12];
    int keyword_count;
} object_lore_profile;

typedef enum object_lore_alignment
{
    OBJECT_LORE_ALIGNMENT_NEUTRAL = 0,
    OBJECT_LORE_ALIGNMENT_NOBLE = 1,
    OBJECT_LORE_ALIGNMENT_EVIL = 2
} object_lore_alignment;

static void object_lore_add_keyword(object_lore_profile* profile, cptr keyword)
{
    if (!profile || !keyword || !keyword[0])
        return;

    for (int i = 0; i < profile->keyword_count; i++)
    {
        if (streq(profile->keywords[i], keyword))
            return;
    }

    if (profile->keyword_count < (int)N_ELEMENTS(profile->keywords))
        profile->keywords[profile->keyword_count++] = keyword;
}

static void object_lore_trim_copy(const char* start, size_t len, char* out,
    size_t out_sz)
{
    if (!out || out_sz == 0)
        return;

    while (len > 0 && isspace((unsigned char)*start))
    {
        start++;
        len--;
    }

    while (len > 0 && isspace((unsigned char)start[len - 1]))
        len--;

    if (len >= out_sz)
        len = out_sz - 1;

    if (len > 0)
        memcpy(out, start, len);
    out[len] = '\0';
}

static int object_lore_keyword_rank(const object_lore_profile* profile,
    cptr keyword)
{
    if (!profile || !keyword || !keyword[0])
        return -1;

    for (int i = 0; i < profile->keyword_count; i++)
    {
        if (streq(profile->keywords[i], keyword))
            return i;
    }

    return -1;
}

static bool object_lore_text_is_structured(cptr raw)
{
    while (raw && *raw && isspace((unsigned char)*raw))
        raw++;

    return raw && (*raw == '[');
}

static object_lore_alignment object_lore_actual_alignment(
    const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr || !o_ptr->k_idx)
        return OBJECT_LORE_ALIGNMENT_NEUTRAL;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

    if (has_noble && !has_evil)
        return OBJECT_LORE_ALIGNMENT_NOBLE;
    if (has_evil && !has_noble)
        return OBJECT_LORE_ALIGNMENT_EVIL;

    return OBJECT_LORE_ALIGNMENT_NEUTRAL;
}

static void object_lore_add_alignment_keyword(object_lore_profile* profile,
    object_lore_alignment alignment)
{
    switch (alignment)
    {
    case OBJECT_LORE_ALIGNMENT_NOBLE:
        object_lore_add_keyword(profile, "noble");
        object_lore_add_keyword(profile, "good");
        break;
    case OBJECT_LORE_ALIGNMENT_EVIL:
        object_lore_add_keyword(profile, "evil");
        break;
    case OBJECT_LORE_ALIGNMENT_NEUTRAL:
    default:
        object_lore_add_keyword(profile, "neutral");
        break;
    }
}

static void object_lore_profile_for_object(const object_type* o_ptr,
    object_lore_profile* profile)
{
    if (!profile)
        return;

    memset(profile, 0, sizeof(*profile));
    if (!o_ptr || !o_ptr->k_idx)
        return;

    switch (o_ptr->tval)
    {
    case TV_AMULET:
        object_lore_add_keyword(profile, "amulet");
        break;
    case TV_RING:
        object_lore_add_keyword(profile, "ring");
        break;
    case TV_ARROW:
        object_lore_add_keyword(profile, "arrow");
        break;
    case TV_BOW:
        if (o_ptr->sval == SV_SHORT_BOW)
            object_lore_add_keyword(profile, "shortbow");
        else if (o_ptr->sval == SV_LONG_BOW)
            object_lore_add_keyword(profile, "longbow");
        else if (o_ptr->sval == 31)
            object_lore_add_keyword(profile, "dragonhorn_bow");
        object_lore_add_keyword(profile, "bow");
        break;
    case TV_LIGHT:
        if (o_ptr->sval == SV_LIGHT_LANTERN)
            object_lore_add_keyword(profile, "lantern");
        else if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            object_lore_add_keyword(profile, "lesser_jewel");
        else if (o_ptr->sval == SV_LIGHT_FEANORIAN)
            object_lore_add_keyword(profile, "feanorian_lamp");
        object_lore_add_keyword(profile, "light");
        break;
    case TV_SOFT_ARMOR:
        if (o_ptr->sval == SV_ROBE)
            object_lore_add_keyword(profile, "robe");
        else if (o_ptr->sval == SV_LEATHER_ARMOR)
            object_lore_add_keyword(profile, "leather_armour");
        else if (o_ptr->sval == SV_STUDDED_LEATHER)
            object_lore_add_keyword(profile, "studded_armour");
        object_lore_add_keyword(profile, "soft_armour");
        break;
    case TV_MAIL:
        if (o_ptr->sval == SV_MAIL_CORSLET
            || o_ptr->sval == SV_DENTED_MAIL_CORSLET)
        {
            object_lore_add_keyword(profile, "mail_corslet");
        }
        else if (o_ptr->sval == SV_LONG_CORSLET)
            object_lore_add_keyword(profile, "hauberk");
        else if (o_ptr->sval == SV_MITHRIL_CORSLET)
            object_lore_add_keyword(profile, "mithril_mail");
        object_lore_add_keyword(profile, "mail");
        break;
    case TV_SHIELD:
        object_lore_add_keyword(profile, "shield");
        break;
    case TV_CLOAK:
        object_lore_add_keyword(profile, "cloak");
        break;
    case TV_HELM:
    case TV_CROWN:
        if (o_ptr->tval == TV_HELM && o_ptr->sval == SV_GREAT_HELM)
            object_lore_add_keyword(profile, "great_helm");
        else if (o_ptr->tval == TV_HELM && o_ptr->sval == SV_MITHRIL_HELM)
            object_lore_add_keyword(profile, "mithril_helm");
        object_lore_add_keyword(profile, "helm");
        break;
    case TV_BOOTS:
        if (o_ptr->sval == SV_PAIR_OF_STEEL_GREAVES
            || o_ptr->sval == SV_PAIR_OF_MITHRIL_GREAVES
            || o_ptr->sval == SV_PAIR_OF_DENTED_GREAVES)
        {
            object_lore_add_keyword(profile, "greaves");
        }
        else
        {
            object_lore_add_keyword(profile, "boots");
        }
        break;
    case TV_GLOVES:
        if (o_ptr->sval == SV_SET_OF_GAUNTLETS
            || o_ptr->sval == SV_SET_OF_CRACKED_GAUNTLETS)
        {
            object_lore_add_keyword(profile, "gauntlets");
        }
        object_lore_add_keyword(profile, "gloves");
        break;
    case TV_HAFTED:
        if (o_ptr->sval == SV_QUARTERSTAFF)
            object_lore_add_keyword(profile, "quarterstaff");
        else if (o_ptr->sval == SV_WAR_HAMMER)
            object_lore_add_keyword(profile, "war_hammer");
        break;
    case TV_DIGGING:
        if (o_ptr->sval == SV_MATTOCK)
            object_lore_add_keyword(profile, "mattock");
        break;
    case TV_POLEARM:
        if (o_ptr->sval == SV_SPEAR)
            object_lore_add_keyword(profile, "spear");
        else if (o_ptr->sval == SV_GREAT_SPEAR)
            object_lore_add_keyword(profile, "great_spear");
        else if (o_ptr->sval == SV_GLAIVE)
            object_lore_add_keyword(profile, "glaive");
        else if (o_ptr->sval == SV_HAND_AXE)
            object_lore_add_keyword(profile, "hand_axe");
        else if (o_ptr->sval == SV_BATTLE_AXE)
            object_lore_add_keyword(profile, "battle_axe");
        else if (o_ptr->sval == SV_GREAT_AXE)
            object_lore_add_keyword(profile, "great_axe");
        object_lore_add_keyword(profile, "polearm");
        object_lore_add_keyword(profile, "axe");
        break;
    case TV_SWORD:
        if (o_ptr->sval == SV_DAGGER)
            object_lore_add_keyword(profile, "dagger");
        else if (o_ptr->sval == SV_CURVED_SWORD)
            object_lore_add_keyword(profile, "curved_sword");
        else if (o_ptr->sval == SV_SHORT_SWORD)
            object_lore_add_keyword(profile, "short_sword");
        else if (o_ptr->sval == SV_GREAT_SWORD)
            object_lore_add_keyword(profile, "great_sword");
        object_lore_add_keyword(profile, "sword");
        break;
    default:
        break;
    }
}

static bool object_lore_select_segment(cptr raw,
    const object_lore_profile* profile, char* out, size_t out_sz)
{
    const char* p;
    bool found = false;
    bool have_default = false;
    int best_rank = 9999;
    char tag_buf[64];
    char text_buf[2048];
    char best_buf[2048];
    char default_buf[2048];

    if (!raw || !profile || !out || out_sz == 0)
        return false;

    out[0] = '\0';
    best_buf[0] = '\0';
    default_buf[0] = '\0';
    p = raw;

    while (*p)
    {
        const char* tag_start;
        const char* tag_end;
        const char* text_start;
        const char* next;
        const char* text_end;
        int rank;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p != '[')
            break;

        tag_start = p + 1;
        tag_end = strchr(tag_start, ']');
        if (!tag_end)
            break;

        object_lore_trim_copy(tag_start, (size_t)(tag_end - tag_start),
            tag_buf, sizeof(tag_buf));
        text_start = tag_end + 1;
        next = strstr(text_start, "||");
        text_end = next ? next : (text_start + strlen(text_start));
        object_lore_trim_copy(text_start, (size_t)(text_end - text_start),
            text_buf, sizeof(text_buf));

        if (streq(tag_buf, "default"))
        {
            SDL_strlcpy(default_buf, text_buf, sizeof(default_buf));
            have_default = true;
        }
        else
        {
            rank = object_lore_keyword_rank(profile, tag_buf);
            if (rank >= 0 && rank < best_rank)
            {
                SDL_strlcpy(best_buf, text_buf, sizeof(best_buf));
                best_rank = rank;
                found = true;
            }
        }

        if (!next)
            break;
        p = next + 2;
    }

    if (found)
    {
        SDL_strlcpy(out, best_buf, out_sz);
        return true;
    }

    if (have_default)
    {
        SDL_strlcpy(out, default_buf, out_sz);
        return true;
    }

    return false;
}

static cptr object_lore_select_base_text(const object_type* o_ptr, char* out,
    size_t out_sz)
{
    cptr raw;
    object_lore_profile profile;

    if (!o_ptr || !o_ptr->k_idx || !k_info[o_ptr->k_idx].text)
        return NULL;

    raw = k_text + k_info[o_ptr->k_idx].text;
    if (!object_lore_text_is_structured(raw))
        return raw;

    object_lore_profile_for_object(o_ptr, &profile);
    object_lore_add_alignment_keyword(&profile,
        object_lore_actual_alignment(o_ptr));
    if (!object_lore_select_segment(raw, &profile, out, out_sz))
        return NULL;

    return out;
}

static bool screen_out_legacy_ego_lore(cptr raw_text)
{
    if (!raw_text || !raw_text[0])
        return false;

    p_text_out("\n\n   ");
    p_text_out(raw_text);
    return true;
}

static bool screen_out_ego_lore(const object_type* o_ptr)
{
    byte ego_pfx;
    byte ego_sfx;
    cptr prefix_text = NULL;
    cptr suffix_text = NULL;
    bool prefix_structured = false;
    bool suffix_structured = false;
    bool has_description = false;
    char prefix_buf[1024];
    char suffix_buf[1024];
    object_lore_profile profile;

    if (!o_ptr || !o_ptr->k_idx || !object_known_p(o_ptr))
        return false;

    ego_pfx = object_ego_prefix(o_ptr);
    ego_sfx = object_ego_suffix(o_ptr);

    if (ego_pfx && e_info[ego_pfx].text)
        prefix_text = e_text + e_info[ego_pfx].text;
    if (ego_sfx && (ego_sfx != ego_pfx) && e_info[ego_sfx].text)
        suffix_text = e_text + e_info[ego_sfx].text;
    if (!prefix_text && !suffix_text)
        return false;

    object_lore_profile_for_object(o_ptr, &profile);
    prefix_buf[0] = '\0';
    suffix_buf[0] = '\0';

    if (prefix_text && object_lore_text_is_structured(prefix_text))
    {
        prefix_structured = object_lore_select_segment(prefix_text, &profile,
            prefix_buf, sizeof(prefix_buf));
    }
    if (suffix_text && object_lore_text_is_structured(suffix_text))
    {
        suffix_structured = object_lore_select_segment(suffix_text, &profile,
            suffix_buf, sizeof(suffix_buf));
    }

    if (prefix_structured || suffix_structured)
    {
        p_text_out("\n\n   ");
        if (prefix_buf[0])
        {
            p_text_out(prefix_buf);
            if (suffix_buf[0])
                p_text_out(" ");
        }
        if (suffix_buf[0])
            p_text_out(suffix_buf);
        has_description = true;
    }

    if (prefix_text && !prefix_structured)
        has_description |= screen_out_legacy_ego_lore(prefix_text);
    if (suffix_text && !suffix_structured)
        has_description |= screen_out_legacy_ego_lore(suffix_text);

    return has_description;
}

/*
 * Header for additional information when printing to screen.
 */
bool object_info_screen_out_head(const object_type* o_ptr)
{
    char* o_name;
    char base_desc_buf[2048];
    cptr base_desc = NULL;
    int name_size = OBJECT_INFO_NAME_BUF_COLS;

    bool has_description = false;

    log_trace("screen_out_head: Starting, wrap_cols=%d",
        OBJECT_INFO_CAPTURE_WRAP_COLS);

    /* Allocate memory sized for semantic capture, not the legacy term width. */
    o_name = mem_alloc_array(name_size, char);

    /* Description */
    object_desc(o_name, name_size, o_ptr, true, 3);

    log_trace("screen_out_head: About to print object name at current position");

    /* Use same color logic as inventory/equipment displays */
    byte base_color;

    /* Determine base color from item type */
    if (weapon_glows(o_ptr))
    {
        base_color = TERM_L_BLUE;
    }
    else
    {
        base_color = object_default_text_color(o_ptr);
    }

    /* Apply artifact/shade coloring using the same function as inventory */
    byte name_color = object_display_color(o_ptr, base_color);

    /* Print, in colour */
    text_out_c(name_color, format("%^s", o_name));

    /* Show weight information */
    {
        char weight_buf[64];
        int total_weight = o_ptr->weight * o_ptr->number;
        int each_weight = o_ptr->weight;

        if (o_ptr->number > 1)
        {
            strnfmt(weight_buf, sizeof(weight_buf),
                " %3d.%1d lb (%3d.%1d lb each)", total_weight / 10,
                total_weight % 10, each_weight / 10, each_weight % 10);
        }
        else
        {
            strnfmt(weight_buf, sizeof(weight_buf), " %3d.%1d lb",
                total_weight / 10, total_weight % 10);
        }
        text_out_c(TERM_L_UMBER, weight_buf);
    }

    /* Debug: compact smithing difficulty + weight rarity */
    if (op_ptr->opt[OPT_show_smithing_difficulty] && object_known_p(o_ptr)
        && object_uses_smithing_difficulty(o_ptr))
    {
        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
        int sd = object_smithing_difficulty(o_ptr);
        int wr = object_weight_rarity(o_ptr, depth);
        text_out_c(TERM_SLATE, format(" {%d,%d}", sd, wr));
    }

    /* Free up the memory */
    mem_free_null(o_name);

    /* Display the known artefact description */
    if (!adult_rand_artefacts && o_ptr->name1 && object_known_p(o_ptr)
        && a_info[o_ptr->name1].text)
    {
        p_text_out("\n\n   ");
        p_text_out(a_text + a_info[o_ptr->name1].text);
        has_description = true;
    }
    /* Display the known object description */
    else if (object_aware_p(o_ptr) || object_known_p(o_ptr))
    {
        base_desc = object_lore_select_base_text(o_ptr, base_desc_buf,
            sizeof(base_desc_buf));
        if (base_desc && base_desc[0])
        {
            p_text_out("\n\n   ");
            p_text_out(base_desc);
            has_description = true;
        }

        if (screen_out_ego_lore(o_ptr))
            has_description = true;
    }

    return has_description;
}
