/* ui/colors.h - Color name and attribute utilities */

#ifndef INCLUDED_UI_COLORS_H
#define INCLUDED_UI_COLORS_H

#include "../h-basic.h"

#define UI_COLOR_PRESET_MAX 16
#define UI_COLOR_PRESET_ID_LEN 64
#define UI_COLOR_PRESET_LABEL_LEN 64

extern int use_graphics;
extern bool use_bigtile;
extern bool use_transparency;
extern byte angband_color_table[256][4];

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name with optional shade suffix.
 */
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);
extern cptr attr_to_text(byte a);
extern bool ui_colors_use_backgrounds(void);
extern void ui_colors_set_backgrounds(bool enabled);
extern bool ui_colors_load_palette_presets(void);
extern int ui_colors_palette_preset_count(void);
extern cptr ui_colors_palette_preset_id(int index);
extern cptr ui_colors_palette_preset_label(int index);
extern cptr ui_colors_current_palette_preset(void);
extern bool ui_colors_apply_palette_preset(cptr id);

#endif /* INCLUDED_UI_COLORS_H */
