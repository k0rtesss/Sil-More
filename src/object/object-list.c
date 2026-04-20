/* File: object-list.c */
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

#include "angband.h"
#include "log/log.h"

/*
 * Excise a dungeon object from any stacks
 */
void excise_object_idx(int o_idx)
{
    object_type* j_ptr;

    s16b this_o_idx, next_o_idx = 0;

    s16b prev_o_idx = 0;

    /* Object */
    j_ptr = &o_list[o_idx];

    /* Monster */
    if (j_ptr->held_m_idx)
    {
        monster_type* m_ptr;

        /* Monster */
        m_ptr = &mon_list[j_ptr->held_m_idx];

        /* Scan all objects in the grid */
        for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
             this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Done */
            if (this_o_idx == o_idx)
            {
                /* No previous */
                if (prev_o_idx == 0)
                {
                    /* Remove from list */
                    m_ptr->hold_o_idx = next_o_idx;
                }

                /* Real previous */
                else
                {
                    object_type* i_ptr;

                    /* Previous object */
                    i_ptr = &o_list[prev_o_idx];

                    /* Remove from list */
                    i_ptr->next_o_idx = next_o_idx;
                }

                /* Forget next pointer */
                o_ptr->next_o_idx = 0;

                /* Done */
                break;
            }

            /* Save prev_o_idx */
            prev_o_idx = this_o_idx;
        }
    }

    /* Dungeon */
    else
    {
        int y = j_ptr->iy;
        int x = j_ptr->ix;

        /* Scan all objects in the grid */
        for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[this_o_idx];

            /* Get the next object */
            next_o_idx = o_ptr->next_o_idx;

            /* Done */
            if (this_o_idx == o_idx)
            {
                /* No previous */
                if (prev_o_idx == 0)
                {
                    /* Remove from list */
                    cave_o_idx[y][x] = next_o_idx;
                }

                /* Real previous */
                else
                {
                    object_type* i_ptr;

                    /* Previous object */
                    i_ptr = &o_list[prev_o_idx];

                    /* Remove from list */
                    i_ptr->next_o_idx = next_o_idx;
                }

                /* Forget next pointer */
                o_ptr->next_o_idx = 0;

                /* Done */
                break;
            }

            /* Save prev_o_idx */
            prev_o_idx = this_o_idx;
        }
    }
}

/*
 * Delete a dungeon object
 *
 * Handle "stacks" of objects correctly.
 */
void delete_object_idx(int o_idx)
{
    object_type* j_ptr;

    /* Excise */
    excise_object_idx(o_idx);

    /* Object */
    j_ptr = &o_list[o_idx];

    /* Dungeon floor */
    if (!(j_ptr->held_m_idx))
    {
        /* Visual update */
        dungeon_mark_map_for_redraw();
    }

    /* Wipe the object */
    object_wipe(j_ptr);

    /* Count objects */
    o_cnt--;
}

/*
 * Deletes all objects at given location
 */
void delete_object(int y, int x)
{
    s16b this_o_idx, next_o_idx = 0;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Wipe the object */
        object_wipe(o_ptr);

        /* Count objects */
        o_cnt--;
    }

    /* Objects are gone */
    cave_o_idx[y][x] = 0;

    /* Visual update */
    dungeon_mark_map_for_redraw();
}

/*
 * Move an object from index i1 to index i2 in the object list
 */
static void compact_objects_aux(int i1, int i2)
{
    int i;

    object_type* o_ptr;

    /* Do nothing */
    if (i1 == i2)
        return;

    /* Repair objects */
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip "dead" objects */
        if (!o_ptr->k_idx)
            continue;

        /* Repair "next" pointers */
        if (o_ptr->next_o_idx == i1)
        {
            /* Repair */
            o_ptr->next_o_idx = i2;
        }
    }

    /* Get the object */
    o_ptr = &o_list[i1];

    /* Monster */
    if (o_ptr->held_m_idx)
    {
        monster_type* m_ptr;

        /* Get the monster */
        m_ptr = &mon_list[o_ptr->held_m_idx];

        /* Repair monster */
        if (m_ptr->hold_o_idx == i1)
        {
            /* Repair */
            m_ptr->hold_o_idx = i2;
        }
    }

    /* Dungeon */
    else
    {
        int y, x;

        /* Get location */
        y = o_ptr->iy;
        x = o_ptr->ix;

        /* Repair grid */
        if (cave_o_idx[y][x] == i1)
        {
            /* Repair */
            cave_o_idx[y][x] = i2;
        }
    }

    /* Hack -- move object */
    memcpy(&o_list[i2], &o_list[i1], sizeof(object_type));

    /* Hack -- wipe hole */
    object_wipe(o_ptr);
}

/*
 * Compact and Reorder the object list
 *
 * This function can be very dangerous, use with caution!
 *
 * When actually "compacting" objects, we base the saving throw on a
 * combination of object level, distance from player, and current
 * "desperation".
 *
 * After "compacting" (if needed), we "reorder" the objects into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */
void compact_objects(int size)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, y, x, num, cnt;

    int cur_lev, cur_dis, chance;

    /* Compact */
    if (size)
    {
        /* Message */
        msg_print("Compacting objects...");

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Window stuff */
        p_ptr->window |= (PW_OVERHEAD);
    }

    /* Compact at least 'size' objects */
    for (num = 0, cnt = 1; num < size; cnt++)
    {
        bool saw_non_artefact = false;

        /* Get more vicious each iteration */
        cur_lev = 5 * cnt;

        /* Get closer each iteration */
        cur_dis = 5 * (20 - cnt);

        /* Examine the objects */
        for (i = 1; i < o_max; i++)
        {
            object_type* o_ptr = &o_list[i];

            object_kind* k_ptr = &k_info[o_ptr->k_idx];

            /* Skip dead objects */
            if (!o_ptr->k_idx)
                continue;

            /* Never compact artefacts; dropped artefacts must not disappear. */
            if (artefact_p(o_ptr))
                continue;

            saw_non_artefact = true;

            /* Hack -- High level objects start out "immune" */
            if (k_ptr->level > cur_lev)
                continue;

            /* Monster */
            if (o_ptr->held_m_idx)
            {
                monster_type* m_ptr;

                /* Get the monster */
                m_ptr = &mon_list[o_ptr->held_m_idx];

                /* Get the location */
                y = m_ptr->fy;
                x = m_ptr->fx;

                /* Monsters protect their objects */
                if (percent_chance(90))
                    continue;
            }

            /* Dungeon */
            else
            {
                /* Get the location */
                y = o_ptr->iy;
                x = o_ptr->ix;
            }

            /* Nearby objects start out "immune" */
            if ((cur_dis > 0) && (distance(py, px, y, x) < cur_dis))
                continue;

            /* Saving throw */
            chance = 90;

            /* Apply the saving throw */
            if (percent_chance(chance))
                continue;

            /* Delete the object */
            delete_object_idx(i);

            /* Count it */
            num++;
        }

        /* Avoid looping forever when only artefacts remain. */
        if (!saw_non_artefact)
            break;
    }

    /* Excise dead objects (backwards!) */
    for (i = o_max - 1; i >= 1; i--)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip real objects */
        if (o_ptr->k_idx)
            continue;

        /* Move last object into open hole */
        compact_objects_aux(o_max - 1, i);

        /* Compress "o_max" */
        o_max--;
    }
}

/*
 * Delete all the items when player leaves the level
 *
 * Note -- we do NOT visually reflect these (irrelevant) changes
 *
 * Hack -- we clear the "cave_o_idx[y][x]" field for every grid,
 * and the "m_ptr->next_o_idx" field for every monster, since
 * we know we are clearing every object.  Technically, we only
 * clear those fields for grids/monsters containing objects,
 * and we clear it once for every such object.
 */
void wipe_o_list(void)
{
    int i;

    /* Delete the existing objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Mega-Hack -- preserve artefacts */
        // Sil-y: no longer preserving artefacts
        // if (!character_dungeon || (PRESERVE_MODE))
        //{
        //	/* Hack -- Preserve unknown artefacts */
        //	if (artefact_p(o_ptr) && !object_known_p(o_ptr))
        //	{
        //		/* Mega-Hack -- Preserve the artefact */
        //		a_info[o_ptr->name1].cur_num = 0;
        //	}
        //}

        /* Monster */
        if (o_ptr->held_m_idx)
        {
            monster_type* m_ptr;

            /* Monster */
            m_ptr = &mon_list[o_ptr->held_m_idx];

            /* Hack -- see above */
            m_ptr->hold_o_idx = 0;
        }

        /* Dungeon */
        else
        {
            /* Get the location */
            int y = o_ptr->iy;
            int x = o_ptr->ix;

            /* Hack -- see above */
            cave_o_idx[y][x] = 0;
        }

        /*Wipe the randart if necessary*/
        if (o_ptr->name1)
            artefact_wipe(o_ptr->name1);

        /* Wipe the object */
        memset(o_ptr, 0, sizeof(object_type));
    }

    /* Reset "o_max" */
    o_max = 1;

    /* Reset "o_cnt" */
    o_cnt = 0;
}

/*
 * Get and return the index of a "free" object.
 *
 * This routine should almost never fail, but in case it does,
 * we must be sure to handle "failure" of this routine.
 */
s16b o_pop(void)
{
    int attempt;
    int i;

    for (attempt = 0; attempt < 2; attempt++)
    {
        /* Initial allocation */
        if (o_max < z_info->o_max)
        {
            /* Get next space */
            i = o_max;

            /* Expand object array */
            o_max++;

            /* Count objects */
            o_cnt++;

            /* Use this object */
            return (i);
        }

        /* Recycle dead objects */
        for (i = 1; i < o_max; i++)
        {
            object_type* o_ptr;

            /* Get the object */
            o_ptr = &o_list[i];

            /* Skip live objects */
            if (o_ptr->k_idx)
                continue;

            /* Count objects */
            o_cnt++;

            /* Use this object */
            return (i);
        }

        /* Make space by compacting ordinary objects, then retry once. */
        if (attempt == 0)
            compact_objects(1);
    }

    /* Warn the player (except during dungeon creation) */
    if (character_dungeon)
        msg_print("Too many objects!");

    /* Oops */
    return (0);
}

/*
 * Get the first object at a dungeon location
 * or NULL if there isn't one.
 */
object_type* get_first_object(int y, int x)
{
    s16b o_idx = cave_o_idx[y][x];

    if (o_idx)
        return (&o_list[o_idx]);

    /* No object */
    return (NULL);
}

/*
 * Get the next object in a stack or
 * NULL if there isn't one.
 */
object_type* get_next_object(const object_type* o_ptr)
{
    if (o_ptr->next_o_idx)
        return (&o_list[o_ptr->next_o_idx]);

    /* No more objects */
    return (NULL);
}

/*
 * Check ground artifacts within 33-cell radius of player and mark as seen
 * Only checks changed positions (tracked via lastpx/lastpy) for efficiency
 */
void check_artifact_visibility(void)
{
    int x, y;
    int px = p_ptr->px;
    int py = p_ptr->py;
    static int last_px = -1;
    static int last_py = -1;
    
    /* First call - mark everything in radius */
    if (last_px < 0 || last_py < 0)
    {
        for (y = py - 33; y <= py + 33; y++)
        {
            for (x = px - 33; x <= px + 33; x++)
            {
                if (!in_bounds(y, x)) continue;
                
                /* Check objects at this location */
                s16b this_o_idx = cave_o_idx[y][x];
                while (this_o_idx)
                {
                    object_type* o_ptr = &o_list[this_o_idx];
                    
                    /* If artifact and not already seen */
                    if (o_ptr->name1 && !(o_ptr->ident & IDENT_ARTIFACT_SEEN))
                    {
                        /* Mark as seen */
                        o_ptr->ident |= IDENT_ARTIFACT_SEEN;
                        log_trace("Artifact %d marked as seen at (%d,%d)", o_ptr->name1, y, x);
                    }
                    
                    this_o_idx = o_ptr->next_o_idx;
                }
            }
        }
        last_px = px;
        last_py = py;
        return;
    }
    
    /* Player moved - check only new cells that entered the radius */
    int dx = px - last_px;
    int dy = py - last_py;
    
    if (dx == 0 && dy == 0) return; /* No movement */
    
    /* Check cells that entered the 66x66 radius */
    for (y = py - 33; y <= py + 33; y++)
    {
        for (x = px - 33; x <= px + 33; x++)
        {
            if (!in_bounds(y, x)) continue;
            
            /* Only check if this cell wasn't in the old radius */
            int old_dx = x - last_px;
            int old_dy = y - last_py;
            if (old_dx >= -33 && old_dx <= 33 && old_dy >= -33 && old_dy <= 33)
                continue; /* Was already checked */
            
            /* Check objects at this new location */
            s16b this_o_idx = cave_o_idx[y][x];
            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                
                /* If artifact and not already seen */
                if (o_ptr->name1 && !(o_ptr->ident & IDENT_ARTIFACT_SEEN))
                {
                    /* Mark as seen */
                    o_ptr->ident |= IDENT_ARTIFACT_SEEN;
                    log_trace("Artifact %d marked as seen at (%d,%d)", o_ptr->name1, y, x);
                }
                
                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
    
    last_px = px;
    last_py = py;
}
