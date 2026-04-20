/* File: object-display.h */
/*
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
 * Object display attributes.
 * Graphics overrides, colors, and visual reset functions.
 */

#ifndef INCLUDED_OBJECT_DISPLAY_H
#define INCLUDED_OBJECT_DISPLAY_H

#include "../h-basic.h"

typedef struct object_type object_type;

byte object_attr_graphics_override(const object_type* o_ptr, byte base_attr);
char object_char_graphics_override(const object_type* o_ptr, char base_char);
byte object_default_text_color(const object_type* o_ptr);
byte object_display_color(const object_type* o_ptr, byte base_color);
bool object_is_unidentified_for_display(const object_type* o_ptr);
void inventory_menu_set_include_equip(bool include);
bool inventory_menu_get_include_equip(void);
void reset_visuals(bool unused);

#endif /* INCLUDED_OBJECT_DISPLAY_H */
