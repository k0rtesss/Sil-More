/* File: object/object-info.h */

#ifndef INCLUDED_OBJECT_INFO_H
#define INCLUDED_OBJECT_INFO_H

#include "angband.h"

#ifndef OBJECT_INFO_SCREEN_ACTION_DEFINED
#define OBJECT_INFO_SCREEN_ACTION_DEFINED
typedef struct object_info_screen_action
{
    int key;
    cptr token;
} object_info_screen_action;
#endif

bool object_info_out(const object_type* o_ptr);
/* Describe the mechanics of a known design, optionally restricted to a
 * subset of its flags. Abilities come from the supplied object. */
void object_info_effect_text(const object_type* o_ptr, const u32b mask[4],
    char* text, size_t size);
/* Shared item name, combat notation, volume and weight in both views. */
void object_info_item_header_out(const object_type* o_ptr);
cptr object_lore_select_base_text(const object_type* o_ptr, char* out,
    size_t out_sz);
void note_info_screen(const object_type* o_ptr);
void object_info_screen(const object_type* o_ptr);
/* Enable a Full/Comparison switch for a candidate and replacement alternatives.
 * Full is the initial view. Leave compare false for sets/storage exchanges. */
void object_info_screen_multi(const object_type** objects, const char** headings,
    int count, bool compare);
char object_info_screen_multi_with_actions(const object_type** objects,
    const char** headings, int count, cptr footer,
    const object_info_screen_action* actions, int action_count, bool compare);
bool object_info_overlay_show_multi(const object_type** objects,
    const char** headings, int count, bool compare);
void object_info_overlay_clear(void);

#endif /* INCLUDED_OBJECT_INFO_H */
