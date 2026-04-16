/* File: angband.h */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#ifndef INCLUDED_ANGBAND_H
#define INCLUDED_ANGBAND_H

/*
 * Include the low-level includes.
 */
#include "h-basic.h"

/*
 * Include the mid-level includes.
 */
#include "log/fatal.h"
#include "mem/alloc.h"
#include "format.h"
#include "platform-frame.h"
#include "rng.h"
#include "support/strl.h"

/* Story font flags used by semantic document and menu rendering. */
#define STORY_FLAG_USE        0x01
#define STORY_FLAG_CELL_ALIGN 0x02

/* Background-variant color encoding used by map/tile rendering. */
#define MAX_COLORS 32
#define BG_BLACK 0
#define BG_SAME 1
#define BG_DARK 2

/*
 * Include the high-level includes.
 */
#include "config.h"
#include "defines.h"
#include "drop_system.h"
#include "types.h"
#include "supplies.h"
#include "app/app-movement.h"
#include "cave/cave-state.h"
#include "cave/cave.h"
#include "cmd/combat/cmd-combat.h"
#include "cmd/item/cmd-item.h"
#include "cmd/movement/cmd-movement.h"
#include "cmd/ui/cmd-ui.h"
#include "fs/pref-files.h"
#include "fs/pref-time.h"
#include "fs/savefile-name.h"
#include "game-event.h"
#include "init/init-data.h"
#include "init/init-paths.h"
#include "level-generation/level-generation.h"
#include "melee/melee.h"
#include "metarun.h"
#include "monster/monster-death.h"
#include "monster/monster.h"
#include "monster/monster-state.h"
#include "object/object.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-flags.h"
#include "object/object-flavor.h"
#include "object/object-info.h"
#include "object/object-state.h"
#include "object/object-util.h"
#include "player/player.h"
#include "quest/quest.h"
#include "runtime/runtime-game.h"
#include "runtime/game-update.h"
#include "runtime/runtime-state.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_ui.h"
#include "signals.h"
#include "smithing/smithing.h"
#include "spell/spell.h"
#include "support/game-tables.h"
#include "support/macro-state.h"
#include "support/util.h"
#include "support/text-output.h"
#include "ui/colors.h"
#include "ui/layout.h"
#include "ui/story_font.h"
#include "ui/targeting.h"
#include "ui/ui-character-name.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-death.h"
#include "ui/ui-file-viewer.h"
#include "ui/ui-help.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-story.h"

/***** Some older copyright messages follow below *****/

/*
 * Note that these copyright messages apply to an ancient version
 * of Angband, as in, from pre-2.4.frog-knows days, and thus the
 * references to version numbers may be rather misleading...
 */

/*
 * UNIX ANGBAND Version 5.0
 */

/* Original copyright message follows. */

/*
 * ANGBAND Version 4.8	COPYRIGHT (c) Robert Alan Koeneke
 *
 *	 I lovingly dedicate this game to hackers and adventurers
 *	 everywhere...
 *
 *	 Designer and Programmer:
 *		Robert Alan Koeneke
 *		University of Oklahoma
 *
 *	 Assistant Programmer:
 *		Jimmey Wayne Todd
 *		University of Oklahoma
 *
 *	 Assistant Programmer:
 *		Gary D. McAdoo
 *		University of Oklahoma
 *
 *	 UNIX Port:
 *		James E. Wilson
 *		UC Berkeley
 *		wilson@ernie.Berkeley.EDU
 *		ucbvax!ucbernie!wilson
 */

/*
 *	 ANGBAND may be copied and modified freely as long as the above
 *	 credits are retained.	No one who-so-ever may sell or market
 *	 this software in any form without the expressed written consent
 *	 of the author Robert Alan Koeneke.
 */

/*
 * Inline string helper functions (replacing z-util.c implementations)
 * These provide simple wrappers for common string operations.
 */
#include <stdlib.h>
#include <string.h>

/* String equality check */
static inline bool streq(const char* a, const char* b) {
    return (strcmp(a, b) == 0);
}

/* ------------------------------------------------------------------------ */
/* Ego affix helpers                                                        */
/* ------------------------------------------------------------------------ */

/*
 * Ego items now support a prefix and a suffix.
 *
 * Storage:
 * - Suffix ego index is stored in object_type.name2 (legacy ego field).
 * - Prefix ego index is stored in object_type.unused2 (reserved savefile field).
 *
 * Do not access these fields directly outside low-level helpers; use the
 * accessors below to keep semantics consistent across the codebase.
 */
static inline byte object_ego_prefix(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    if (o_ptr->unused2 <= 0)
        return 0;
    if (o_ptr->unused2 > 255)
        return 0;
    return (byte)o_ptr->unused2;
}

static inline void object_set_ego_prefix(object_type* o_ptr, int e_idx)
{
    if (!o_ptr)
        return;
    if (e_idx <= 0)
    {
        o_ptr->unused2 = 0;
        return;
    }
    o_ptr->unused2 = (s32b)(byte)e_idx;
}

static inline byte object_ego_suffix(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    return o_ptr->name2;
}

static inline void object_set_ego_suffix(object_type* o_ptr, int e_idx)
{
    if (!o_ptr)
        return;
    if (e_idx <= 0)
    {
        o_ptr->name2 = 0;
        return;
    }
    o_ptr->name2 = (byte)e_idx;
}

static inline bool object_has_ego(const object_type* o_ptr)
{
    return object_ego_prefix(o_ptr) || object_ego_suffix(o_ptr);
}

static inline bool object_has_ego_idx(const object_type* o_ptr, int e_idx)
{
    if (!o_ptr || e_idx <= 0 || e_idx > 255)
        return false;
    return object_ego_prefix(o_ptr) == (byte)e_idx
        || object_ego_suffix(o_ptr) == (byte)e_idx;
}

static inline s32b object_runtime_state(const object_type* o_ptr)
{
    if (!o_ptr)
        return OBJECT_RUNTIME_STATE_NONE;
    return o_ptr->unused3;
}

static inline void object_set_runtime_state(object_type* o_ptr, s32b state)
{
    if (!o_ptr)
        return;
    o_ptr->unused3 = state;
}

static inline s32b object_runtime_payload(const object_type* o_ptr)
{
    if (!o_ptr)
        return 0;
    return o_ptr->unused4;
}

static inline void object_set_runtime_payload(object_type* o_ptr, s32b payload)
{
    if (!o_ptr)
        return;
    o_ptr->unused4 = payload;
}

static inline bool ego_name_is_prefix(const char* name)
{
    if (!name || !name[0])
        return false;
    size_t len = strlen(name);
    return (len >= 2 && name[0] == '(' && name[len - 1] == ')');
}

/* Check if string t is a prefix of string s */
static inline bool prefix(const char* s, const char* t) {
    while (*t) {
        if (*t++ != *s++) return false;
    }
    return true;
}

/* Check if string t is a suffix of string s */
static inline bool suffix(const char* s, const char* t) {
    size_t slen = strlen(s);
    size_t tlen = strlen(t);
    if (tlen > slen) return false;
    return (strcmp(s + slen - tlen, t) == 0);
}

/* Duplicate a string using standard allocation. */
static inline char* str_dup(const char* str) {
    if (!str)
        return NULL;

    size_t len = strlen(str) + 1;
    char* copy = malloc(len);
    if (!copy)
        return NULL;

    memcpy(copy, str, len);
    return copy;
}

/* Free a string allocated with str_dup and return NULL */
static inline void* str_free(const char* str) {
    if (str)
        free((void*)str);
    return NULL;
}

#endif
