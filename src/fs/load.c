/* File: load.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "blitz.h"
#include "fs/file.h"
#include "log/log.h"
#include "platform-config.h"
#include "platform-frame.h"
#include "runtime/runtime-cli.h"
#include "player/killer.h"
#include "support/reliability-checks.h"
#include "score/score_guid.h"
#include <string.h> /* memset, strstr */
#include <stdio.h>  /* FILE, getc, ftell, fseek, ferror */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  /* O_RDONLY */
#include <errno.h>
#include <stdbool.h>

/* #include "init.h"  not required directly here after refactor */
#include "metarun.h"
#include "fs/load-internal.h"

/*
 * This file loads savefiles from Sil.
 *
 * We attempt to prevent corrupt savefiles from inducing memory errors.
 *
 * Note that this file should not use the random number generator, the
 * object flavors, the visual attr/char mappings, or anything else which
 * is initialized *after* or *during* the "load character" function.
 *
 * This file assumes that the monster/object records are initialized
 * to zero, and the race/kind tables have been loaded correctly.  The
 * order of object stacks is currently not saved in the savefiles, but
 * the "next" pointers are saved, so all necessary knowledge is present.
 *
 * We should implement simple "savefile extenders" using some form of
 * "sized" chunks of bytes, with a {size,type,data} format, so everyone
 * can know the size, interested people can know the type, and the actual
 * data is available to the parsing routines that acknowledge the type.
 *
 * Consider changing the "globe of invulnerability" code so that it
 * takes some form of "maximum damage to protect from" in addition to
 * the existing "number of turns to protect for", and where each hit
 * by a monster will reduce the shield by that amount.  XXX XXX XXX
 */

/*
 * Local "savefile" pointer
 */
static ang_file* fff;

/*
 * Hack -- old "encryption" byte
 */
static byte xor_byte;

/*
 * Hack -- simple "checksum" on the actual values
 */
static u32b v_check = 0L;

/*
 * Hack -- simple "checksum" on the encoded bytes
 */
static u32b x_check = 0L;

/* Debug: count bytes consumed from save stream (post-decode) */
u32b load_byte_offset = 0;
bool load_read_failed = false;
void note(cptr msg);

errr load_expect_stream_ok(cptr context)
{
    if (!load_read_failed)
        return 0;

    note(format("Savefile truncated while reading %s.", context));
    log_error("load: stream failure while reading %s at offset %u", context,
        (unsigned)load_byte_offset);
    return (-1);
}

/* Track feature availability for the currently loaded savefile. */
bool savefile_has_runtime_overrides = false;
bool savefile_has_monster_shatter = false;
bool savefile_has_song_duels = false;
bool savefile_has_ability_timeline = false;
bool savefile_has_varda_quest = false;
bool savefile_has_artifact_seen = false;
bool savefile_has_skeleton_notes = false;
bool savefile_has_skeleton_hint_mask = false;
bool savefile_has_skeleton_hint_mask32 = false;
bool savefile_has_partition_meta = false;
bool savefile_has_partition_meta_types = false;
bool savefile_has_cave_info_hi = false;
bool savefile_has_hint_messages = false;
bool savefile_has_hint_message_meta = false;
bool savefile_has_thrall_quest = false;
bool savefile_has_thrall_quest_requested = false;
bool savefile_has_randart_flags4 = false;
bool savefile_has_item_bonuses = false;
bool savefile_has_randart_bonuses = false;

/* Version comparison helpers: update these when bumping savefile semantics. */
static int savefile_version_compare(byte major, byte minor, byte patch, byte extra)
{
    if (sf_major != major)
        return (sf_major > major) ? 1 : -1;
    if (sf_minor != minor)
        return (sf_minor > minor) ? 1 : -1;
    if (sf_patch != patch)
        return (sf_patch > patch) ? 1 : -1;
    if (sf_extra != extra)
        return (sf_extra > extra) ? 1 : -1;
    return 0;
}

bool savefile_version_at_least(byte major, byte minor, byte patch, byte extra)
{
    return savefile_version_compare(major, minor, patch, extra) >= 0;
}

static bool savefile_version_at_most(byte major, byte minor, byte patch, byte extra)
{
    return savefile_version_compare(major, minor, patch, extra) <= 0;
}

static bool savefile_version_supported(void)
{
    /* Reject savefiles older than our documented floor. */
    if (!savefile_version_at_least(OLD_VERSION_MAJOR, OLD_VERSION_MINOR, OLD_VERSION_PATCH, 0))
        return false;

    /* Reject savefiles from the future. */
    if (!savefile_version_at_most(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_EXTRA))
        return false;

    /* Enforce the minimum extra value for the current release series. */
    if (sf_major == VERSION_MAJOR && sf_minor == VERSION_MINOR && sf_patch == VERSION_PATCH)
    {
#if MIN_VERSION_EXTRA > 0
        if (sf_extra < MIN_VERSION_EXTRA)
            return false;
#endif
    }

    return true;
}

static void load_init_version_features(void)
{
    savefile_has_runtime_overrides = savefile_version_at_least(0, 9, 0, 3);
    savefile_has_monster_shatter = savefile_version_at_least(0, 9, 0, 4);
    savefile_has_song_duels = savefile_version_at_least(0, 9, 0, 5);
    savefile_has_ability_timeline = savefile_version_at_least(0, 9, 1, 1);
    savefile_has_varda_quest = savefile_version_at_least(0, 9, 1, 3);
    savefile_has_artifact_seen = savefile_version_at_least(0, 9, 1, 4);
    savefile_has_skeleton_notes = savefile_version_at_least(0, 9, 1, 5);
    savefile_has_skeleton_hint_mask = savefile_version_at_least(0, 9, 1, 6);
    savefile_has_skeleton_hint_mask32 = savefile_version_at_least(0, 9, 1, 13);
    savefile_has_partition_meta = savefile_version_at_least(0, 9, 1, 7);
    savefile_has_partition_meta_types = savefile_version_at_least(0, 9, 1, 9);
    savefile_has_cave_info_hi = savefile_version_at_least(0, 9, 1, 8);
    savefile_has_hint_messages = savefile_version_at_least(0, 9, 1, 10);
    savefile_has_hint_message_meta = savefile_version_at_least(0, 9, 5, 7);
    savefile_has_thrall_quest = savefile_version_at_least(0, 9, 1, 11);
    savefile_has_thrall_quest_requested = savefile_version_at_least(0, 9, 1, 12);
    savefile_has_randart_flags4 = savefile_version_at_least(0, 9, 5, 1);
    savefile_has_item_bonuses = savefile_version_at_least(0, 9, 5, 2);
    savefile_has_randart_bonuses = savefile_version_at_least(0, 9, 5, 3);
}
/* For backward-compatible reading: if the door-choices block is absent,
 * we prefetch the next u16 (objects count) here after probing. */
u16b objects_count_prefetch = 0xFFFF;
/* Back-compat: some intermediate builds wrote door-choices block before
 * cave_color (i.e., between cave_feat and cave_color). We'll probe there; if
 * no magic, treat the two bytes as the first (count,value) pair for the
 * cave_color RLE, staging them here. */
bool color_rle_pair_prefetched = false;
byte color_rle_count_prefetch = 0;
byte color_rle_value_prefetch = 0;

u16b new_artefacts;
u16b art_norm_count;

/*
 * Hack -- Show information on the screen, one line at a time.
 *
 * Avoid the top two lines, to avoid interference with "msg_print()".
 */
void note(cptr msg)
{
    message_topline_override(TERM_WHITE, msg);

    /* Flush it */
    platform_frame_present();
}

/*
 * This counts the artefacts generated so far
 */
static int artefact_count(void)
{
    int i, count = 0;

    // note that it only counts through the fixed and random artefacts, not the
    // self-made ones
    for (i = 0; i < z_info->art_rand_max; i++)
    {
        if (((&a_info[i])->cur_num > 0)
            && !((a_info[i].flags3 & (TR3_INSTA_ART))))
        {
            count++;
        }
    }

    return (count);
}

/*
 * Hack -- determine if an item is "wearable" (or a missile)
 */
static bool wearable_p(const object_type* o_ptr)
{
    /* Valid "tval" codes */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    {
        return (true);
    }
    }

    /* Nope */
    return (false);
}

/*
 * The following functions are used to load the basic building blocks
 * of savefiles.  They also maintain the "checksum" info.
 */

static byte sf_get(void)
{
    byte c, v;

    if (load_read_failed)
        return 0;

    /* Read a byte from the stream */
    if (SDL_ReadIO(fff, &c, 1) != 1)
    {
        log_error("sf_get: Failed to read byte at offset %ld", load_byte_offset);
        load_read_failed = true;
        return (0);
    }
    
    /* Decode the value */
    v = c ^ xor_byte;
    xor_byte = c;

    /* Maintain the checksum info */
    v_check += v;
    x_check += xor_byte;

    /* Track offset (decoded payload byte) */
    load_byte_offset++;
    
    /* Return the value */
    return (v);
}

void rd_byte(byte* ip) {
    if (ip)
        *ip = sf_get();
    /* load_byte_offset already incremented by sf_get() */
}

void rd_bool(bool* bp) {
    if (bp)
        *bp = sf_get() != 0;  // Any non-zero value becomes true
    /* load_byte_offset already incremented by sf_get() */
}

void rd_u16b(u16b* ip)
{
    if (!ip)
        return;

    *ip = 0;
    if (load_read_failed)
        return;

    (*ip) = sf_get();
    if (load_read_failed)
        return;
    (*ip) |= ((u16b)(sf_get()) << 8);
    /* load_byte_offset already incremented by sf_get() calls */
    log_trace("[load:%06u] rd_u16b: 0x%04X (%u)", (unsigned)(load_byte_offset - 2), (unsigned)*ip, (unsigned)*ip);
}

void rd_s16b(s16b* ip) { 
    rd_u16b((u16b*)ip);
    log_trace("[load:%06u] rd_s16b: %d", (unsigned)(load_byte_offset - 2), (int)*ip);
}

void rd_u32b(u32b* ip)
{
    if (!ip)
        return;

    *ip = 0;
    if (load_read_failed)
        return;

    (*ip) = sf_get();
    if (load_read_failed)
        return;
    (*ip) |= ((u32b)(sf_get()) << 8);
    if (load_read_failed)
        return;
    (*ip) |= ((u32b)(sf_get()) << 16);
    if (load_read_failed)
        return;
    (*ip) |= ((u32b)(sf_get()) << 24);
    /* load_byte_offset already incremented by sf_get() calls */
    log_trace("[load:%06u] rd_u32b: 0x%08X (%u)", (unsigned)(load_byte_offset - 4), (unsigned)*ip, (unsigned)*ip);
}

void rd_s32b(s32b* ip) { 
    rd_u32b((u32b*)ip);
    log_trace("[load:%06u] rd_s32b: %d", (unsigned)(load_byte_offset - 4), (int)*ip);
}

/*
 * Hack -- read a string
 */
bool rd_string(char* str, int max)
{
    int i;

    if (!str || max <= 0)
        return false;

    str[0] = '\0';

    /* Read the string */
    for (i = 0; true; i++)
    {
        byte tmp8u;

        if (load_read_failed)
            break;

        /* Read a byte */
        rd_byte(&tmp8u);
        if (load_read_failed)
            break;

        /* Collect string while legal */
        if (i < max)
            str[i] = tmp8u;

        /* End of string */
        if (!tmp8u)
            break;
    }

    /* Terminate */
    str[max - 1] = '\0';
    return !load_read_failed;
}

/*
 * Hack -- strip some bytes
 */
void strip_bytes(int n)
{
    byte tmp8u;

    /* Strip the bytes */
    while (n-- && !load_read_failed)
        rd_byte(&tmp8u);
}

/*
 * Read an object
 *
 * This function attempts to "repair" old savefiles, and to extract
 * the most up to date values for various object fields.
 */
static void convert_old_staff_of_warding(object_type* o_ptr)
{
    if (!o_ptr || o_ptr->tval != TV_STAFF || o_ptr->sval != SV_STAFF_WARDING)
        return;

    int uses = 0;
    if (o_ptr->pval > 0)
    {
        uses = o_ptr->pval / CHANNELING_CHARGE_MULTIPLIER;
        if (uses <= 0)
            uses = 1;
    }
    else if (o_ptr->number > 0)
    {
        uses = o_ptr->number;
    }

    if (uses < 0)
        uses = 0;

    o_ptr->tval = TV_GEM;
    o_ptr->sval = SV_GEM_WARDING;
    o_ptr->pval = 0;
    o_ptr->timeout = 0;

    if (uses > MAX_STACK_SIZE - 1)
        uses = MAX_STACK_SIZE - 1;
    o_ptr->number = uses;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr)
    {
        o_ptr->weight = k_ptr->weight;
    }

    if (o_ptr->number <= 0)
        o_ptr->ident |= IDENT_EMPTY;
    else
        o_ptr->ident &= ~(IDENT_EMPTY);

    memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
    memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));
}

static void object_derive_stat_skill_bonuses_from_pval(object_type* o_ptr)
{
    if (!o_ptr)
        return;

    memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
    memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));

    u32b f1, f2, f3;
    object_flags(o_ptr, &f1, &f2, &f3);

    if (f1 & TR1_STR)
        o_ptr->stat_bonus[A_STR] += o_ptr->pval;
    if (f1 & TR1_NEG_STR)
        o_ptr->stat_bonus[A_STR] -= o_ptr->pval;

    if (f1 & TR1_DEX)
        o_ptr->stat_bonus[A_DEX] += o_ptr->pval;
    if (f1 & TR1_NEG_DEX)
        o_ptr->stat_bonus[A_DEX] -= o_ptr->pval;

    if (f1 & TR1_CON)
        o_ptr->stat_bonus[A_CON] += o_ptr->pval;
    if (f1 & TR1_NEG_CON)
        o_ptr->stat_bonus[A_CON] -= o_ptr->pval;

    if (f1 & TR1_GRA)
        o_ptr->stat_bonus[A_GRA] += o_ptr->pval;
    if (f1 & TR1_NEG_GRA)
        o_ptr->stat_bonus[A_GRA] -= o_ptr->pval;

    if (f1 & TR1_MEL)
        o_ptr->skill_bonus[S_MEL] += o_ptr->pval;
    if (f1 & TR1_ARC)
        o_ptr->skill_bonus[S_ARC] += o_ptr->pval;
    if (f1 & TR1_STL)
        o_ptr->skill_bonus[S_STL] += o_ptr->pval;
    if (f1 & TR1_PER)
        o_ptr->skill_bonus[S_PER] += o_ptr->pval;
    if (f1 & TR1_WIL)
        o_ptr->skill_bonus[S_WIL] += o_ptr->pval;
    if (f1 & TR1_SMT)
        o_ptr->skill_bonus[S_SMT] += o_ptr->pval;
    if (f1 & TR1_SNG)
        o_ptr->skill_bonus[S_SNG] += o_ptr->pval;
}

void artefact_derive_stat_skill_bonuses_from_pval(artefact_type* a_ptr)
{
    if (!a_ptr)
        return;

    memset(a_ptr->stat_bonus, 0, sizeof(a_ptr->stat_bonus));
    memset(a_ptr->skill_bonus, 0, sizeof(a_ptr->skill_bonus));
    memset(a_ptr->stat_bonus_set, 0, sizeof(a_ptr->stat_bonus_set));
    memset(a_ptr->skill_bonus_set, 0, sizeof(a_ptr->skill_bonus_set));

    if (a_ptr->flags1 & TR1_STR)
        a_ptr->stat_bonus[A_STR] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_STR)
        a_ptr->stat_bonus[A_STR] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_DEX)
        a_ptr->stat_bonus[A_DEX] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_DEX)
        a_ptr->stat_bonus[A_DEX] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_CON)
        a_ptr->stat_bonus[A_CON] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_CON)
        a_ptr->stat_bonus[A_CON] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_GRA)
        a_ptr->stat_bonus[A_GRA] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_GRA)
        a_ptr->stat_bonus[A_GRA] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_MEL)
        a_ptr->skill_bonus[S_MEL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_ARC)
        a_ptr->skill_bonus[S_ARC] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_STL)
        a_ptr->skill_bonus[S_STL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_PER)
        a_ptr->skill_bonus[S_PER] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_WIL)
        a_ptr->skill_bonus[S_WIL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_SMT)
        a_ptr->skill_bonus[S_SMT] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_SNG)
        a_ptr->skill_bonus[S_SNG] += a_ptr->pval;
}

errr rd_item(object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_kind* k_ptr;

    char buf[128];

    int i;

    /* Kind */
    rd_s16b(&o_ptr->k_idx);

    /* Paranoia */
    if ((o_ptr->k_idx < 0) || (o_ptr->k_idx >= z_info->k_max))
    {
        return (-1);
    }

    /* Hallucinatory Kind */
    rd_s16b(&o_ptr->image_k_idx);

    /* Location */
    rd_byte(&o_ptr->iy);
    rd_byte(&o_ptr->ix);

    /* Type/Subtype */
    rd_byte(&o_ptr->tval);
    rd_byte(&o_ptr->sval);

    /* Special pval */
    rd_s16b(&o_ptr->pval);

    rd_byte(&o_ptr->discount);

    rd_byte(&o_ptr->number);
    rd_s16b(&o_ptr->weight);

    rd_byte(&o_ptr->name1);
    rd_byte(&o_ptr->name2);

    rd_s16b(&o_ptr->timeout);

    rd_s16b(&o_ptr->att);
    rd_byte(&o_ptr->dd);
    rd_byte(&o_ptr->ds);
    rd_s16b(&o_ptr->evn);
    rd_byte(&o_ptr->pd);
    rd_byte(&o_ptr->ps);
    rd_byte(&o_ptr->pickup);
    rd_s16b(&o_ptr->pickup_slot);

    rd_u32b(&o_ptr->ident);

    rd_byte(&o_ptr->marked);

    /* Monster holding object */
    rd_s16b(&o_ptr->held_m_idx);

    /* Special powers */
    rd_byte(&o_ptr->xtra1);

    // granted abilities
    rd_byte(&o_ptr->abilities);
    for (i = 0; i < 8; i++)
    {
        rd_byte(&o_ptr->skilltype[i]);
        rd_byte(&o_ptr->abilitynum[i]);
    }

    rd_s32b(&o_ptr->unused1);
    rd_s32b(&o_ptr->unused2);
    rd_s32b(&o_ptr->unused3);
    rd_s32b(&o_ptr->unused4);

    // bane_type for each ability slot (8 bytes)
    for (i = 0; i < 8; i++)
    {
        rd_byte(&o_ptr->bane_type[i]);
    }

    /* Per-stat/skill modifiers */
    if (savefile_has_item_bonuses)
    {
        for (i = 0; i < A_MAX; i++)
        {
            rd_s16b(&o_ptr->stat_bonus[i]);
        }
        for (i = 0; i < S_MAX; i++)
        {
            rd_s16b(&o_ptr->skill_bonus[i]);
        }
    }
    else
    {
        memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
        memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));
    }

    /* Inscription */
    rd_string(buf, sizeof(buf));
    if (load_read_failed)
        return (-1);

    /* Save the inscription */
    if (buf[0])
        o_ptr->obj_note = quark_add(buf);

    /* Obtain the "kind" template */
    k_ptr = &k_info[o_ptr->k_idx];

    /* Obtain tval/sval from k_info */
    o_ptr->tval = k_ptr->tval;
    o_ptr->sval = k_ptr->sval;

    /* Hack -- notice "broken" items */
    if (k_ptr->cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Repair non "wearable" items */
    if (!wearable_p(o_ptr))
    {
        /* Get the correct fields */
        o_ptr->att = k_ptr->att;
        o_ptr->evn = k_ptr->evn;

        /* Get the correct fields */
        o_ptr->dd = k_ptr->dd;
        o_ptr->ds = k_ptr->ds;
        o_ptr->pd = k_ptr->pd;
        o_ptr->ps = k_ptr->ps;

        /* Get the correct weight */
        o_ptr->weight = k_ptr->weight;

        /* Paranoia */
        o_ptr->name1 = 0;
        object_set_ego_suffix(o_ptr, 0);
        object_set_ego_prefix(o_ptr, 0);

        /* All done */
        return (0);
    }

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Migrate legacy visible {uncursed} to a hidden persisted marker. */
    if (o_ptr->discount == INSCRIP_UNCURSED)
    {
        o_ptr->ident |= IDENT_UNCURSED;
        o_ptr->discount = 0;
    }

    /* Preserve cleansed curse state without showing {uncursed}. */
    if ((f3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        && !(o_ptr->ident & IDENT_UNCURSED))
        o_ptr->ident |= (IDENT_CURSED);

    /* Paranoia */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr;

        /*hack - adjust if new artefact*/
        if (o_ptr->name1 >= art_norm_count)
        {
            o_ptr->name1 += new_artefacts;
        }

        /* Paranoia */
        if (o_ptr->name1 >= z_info->art_max)
        {
            return (-1);
        }

        /* Obtain the artefact info */
        a_ptr = &a_info[o_ptr->name1];

        /* Verify that artefact */
        if (a_ptr->tval + a_ptr->sval == 0)
        {
            o_ptr->name1 = 0;
        }
    }

    /*
     * Ego items
     *
     * - Prefix ego is stored in o_ptr->unused2 (see object_ego_prefix()).
     * - Suffix ego is stored in o_ptr->name2 (see object_ego_suffix()).
     *
     * For compatibility with older saves (which only had one ego in name2),
     * migrate prefix-type egos from suffix->prefix based on the ego name.
     */
    if (o_ptr->unused2 < 0 || o_ptr->unused2 > 255)
        o_ptr->unused2 = 0;

    if (object_ego_prefix(o_ptr))
    {
        byte e_idx = object_ego_prefix(o_ptr);
        ego_item_type* e_ptr;

        if (e_idx >= z_info->e_max)
        {
            object_set_ego_prefix(o_ptr, 0);
        }
        else
        {
            e_ptr = &e_info[e_idx];
            if (!e_ptr->name)
            {
                object_set_ego_prefix(o_ptr, 0);
            }
            else if (!ego_name_is_prefix(e_name + e_ptr->name))
            {
                /* Corrupted/legacy: move to suffix slot if possible. */
                if (!object_ego_suffix(o_ptr))
                    object_set_ego_suffix(o_ptr, e_idx);
                object_set_ego_prefix(o_ptr, 0);
            }
        }
    }

    if (object_ego_suffix(o_ptr))
    {
        byte e_idx = object_ego_suffix(o_ptr);
        ego_item_type* e_ptr;

        if (e_idx >= z_info->e_max)
        {
            return (-1);
        }

        e_ptr = &e_info[e_idx];
        if (!e_ptr->name)
        {
            object_set_ego_suffix(o_ptr, 0);
        }
        else if (ego_name_is_prefix(e_name + e_ptr->name))
        {
            /* Legacy: prefix egos used to live in name2. */
            if (!object_ego_prefix(o_ptr))
                object_set_ego_prefix(o_ptr, e_idx);
            object_set_ego_suffix(o_ptr, 0);
        }
    }

    /* Hack -- extract the "broken" flag */
    if (o_ptr->pval < 0)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Artefacts */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr;

        /* Obtain the artefact info */
        a_ptr = &a_info[o_ptr->name1];

        /* Get the new artefact "pval" */
        o_ptr->pval = a_ptr->pval;

        /* Get the new artefact fields */
        o_ptr->dd = a_ptr->dd;
        o_ptr->ds = a_ptr->ds;
        o_ptr->pd = a_ptr->pd;
        o_ptr->ps = a_ptr->ps;
        o_ptr->evn = a_ptr->evn;

        /* Get the new artefact weight */
        o_ptr->weight = a_ptr->weight;

        /* Ensure artefact-granted abilities are present (some generators may omit them). */
        for (int ai = 0; ai < a_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); ai++)
        {
            bool found = false;
            for (int oi = 0; oi < o_ptr->abilities; oi++)
            {
                if (o_ptr->skilltype[oi] == a_ptr->skilltype[ai]
                    && o_ptr->abilitynum[oi] == a_ptr->abilitynum[ai])
                {
                    found = true;
                    /* Also copy bane_type if this is a Bane ability */
                    o_ptr->bane_type[oi] = a_ptr->bane_type[ai];
                    break;
                }
            }
            if (!found)
            {
                int idx = o_ptr->abilities;
                o_ptr->skilltype[idx] = a_ptr->skilltype[ai];
                o_ptr->abilitynum[idx] = a_ptr->abilitynum[ai];
                o_ptr->bane_type[idx] = a_ptr->bane_type[ai];
                o_ptr->abilities++;
            }
        }

        /* Hack -- extract the "broken" flag */
        if (!a_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }

    /* Ego items */
    if (object_ego_prefix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_prefix(o_ptr)];
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }
    if (object_ego_suffix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_suffix(o_ptr)];
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }

    convert_old_staff_of_warding(o_ptr);

    /* Back-compat: derive bonuses from pval+flags for older saves. */
    if (o_ptr->name1 && !savefile_has_item_bonuses)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
        memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));
    }
    else if (!savefile_has_item_bonuses)
    {
        object_derive_stat_skill_bonuses_from_pval(o_ptr);
    }

    /* Log staff loading for debugging disappearing staff bug */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("Loaded staff: k_idx=%d sval=%d pval=%d number=%d", 
                  o_ptr->k_idx, o_ptr->sval, o_ptr->pval, o_ptr->number);
    }

    /* Success */
    return (0);
}

/*
 * Read a monster
 */
void rd_monster(monster_type* m_ptr)
{
    int i;

    /* Read the monster race */
    rd_s16b(&m_ptr->r_idx);

    /* Read the other information */
    rd_s16b(&m_ptr->image_r_idx);
    rd_byte(&m_ptr->fy);
    rd_byte(&m_ptr->fx);
    rd_s16b(&m_ptr->hp);
    rd_s16b(&m_ptr->maxhp);
    rd_s16b(&m_ptr->alertness);
    rd_byte(&m_ptr->skip_next_turn);
    rd_byte(&m_ptr->mspeed);
    rd_byte(&m_ptr->energy);
    rd_byte(&m_ptr->stunned);
    rd_byte(&m_ptr->confused);
    rd_s16b(&m_ptr->hasted);
    rd_s16b(&m_ptr->slowed);
    rd_byte(&m_ptr->stance);
    rd_s16b(&m_ptr->morale);
    rd_s16b(&m_ptr->tmp_morale);
    rd_byte(&m_ptr->noise);
    rd_byte(&m_ptr->encountered);
    rd_byte(&m_ptr->target_y);
    rd_byte(&m_ptr->target_x);
    rd_s16b(&m_ptr->wandering_idx);
    rd_byte(&m_ptr->wandering_dist);
    rd_byte(&m_ptr->mana);
    rd_byte(&m_ptr->song);
    rd_byte(&m_ptr->skip_this_turn);

    if (savefile_has_song_duels)
    {
        rd_byte(&m_ptr->song_contest_stacks);
        rd_byte(&m_ptr->song_lament_stacks);
        rd_byte(&m_ptr->song_lockout_timer);
        rd_byte(&m_ptr->song_hp_loss_lo);
        rd_s32b(&m_ptr->song_contest_last_turn);
        rd_s32b(&m_ptr->song_lament_last_turn);
        rd_s16b(&m_ptr->song_will_penalty);
        rd_s16b(&m_ptr->song_stealth_penalty);
        rd_s16b(&m_ptr->song_evasion_penalty);
        rd_byte(&m_ptr->song_armor_dice_penalty);
        rd_byte(&m_ptr->song_hp_loss_hi);
        rd_byte(&m_ptr->song_contest_completed);
        rd_byte(&m_ptr->song_lament_completed);
    }
    else
    {
        // legacy spare byte
        strip_bytes(1);
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lockout_timer = 0;
        m_ptr->song_hp_loss_lo = 0;
        m_ptr->song_contest_last_turn = 0;
        m_ptr->song_lament_last_turn = 0;
        m_ptr->song_will_penalty = 0;
        m_ptr->song_stealth_penalty = 0;
        m_ptr->song_evasion_penalty = 0;
        m_ptr->song_armor_dice_penalty = 0;
        m_ptr->song_hp_loss_hi = 0;
        m_ptr->song_contest_completed = 0;
        m_ptr->song_lament_completed = 0;
    }

    rd_s16b(&m_ptr->consecutive_attacks);
    rd_s16b(&m_ptr->turns_stationary);
    rd_u32b(&m_ptr->mflag);

    for (i = 0; i < ACTION_MAX; i++)
    {
        rd_byte(&m_ptr->previous_action[i]);
    }

    if (savefile_has_song_duels)
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            rd_byte(&m_ptr->blow_dd_reduction[i]);
        }
    }
    else
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            m_ptr->blow_dd_reduction[i] = 0;
        }
    }

    if (savefile_has_monster_shatter)
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            rd_byte(&m_ptr->blow_ds_reduction[i]);
        }

        rd_byte(&m_ptr->armor_ps_reduction);
        rd_byte(&m_ptr->shatter_padding[0]);
        rd_byte(&m_ptr->shatter_padding[1]);
        rd_byte(&m_ptr->shatter_padding[2]);
    }
    else
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            m_ptr->blow_ds_reduction[i] = 0;
        }

        m_ptr->armor_ps_reduction = 0;
        memset(m_ptr->shatter_padding, 0, sizeof(m_ptr->shatter_padding));
        strip_bytes(8);
    }

    /* Thrall quest data */
    if (savefile_has_thrall_quest)
    {
        rd_byte(&m_ptr->thrall_quest_item);
        if (savefile_has_thrall_quest_requested)
        {
            rd_byte(&m_ptr->thrall_quest_requested);
        }
        else
        {
            m_ptr->thrall_quest_requested = 0;
        }
        rd_byte(&m_ptr->thrall_quest_completed);
    }
    else
    {
        m_ptr->thrall_quest_item = THRALL_QUEST_NONE;
        m_ptr->thrall_quest_requested = 0;
        m_ptr->thrall_quest_completed = 0;
    }
}

/*
 * Read the monster lore
 */
static void rd_lore(int r_idx)
{
    int i;

    monster_race* r_ptr = &r_info[r_idx];
    monster_lore* l_ptr = &l_list[r_idx];

    /* Count deaths/sights/kills */
    rd_s16b(&l_ptr->deaths);
    rd_s16b(&l_ptr->psights);
    rd_s16b(&l_ptr->tsights);
    rd_s16b(&l_ptr->pkills);
    rd_s16b(&l_ptr->tkills);

    /* Count notices and ignores */
    rd_byte(&l_ptr->notice);
    rd_byte(&l_ptr->ignore);

    rd_byte(&l_ptr->drop_item);

    rd_byte(&l_ptr->ranged);

    /* Count blows of each type */
    for (i = 0; i < MONSTER_BLOW_MAX; i++)
        rd_byte(&l_ptr->blows[i]);

    /* Memorize flags */
    rd_u32b(&l_ptr->flags1);
    rd_u32b(&l_ptr->flags2);
    rd_u32b(&l_ptr->flags3);
    rd_u32b(&l_ptr->flags4);

    /* Read the "Racial" monster limit per level */
    rd_byte(&r_ptr->max_num);

    // 8 spare bytes
    strip_bytes(8);

    /* Repair the lore flags */
    l_ptr->flags1 &= r_ptr->flags1;
    l_ptr->flags2 &= r_ptr->flags2;
    l_ptr->flags3 &= r_ptr->flags3;
    l_ptr->flags4 &= r_ptr->flags4;
}

static void rd_monster_race_stats(monster_race* r_ptr)
{
    byte tmp8u;
    s16b tmp16s;
    u32b tmp32u;

    rd_byte(&tmp8u);
    r_ptr->hdice = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->hside = tmp8u;
    rd_s16b(&tmp16s);
    r_ptr->evn = tmp16s;
    rd_byte(&tmp8u);
    r_ptr->pd = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->ps = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->speed = tmp8u;
    rd_s16b(&tmp16s);
    r_ptr->light = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->sleep = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->per = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->stl = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->wil = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->extra = tmp16s;
    rd_byte(&tmp8u);
    r_ptr->freq_ranged = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->spell_power = tmp8u;
    rd_u32b(&tmp32u);
    r_ptr->mon_power = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags1 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags2 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags3 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags4 = tmp32u;

    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        rd_byte(&tmp8u);
        r_ptr->blow[i].method = tmp8u;
        rd_byte(&tmp8u);
        r_ptr->blow[i].effect = tmp8u;
        rd_s16b(&tmp16s);
        r_ptr->blow[i].att = tmp16s;
        rd_byte(&tmp8u);
        r_ptr->blow[i].dd = tmp8u;
        rd_byte(&tmp8u);
        r_ptr->blow[i].ds = tmp8u;
    }

    rd_byte(&tmp8u);
    r_ptr->level = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->rarity = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->d_attr = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->d_char = (char)tmp8u;
    rd_byte(&tmp8u);
    r_ptr->x_attr = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->x_char = (char)tmp8u;
}

static void restore_monster_races_from_base(void)
{
    if (!r_base)
        return;

    for (int r = 0; r < z_info->r_max; r++)
    {
        byte saved_max_num = r_info[r].max_num;

        r_info[r] = r_base[r];
        r_info[r].max_num = saved_max_num;
    }
}

static void rd_monster_runtime_overrides(void)
{
    u16b count = 0;

    rd_u16b(&count);

    if (!count)
        return;

    log_debug(
        "Discarding %u legacy monster race runtime overrides",
        (unsigned)count);

    for (u16b n = 0; n < count; n++)
    {
        u16b r_idx = 0;
        monster_race scratch;

        rd_u16b(&r_idx);
        memset(&scratch, 0, sizeof(scratch));
        rd_monster_race_stats(&scratch);

        if (r_idx >= z_info->r_max)
        {
            log_error(
                "Invalid monster race index %u in override block (max %u)",
                (unsigned)r_idx, (unsigned)z_info->r_max);
        }
    }
}
/*
 * Read RNG state
 */
static void rd_randomizer(void)
{
    int i;
    u16b dummy;
    u32b tmp32;
    u32b lo = 0;
    u32b hi = 0;

    strip_bytes(8);
    rd_u16b(&dummy);

    for (i = 0; i < 63; i++)
    {
        rd_u32b(&tmp32);
        if (i == 0)
            lo = tmp32;
        else if (i == 1)
            hi = tmp32;
    }

    Rand_state_import(((u64b)hi << 32) | lo);
}

/*
 * Read options
 *
 * Note that the normal options are stored as a set of 256 bit flags,
 * plus a set of 256 bit masks to indicate which bit flags were defined
 * at the time the savefile was created.  This will allow new options
 * to be added, and old options to be removed, at any time, without
 * hurting old savefiles.
 *
 * The window options are stored in the same way, but note that each
 * window gets 32 options, and their order is fixed by certain defines.
 */
static void rd_options(void)
{
    int i, n;

    byte b;

    u32b flag[8];
    u32b mask[8];
    u32b window_flag[ANGBAND_TERM_MAX];
    u32b window_mask[ANGBAND_TERM_MAX];

    /*** Special info */

    /* Read "delay_factor" */
    rd_byte(&b);
    op_ptr->delay_factor = b;

    /* Read "hitpoint_warn" */
    rd_byte(&b);
    op_ptr->hitpoint_warn = b;

    /* Read "main_combat_rolls" */
    rd_byte(&b);
    op_ptr->main_combat_rolls = b;
    /* Ensure it's in valid range */
    if (op_ptr->main_combat_rolls > 3)
        op_ptr->main_combat_rolls = 0;

    /* Read "ability_desc_mode" */
    rd_byte(&b);
    op_ptr->ability_desc_mode = b;
    if (op_ptr->ability_desc_mode > 2)
        op_ptr->ability_desc_mode = 0;

    /* Read "vault_drop_frequency" */
    rd_byte(&b);
    op_ptr->vault_drop_frequency = b;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;

    /* Read "intro_style" */
    rd_byte(&b);
    op_ptr->intro_style = b;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_FLAME;

    if (savefile_version_at_least(0, 9, 5, 5))
    {
        rd_byte(&b);
        op_ptr->level_entry_narrative_mode = b;
        if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
            op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;

        rd_byte(&b);
        op_ptr->partition_narrative_mode = b;
        if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
            op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;

        rd_byte(&b);
        op_ptr->noble_item_spawn_mode = b;
        if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
            op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;

        /* Skip 1 remaining spare byte */
        strip_bytes(1);
    }
    else
    {
        /* Old savefiles used the boolean show_level_entry_banner option. */
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
        strip_bytes(4);
    }

    /*** Normal Options ***/

    /* Read the option flags */
    for (n = 0; n < 8; n++)
        rd_u32b(&flag[n]);

    /* Read the option masks */
    for (n = 0; n < 8; n++)
        rd_u32b(&mask[n]);

    /* Analyze the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        int os = i / 32;
        int ob = i % 32;

        /* Process real entries */
        if (option_text[i])
        {
            /* Process saved entries */
            if (mask[os] & (1L << ob))
            {
                /* Set flag */
                if (flag[os] & (1L << ob))
                {
                    /* Set */
                    op_ptr->opt[i] = true;
                }

                /* Clear flag */
                else
                {
                    /* Set */
                    op_ptr->opt[i] = false;
                }
            }
        }
    }

    if (!savefile_version_at_least(0, 9, 5, 5))
    {
        op_ptr->level_entry_narrative_mode = op_ptr->opt[OPT_show_level_entry_banner]
            ? LEVEL_ENTRY_NARRATIVE_BANNER_DELAY
            : LEVEL_ENTRY_NARRATIVE_OFF;
        op_ptr->partition_narrative_mode = op_ptr->opt[OPT_show_partition_narrative]
            ? PARTITION_NARRATIVE_BANNER
            : PARTITION_NARRATIVE_OFF;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    /*** Window Options ***/

    /* Read the window flags */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        rd_u32b(&window_flag[n]);
    }

    /* Read the window masks */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        rd_u32b(&window_mask[n]);
    }

    /* Analyze the options */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        op_ptr->window_flag[n] = 0;

        /* Analyze the options */
        for (i = 0; i < 32; i++)
        {
            /* Process valid flags */
            if (window_flag_desc[i])
            {
                /* Process valid flags */
                if (window_mask[n] & (1L << i))
                {
                    /* Set */
                    if (window_flag[n] & (1L << i))
                    {
                        /* Set */
                        op_ptr->window_flag[n] |= (1L << i);
                    }
                }
            }
        }
    }
}

u32b randart_version;

/*
 * Read the saved messages
 */
static void rd_messages(void)
{
    int i;
    char buf[128];
    u16b tmp16u;

    s16b num;

    /* Total */
    rd_s16b(&num);
    if (load_read_failed)
        return;
    if (num < 0)
        num = 0;
    if (num > MESSAGE_MAX)
        num = MESSAGE_MAX;
    log_debug("Loading %d message history entries", num);

    /* Read the messages */
    for (i = 0; i < num; i++)
    {
        /* Read the message */
        if (!rd_string(buf, sizeof(buf)))
            return;

        /* Read the message type */
        rd_u16b(&tmp16u);
        if (load_read_failed)
            return;

        /* Save the message */
        message_add(buf, tmp16u);
    }
}

/*
 * Actually read the savefile
 */
static errr rd_savefile_new_aux(void)
{
    int i;

    byte tmp8u;
    u16b tmp16u;

    u32b n_x_check, n_v_check;
    u32b o_x_check, o_v_check;

    load_init_version_features();

    /* Reset load byte offset counter */
    load_byte_offset = 0;
    log_trace("=== LOAD: Reset byte offset counter ===");

    /* Mention the savefile version */
    note(
        format("Loading a %d.%d.%d savefile...", sf_major, sf_minor, sf_patch));

    /* Strip the version bytes */
    strip_bytes(4);

    /* Hack -- decrypt */
    xor_byte = sf_extra;

    /* Clear the checksums */
    v_check = 0L;
    x_check = 0L;
    load_read_failed = false;

    /* Operating system info */
    rd_u32b(&sf_xtra);

    /* Time of savefile creation */
    rd_u32b(&sf_when);

    /* Number of resurrections */
    rd_u16b(&sf_lives);

    /* Number of times played */
    rd_u16b(&sf_saves);

    // 8 spare bytes
    strip_bytes(8);

    /* Read RNG state */
    rd_randomizer();
    if (load_expect_stream_ok("randomizer"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Randomizer Info");

    /* Then the options */
    rd_options();
    if (load_expect_stream_ok("options"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Option Flags");

    /* Then the "messages" */
    rd_messages();
    if (load_expect_stream_ok("messages"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Messages");

    /* Monster Memory */
    rd_u16b(&tmp16u);
    log_debug("Loading %d monster race records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->r_max)
    {
        note(format("Too many (%u) monster races!", tmp16u));
        return (-1);
    }

    /* Read the available records */
    for (i = 0; i < tmp16u; i++)
    {
        /* Read the lore */
        rd_lore(i);
    }
    if (load_expect_stream_ok("monster memory"))
        return (-1);
    restore_monster_races_from_base();
    if (savefile_has_runtime_overrides)
    {
        rd_monster_runtime_overrides();
        if (load_expect_stream_ok("monster runtime overrides"))
            return (-1);
    }
    if (runtime_cli_fiddle())
        note("Loaded Monster Memory");

    /* Object Memory */
    rd_u16b(&tmp16u);
    log_debug("Loading %d object kind records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->k_max)
    {
        note(format("Too many (%u) object kinds!", tmp16u));
        return (-1);
    }

    /* Read the object memory */
    for (i = 0; i < tmp16u; i++)
    {
        byte memory_flags = 0;

        object_kind* k_ptr = &k_info[i];

        rd_byte(&memory_flags);

        k_ptr->aware = (memory_flags & 0x01) ? true : false;
        k_ptr->tried = (memory_flags & 0x02) ? true : false;
        k_ptr->everseen = (memory_flags & 0x08) ? true : false;

        rd_byte(&tmp8u);
    }
    if (load_expect_stream_ok("object memory"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Object Memory");

    /* Load the Artefacts */
    rd_u16b(&tmp16u);
    log_debug("Loading %d artefact records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->art_max)
    {
        note(format("Too many (%u) artefacts!", tmp16u));
        return (-1);
    }

    /* Read the artefact flags */
    for (i = 0; i < tmp16u; i++)
    {
        rd_byte(&tmp8u);
        a_info[i].cur_num = tmp8u;
        rd_byte(&tmp8u);
        a_info[i].found_num = tmp8u;
        if (savefile_has_artifact_seen)
        {
            rd_byte(&tmp8u);
            a_info[i].seen = tmp8u;
        }
        else
        {
            /* Older saves don't have seen field - default to 0 */
            a_info[i].seen = 0;
        }
    }
    if (load_expect_stream_ok("artefact memory"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Artefacts");

    /* Read the extra stuff */
    log_debug("Loading extra player information");
    if (rd_extra())
        return (-1);
    if (load_expect_stream_ok("extra player information"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded extra information");

    log_debug("Loading random artefacts");
    if (rd_randarts())
        return (-1);
    if (load_expect_stream_ok("random artefacts"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Random Artefacts");

    log_debug("Loading notes");
    if (rd_notes())
        return (-1);
    if (load_expect_stream_ok("notes"))
        return (-1);
    if (runtime_cli_fiddle())
        note("Loaded Notes");

    /* Important -- Initialize the race/character */
    rp_ptr = &p_info[p_ptr->prace];
    current_character_profile = &c_info[p_ptr->pcharacter];

    /* Read the inventory */
    log_debug("Loading player inventory");
    if (rd_inventory())
    {
        note("Unable to read inventory");
        return (-1);
    }

    /* I'm not dead yet... */
    if (!p_ptr->is_dead)
    {
        /* Dead players have no dungeon */
        note("Restoring Dungeon...");
        log_debug("Loading dungeon data");
        if (rd_dungeon())
        {
            note("Error reading dungeon data");
            return (-1);
        }
    }

    /* Save the checksum */
    n_v_check = v_check;

    /* Read the old checksum */
    rd_u32b(&o_v_check);

    log_debug("Checksum validation: expected=%u, file=%u", n_v_check, o_v_check);

    /* Verify */
    if (o_v_check != n_v_check)
    {
        log_error("Invalid checksum: expected %u, got %u", n_v_check, o_v_check);
        note("Invalid checksum");
        return (-1);
    }

    /* Save the encoded checksum */
    n_x_check = x_check;

    /* Read the checksum */
    rd_u32b(&o_x_check);

    log_debug("Encoded checksum validation: expected=%u, file=%u", n_x_check, o_x_check);

    /* Verify */
    if (o_x_check != n_x_check)
    {
        log_error("Invalid encoded checksum: expected %u, got %u", n_x_check, o_x_check);
        note("Invalid encoded checksum");
        return (-1);
    }

    /* Success */
    return (0);
}

/*
 * Actually read the savefile
 */
static errr rd_savefile(void)
{
    errr err;

    log_debug("Opening savefile for reading");

    /* Grab permissions */
    safe_setuid_grab();

    /* The savefile is a binary file */
    fff = ang_file_open(savefile, "rb");

    /* Drop permissions */
    safe_setuid_drop();

    /* Paranoia */
    if (!fff)
    {
        log_error("Failed to open savefile: %s", savefile);
        return (-1);
    }

    /* Call the sub-function */
    err = rd_savefile_new_aux();
    log_debug("rd_savefile_new_aux returned: %d", err);

    /* Note: SDL doesn't have ferror equivalent - errors are caught during read operations */
    
    /* Close the file */
    ang_file_close(fff);
    log_debug("Savefile closed");

    /* Result */
    return (err);
}

/*
 * Attempt to Load a "savefile"
 *
 * On multi-user systems, you may only "read" a savefile if you will be
 * allowed to "write" it later, this prevents painful situations in which
 * the player loads a savefile belonging to someone else, and then is not
 * allowed to save his game when he quits.
 *
 * We return "true" if the savefile was usable, and we set the global
 * flag "character_loaded" if a real, living, character was loaded.
 *
 * Note that we always try to load the "current" savefile, even if
 * there is no such file, so we must check for "empty" savefile names.
 */
bool load_player(void)
{
    ang_file* fd = NULL;

    errr err = 0;

    byte vvv[4];

#ifdef VERIFY_TIMESTAMP
    struct stat statbuf;
#endif /* VERIFY_TIMESTAMP */

    cptr what = "generic";

    log_debug("Loading savefile '%s'", savefile);

    /* Paranoia */
    turn = 0;

    /* Paranoia */
    p_ptr->is_dead = false;
    killer_reset();

    // Set a flag to show that we are restoring a game
    p_ptr->restoring = true;

    /* Allow empty savefile name */
    if (!savefile[0])
        return (true);

    /* Grab permissions */
    safe_setuid_grab();

    /* Open the savefile */
    fd = ang_file_open(savefile, "rb");

    /* Drop permissions */
    safe_setuid_drop();

    /* No file */
    if (!fd)
    {
        /* Give a message */
        // msg_format("Savefile \"%s\" does not exist.", savefile);
        // message_flush();

        /* Allow this */
        log_debug("Savefile '%s' does not exist", savefile);
        p_ptr->restoring = false;
        return (false);
    }

    log_debug("Savefile exists, proceeding with load");

    /* Close the file */
    ang_file_close(fd);

#ifdef VERIFY_SAVEFILE

    /* Verify savefile usage */
    if (!err)
    {
        ang_file* fkk;

        char temp[1024];

        /* Extract name of lock file */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".lok", sizeof(temp));

        /* Grab permissions */
        safe_setuid_grab();

        /* Check for lock */
        fkk = ang_file_open(temp, "r");

        /* Drop permissions */
        safe_setuid_drop();

        /* Oops, lock exists */
        if (fkk)
        {
            /* Close the file */
            ang_file_close(fkk);

            /* Message */
            msg_print("Savefile is currently in use.");
            message_flush();

            /* Oops */
            return (false);
        }

        /* Grab permissions */
        safe_setuid_grab();

        /* Create a lock file */
        fkk = ang_file_open(temp, "w");

        /* Drop permissions */
        safe_setuid_drop();

        /* Dump a line of info */
        fprintf(fkk, "Lock file for savefile '%s'\n", savefile);

        /* Close the lock file */
        ang_file_close(fkk);
    }

#endif /* VERIFY_SAVEFILE */

    /* Okay */
    if (!err)
    {
        /* Grab permissions */
        safe_setuid_grab();

        /* Open the savefile */
        fd = ang_file_open(savefile, "rb");

        /* Drop permissions */
        safe_setuid_drop();

        /* No file */
        if (!fd)
            err = -1;

        /* Message (below) */
        if (err)
            what = "Cannot open savefile";
    }

    /* Process file */
    if (!err)
    {
#ifdef VERIFY_TIMESTAMP
        /* Note: fstat requires an integer file descriptor, not the ang_file facade. */
        /* Timestamp verification disabled for SDL builds */
        log_debug("Timestamp verification skipped (not supported with ang_file)");
#endif /* VERIFY_TIMESTAMP */

        /* Read the first four bytes */
        if (sdl_read(fd, (char*)(vvv), sizeof(vvv)))
            err = -1;

        /* What */
        if (err)
            what = "Cannot read savefile";

        /* Close the file */
        ang_file_close(fd);
    }

    /* Process file */
    if (!err)
    {
        /* Extract version */
        sf_major = vvv[0];
        sf_minor = vvv[1];
        sf_patch = vvv[2];
        sf_extra = vvv[3];
        log_debug("Version bytes read: %u.%u.%u extra=%u", (unsigned)sf_major, (unsigned)sf_minor, (unsigned)sf_patch, (unsigned)sf_extra);

        if (!savefile_version_supported())
        {
            err = -1;
            what = "Incompatible savefile version";
            log_error("Savefile version %u.%u.%u extra=%u is outside supported range [%u.%u.%u extra>=0 .. %u.%u.%u extra<=%u] (current release requires extra >= %u)",
                (unsigned)sf_major, (unsigned)sf_minor, (unsigned)sf_patch, (unsigned)sf_extra,
                (unsigned)OLD_VERSION_MAJOR, (unsigned)OLD_VERSION_MINOR, (unsigned)OLD_VERSION_PATCH,
                (unsigned)VERSION_MAJOR, (unsigned)VERSION_MINOR, (unsigned)VERSION_PATCH, (unsigned)VERSION_EXTRA,
                (unsigned)MIN_VERSION_EXTRA);
        }
        else
        {
            load_init_version_features();
        }

        load_byte_offset = 0; /* reset counter before decoding stream */

        if (!err)
        {
            /* Attempt to load */
            err = rd_savefile();
            if (err) {
                log_error("Read savefile failed");
            } else {
                log_debug("Read savefile success");
            }
            if (!err) {
                log_debug("load: post-read flags (is_dead=%d, wizard=%d, noscore=0x%04X)",
                         p_ptr->is_dead, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore);
            }

            /* Message (below) */
            if (err)
                what = "Cannot parse savefile";
        }
    }

    /* Paranoia */
    if (!err)
    {
        /* Invalid turn */
        if (!turn)
            err = -1;

        /* Message (below) */
        if (err)
            what = "Broken savefile";
    }

#ifdef VERIFY_TIMESTAMP
    /* Verify timestamp */
    if (!err && !runtime_cli_wizard())
    {
        /* Hack -- Verify the timestamp */
        if (sf_when > (statbuf.st_ctime + 100)
            || sf_when < (statbuf.st_ctime - 100))
        {
            /* Message */
            what = "Invalid timestamp";

            /* Oops */
            err = -1;
        }
    }
#endif /* VERIFY_TIMESTAMP */

    /* Okay */
    if (!err)
    {
        /* App-wide settings live in the SDL JSON config, so they must win over
         * any copies serialized in the savefile. */
        platform_load_app_options();

        // if Morgoth has lost his crown...
        if ((&a_info[ART_MORGOTH_3])->cur_num == 1)
        {
            // lower Morgoth's protection, remove his light source, increase his
            // will and perception
            (&r_info[R_IDX_MORGOTH])->pd -= 1;
            (&r_info[R_IDX_MORGOTH])->light = 0;
            (&r_info[R_IDX_MORGOTH])->wil += 5;
            (&r_info[R_IDX_MORGOTH])->per += 5;
        }

        /* Player is dead */
        if (p_ptr->is_dead)
        {
            log_info("Loading a dead character");
            /* Cheat death (unless the character retired) */
            if (runtime_cli_wizard())
            {
                log_info("Wizard mode: resurrecting dead character");
                /*heal the player*/
                hp_player(100, true, true);

                /* Forget death */
                p_ptr->is_dead = false;

                /* A character was loaded */
                character_loaded = true;

                // put the character somewhere sensible
                p_ptr->depth = min_depth();

                // Mark savefile
                p_ptr->noscore |= 0x0001;

                /* Done */
                return (true);
            }

            /* Forget death */
            p_ptr->is_dead = false;

            /* Count lives */
            sf_lives++;

            /* Forget turns */
            turn = 0;
            playerturn = 0;

            /* A dead character was loaded */
            character_loaded_dead = true;
            log_info("Character loaded dead");

            /* Done */
            return (false);
        }

        /* A character was loaded */
        character_loaded = true;
        log_info("%s", character_loaded ? "Character loaded" : "Character not loaded");

        /* Still alive */
        if (p_ptr->chp >= 0)
        {
            /* Reset cause of death */
            SDL_strlcpy(
                p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
        }

        // count the artefacts seen for the player
        p_ptr->artefacts = artefact_count();
        log_debug("Character has seen %d artefacts", p_ptr->artefacts);

        /* Process player name to update base_name and savefile path */
        process_player_name(true);
        log_debug("Processed player name after load: base_name='%s', savefile='%s'", 
                 op_ptr->base_name, savefile);

        /* Reapply Morgoth's anger state to the r_info template */
        if (p_ptr->morgoth_state > 0)
        {
            log_debug("load: reapplying morgoth_state %d to r_info template", 
                     p_ptr->morgoth_state);
            
            /* Save current state, then reset to 0 and reapply */
            s16b saved_state = p_ptr->morgoth_state;
            p_ptr->morgoth_state = 0;
            anger_morgoth(saved_state);
        }
        else
        {
            log_debug("load: morgoth_state is 0, no reapplication needed");
        }

        /* Success */
        return (true);
    }

#ifdef VERIFY_SAVEFILE

    /* Verify savefile usage */
    if (true)
    {
        char temp[1024];

        /* Extract name of lock file */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".lok", sizeof(temp));

        /* Grab permissions */
        safe_setuid_grab();

        /* Remove lock */
        fd_kill(temp);

        /* Drop permissions */
        safe_setuid_drop();
    }

#endif /* VERIFY_SAVEFILE */

    /* Message */
    msg_format("Error (%s) reading %d.%d.%d savefile.", what, sf_major,
        sf_minor, sf_patch);
    message_flush();

    /* Oops */
    return (false);
}
