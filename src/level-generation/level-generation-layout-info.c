/* File: level-generation-layout-info.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

void level_partition_meta_get(partition_meta_save* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    /* Populate metadata if this is a loaded level. */
    if (current_partition_rows <= 0 || current_partition_cols <= 0
        || current_partition_count <= 0)
    {
        (void)partition_mode_for_point(p_ptr->py, p_ptr->px);
    }

    out->grid_rows = (s16b)current_partition_rows;
    out->grid_cols = (s16b)current_partition_cols;
    out->partition_count = (s16b)current_partition_count;

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->modes[i] = (byte)current_partition_modes[i];

    for (int i = 0; i < PARTITION_META_MAX; ++i)
        out->big_cave_types[i] = (byte)current_partition_big_cave_types[i];
}

void level_partition_meta_set(const partition_meta_save* in)
{
    if (!in)
        return;

    int rows = in->grid_rows;
    int cols = in->grid_cols;
    int count = in->partition_count;

    if (rows <= 0 || cols <= 0 || count <= 0 || count > PARTITION_META_MAX
        || rows * cols != count)
    {
        current_partition_rows = 0;
        current_partition_cols = 0;
        current_partition_count = 0;
        reset_partition_population_metadata();
        for (int i = 0; i < PARTITION_META_MAX; ++i)
        {
            current_partition_modes[i] = QUAD_MODE_ROOMY;
            current_partition_densities[i] = DENSITY_NORMAL;
            current_partition_big_cave_types[i] = BIG_CAVE_NONE;
        }
        return;
    }

    remember_partition_grid(rows, cols, count);
    for (int i = 0; i < PARTITION_META_MAX; ++i)
    {
        quadrant_mode_t mode = QUAD_MODE_ROOMY;
        big_cave_type_t cave_type = BIG_CAVE_NONE;

        if (i < count)
        {
            byte raw = in->modes[i];

            if (raw <= QUAD_MODE_BIG_CAVE)
                mode = (quadrant_mode_t)raw;

            if (mode == QUAD_MODE_BIG_CAVE)
            {
                byte raw_type = in->big_cave_types[i];

                if (raw_type > BIG_CAVE_NONE && raw_type < BIG_CAVE_TYPE_MAX)
                    cave_type = (big_cave_type_t)raw_type;
            }
        }

        current_partition_modes[i] = mode;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = cave_type;
    }
}

void level_layout_info_current(level_layout_info* out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->map_wid = p_ptr->cur_map_wid;
    out->map_hgt = p_ptr->cur_map_hgt;
    out->partition_rows = current_partition_rows;
    out->partition_cols = current_partition_cols;
    out->partition_count = current_partition_count;

    int area_by_kind[LEVEL_PART_MAX] = {0};

    for (int i = 0; i < current_partition_count; ++i)
    {
        level_partition_kind kind =
            partition_kind_from_mode(current_partition_modes[i]);
        int y1 = 0, y2 = 0, x1 = 0, x2 = 0;
        int area = 0;

        if (compute_partition_bounds(
                i, current_partition_rows, current_partition_cols,
                &y1, &y2, &x1, &x2))
        {
            area = (y2 - y1 + 1) * (x2 - x1 + 1);
        }

        if (kind == LEVEL_PART_LABYRINTH)
            out->labyrinth_parts++;
        else if (kind == LEVEL_PART_BIG_CAVE)
            out->big_cave_parts++;
        else if (kind == LEVEL_PART_CHASM)
            out->chasm_parts++;

        if (kind > LEVEL_PART_NONE && kind < LEVEL_PART_MAX)
            area_by_kind[kind] += area;
    }

    {
        const level_partition_kind preference[] = {LEVEL_PART_LABYRINTH,
            LEVEL_PART_BIG_CAVE, LEVEL_PART_CHASM, LEVEL_PART_RUINED,
            LEVEL_PART_CAVEY, LEVEL_PART_ROOMY};
        int dominant_area = 0;
        level_partition_kind dominant_kind = LEVEL_PART_NONE;

        for (size_t i = 0; i < N_ELEMENTS(preference); ++i)
        {
            level_partition_kind kind = preference[i];
            int area = area_by_kind[kind];

            if (area > dominant_area)
            {
                dominant_area = area;
                dominant_kind = kind;
            }
        }

        out->dominant_kind = dominant_kind;
    }
}
