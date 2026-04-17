/* File: save-notes-inventory.c */

#include "angband.h"
#include "fs/save-internal.h"
#include "log/log.h"

void wr_notes(void)
{
    char end_note[80];
    char tmpstr[100];
    char ch;
    bool done = false;

    int i = 0;
    int j = 0;

    while (!done)
    {
        j = 0;

        while (true)
        {
            ch = notes_buffer[i];

            tmpstr[j] = ch;

            i++;
            j++;

            if (ch == '\n')
            {
                tmpstr[j - 1] = '\0';

                wr_string(tmpstr);
                break;
            }

            if (ch == '\0')
            {
                done = true;
                break;
            }
        }
    }

    SDL_strlcpy(end_note, NOTES_MARK, sizeof(end_note));
    wr_string(end_note);
}

void save_write_inventory(void)
{
    int i;

    log_debug("Writing smithing item");
    log_trace("[save:%06u] === BEGIN SMITHING ITEM ===",
        (unsigned)save_byte_offset);
    wr_item(smith_o_ptr);
    log_trace("[save:%06u] === END SMITHING ITEM ===",
        (unsigned)save_byte_offset);

    log_debug("Writing player inventory");
    log_trace("[save:%06u] === BEGIN INVENTORY ===",
        (unsigned)save_byte_offset);
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        log_trace("[save:%06u] Writing inventory slot %d",
            (unsigned)save_byte_offset, i);
        wr_u16b((u16b)i);

        log_trace("Writing inventory item %d", i);
        wr_item(o_ptr);
    }

    log_trace("[save:%06u] Writing inventory sentinel 0xFFFF",
        (unsigned)save_byte_offset);
    wr_u16b(0xFFFF);
    log_trace("[save:%06u] === END INVENTORY ===",
        (unsigned)save_byte_offset);

    log_trace("[save:%06u] === BEGIN SUPPLIES ===",
        (unsigned)save_byte_offset);
    {
        wr_u16b(SAVEFILE_SUPPLY_BLOCK_MAGIC);
        u16b supply_count = (u16b)supplies_entry_count();
        wr_u16b(supply_count);
        log_debug("Writing %u supply entries", (unsigned)supply_count);
        for (u16b si = 0; si < supply_count; si++)
        {
            object_type* supply_obj = supplies_entry_at(si);
            s32b stored_units = 0;
            if (supply_obj && supply_obj->k_idx)
            {
                log_trace("[save:%06u] Writing supply entry %u",
                    (unsigned)save_byte_offset, (unsigned)si);
                wr_item(supply_obj);
                stored_units = supplies_entry_units(si);
            }
            else
            {
                object_type blank;
                object_wipe(&blank);
                log_trace("[save:%06u] Writing blank supply entry %u",
                    (unsigned)save_byte_offset, (unsigned)si);
                wr_item(&blank);
            }
            wr_s32b(stored_units);
        }
    }
    log_trace("[save:%06u] === END SUPPLIES ===", (unsigned)save_byte_offset);
}
