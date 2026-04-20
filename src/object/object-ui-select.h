/* File: object-ui-select.h */
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
 * Object item selection UI.
 */

#ifndef INCLUDED_OBJECT_UI_SELECT_H
#define INCLUDED_OBJECT_UI_SELECT_H

#include "../h-basic.h"

typedef struct object_type object_type;

extern bool item_tester_full;
extern byte item_tester_tval;
extern bool (*item_tester_hook)(const object_type*);

void toggle_inven_equip(void);
bool get_item(int* cp, cptr pmt, cptr str, int mode);

#endif /* INCLUDED_OBJECT_UI_SELECT_H */
