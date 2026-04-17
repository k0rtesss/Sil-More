/* File: save-dungeon.c */

#include "angband.h"
#include "fs/save-internal.h"
#include "log/log.h"

#define IMPORTANT_FLAGS_LO                                                     \
    (CAVE_MARK | CAVE_GLOW | CAVE_ICKY | CAVE_ROOM | CAVE_G_VAULT | CAVE_HIDDEN)
#define IMPORTANT_FLAGS_HI (CAVE_CHASM_AREA)
#define IMPORTANT_FLAGS_16 (IMPORTANT_FLAGS_LO | IMPORTANT_FLAGS_HI)

void wr_dungeon(void)
{
    int i, y, x;

    byte tmp8u;

    byte count;
    byte prev_char;

    log_debug("Writing dungeon level %d (%dx%d)", p_ptr->depth,
        p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    log_trace("[save:%06u] === BEGIN DUNGEON ===", (unsigned)save_byte_offset);

    log_trace("[save:%06u] Writing dungeon header",
        (unsigned)save_byte_offset);
    wr_s16b(p_ptr->depth);
    wr_s16b(p_ptr->py);
    wr_s16b(p_ptr->px);
    wr_byte(p_ptr->cur_map_hgt);
    wr_byte(p_ptr->cur_map_wid);
    log_trace(
        "[save:%06u] Dungeon header written: depth=%d, py=%d, px=%d, hgt=%d, wid=%d",
        (unsigned)save_byte_offset, p_ptr->depth, p_ptr->py, p_ptr->px,
        p_ptr->cur_map_hgt, p_ptr->cur_map_wid);

    log_trace("[save:%06u] === BEGIN CAVE_INFO RLE ===",
        (unsigned)save_byte_offset);
    count = 0;
    prev_char = 0;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            u16b info = (u16b)(cave_info[y][x] & IMPORTANT_FLAGS_16);
            tmp8u = (byte)(info & 0x00FF);

            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }
            else
            {
                count++;
            }
        }
    }

    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_INFO RLE ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN CAVE_INFO_HI RLE ===",
        (unsigned)save_byte_offset);
    {
        const u16b CAVE_INFO_HI_MAGIC = 0xC1F0;

        wr_u16b(CAVE_INFO_HI_MAGIC);

        count = 0;
        prev_char = 0;

        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                u16b info = (u16b)(cave_info[y][x] & IMPORTANT_FLAGS_16);
                tmp8u = (byte)((info >> 8) & 0x00FF);

                if ((tmp8u != prev_char) || (count == MAX_UCHAR))
                {
                    wr_byte((byte)count);
                    wr_byte((byte)prev_char);
                    prev_char = tmp8u;
                    count = 1;
                }
                else
                {
                    count++;
                }
            }
        }

        if (count)
        {
            wr_byte((byte)count);
            wr_byte((byte)prev_char);
        }
    }
    log_trace("[save:%06u] === END CAVE_INFO_HI RLE ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN CAVE_FEAT RLE ===",
        (unsigned)save_byte_offset);
    count = 0;
    prev_char = 0;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            tmp8u = cave_feat[y][x];

            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }
            else
            {
                count++;
            }
        }
    }

    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_FEAT RLE ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN CAVE_COLOR RLE ===",
        (unsigned)save_byte_offset);
    count = 0;
    prev_char = 0;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            tmp8u = cave_color[y][x];

            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }
            else
            {
                count++;
            }
        }
    }

    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_COLOR RLE ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN DOOR_CHOICES ===",
        (unsigned)save_byte_offset);
    {
        const u16b DOOR_CHOICES_MAGIC = 0xD00D;
        byte buf[64];
        int cap = (z_info && z_info->style_max > 0) ? z_info->style_max : 0;
        if (cap > 64)
            cap = 64;
        styles_copy_level_door_choices(buf, cap);
        log_debug("Writing door-choices block: magic=0x%04X, len=%d",
            DOOR_CHOICES_MAGIC, cap);
        wr_u16b(DOOR_CHOICES_MAGIC);
        wr_byte((byte)cap);
        for (int choice_idx = 0; choice_idx < cap; ++choice_idx)
            wr_byte(buf[choice_idx]);
    }
    log_trace("[save:%06u] === END DOOR_CHOICES ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] Compacting objects and monsters",
        (unsigned)save_byte_offset);
    compact_objects(0);
    compact_monsters(0);

    log_trace("[save:%06u] === BEGIN OBJECTS ===", (unsigned)save_byte_offset);
    if (o_max == 0)
    {
        log_warn("o_max was 0; clamping to 1 to avoid invalid object count");
        wr_u16b(1);
    }
    else
    {
        wr_u16b(o_max);
    }
    log_debug("Writing %d objects to savefile (o_max=%u)",
        o_max ? (o_max - 1) : 0, (unsigned)o_max);

    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        wr_item(o_ptr);
    }
    log_trace("[save:%06u] === END OBJECTS ===", (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN MONSTERS ===",
        (unsigned)save_byte_offset);
    wr_u16b(mon_max);
    log_debug("Writing %d monsters to savefile", mon_max - 1);
    log_live_special_vault_only_monsters("save wr_dungeon");

    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        wr_monster(m_ptr);
    }
    log_trace("[save:%06u] === END MONSTERS ===", (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN WANDERING MONSTERS ===",
        (unsigned)save_byte_offset);
    for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
    {
        wr_byte(flow_center_y[i]);
        wr_byte(flow_center_x[i]);
        wr_s16b(wandering_pause[i]);
    }
    log_trace("[save:%06u] === END WANDERING MONSTERS ===",
        (unsigned)save_byte_offset);

    log_debug("Dungeon data write completed - %d objects, %d monsters",
        o_max - 1, mon_max - 1);
    log_trace("[save:%06u] === END DUNGEON ===", (unsigned)save_byte_offset);
}
