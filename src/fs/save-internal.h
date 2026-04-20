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

#ifndef INCLUDED_FS_SAVE_INTERNAL_H
#define INCLUDED_FS_SAVE_INTERNAL_H

/*
 * Lane-local save helpers shared across src/fs/save*.c.
 * This is intentionally not a public subsystem header.
 */

extern u32b save_byte_offset;

void save_wr_byte(byte v);
void save_wr_u16b(u16b v);
void save_wr_s16b(s16b v);
void save_wr_u32b(u32b v);
void save_wr_s32b(s32b v);
void save_wr_string(cptr str);
void save_wr_item(const object_type* o_ptr);
void save_wr_monster(const monster_type* m_ptr);

void save_write_extra(void);
void save_write_randarts(void);
void save_write_notes(void);
void save_write_inventory(void);
void save_write_dungeon(void);

#define wr_byte save_wr_byte
#define wr_u16b save_wr_u16b
#define wr_s16b save_wr_s16b
#define wr_u32b save_wr_u32b
#define wr_s32b save_wr_s32b
#define wr_string save_wr_string
#define wr_item save_wr_item
#define wr_monster save_wr_monster
#define wr_extra save_write_extra
#define wr_randarts save_write_randarts
#define wr_notes save_write_notes
#define wr_dungeon save_write_dungeon

#endif /* INCLUDED_FS_SAVE_INTERNAL_H */
