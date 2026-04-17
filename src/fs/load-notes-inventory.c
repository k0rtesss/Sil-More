/* File: load-notes-inventory.c */

#include "angband.h"
#include "fs/load-internal.h"
#include "log/log.h"
#include "runtime/runtime-cli.h"
#include <string.h>

bool rd_notes(void)
{
    int alive = (!p_ptr->is_dead || runtime_cli_wizard());
    char tmpstr[100];
    int i;

    for (i = 0; i < NOTES_LENGTH; i++)
        notes_buffer[i] = '\0';

    if (alive)
    {
        while (true)
        {
            if (!rd_string(tmpstr, sizeof(tmpstr)))
                return (-1);
            if (strstr(tmpstr, NOTES_MARK))
                break;
            SDL_strlcat(
                notes_buffer, format("%s\n", tmpstr), sizeof(notes_buffer));
        }
    }
    else
    {
        while (true)
        {
            if (!rd_string(tmpstr, sizeof(tmpstr)))
                return (-1);
            if (strstr(tmpstr, NOTES_MARK))
                break;
        }
    }

    return 0;
}

errr rd_inventory(void)
{
    int slot = 0;

    object_type* i_ptr;
    object_type object_type_body;

    log_debug("Loading smithing object and player inventory");
    log_trace("[load:%06u] === BEGIN SMITHING ITEM ===",
        (unsigned)load_byte_offset);

    object_wipe(smith_o_ptr);

    if (rd_item(smith_o_ptr))
    {
        note("Error reading smithing item");
        return (-1);
    }
    log_trace("[load:%06u] === END SMITHING ITEM ===",
        (unsigned)load_byte_offset);

    log_trace("[load:%06u] === BEGIN INVENTORY ===",
        (unsigned)load_byte_offset);
    while (1)
    {
        u16b n;

        rd_u16b(&n);
        if (load_read_failed)
        {
            note("Error reading inventory index");
            return (-1);
        }

        if (n == 0xFFFF)
        {
            log_trace("[load:%06u] Found inventory sentinel 0xFFFF",
                (unsigned)(load_byte_offset - 2));
            break;
        }

        log_trace("[load:%06u] Loading inventory slot %u",
            (unsigned)(load_byte_offset - 2), (unsigned)n);

        i_ptr = &object_type_body;
        object_wipe(i_ptr);

        if (rd_item(i_ptr))
        {
            log_warn("Error reading inventory item");
            note("Error reading item");
            return (-1);
        }

        if (!i_ptr->k_idx)
            return (-1);

        if (n >= INVEN_TOTAL)
            return (-1);

        if (n >= INVEN_WIELD)
        {
            object_copy(&inventory[n], i_ptr);

            if (i_ptr->tval == TV_STAFF)
            {
                log_debug(
                    "Loaded equipped staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                    n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            p_ptr->equip_cnt++;
        }
        else if (p_ptr->inven_cnt == INVEN_PACK)
        {
            note("Too many items in the inventory!");
            return (-1);
        }
        else
        {
            n = slot++;
            object_copy(&inventory[n], i_ptr);

            if (i_ptr->tval == TV_STAFF)
            {
                log_debug(
                    "Loaded pack staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                    n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            p_ptr->inven_cnt++;
        }
    }
    log_trace("[load:%06u] === END INVENTORY ===", (unsigned)load_byte_offset);

    log_debug("Inventory loaded: %d items carried, %d items equipped",
        p_ptr->inven_cnt, p_ptr->equip_cnt);

    log_trace("[load:%06u] === BEGIN SUPPLIES ===",
        (unsigned)load_byte_offset);
    supplies_reset_store();

    u16b supply_count = 0;
    u16b supply_marker = 0;
    rd_u16b(&supply_marker);
    if (load_read_failed)
    {
        supplies_set_allow_overflow(false);
        note("Error reading supply count");
        return (-1);
    }
    if (supply_marker == SAVEFILE_SUPPLY_BLOCK_MAGIC)
    {
        rd_u16b(&supply_count);
        if (load_read_failed)
        {
            supplies_set_allow_overflow(false);
            note("Error reading supply count");
            return (-1);
        }
    }
    else
    {
        supply_count = supply_marker;
    }
    log_debug("Loading %u supply entries", (unsigned)supply_count);
    supplies_set_allow_overflow(true);
    for (u16b si = 0; si < supply_count; si++)
    {
        object_type supply;
        object_wipe(&supply);
        if (rd_item(&supply))
        {
            log_warn("Error reading supply entry");
            note("Error reading supplies");
            supplies_set_allow_overflow(false);
            return (-1);
        }

        s32b stored_units = 0;
        rd_s32b(&stored_units);
        if (load_read_failed)
        {
            supplies_set_allow_overflow(false);
            note("Error reading supply data");
            return (-1);
        }

        if (supply.tval == TV_GEM)
        {
            int count = (int)stored_units;
            if (count <= 0)
                count = supply.number;
            if (count < 0)
                count = 0;

            supply.pval = 0;
            supply.ident &= ~(IDENT_EMPTY);

            int stack_limit = object_stack_limit(&supply);
            while (count > 0)
            {
                int chunk = MIN(count, stack_limit);
                if (chunk <= 0)
                    break;
                object_type part;
                object_copy(&part, &supply);
                part.number = (byte)chunk;
                count -= chunk;
                supplies_absorb_object(&part);
            }
            continue;
        }

        if (supply.k_idx)
            supplies_absorb_object(&supply);
    }
    supplies_set_allow_overflow(false);

    log_trace("[load:%06u] === END SUPPLIES ===", (unsigned)load_byte_offset);

    supplies_ingest_pack();

    return (0);
}
