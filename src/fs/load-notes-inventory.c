/* File: fs/load-notes-inventory.c -- carved from load.c (shares state via fs/load-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "player/killer.h"
#include "score/score_guid.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include "metarun.h"
#include "fs/load-internal.h"

/*
 * Read the notes. Every new savefile has at least NOTES_MARK.
 */
bool rd_notes(void)
{
    bool alive = (!p_ptr->is_dead || arg_wizard);
    size_t used = 0;

    memset(notes_buffer, 0, sizeof(notes_buffer));

    while (true)
    {
        size_t start = used;
        size_t length = 0;
        bool is_marker = true;

        /* Notes are NUL-terminated records.  Read directly into the bounded
         * buffer so a long record cannot overflow or silently lose its tail. */
        while (true)
        {
            byte ch;
            u32b before = load_byte_offset;
            rd_byte(&ch);
            if (load_byte_offset == before)
            {
                note("Unexpected end of savefile while reading notes");
                return true;
            }
            if (!ch)
                break;

            if (length >= sizeof(NOTES_MARK) - 1 || ch != NOTES_MARK[length])
                is_marker = false;
            length++;
            if (alive && used < sizeof(notes_buffer) - 1)
                notes_buffer[used++] = (char)ch;
        }

        /* A player's note may contain the marker as part of its text. */
        if (is_marker && length == sizeof(NOTES_MARK) - 1)
        {
            notes_buffer[start] = '\0';
            return false;
        }
        if (alive && used < sizeof(notes_buffer) - 1)
            notes_buffer[used++] = '\n';
        notes_buffer[used] = '\0';
    }
}


/* Preserve every arrow when importing the pre-0.9.7.9 Quiver representation.
 * Old stacks may exceed the current mixed Quiver's capacity. */
static errr migrate_legacy_quiver(void)
{
    if (inventory[INVEN_QUIVER1].k_idx
        && inventory[INVEN_QUIVER1].tval == TV_ARROW)
    {
        object_type arrow;
        object_copy(&arrow, &inventory[INVEN_QUIVER1]);
        /* Remove the old slot before computing space: it is included in the
         * Quiver count while a legacy save is being migrated. */
        object_wipe(&inventory[INVEN_QUIVER1]);
        if (p_ptr->equip_cnt > 0)
            p_ptr->equip_cnt--;
        (void)player_quiver_absorb_arrow(&arrow);
        if (arrow.number > 0)
        {
            arrow.storage = OBJECT_STORAGE_PACK;
            if (!player_carried_extra_load(&arrow))
            {
                note("Unable to preserve excess legacy Quiver arrows");
                return -1;
            }
        }
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx || inventory[i].tval != TV_ARROW
            || inventory[i].pickup_slot != INVEN_QUIVER1)
        {
            continue;
        }

        object_type arrow;
        object_copy(&arrow, &inventory[i]);
        int placed = player_quiver_absorb_arrow(&arrow);
        inventory[i].storage = OBJECT_STORAGE_PACK;
        inventory[i].pickup = false;
        inventory[i].pickup_slot = -1;
        if (placed > 0)
        {
            inven_item_increase(i, -placed);
            inven_item_optimize(i);
            if (!arrow.number)
                i--;
        }
    }
    return 0;
}

/*
 * Read the player inventory (and the smithing object)
 *
 * Note that the inventory is "re-sorted" later by "dungeon()".
 */
errr rd_inventory(void)
{
    int slot = 0;

    object_type* i_ptr;
    object_type object_type_body;

    log_debug("Loading smithing object and player inventory");
    log_trace("[load:%06u] === BEGIN SMITHING ITEM ===", (unsigned)load_byte_offset);

    /*
     * Start from a clean inventory.  The metarun autoload reads several
     * savefiles through this same path into the shared inventory[] globals,
     * and this function only ever writes the slots a savefile actually
     * stored.  Without clearing first, a slot that the current character
     * left empty (e.g. an active throwing stack emptied by its last throw) keeps
     * whatever a previously loaded character had there, duplicating the
     * thrown weapon (one phantom in the active hand, one real copy on the floor).
     */
    for (int wipe_slot = 0; wipe_slot < INVEN_TOTAL; wipe_slot++)
        object_wipe(&inventory[wipe_slot]);
    p_ptr->inven_cnt = 0;
    p_ptr->equip_cnt = 0;
    player_carried_extra_reset_store();
    player_quiver_reset_store();

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);

    /* Read the smithing object */
    if (rd_item(smith_o_ptr))
    {
        note("Error reading smithing item");
        return (-1);
    }
    log_trace("[load:%06u] === END SMITHING ITEM ===", (unsigned)load_byte_offset);

    log_trace("[load:%06u] === BEGIN INVENTORY ===", (unsigned)load_byte_offset);
    /* Read until done */
    while (1)
    {
        u16b n;

        /* Get the next item index */
        rd_u16b(&n);

        /* Nope, we reached the end */
        if (n == 0xFFFF)
        {
            log_trace("[load:%06u] Found inventory sentinel 0xFFFF", (unsigned)(load_byte_offset - 2));
            break;
        }

        log_trace("[load:%06u] Loading inventory slot %u", (unsigned)(load_byte_offset - 2), (unsigned)n);

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Read the item */
        if (rd_item(i_ptr))
        {
            log_warn("Error reading inventory item");
            note("Error reading item");
            return (-1);
        }

        /* Hack -- verify item */
        if (!i_ptr->k_idx)
            return (-1);

        if (i_ptr->number <= 0)
        {
            log_warn("Skipping stale empty inventory slot %u while loading",
                (unsigned)n);
            continue;
        }

        /* Verify slot */
        if (n >= INVEN_TOTAL)
            return (-1);

        /* Wield equipment */
        if (n >= INVEN_WIELD)
        {
            /* Copy object */
            object_copy(&inventory[n], i_ptr);

            /* Log a staff found in the retired equipment range. */
            if (i_ptr->tval == TV_STAFF)
            {
                log_debug("Loaded legacy staff slot %d: k_idx=%d sval=%d pval=%d number=%d",
                          n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            /* One more item */
            p_ptr->equip_cnt++;
        }

        /* Warning -- backpack is full */
        else if (p_ptr->inven_cnt == INVEN_PACK)
        {
            /* Oops */
            note("Too many items in the inventory!");

            /* Fail */
            return (-1);
        }

        /* Carry inventory */
        else
        {
            /* Get a slot */
            n = slot++;

            /* Copy object */
            object_copy(&inventory[n], i_ptr);

            /* Log pack staff loading */
            if (i_ptr->tval == TV_STAFF)
            {
                log_debug("Loaded pack staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                          n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            /* One more item */
            p_ptr->inven_cnt++;
        }
    }
    log_trace("[load:%06u] === END INVENTORY ===", (unsigned)load_byte_offset);

    log_debug("Inventory loaded: %d items carried, %d items equipped", p_ptr->inven_cnt, p_ptr->equip_cnt);

    if (savefile_version_at_least(0, 9, 7, 12))
    {
        u16b extra_magic = 0;
        u32b extra_count = 0;

        log_trace("[load:%06u] === BEGIN CARRIED EXTRA ===",
            (unsigned)load_byte_offset);
        rd_u16b(&extra_magic);
        rd_u32b(&extra_count);
        if (extra_magic != SAVEFILE_CARRIED_EXTRA_BLOCK_MAGIC
            || extra_count >= (u32b)(QUIVER_INDEX - CARRIED_EXTRA_INDEX))
        {
            log_warn("Invalid expandable carried block marker/count 0x%04X/%u",
                (unsigned)extra_magic, (unsigned)extra_count);
            note("Error reading expandable carried inventory");
            return (-1);
        }

        for (u32b i = 0; i < extra_count; i++)
        {
            object_type carried;

            object_wipe(&carried);
            if (rd_item(&carried) || !carried.k_idx || carried.number <= 0
                || !player_carried_extra_load(&carried))
            {
                note("Error reading expandable carried inventory entry");
                return (-1);
            }
        }
        log_trace("[load:%06u] === END CARRIED EXTRA ===",
            (unsigned)load_byte_offset);
    }

    if (savefile_version_at_least(0, 9, 7, 9))
    {
        u16b quiver_magic = 0;
        u16b quiver_count = 0;
        s16b selected_arrow = 0;

        rd_u16b(&quiver_magic);
        rd_u16b(&quiver_count);
        if (savefile_version_at_least(0, 9, 7, 13))
            rd_s16b(&selected_arrow);
        if (quiver_magic != SAVEFILE_QUIVER_BLOCK_MAGIC
            || quiver_count > QUIVER_ARROW_CAPACITY)
        {
            log_warn("Invalid Quiver block marker/count 0x%04X/%u",
                (unsigned)quiver_magic, (unsigned)quiver_count);
            note("Error reading Quiver");
            return (-1);
        }

        for (u16b i = 0; i < quiver_count; i++)
        {
            object_type arrow;
            int original;

            object_wipe(&arrow);
            if (rd_item(&arrow) || arrow.tval != TV_ARROW || arrow.number <= 0)
            {
                note("Error reading Quiver arrow");
                return (-1);
            }
            original = arrow.number;
            if (player_quiver_absorb_arrow(&arrow) != original)
            {
                note("Too many arrows in Quiver");
                return (-1);
            }
        }
        player_quiver_restore_selected_arrow(selected_arrow);
    }
    else
    {
        /* Development saves also used marked Pack entries.  Keep any arrows
         * that do not fit in the new Quiver as ordinary carried arrows. */
        if (migrate_legacy_quiver())
            return -1;
    }

    log_trace("[load:%06u] === BEGIN SUPPLIES ===", (unsigned)load_byte_offset);
    supplies_reset_store();
    bool has_supply_block = false;

    if (savefile_version_at_least(0, 9, 6, 0))
    {
        u16b supply_magic = 0;
        rd_u16b(&supply_magic);
        if (supply_magic != SAVEFILE_SUPPLY_BLOCK_MAGIC)
        {
            log_warn("Invalid supplies block marker 0x%04X",
                (unsigned)supply_magic);
            note("Error reading supplies");
            return (-1);
        }
        has_supply_block = true;
    }
    else
    {
        has_supply_block = legacy_savefile_has_supply_block();
    }

    if (has_supply_block)
    {
        u16b supply_count = 0;
        rd_u16b(&supply_count);
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
    }
    else
    {
        log_debug("No legacy supplies block present; migration will rebuild supplies if needed");
    }

    supplies_set_allow_overflow(false);

    log_trace("[load:%06u] === END SUPPLIES ===", (unsigned)load_byte_offset);

    if (savefile_version_at_least(0, 9, 6, 0))
        supplies_ingest_pack();

    jewelry_presets_reset();
    if (savefile_version_at_least(0, 9, 6, 7))
    {
        u16b preset_magic = 0;
        byte preset_count = 0;

        log_trace("[load:%06u] === BEGIN JEWELRY PRESETS ===",
            (unsigned)load_byte_offset);
        rd_u16b(&preset_magic);
        if (preset_magic != SAVEFILE_JEWELRY_PRESET_BLOCK_MAGIC)
        {
            log_warn("Invalid jewelry preset block marker 0x%04X",
                (unsigned)preset_magic);
            note("Error reading jewelry presets");
            return (-1);
        }

        rd_byte(&preset_count);
        for (byte preset = 0; preset < preset_count; preset++)
        {
            byte is_set = 0;
            char preset_name[JEWELRY_PRESET_NAME_MAX];
            rd_byte(&is_set);
            preset_name[0] = '\0';
            if (savefile_version_at_least(0, 9, 7, 11))
            {
                rd_string(preset_name, sizeof(preset_name));
                if (preset < JEWELRY_PRESET_MAX)
                    jewelry_preset_set_name(preset, preset_name);
            }

            for (byte slot_idx = 0; slot_idx < JEWELRY_PRESET_SLOT_MAX;
                 slot_idx++)
            {
                object_type preset_obj;
                object_wipe(&preset_obj);

                if (rd_item(&preset_obj))
                {
                    log_warn("Error reading jewelry preset entry");
                    note("Error reading jewelry presets");
                    return (-1);
                }

                if (is_set && preset < JEWELRY_PRESET_MAX
                    && preset_obj.k_idx)
                {
                    jewelry_preset_set_object(preset, slot_idx, &preset_obj);
                }
            }
        }
        log_trace("[load:%06u] === END JEWELRY PRESETS ===",
            (unsigned)load_byte_offset);
    }

    /* Success */
    return (0);
}
