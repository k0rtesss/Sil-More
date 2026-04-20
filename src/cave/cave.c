/* File: cave/cave.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "app/app-session.h"
#include "cave/cave-internal.h"
#include "log/log.h"
#include "project-path.h"
#include "ui/colors.h"
/* Standard headers for utility functions used in this file */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>

int view_n = 0;
u16b* view_g;
int temp_n = 0;
u16b* temp_g;
byte* temp_y;
byte* temp_x;

u16b (*cave_info)[256];
byte (*cave_feat)[MAX_DUNGEON_WID];
byte (*cave_color)[MAX_DUNGEON_WID];
s16b (*cave_light)[MAX_DUNGEON_WID];
s16b (*cave_o_idx)[MAX_DUNGEON_WID];
s16b (*cave_m_idx)[MAX_DUNGEON_WID];

byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_when)[MAX_DUNGEON_WID];
int scent_when = 250;
byte flow_center_y[MAX_FLOWS];
byte flow_center_x[MAX_FLOWS];
byte update_center_y[MAX_FLOWS];
byte update_center_x[MAX_FLOWS];
s16b wandering_pause[MAX_FLOWS];
s16b image_count;
bool shimmer_objects;

bool player_suppresses_unseen_grid_info(void)
{
    return (!p_ptr->is_dead) && (p_ptr->rage || g_labyrinth_view_active);
}

bool grid_info_is_available(int y, int x)
{
    return !player_suppresses_unseen_grid_info() || player_can_see_bold(y, x);
}

bool random_stair_location(int* sy, int* sx)
{
    int stair_y[100];
    int stair_x[100];
    int stair_num = 0;
    int y, x;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_stair_bold(y, x))
            {
                stair_y[stair_num] = y;
                stair_x[stair_num] = x;
                if (stair_num < 99)
                    stair_num++;
            }
        }
    }

    if (stair_num == 0)
        return false;

    stair_num = rand_int(stair_num);
    *sy = stair_y[stair_num];
    *sx = stair_x[stair_num];

    return true;
}

/*
 * Maximum number of grids in a single octant
 */
#define VINFO_MAX_GRIDS 161

/*
 * Maximum number of slopes in a single octant
 */
#define VINFO_MAX_SLOPES 126

/*
 * Mask of bits used in a single octant
 */
#define VINFO_BITS_3 0x3FFFFFFF
#define VINFO_BITS_2 0xFFFFFFFF
#define VINFO_BITS_1 0xFFFFFFFF
#define VINFO_BITS_0 0xFFFFFFFF

/*
 * Forward declare
 */
typedef struct vinfo_type vinfo_type;

/*
 * The 'vinfo_type' structure
 */
struct vinfo_type
{
    s16b grid[8];

    /* LOS slopes (up to 128) */
    u32b bits_3;
    u32b bits_2;
    u32b bits_1;
    u32b bits_0;

    /* Index of the first LOF slope */
    byte slope_fire_index1;

    /* Index of the (possible) second LOF slope */
    byte slope_fire_index2;

    vinfo_type* next_0;
    vinfo_type* next_1;

    byte y;
    byte x;
    byte d;
    byte r;
};

/*
 * The array of "vinfo" objects, initialized by "vinfo_init()"
 */
static vinfo_type vinfo[VINFO_MAX_GRIDS];

/*
 * Slope scale factor
 */
#define SCALE 100000L

/*
 * The actual slopes (for reference)
 */

/* Bit :     Slope   Grids */
/* --- :     -----   ----- */
/*   0 :      2439      21 */
/*   1 :      2564      21 */
/*   2 :      2702      21 */
/*   3 :      2857      21 */
/*   4 :      3030      21 */
/*   5 :      3225      21 */
/*   6 :      3448      21 */
/*   7 :      3703      21 */
/*   8 :      4000      21 */
/*   9 :      4347      21 */
/*  10 :      4761      21 */
/*  11 :      5263      21 */
/*  12 :      5882      21 */
/*  13 :      6666      21 */
/*  14 :      7317      22 */
/*  15 :      7692      20 */
/*  16 :      8108      21 */
/*  17 :      8571      21 */
/*  18 :      9090      20 */
/*  19 :      9677      21 */
/*  20 :     10344      21 */
/*  21 :     11111      20 */
/*  22 :     12000      21 */
/*  23 :     12820      22 */
/*  24 :     13043      22 */
/*  25 :     13513      22 */
/*  26 :     14285      20 */
/*  27 :     15151      22 */
/*  28 :     15789      22 */
/*  29 :     16129      22 */
/*  30 :     17241      22 */
/*  31 :     17647      22 */
/*  32 :     17948      23 */
/*  33 :     18518      22 */
/*  34 :     18918      22 */
/*  35 :     20000      19 */
/*  36 :     21212      22 */
/*  37 :     21739      22 */
/*  38 :     22580      22 */
/*  39 :     23076      22 */
/*  40 :     23809      22 */
/*  41 :     24137      22 */
/*  42 :     24324      23 */
/*  43 :     25714      23 */
/*  44 :     25925      23 */
/*  45 :     26315      23 */
/*  46 :     27272      22 */
/*  47 :     28000      23 */
/*  48 :     29032      23 */
/*  49 :     29411      23 */
/*  50 :     29729      24 */
/*  51 :     30434      23 */
/*  52 :     31034      23 */
/*  53 :     31428      23 */
/*  54 :     33333      18 */
/*  55 :     35483      23 */
/*  56 :     36000      23 */
/*  57 :     36842      23 */
/*  58 :     37142      24 */
/*  59 :     37931      24 */
/*  60 :     38461      24 */
/*  61 :     39130      24 */
/*  62 :     39393      24 */
/*  63 :     40740      24 */
/*  64 :     41176      24 */
/*  65 :     41935      24 */
/*  66 :     42857      23 */
/*  67 :     44000      24 */
/*  68 :     44827      24 */
/*  69 :     45454      23 */
/*  70 :     46666      24 */
/*  71 :     47368      24 */
/*  72 :     47826      24 */
/*  73 :     48148      24 */
/*  74 :     48387      24 */
/*  75 :     51515      25 */
/*  76 :     51724      25 */
/*  77 :     52000      25 */
/*  78 :     52380      25 */
/*  79 :     52941      25 */
/*  80 :     53846      25 */
/*  81 :     54838      25 */
/*  82 :     55555      24 */
/*  83 :     56521      25 */
/*  84 :     57575      26 */
/*  85 :     57894      25 */
/*  86 :     58620      25 */
/*  87 :     60000      23 */
/*  88 :     61290      25 */
/*  89 :     61904      25 */
/*  90 :     62962      25 */
/*  91 :     63636      25 */
/*  92 :     64705      25 */
/*  93 :     65217      25 */
/*  94 :     65517      25 */
/*  95 :     67741      26 */
/*  96 :     68000      26 */
/*  97 :     68421      26 */
/*  98 :     69230      26 */
/*  99 :     70370      26 */
/* 100 :     71428      25 */
/* 101 :     72413      26 */
/* 102 :     73333      26 */
/* 103 :     73913      26 */
/* 104 :     74193      27 */
/* 105 :     76000      26 */
/* 106 :     76470      26 */
/* 107 :     77777      25 */
/* 108 :     78947      26 */
/* 109 :     79310      26 */
/* 110 :     80952      26 */
/* 111 :     81818      26 */
/* 112 :     82608      26 */
/* 113 :     84000      26 */
/* 114 :     84615      26 */
/* 115 :     85185      26 */
/* 116 :     86206      27 */
/* 117 :     86666      27 */
/* 118 :     88235      27 */
/* 119 :     89473      27 */
/* 120 :     90476      27 */
/* 121 :     91304      27 */
/* 122 :     92000      27 */
/* 123 :     92592      27 */
/* 124 :     93103      28 */
/* 125 :    100000      13 */

/*
 * Forward declare
 */
typedef struct vinfo_hack vinfo_hack;

/*
 * Temporary data used by "vinfo_init()"
 *
 *	- Number of line of sight slopes
 *
 *	- Slope values
 *
 *	- Slope range for each grid
 */
struct vinfo_hack
{
    int num_slopes;

    long slopes[VINFO_MAX_SLOPES];

    long slopes_min[MAX_SIGHT + 1][MAX_SIGHT + 1];
    long slopes_max[MAX_SIGHT + 1][MAX_SIGHT + 1];
};

/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static bool ang_sort_comp_hook_longs(const void* u, const void* v, int a, int b)
{
    long* x = (long*)(u);

    /* Unused parameter */
    (void)v;

    return (x[a] <= x[b]);
}

/*
 * Sorting hook -- comp function -- array of long's (see below)
 *
 * We use "u" to point to an array of long integers.
 */
static void ang_sort_swap_hook_longs(void* u, void* v, int a, int b)
{
    long* x = (long*)(u);

    long temp;

    /* Unused parameter */
    (void)v;

    /* Swap */
    temp = x[a];
    x[a] = x[b];
    x[b] = temp;
}

/*
 * Save a slope
 */
static void vinfo_init_aux(vinfo_hack* hack, int y, int x, long m)
{
    int i;

    /* Handle "legal" slopes */
    if ((m > 0) && (m <= SCALE))
    {
        /* Look for that slope */
        for (i = 0; i < hack->num_slopes; i++)
        {
            if (hack->slopes[i] == m)
                break;
        }

        /* New slope */
        if (i == hack->num_slopes)
        {
            /* Paranoia */
            if (hack->num_slopes >= VINFO_MAX_SLOPES)
            {
                quit(format("Too many LOS slopes (%d)!", VINFO_MAX_SLOPES));
            }

            /* Save the slope, increment count */
            hack->slopes[hack->num_slopes++] = m;
        }
    }

    /* Track slope range */
    if (hack->slopes_min[y][x] > m)
        hack->slopes_min[y][x] = m;
    if (hack->slopes_max[y][x] < m)
        hack->slopes_max[y][x] = m;
}

/*
 * Initialize the "vinfo" array
 *
 * Full Octagon (radius 20), Grids=1149
 *
 * Quadrant (south east), Grids=308, Slopes=251
 *
 * Octant (east then south), Grids=161, Slopes=126
 *
 * This function assumes that VINFO_MAX_GRIDS and VINFO_MAX_SLOPES
 * have the correct values, which can be derived by setting them to
 * a number which is too high, running this function, and using the
 * error messages to obtain the correct values.
 */
errr vinfo_init(void)
{
    int i, g;
    int y, x;

    long m;

    vinfo_hack* hack;

    int num_grids = 0;

    int queue_head = 0;
    int queue_tail = 0;
    vinfo_type* queue[VINFO_MAX_GRIDS * 2];

    /* Make hack */
    hack = mem_alloc(vinfo_hack);

    /* Analyze grids */
    for (y = 0; y <= MAX_SIGHT; ++y)
    {
        for (x = y; x <= MAX_SIGHT; ++x)
        {
            /* Skip grids which are out of sight range */
            if (distance(0, 0, y, x) > MAX_SIGHT)
                continue;

            /* Default slope range */
            hack->slopes_min[y][x] = 999999999;
            hack->slopes_max[y][x] = 0;

            /* Paranoia */
            if (num_grids >= VINFO_MAX_GRIDS)
            {
                quit(format(
                    "Too many grids (%d >= %d)!", num_grids, VINFO_MAX_GRIDS));
            }

            /* Count grids */
            num_grids++;

            /* Slope to the top right corner */
            m = SCALE * (1000L * y - 500) / (1000L * x + 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to top left corner */
            m = SCALE * (1000L * y - 500) / (1000L * x - 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to bottom right corner */
            m = SCALE * (1000L * y + 500) / (1000L * x + 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);

            /* Slope to bottom left corner */
            m = SCALE * (1000L * y + 500) / (1000L * x - 500);

            /* Handle "legal" slopes */
            vinfo_init_aux(hack, y, x, m);
        }
    }

    /* Enforce maximal efficiency (grids) */
    if (num_grids < VINFO_MAX_GRIDS)
    {
        quit(format("Too few grids (%d < %d)!", num_grids, VINFO_MAX_GRIDS));
    }

    /* Enforce maximal efficiency (line of sight slopes) */
    if (hack->num_slopes < VINFO_MAX_SLOPES)
    {
        quit(format("Too few LOS slopes (%d < %d)!", hack->num_slopes,
            VINFO_MAX_SLOPES));
    }

    /* Sort slopes numerically */
    ang_sort_comp = ang_sort_comp_hook_longs;

    /* Sort slopes numerically */
    ang_sort_swap = ang_sort_swap_hook_longs;

    /* Sort the (unique) LOS slopes */
    ang_sort(hack->slopes, NULL, hack->num_slopes);

    /* Enqueue player grid */
    queue[queue_tail++] = &vinfo[0];

    /* Process queue */
    while (queue_head < queue_tail)
    {
        int e;

        /* Index */
        e = queue_head++;

        /* Main Grid */
        g = vinfo[e].grid[0];

        /* Location */
        y = GRID_Y(g);
        x = GRID_X(g);

        /* Compute grid offsets */
        vinfo[e].grid[0] = GRID(+y, +x);
        vinfo[e].grid[1] = GRID(+x, +y);
        vinfo[e].grid[2] = GRID(+x, -y);
        vinfo[e].grid[3] = GRID(+y, -x);
        vinfo[e].grid[4] = GRID(-y, -x);
        vinfo[e].grid[5] = GRID(-x, -y);
        vinfo[e].grid[6] = GRID(-x, +y);
        vinfo[e].grid[7] = GRID(-y, +x);

        /* Skip player grid */
        if (e > 0)
        {
            long slope_fire;

            long tmp0 = 0;
            long tmp1 = 0;
            long tmp2 = 999999L;

            /* Determine LOF slope for this grid */
            if (x == 0)
                slope_fire = SCALE;
            else
                slope_fire = SCALE * (1000L * y) / (1000L * x);

            /* Analyze LOS slopes */
            for (i = 0; i < hack->num_slopes; ++i)
            {
                m = hack->slopes[i];

                /* Memorize intersecting slopes */
                if ((hack->slopes_min[y][x] < m)
                    && (hack->slopes_max[y][x] > m))
                {
                    /* Add it to the LOS slope set */
                    switch (i / 32)
                    {
                    case 3:
                        vinfo[e].bits_3 |= (1L << (i % 32));
                        break;
                    case 2:
                        vinfo[e].bits_2 |= (1L << (i % 32));
                        break;
                    case 1:
                        vinfo[e].bits_1 |= (1L << (i % 32));
                        break;
                    case 0:
                        vinfo[e].bits_0 |= (1L << (i % 32));
                        break;
                    }

                    /* Check for exact match with the LOF slope */
                    if (m == slope_fire)
                        tmp0 = i;

                    /* Remember index of nearest LOS slope < than LOF slope */
                    else if ((m < slope_fire) && (m > tmp1))
                        tmp1 = i;

                    /* Remember index of nearest LOS slope > than LOF slope */
                    else if ((m > slope_fire) && (m < tmp2))
                        tmp2 = i;
                }
            }

            /* There is a perfect match with one of the LOS slopes */
            if (tmp0)
            {
                /* Save the (unique) slope */
                vinfo[e].slope_fire_index1 = tmp0;

                /* Mark the other empty */
                vinfo[e].slope_fire_index2 = 0;
            }

            /* The LOF slope lies between two LOS slopes */
            else
            {
                /* Save the first slope */
                vinfo[e].slope_fire_index1 = tmp1;

                /* Save the second slope */
                vinfo[e].slope_fire_index2 = tmp2;
            }
        }

        /* Default */
        vinfo[e].next_0 = &vinfo[0];

        /* Grid next child */
        if (distance(0, 0, y, x + 1) <= MAX_SIGHT)
        {
            g = GRID(y, x + 1);

            if (queue[queue_tail - 1]->grid[0] != g)
            {
                vinfo[queue_tail].grid[0] = g;
                queue[queue_tail] = &vinfo[queue_tail];
                queue_tail++;
            }

            vinfo[e].next_0 = &vinfo[queue_tail - 1];
        }

        /* Default */
        vinfo[e].next_1 = &vinfo[0];

        /* Grid diag child */
        if (distance(0, 0, y + 1, x + 1) <= MAX_SIGHT)
        {
            g = GRID(y + 1, x + 1);

            if (queue[queue_tail - 1]->grid[0] != g)
            {
                vinfo[queue_tail].grid[0] = g;
                queue[queue_tail] = &vinfo[queue_tail];
                queue_tail++;
            }

            vinfo[e].next_1 = &vinfo[queue_tail - 1];
        }

        /* Hack -- main diagonal has special children */
        if (y == x)
            vinfo[e].next_0 = vinfo[e].next_1;

        /* Grid coordinates, approximate distance  */
        vinfo[e].y = y;
        vinfo[e].x = x;
        vinfo[e].d = ((y > x) ? (y + x / 2) : (x + y / 2));
        vinfo[e].r = ((!y) ? x : (!x) ? y : (y == x) ? y : 0);
    }

    /* Verify maximal bits XXX XXX XXX */
    if (((vinfo[1].bits_3 | vinfo[2].bits_3) != VINFO_BITS_3)
        || ((vinfo[1].bits_2 | vinfo[2].bits_2) != VINFO_BITS_2)
        || ((vinfo[1].bits_1 | vinfo[2].bits_1) != VINFO_BITS_1)
        || ((vinfo[1].bits_0 | vinfo[2].bits_0) != VINFO_BITS_0))
    {
        quit("Incorrect bit masks!");
    }

    /* Kill hack */
    mem_free_null(hack);

    /* Success */
    return (0);
}

/*
 * Forget the "CAVE_VIEW" grids, redrawing as needed
 */
void forget_view(void)
{
    int i, g;

    int fast_view_n = view_n;
    u16b* fast_view_g = view_g;

    u16b* fast_cave_info = &cave_info[0][0];

    /* None to forget */
    if (!fast_view_n)
        return;

    /* Clear them all */
    for (i = 0; i < fast_view_n; i++)
    {
        /* Grid */
        g = fast_view_g[i];

        /* Clear "CAVE_VIEW" and "CAVE_SEEN" flags */
        fast_cave_info[g] &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

        /* Clear "CAVE_LITE" flag */
        /* fast_cave_info[g] &= ~(CAVE_LITE); */

        /* Redraw through snapshot invalidation. */
        dungeon_mark_map_for_redraw();
    }

    /* None left */
    fast_view_n = 0;

    /* Save 'view_n' */
    view_n = fast_view_n;
}

static bool same_side_of_wall_as_player(int y, int x, int fy, int fx)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    bool same = true;

    // if one above and one below
    if (((py <= y) && (fy >= y)) || ((py >= y) && (fy <= y)))
    {
        if ((px < x) && (fx < x))
        {
            if (cave_info[y][x - 1] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else if ((px > x) && (fx > x))
        {
            if (cave_info[y][x + 1] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else
        {
            same = false;
        }
    }

    // if one left and one right
    if (((px <= x) && (fx >= x)) || ((px >= x) && (fx <= x)))
    {
        // if both above
        if ((py < y) && (fy < y))
        {
            if (cave_info[y - 1][x] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else if ((py > y) && (fy > y))
        {
            if (cave_info[y + 1][x] & (CAVE_WALL))
            {
                same = false;
            }
        }
        else
        {
            same = false;
        }
    }

    return (same);
}

/*
 * Calculate the complete field of view using a new algorithm
 *
 * If "view_g" and "temp_g" were global pointers to arrays of grids, as
 * opposed to actual arrays of grids, then we could be more efficient by
 * using "pointer swapping".
 *
 * Note the following idiom, which is used in the function below.
 * This idiom processes each "octant" of the field of view, in a
 * clockwise manner, starting with the east strip, south side,
 * and for each octant, allows a simple calculation to set "g"
 * equal to the proper grids, relative to "pg", in the octant.
 *
 *   for (o2 = 0; o2 < 8; o2++)
 *   ...
 *         g = pg + p->grid[o2];
 *   ...
 *
 *
 * Normally, vision along the major axes is more likely than vision
 * along the diagonal axes, so we check the bits corresponding to
 * the lines of sight near the major axes first.
 *
 * We use the "temp_g" array (and the "CAVE_TEMP" flag) to keep track of
 * which grids were previously marked "CAVE_SEEN", since only those grids
 * whose "CAVE_SEEN" value changes during this routine must be redrawn.
 *
 * This function is now responsible for maintaining the "CAVE_SEEN"
 * flags as well as the "CAVE_VIEW" flags, which is good, because
 * the only grids which normally need to be memorized and/or redrawn
 * are the ones whose "CAVE_SEEN" flag changes during this routine.
 *
 * Basically, this function divides the "octagon of view" into octants of
 * grids (where grids on the main axes and diagonal axes are "shared" by
 * two octants), and processes each octant one at a time, processing each
 * octant one grid at a time, processing only those grids which "might" be
 * viewable, and setting the "CAVE_VIEW" flag for each grid for which there
 * is an (unobstructed) line of sight from the center of the player grid to
 * any internal point in the grid (and collecting these "CAVE_VIEW" grids
 * into the "view_g" array), and setting the "CAVE_SEEN" flag for the grid
 * if, in addition, the grid is "illuminated" in some way.
 *
 * This function relies on a theorem (suggested and proven by Mat Hostetter)
 * which states that in each octant of a field of view, a given grid will
 * be "intersected" by one or more unobstructed "lines of sight" from the
 * center of the player grid if and only if it is "intersected" by at least
 * one such unobstructed "line of sight" which passes directly through some
 * corner of some grid in the octant which is not shared by any other octant.
 * The proof is based on the fact that there are at least three significant
 * lines of sight involving any non-shared grid in any octant, one which
 * intersects the grid and passes though the corner of the grid closest to
 * the player, and two which "brush" the grid, passing through the "outer"
 * corners of the grid, and that any line of sight which intersects a grid
 * without passing through the corner of a grid in the octant can be "slid"
 * slowly towards the corner of the grid closest to the player, until it
 * either reaches it or until it brushes the corner of another grid which
 * is closer to the player, and in either case, the existanc of a suitable
 * line of sight is thus demonstrated.
 *
 * It turns out that in each octant of the radius 20 "octagon of view",
 * there are 161 grids (with 128 not shared by any other octant), and there
 * are exactly 126 distinct "lines of sight" passing from the center of the
 * player grid through any corner of any non-shared grid in the octant.  To
 * determine if a grid is "viewable" by the player, therefore, you need to
 * simply show that one of these 126 lines of sight intersects the grid but
 * does not intersect any wall grid closer to the player.  So we simply use
 * a bit vector with 126 bits to represent the set of interesting lines of
 * sight which have not yet been obstructed by wall grids, and then we scan
 * all the grids in the octant, moving outwards from the player grid.  For
 * each grid, if any of the lines of sight which intersect that grid have not
 * yet been obstructed, then the grid is viewable.  Furthermore, if the grid
 * is a wall grid, then all of the lines of sight which intersect the grid
 * should be marked as obstructed for future reference.  Also, we only need
 * to check those grids for whom at least one of the "parents" was a viewable
 * non-wall grid, where the parents include the two grids touching the grid
 * but closer to the player grid (one adjacent, and one diagonal).  For the
 * bit vector, we simply use 4 32-bit integers.  All of the static values
 * which are needed by this function are stored in the large "vinfo" array
 * (above), which is machine generated by another program.  XXX XXX XXX
 *
 * Hack -- The queue must be able to hold more than VINFO_MAX_GRIDS grids
 * because the grids at the edge of the field of view use "grid zero" as
 * their children, and the queue must be able to hold several of these
 * special grids.  Because the actual number of required grids is bizarre,
 * we simply allocate twice as many as we would normally need.  XXX XXX XXX
 */
void update_view(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int pg = GRID(py, px);

    int i, j, g, o2;

    int player_light = p_ptr->cur_light;
    int player_rad = ABS(player_light);

    int fy, fx, k;

    int fast_view_n = view_n;
    u16b* fast_view_g = view_g;

    int fast_temp_n = 0;
    u16b* fast_temp_g = temp_g;

    u16b* fast_cave_info = &cave_info[0][0];

    u16b info;

    bool in_pit = cave_pit_bold(p_ptr->py, p_ptr->px) && !p_ptr->leaping;

    /*** Step 0 -- Begin ***/

    /* Save the old "view" grids for later */
    for (i = 0; i < fast_view_n; i++)
    {
        /* Grid */
        g = fast_view_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Save "CAVE_SEEN" grids */
        if (info & (CAVE_SEEN))
        {
            /* Set "CAVE_TEMP" flag */
            info |= (CAVE_TEMP);

            /* Save grid for later */
            fast_temp_g[fast_temp_n++] = g;
        }

        /* Clear "CAVE_VIEW", "CAVE_SEEN" & cave_fire flags */
        info &= ~(CAVE_VIEW | CAVE_SEEN | CAVE_FIRE);

        /* Clear "CAVE_LITE" flag */
        /* info &= ~(CAVE_LITE); */

        /* Save cave info */
        fast_cave_info[g] = info;
    }

    /* Reset the "view" array */
    fast_view_n = 0;

    /*** Step 1 -- player grid ***/

    /* Player grid */
    g = pg;

    /* Get grid info */
    info = fast_cave_info[g];

    /* Assume viewable */
    info |= (CAVE_VIEW | CAVE_FIRE | CAVE_SEEN);

    /* Save cave info */
    fast_cave_info[g] = info;

    /* Save in array */
    fast_view_g[fast_view_n++] = g;

    /*** Step 2 -- octants ***/

    /* Scan each octant */
    for (o2 = 0; o2 < 8; o2++)
    {
        vinfo_type* p;

        /* Last added */
        vinfo_type* last = &vinfo[0];

        /* Grid queue */
        int queue_head = 0;
        int queue_tail = 0;
        vinfo_type* queue[VINFO_MAX_GRIDS * 2];

        /* Slope bit vector */
        u32b bits0 = VINFO_BITS_0;
        u32b bits1 = VINFO_BITS_1;
        u32b bits2 = VINFO_BITS_2;
        u32b bits3 = VINFO_BITS_3;

        /* Reset queue */
        queue_head = queue_tail = 0;

        /* Initial grids */
        queue[queue_tail++] = &vinfo[1];
        queue[queue_tail++] = &vinfo[2];

        /* Process queue */
        while (queue_head < queue_tail)
        {
            /* Assume no line of fire */
            bool line_fire = false;

            /* Dequeue next grid */
            p = queue[queue_head++];

            /* Check bits */
            if ((bits0 & (p->bits_0)) || (bits1 & (p->bits_1))
                || (bits2 & (p->bits_2)) || (bits3 & (p->bits_3)))
            {
                /* Extract grid value XXX XXX XXX */
                g = pg + p->grid[o2];

                /* Get grid info */
                info = fast_cave_info[g];

                /* Check for first possible line of fire */
                i = p->slope_fire_index1;

                /* Check line(s) of fire */
                while (true)
                {
                    switch (i / 32)
                    {
                    case 3:
                    {
                        if (bits3 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 2:
                    {
                        if (bits2 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 1:
                    {
                        if (bits1 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    case 0:
                    {
                        if (bits0 & (1L << (i % 32)))
                            line_fire = true;
                        break;
                    }
                    }

                    /* Check second LOF slope if necessary */
                    if ((!p->slope_fire_index2) || (line_fire)
                        || (i == p->slope_fire_index2))
                    {
                        break;
                    }

                    /* Check second possible line of fire */
                    i = p->slope_fire_index2;
                }

                /* Note line of fire */
                if (line_fire)
                {
                    info |= (CAVE_FIRE);
                }

                /* Handle wall */
                if (info & (CAVE_WALL))
                {
                    /* Clear bits */
                    bits0 &= ~(p->bits_0);
                    bits1 &= ~(p->bits_1);
                    bits2 &= ~(p->bits_2);
                    bits3 &= ~(p->bits_3);

                    /* Newly viewable wall */
                    if (!(info & (CAVE_VIEW)))
                    {
                        /* Mark as viewable */
                        info |= (CAVE_VIEW);

                        /* Torch-lit grids */
                        if (p->d <= player_light)
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);

                            /* Mark as "CAVE_LITE" */
                            /* info |= (CAVE_LITE); */
                        }

                        /* Perma-lit grids */
                        else if (info & (CAVE_GLOW))
                        {
                            int y = GRID_Y(g);
                            int x = GRID_X(g);

                            /* Hack -- move towards player */
                            int yy
                                = (y < py) ? (y + 1) : (y > py) ? (y - 1) : y;
                            int xx
                                = (x < px) ? (x + 1) : (x > px) ? (x - 1) : x;

                            /* Check for "complex" illumination */
                            if ((!(cave_info[yy][xx] & (CAVE_WALL))
                                    && (cave_info[yy][xx] & (CAVE_GLOW)))
                                || (!(cave_info[y][xx] & (CAVE_WALL))
                                    && (cave_info[y][xx] & (CAVE_GLOW)))
                                || (!(cave_info[yy][x] & (CAVE_WALL))
                                    && (cave_info[yy][x] & (CAVE_GLOW))))
                            {
                                /* Mark as seen */
                                info |= (CAVE_SEEN);
                            }
                        }

                        /* Save in array */
                        fast_view_g[fast_view_n++] = g;
                    }
                }

                /* Handle non-wall */
                else
                {
                    /* Enqueue child */
                    if (last != p->next_0)
                    {
                        queue[queue_tail++] = last = p->next_0;
                    }

                    /* Enqueue child */
                    if (last != p->next_1)
                    {
                        queue[queue_tail++] = last = p->next_1;
                    }

                    /* Newly viewable non-wall */
                    if (!(info & (CAVE_VIEW)))
                    {
                        /* Mark as "viewable" */
                        info |= (CAVE_VIEW);

                        /* Torch-lit grids */
                        if (p->d <= player_light)
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);

                            /* Mark as "CAVE_LITE" */
                            /* info |= (CAVE_LITE); */
                        }

                        /* Perma-lit grids */
                        else if (info & (CAVE_GLOW))
                        {
                            /* Mark as "CAVE_SEEN" */
                            info |= (CAVE_SEEN);
                        }

                        /* Save in array */
                        fast_view_g[fast_view_n++] = g;
                    }
                }

                /* Save cave info */
                fast_cave_info[g] = info;
            }
        }
    }

    // restrict the view of players in pits
    if (in_pit)
    {
        for (i = 0; i < fast_view_n; i++)
        {
            int y, x;

            g = fast_view_g[i];

            y = GRID_Y(g);
            x = GRID_X(g);

            // quick check to see if the square is not-adjacent
            if ((abs(y - py) > 1) || (abs(x - px) > 1))
            {
                fast_cave_info[g] &= ~(CAVE_SEEN | CAVE_VIEW | CAVE_FIRE);
            }
        }
    }

    /*** Step 2b -- handle the Sil-style light ***/

    /* this is the only step that even looks at these light values */

    // Sil: get the starting light values based on permanent light (and backup
    // old values)
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            if (cave_info[i][j] & (CAVE_GLOW))
            {
                cave_light[i][j] = 1;
            }
            else
            {
                cave_light[i][j] = 0;
            }
            
            /* Chasm partitions absorb light - apply -4 penalty */
            if (cave_info[i][j] & CAVE_CHASM_AREA)
            {
                cave_light[i][j] -= 4;
            }
        }
    }

    // Sil: update the light values with the torch/lantern light

    /* Calculate DARKNESS bonus once (items give +1 light power each) */
    int darkness_bonus = 0;
    {
        int slot;
        for (slot = INVEN_WIELD; slot < INVEN_TOTAL; slot++)
        {
            object_type* o_ptr = &inventory[slot];
            u32b f1, f2, f3;

            if (!o_ptr->k_idx) continue;
            if (slot == INVEN_LITE) continue;

            object_flags(o_ptr, &f1, &f2, &f3);
            if (f2 & TR2_DARKNESS)
                darkness_bonus++;
        }
    }

    for (i = -player_rad; i <= player_rad; i++)
    {
        for (j = -player_rad; j <= player_rad; j++)
        {
            int dist = distance(0, 0, i, j);
            int bonus_light = darkness_bonus;

            if (p_ptr->active_ability[S_WIL][WIL_INNER_LIGHT])
            {
                bonus_light += 2;
            }
            if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
            {
                bonus_light += 3;
            }

            // Don't darken/brighten the centre square too much
            // if (dist == 0) dist++;

            // Sil-y: previously used los(py,px,py+i,px+j) rather than CAVE_VIEW
            // in the below, but this seems better
            if (in_bounds(py + i, px + j) && (dist <= player_rad)
                && (cave_info[py + i][px + j] & (CAVE_VIEW)))
            {
                if (player_light > 0)
                {
                    cave_light[py + i][px + j]
                        += player_rad + 1 - dist + bonus_light;
                }
                if (player_light < 0)
                {
                    cave_light[py + i][px + j]
                        -= player_rad + 1 - dist + bonus_light;
                }
            }
        }
    }

    // Sil: generate darkness or light for the all the monsters
    for (k = 1; k < mon_max; k++) // Sil-x: changed to mon_max from
                                  // z_info->m_max. I think I'm right about this
    {
        /* Check the k'th monster */
        monster_type* m_ptr = &mon_list[k];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Access the location */
        fx = m_ptr->fx;
        fy = m_ptr->fy;

        int mon_light = r_ptr->light;
        int mon_rad = ABS(mon_light);

        bool glow = r_ptr->flags2 & (RF2_GLOW);

        // Do darkness or light for this monster
        if (mon_rad > 0)
        {
            for (i = -mon_rad; i <= mon_rad; i++)
            {
                for (j = -mon_rad; j <= mon_rad; j++)
                {
                    int y = fy + i;
                    int x = fx + j;

                    int dist = distance(0, 0, i, j);

                    g = GRID(y, x);

                    info = fast_cave_info[g];

                    // Don't darken/brighten the centre square too much
                    // if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;

                    if (in_bounds(y, x) && (dist <= mon_rad)
                        && los(fy, fx, y, x))
                    {
                        // Only set it if the player can see it
                        if ((distance(py, px, y, x) <= MAX_SIGHT)
                            && (info & (CAVE_VIEW)))
                        {
                            if (((cave_info[y][x] & (CAVE_WALL))
                                    && same_side_of_wall_as_player(
                                        y, x, fy, fx))
                                || !(cave_info[y][x] & (CAVE_WALL)))
                            {
                                // Glowing monsters lighten their own square
                                if ((i == 0) && (j == 0) && glow)
                                {
                                    cave_light[y][x] += 1;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }

                                // Brighten the square
                                else if (mon_light > 0)
                                {
                                    cave_light[y][x] += mon_rad + 1 - dist;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }
                                // Darken the square
                                else
                                {
                                    cave_light[y][x] -= mon_rad + 1 - dist;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: generate darkness or light for the all the objects
    for (k = 1; k < o_max; k++)
    {
        /* Get the next object from the dungeon */
        object_type* o_ptr = &o_list[k];

        u32b f1, f2, f3;

        int obj_light = 0;
        int obj_rad;

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Access the location */
        fx = o_ptr->ix;
        fy = o_ptr->iy;

        object_flags(o_ptr, &f1, &f2, &f3);

        // The Iron Crown glows
        if ((o_ptr->name1 >= ART_MORGOTH_1) && (o_ptr->name1 <= ART_MORGOTH_3))
        {
            obj_light += o_ptr->pval;
        }

        obj_rad = ABS(obj_light);

        // Do darkness or light for this object
        if (obj_rad > 0)
        {
            for (i = -obj_rad; i <= obj_rad; i++)
            {
                for (j = -obj_rad; j <= obj_rad; j++)
                {
                    int y = fy + i;
                    int x = fx + j;

                    int dist = distance(0, 0, i, j);

                    g = GRID(y, x);

                    info = fast_cave_info[g];

                    // Don't darken/brighten the centre square too much
                    // if ((dist == 0) && (distance(py,px,fy,fx) == 1)) dist++;

                    if (in_bounds(y, x) && (dist <= obj_rad)
                        && los(fy, fx, y, x))
                    {
                        // Only set it if the player can see it
                        if ((distance(py, px, y, x) <= MAX_SIGHT)
                            && (info & (CAVE_VIEW)))
                        {
                            if (((cave_info[y][x] & (CAVE_WALL))
                                    && same_side_of_wall_as_player(
                                        y, x, fy, fx))
                                || !(cave_info[y][x] & (CAVE_WALL)))
                            {
                                // Brighten the square
                                if (obj_light > 0)
                                {
                                    cave_light[y][x] += obj_rad + 1 - dist;

                                    /* Mark as seen */
                                    info |= (CAVE_SEEN);

                                    /* Save cave info */
                                    fast_cave_info[g] = info;

                                    /* Save in array */
                                    fast_view_g[fast_view_n++] = g;
                                }
                                // Darken the square
                                else
                                {
                                    cave_light[y][x] -= obj_rad + 1 - dist;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: this removes the 'seen' flag from squares that have zero or less
    // light
    for (i = 0; i < fast_view_n; i++)
    {
        int y, x;

        g = fast_view_g[i];

        y = GRID_Y(g);
        x = GRID_X(g);

        // Remove 'seen' flag from squares that have zero or less light
        if (cave_light[y][x] <= 0)
        {
            fast_cave_info[g] &= ~(CAVE_SEEN);

            /* Hack -- Forget "boring" grids */
            if (cave_floorlike_bold(y, x) && (cave_info[y][x] & (CAVE_GLOW)))
            {
                /* Forget */
                cave_info[y][x] &= ~(CAVE_MARK);
            }
        }
    }

    /*** Step 3 -- Complete the algorithm ***/

    /* Handle blindness */
    if (p_ptr->blind)
    {
        /* Process "new" grids */
        for (i = 0; i < fast_view_n; i++)
        {
            /* Grid */
            g = fast_view_g[i];

            /* Grid cannot be "CAVE_SEEN" */
            fast_cave_info[g] &= ~(CAVE_SEEN);
        }
    }

    /* Process "new" grids */
    for (i = 0; i < fast_view_n; i++)
    {
        /* Grid */
        g = fast_view_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Was not "CAVE_SEEN", is now "CAVE_SEEN" */
        if ((info & (CAVE_SEEN)) && !(info & (CAVE_TEMP)))
        {
            int y, x;

            /* Location */
            y = GRID_Y(g);
            x = GRID_X(g);

            /* Note */
            note_spot(y, x);

            /* Redraw through snapshot invalidation. */
            dungeon_mark_map_for_redraw();
        }
    }

    // Sil-y: for some reason we need to update the visibility info for monsters
    // (the ->ml attribute).
    //        Otherwise, dark producing monsters are occasionally visible when
    //        they are following you.
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Update the monster */
        update_mon(i, false);
    }

    /* Process "old" grids */
    for (i = 0; i < fast_temp_n; i++)
    {
        /* Grid */
        g = fast_temp_g[i];

        /* Get grid info */
        info = fast_cave_info[g];

        /* Clear "CAVE_TEMP" flag */
        info &= ~(CAVE_TEMP);

        /* Save cave info */
        fast_cave_info[g] = info;

        /* Was "CAVE_SEEN", is now not "CAVE_SEEN" */
        if (!(info & (CAVE_SEEN)))
        {
            /* Redraw through snapshot invalidation. */
            dungeon_mark_map_for_redraw();
        }
    }

    // Sil: this is needed to properly darken certain spots
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            if ((cave_info[i][j] & (CAVE_GLOW))
                && (cave_info[i][j] & (CAVE_VIEW))
                && !(cave_info[i][j] & (CAVE_SEEN)))
            {
                /* Redraw through snapshot invalidation. */
                dungeon_mark_map_for_redraw();
            }
        }
    }

    // Sil: disturb the player when the lighting changes unexpectedly
    for (i = py - MAX_SIGHT; i <= py + MAX_SIGHT; i++)
    {
        for (j = px - MAX_SIGHT; j <= px + MAX_SIGHT; j++)
        {
            if (in_bounds_fully(i, j) && ((i != py) || (j != px)))
            {
                info = cave_info[i][j];

                if ((info & (CAVE_OLD_VIEW)) && (info & (CAVE_VIEW)))
                {
                    // check recently darkened squares
                    if ((info & (CAVE_OLD_LIT)) && (cave_light[i][j] <= 0))
                    {
                        // if they didn't just fall out of torch radius
                        if (!((info & (CAVE_OLD_TORCH))
                                && (distance(py, px, i, j) > player_rad)))
                        {
                            // ignore in some negative light situations (not a
                            // perfect fix, but good enough)
                            if ((p_ptr->old_light >= 0)
                                || (distance(py, px, i, j) > player_rad + 1))
                            {
                                disturb(0, 0);
                                // msg_format("(%d,%d) Disturbed on loss of
                                // light.",i,j);
                            }
                        }
                    }

                    // check recently lit squares
                    if (!(info & (CAVE_OLD_LIT)) && (cave_light[i][j] > 0))
                    {
                        // if they didn't just enter torch radius
                        if (!(!(info & (CAVE_OLD_TORCH))
                                && (distance(py, px, i, j) <= player_rad)))
                        {
                            // ignore in some negative light situations (not a
                            // perfect fix, but good enough)
                            if ((p_ptr->old_light >= 0)
                                || (distance(py, px, i, j) > player_rad + 1))
                            {
                                disturb(0, 0);
                                // msg_format("(%d,%d) Disturbed on gain of
                                // light.",i,j);
                            }
                        }
                    }
                }
            }
        }
    }

    // Sil: record information about view and lighting for next call to
    // update_view()
    //      so that they player can be disturbed when lighting changes
    //      unexpectedly
    p_ptr->old_light = p_ptr->cur_light;
    for (i = 0; i < MAX_DUNGEON_HGT; i++)
    {
        for (j = 0; j < MAX_DUNGEON_WID; j++)
        {
            // store view information for last turn
            if (cave_info[i][j] & (CAVE_VIEW))
            {
                cave_info[i][j] |= (CAVE_OLD_VIEW);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_VIEW);
            }

            // store lighting information for last turn
            if (cave_light[i][j] > 0)
            {
                cave_info[i][j] |= (CAVE_OLD_LIT);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_LIT);
            }

            // store 'torchlight' information for last turn
            if (distance(py, px, i, j) <= p_ptr->old_light)
            {
                cave_info[i][j] |= (CAVE_OLD_TORCH);
            }
            else
            {
                cave_info[i][j] &= ~(CAVE_OLD_TORCH);
            }
        }
    }
    /* ------------------------------------------------------------
     * Meta-run curse: CUR_LIGHTP
     *   Each stack makes darkness 1 level “stronger”.
     *   We post-process the finished cave_light[][] buffer so that
     *   every lit square is dimmed once per stack, down to a floor
     *   of −5 (same as full darkness elsewhere in the engine).
     * ------------------------------------------------------------ */
    {
        int dark_delta = curse_flag_delta_cur(CUR_LIGHTP);
        if (dark_delta)
        {
            int view_i, view_grid, view_y, view_x;

            /* Iterate over the grids we just updated */
            for (view_i = 0; view_i < view_n; view_i++)
            {
                view_grid = view_g[view_i];  /* packed grid index      */
                view_y = GRID_Y(view_grid);  /* unpack coordinates     */
                view_x = GRID_X(view_grid);

                cave_light[view_y][view_x] -= dark_delta;
                if (cave_light[view_y][view_x] < -5)
                    cave_light[view_y][view_x] = -5;
            }
        }
    }

    /* Save 'view_n' */
    view_n = fast_view_n;
}

/*
 * Determine the path taken by a projection.  -BEN-, -LM-
 *
 * The projection will always start one grid from the grid (y1,x1), and will
 * travel towards the grid (y2,x2), touching one grid per unit of distance
 * along the major axis, and stopping when it satisfies certain conditions
 * or has travelled the maximum legal distance of "range".  Projections
 * cannot extend further than MAX_SIGHT (at least at present).
 *
 * A projection only considers those grids which contain the line(s) of fire
 * from the start to the end point.  Along any step of the projection path,
 * either one or two grids may be valid options for the next step.  When a
 * projection has a choice of grids, it chooses that which offers the least
 * resistance.  Given a choice of clear grids, projections prefer to move
 * orthogonally.
 *
 * Also, projections to or from the character must stay within the pre-
 * calculated field of fire ("cave_info & (CAVE_FIRE)").  This is a hack.
 * XXX XXX
 *
 * The path grids are saved into the grid array pointed to by "gp", and
 * there should be room for at least "range" grids in "gp".  Note that
 * due to the way in which distance is calculated, this function normally
 * uses fewer than "range" grids for the projection path, so the result
 * of this function should never be compared directly to "range".  Note
 * that the initial grid (y1,x1) is never saved into the grid array, not
 * even if the initial grid is also the final grid.  XXX XXX XXX
 *
 * We modify y2 and x2 if they are too far away, or (for PROJECT_PASS only)
 * if the projection threatens to leave the dungeon.
 *
 * The "flg" flags can be used to modify the behavior of this function:
 *    PROJECT_STOP:  projection stops when it cannot bypass a monster.
 *    PROJECT_CHCK:  projection notes when it cannot bypass a monster.
 *    PROJECT_THRU:  projection extends past destination grid
 *    PROJECT_PASS:  projection passes through walls
 *    PROJECT_INVISIPASS:  projection passes through invisible walls (ie unknown
 * ones)
 *
 * This function returns the number of grids (if any) in the path.  This
 * may be zero if no grids are legal except for the starting one.
 */
static bool project_path_mask_matches(const project_path_mask* ignore, int y,
    int x)
{
    return ignore && (ignore->y == y) && (ignore->x == x);
}

static int project_path_internal(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg,
    const project_path_mask* ignore)
{
    int i, j, k;
    int dy, dx;
    int num, dist, octant;
    int grids = 0;
    bool line_fire;
    bool full_stop = false;

    int y_a, x_a, y_b, x_b;
    int y = 0, old_y = 0;
    int x = 0, old_x = 0;

    /* Start with all lines of sight unobstructed */
    u32b bits0 = VINFO_BITS_0;
    u32b bits1 = VINFO_BITS_1;
    u32b bits2 = VINFO_BITS_2;
    u32b bits3 = VINFO_BITS_3;

    int slope_fire1 = -1, slope_fire2 = 0;

    /* Projections are either vertical or horizontal */
    bool vertical;

    /* Require projections to be strictly LOF when possible  XXX XXX */
    bool require_strict_lof = false;

    /* Count of grids in LOF, storage of LOF grids */
    u16b tmp_grids[80];

    /* Count of grids in projection path */
    int step;

    /* Remember whether and how a grid is blocked */
    int blockage[2];

    /* Assume no monsters in way */
    bool monster_in_way = false;

    /* Initial grid */
    u16b g0 = (u16b)GRID(y1, x1);

    u16b g;

    /* Pointer to vinfo data */
    vinfo_type* p;

    /* Handle projections of zero length */
    if ((range <= 0) || ((*y2 == y1) && (*x2 == x1)))
        return (0);

    /* Note that the character is the source or target of the projection */
    if (((y1 == p_ptr->py) && (x1 == p_ptr->px))
        || ((*y2 == p_ptr->py) && (*x2 == p_ptr->px)))
    {
        /* Require strict LOF */
        require_strict_lof = true;
    }

    /* Get position change (signed) */
    dy = *y2 - y1;
    dx = *x2 - x1;

    /* Get distance from start to finish */
    dist = distance(y1, x1, *y2, *x2);

    /* Must stay within the field of sight XXX XXX */
    if (dist > MAX_SIGHT)
    {
        /* Always watch your (+/-) when doing rounded integer math. */
        int round_y = (dy < 0 ? -(dist / 2) : (dist / 2));
        int round_x = (dx < 0 ? -(dist / 2) : (dist / 2));

        /* Rescale the endpoints */
        dy = ((dy * (MAX_SIGHT - 1)) + round_y) / dist;
        dx = ((dx * (MAX_SIGHT - 1)) + round_x) / dist;
        *y2 = y1 + dy;
        *x2 = x1 + dx;
    }

    /* Get the correct octant */
    if (dy < 0)
    {
        /* Up and to the left */
        if (dx < 0)
        {
            /* More upwards than to the left - octant 4 */
            if (ABS(dy) > ABS(dx))
                octant = 5;

            /* At least as much left as upwards - octant 3 */
            else
                octant = 4;
        }
        else
        {
            if (ABS(dy) > ABS(dx))
                octant = 6;
            else
                octant = 7;
        }
    }
    else
    {
        if (dx < 0)
        {
            if (ABS(dy) > ABS(dx))
                octant = 2;
            else
                octant = 3;
        }
        else
        {
            if (ABS(dy) > ABS(dx))
                octant = 1;
            else
                octant = 0;
        }
    }

    /* Determine whether the major axis is vertical or horizontal */
    if ((octant == 5) || (octant == 6) || (octant == 2) || (octant == 1))
    {
        vertical = true;
    }
    else
    {
        vertical = false;
    }

    /* Scan the octant, find the grid corresponding to the end point */
    for (j = 1; j < VINFO_MAX_GRIDS; j++)
    {
        int vy, vx;

        /* Point to this vinfo record */
        p = &vinfo[j];

        /* Extract grid value */
        g = (u16b)(g0 + p->grid[octant]);

        /* Get axis coordinates */
        vy = GRID_Y(g);
        vx = GRID_X(g);

        /* Require that grid be correct */
        if ((vy != *y2) || (vx != *x2))
            continue;

        /* Store lines of fire */
        slope_fire1 = p->slope_fire_index1;
        slope_fire2 = p->slope_fire_index2;

        break;
    }

    /* Note failure XXX XXX */
    if (slope_fire1 == -1)
        return (0);

    /* Scan the octant, collect all grids having the correct line of fire */
    for (j = 1; j < VINFO_MAX_GRIDS; j++)
    {
        line_fire = false;

        /* Point to this vinfo record */
        p = &vinfo[j];

        /* See if any lines of sight pass through this grid */
        if (!((bits0 & (p->bits_0)) || (bits1 & (p->bits_1))
                || (bits2 & (p->bits_2)) || (bits3 & (p->bits_3))))
        {
            continue;
        }

        /*
         * Extract grid value.  Use pointer shifting to get the
         * correct grid offset for this octant.
         */
        g = (u16b)(g0 + *((s16b*)(((byte*)(p)) + (octant * 2))));

        y = GRID_Y(g);
        x = GRID_X(g);

        /* Must be legal (this is important) */
        if (!in_bounds_fully(y, x))
            continue;

        /* Check for first possible line of fire */
        i = slope_fire1;

        /* Check line(s) of fire */
        while (true)
        {
            switch (i / 32)
            {
            case 3:
            {
                if (bits3 & (1L << (i % 32)))
                {
                    if (p->bits_3 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 2:
            {
                if (bits2 & (1L << (i % 32)))
                {
                    if (p->bits_2 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 1:
            {
                if (bits1 & (1L << (i % 32)))
                {
                    if (p->bits_1 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            case 0:
            {
                if (bits0 & (1L << (i % 32)))
                {
                    if (p->bits_0 & (1L << (i % 32)))
                        line_fire = true;
                }
                break;
            }
            }

            /* We're done if no second LOF exists, or when we've checked it */
            if ((!slope_fire2) || (i == slope_fire2))
                break;

            /* Check second possible line of fire */
            i = slope_fire2;
        }

        /* This grid contains at least one of the lines of fire */
        if (line_fire)
        {
            /* Do not accept breaks in the series of grids  XXX XXX */
            if ((grids) && ((ABS(y - old_y) > 1) || (ABS(x - old_x) > 1)))
            {
                break;
            }

            /* Optionally, require strict line of fire */
            if ((!require_strict_lof) || (cave_info[y][x] & (CAVE_FIRE))
                || ((flg & (PROJECT_INVISIPASS))
                    && !(cave_info[y][x] & (CAVE_MARK))))
            {
                /* Store grid value */
                tmp_grids[grids++] = g;
            }

            /* Remember previous coordinates */
            old_y = y;
            old_x = x;
        }

        /*
         * Handle wall (unless ignored).  Walls can be in a projection path,
         * but the path cannot pass through them.
         */
        if (!(flg & (PROJECT_PASS)) && (cave_info[y][x] & (CAVE_WALL)))
        {
            if (!(flg & (PROJECT_INVISIPASS))
                || (cave_info[y][x] & (CAVE_MARK)))
            {
                /* Clear any lines of sight passing through this grid */
                bits0 &= ~(p->bits_0);
                bits1 &= ~(p->bits_1);
                bits2 &= ~(p->bits_2);
                bits3 &= ~(p->bits_3);
            }
        }

        /*
         * Handle chasms if they are designated to block the line
         */
        if ((flg & (PROJECT_NO_CHASM)) && (cave_feat[y][x] & (FEAT_CHASM)))
        {
            /* Clear any lines of sight passing through this grid */
            bits0 &= ~(p->bits_0);
            bits1 &= ~(p->bits_1);
            bits2 &= ~(p->bits_2);
            bits3 &= ~(p->bits_3);
        }
    }

    /* Scan the grids along the line(s) of fire */
    for (step = 0, j = 0; j < grids;)
    {
        /* Get the coordinates of this grid */
        y_a = GRID_Y(tmp_grids[j]);
        x_a = GRID_X(tmp_grids[j]);

        /* Get the coordinates of the next grid, if legal */
        if (j < grids - 1)
        {
            y_b = GRID_Y(tmp_grids[j + 1]);
            x_b = GRID_X(tmp_grids[j + 1]);
        }
        else
        {
            y_b = -1;
            x_b = -1;
        }

        /*
         * We always have at least one legal grid, and may have two.  Allow
         * the second grid if its position differs only along the minor axis.
         */
        if (vertical ? y_a == y_b : x_a == x_b)
            num = 2;
        else
            num = 1;

        /* Scan one or both grids */
        for (i = 0; i < num; i++)
        {
            blockage[i] = 0;

            /* Get the coordinates of this grid */
            y = (i == 0 ? y_a : y_b);
            x = (i == 0 ? x_a : x_b);

            /* Determine perpendicular distance */
            k = (vertical ? ABS(x - x1) : ABS(y - y1));

            /* Hack -- Check maximum range */
            if ((i == num - 1) && (step + (k >> 1)) >= range - 1)
            {
                /* End of projection */
                full_stop = true;
            }

            /* Sometimes stop at destination grid */
            if (!(flg & (PROJECT_THRU)))
            {
                if ((y == *y2) && (x == *x2))
                {
                    /* End of projection */
                    full_stop = true;
                }
            }

            /* Usually stop at wall grids */
            if (!(flg & (PROJECT_PASS))
                && (!(flg & (PROJECT_INVISIPASS))
                    || (cave_info[y][x] & (CAVE_MARK))))
            {
                if (!cave_floor_bold(y, x))
                    blockage[i] = 2;
            }

            /* If we don't stop at wall grids, we must explicitly check legality
             */
            else if (!in_bounds_fully(y, x))
            {
                /* End of projection */
                full_stop = true;
                blockage[i] = 3;
            }

            /* Try to avoid monsters/players between the endpoints */
            if ((cave_m_idx[y][x] != 0) && (blockage[i] < 2))
            {
                // Hack: ignore monsters on the designated square if these flags
                // are set
                if (!project_path_mask_matches(ignore, y, x))
                {
                    if (flg & (PROJECT_STOP))
                        blockage[i] = 2;
                    else if (flg & (PROJECT_CHCK))
                        blockage[i] = 1;
                }
            }
        }

        /* Pick the first grid if possible, the second if necessary */
        if ((num == 1) || (blockage[0] <= blockage[1]))
        {
            /* Store the first grid, advance */
            if (blockage[0] < 3)
                gp[step++] = tmp_grids[j];

            /* Blockage of 2 or greater means the projection ends */
            if (blockage[0] >= 2)
                break;

            /* Blockage of 1 means a monster bars the path */
            if (blockage[0] == 1)
            {
                /* Endpoints are always acceptable */
                if ((y != *y2) || (x != *x2))
                    monster_in_way = true;
            }

            /* Handle end of projection */
            if (full_stop)
                break;
        }
        else
        {
            /* Store the second grid, advance */
            if (blockage[1] < 3)
                gp[step++] = tmp_grids[j + 1];

            /* Blockage of 2 or greater means the projection ends */
            if (blockage[1] >= 2)
                break;

            /* Blockage of 1 means a monster bars the path */
            if (blockage[1] == 1)
            {
                /* Endpoints are always acceptable */
                if ((y != *y2) || (x != *x2))
                    monster_in_way = true;
            }

            /* Handle end of projection */
            if (full_stop)
                break;
        }

        /*
         * Hack -- If we require orthogonal movement, but are moving
         * diagonally, we have to plot an extra grid.  XXX XXX
         */
        if ((flg & (PROJECT_ORTH)) && (step > 1))
        {
            /* Get grids for this projection step and the last */
            y_a = GRID_Y(gp[step - 1]);
            x_a = GRID_X(gp[step - 1]);

            y_b = GRID_Y(gp[step - 2]);
            x_b = GRID_X(gp[step - 2]);

            /* The grids differ along both axis -- we moved diagonally */
            if ((y_a != y_b) && (x_a != x_b))
            {
                /* Get locations for the connecting grids */
                int y_c = y_a;
                int x_c = x_b;
                int y_d = y_b;
                int x_d = x_a;

                /* Back up one step */
                step--;

                /* Assume both grids are available */
                blockage[0] = 0;
                blockage[1] = 0;

                /* Hack -- Check legality */
                if (!in_bounds_fully(y_c, x_c))
                    blockage[0] = 2;
                if (!in_bounds_fully(y_d, x_d))
                    blockage[1] = 2;

                /* Usually stop at wall grids */
                if (!(flg & (PROJECT_PASS))
                    && (!(flg & (PROJECT_INVISIPASS))
                        || (cave_info[y][x] & (CAVE_MARK))))
                {
                    if (!cave_floor_bold(y_c, x_c))
                        blockage[0] = 2;
                    if (!cave_floor_bold(y_d, x_d))
                        blockage[1] = 2;
                }

                /* Try to avoid non-initial monsters/players */
                if (cave_m_idx[y_c][x_c] != 0)
                {
                    // Hack: ignore monsters on the designated square if these
                    // flags are set
                    if (!project_path_mask_matches(ignore, y_c, x_c))
                    {
                        if (flg & (PROJECT_STOP))
                            blockage[0] = 2;
                        else if (flg & (PROJECT_CHCK))
                            blockage[0] = 1;
                    }
                }
                if (cave_m_idx[y_d][x_d] != 0)
                {
                    // Hack: ignore monsters on the designated square if these
                    // flags are set
                    if (!project_path_mask_matches(ignore, y_d, x_d))
                    {
                        if (flg & (PROJECT_STOP))
                            blockage[1] = 2;
                        else if (flg & (PROJECT_CHCK))
                            blockage[1] = 1;
                    }
                }

                /* Both grids are blocked -- we have to stop now */
                if ((blockage[0] >= 2) && (blockage[1] >= 2))
                    break;

                /* Accept the first grid if possible, the second if necessary */
                if (blockage[0] <= blockage[1])
                    gp[step++] = GRID(y_c, x_c);
                else
                    gp[step++] = GRID(y_d, x_d);

                /* Re-insert the original grid, take an extra step */
                gp[step++] = GRID(y_a, x_a);

                /* Increase range to accommodate this extra step */
                range++;
            }
        }

        /* Advance to the next unexamined LOF grid */
        j += num;
    }

    /* Accept last grid as the new endpoint */
    *y2 = GRID_Y(gp[step - 1]);
    *x2 = GRID_X(gp[step - 1]);

    /* Return count of grids in projection path */
    if (monster_in_way)
        return (-step);
    else
        return (step);
}

int project_path(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg)
{
    return project_path_internal(gp, range, y1, x1, y2, x2, flg, NULL);
}

/*
 * Determine if a bolt spell cast from (y1,x1) to (y2,x2) will arrive
 * at the final destination, using the "project_path()" function to check
 * the projection path.
 *
 * Accept projection flags, and pass them onto "project_path()".
 *
 * Note that no grid is ever "projectable()" from itself.
 * This function is used to determine if the player can (easily) target
 * a given grid, if a monster can target the player, and if a clear shot
 * exists from monster to player.
 */

byte projectable_with_ignore(int y1, int x1, int y2, int x2, u32b flg,
    const project_path_mask* ignore)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    int grid_n = 0;
    u16b grid_g[512];

    int old_y2 = y2;
    int old_x2 = x2;

    /* We do not have permission to pass through walls */
    if (!(flg & (PROJECT_WALL | PROJECT_PASS)))
    {
        /* The character is the source of the projection */
        if ((y1 == py) && (x1 == px))
        {
            /* Require that destination be in line of fire */
            if (!(cave_info[y2][x2] & (CAVE_FIRE)))
                return (PROJECT_NO);
        }

        /* The character is the target of the projection */
        else if ((y2 == py) && (x2 == px))
        {
            /* Require that source be in line of fire */
            if (!(cave_info[y1][x1] & (CAVE_FIRE)))
                return (PROJECT_NO);
        }
    }

    /* Check the projection path */
    grid_n = project_path_internal(
        grid_g, MAX_RANGE, y1, x1, &y2, &x2, flg, ignore);

    /* No grid is ever projectable from itself */
    if (!grid_n)
        return (PROJECT_NO);

    /* Final grid.  As grid_n may be negative, use absolute value.  */
    y = GRID_Y(grid_g[ABS(grid_n) - 1]);
    x = GRID_X(grid_g[ABS(grid_n) - 1]);

    /* May not end in an unrequested grid */
    if ((y != old_y2) || (x != old_x2))
        return (PROJECT_NO);

    /* May not end in a wall */
    if (!cave_floor_bold(y, x))
        return (PROJECT_NO);

    /* Promise a clear bolt shot if we have verified that there is one */
    if ((flg & (PROJECT_STOP)) || (flg & (PROJECT_CHCK)))
    {
        /* Positive value for grid_n mean no obstacle was found. */
        if (grid_n > 0)
            return (PROJECT_CLEAR);
    }

    /* Assume projectable, but make no promises about clear shots */
    return (PROJECT_NOT_CLEAR);
}

byte projectable(int y1, int x1, int y2, int x2, u32b flg)
{
    return projectable_with_ignore(y1, x1, y2, x2, flg, NULL);
}

/*
 * Standard "find me a location" function
 *
 * Obtains a legal location within the given distance of the initial
 * location, and with "los()" from the source to destination location.
 *
 * This function is often called from inside a loop which searches for
 * locations while increasing the "d" distance.
 *
 * Currently the "m" parameter is unused.
 */
void scatter(int* yp, int* xp, int y, int x, int d, int m)
{
    int nx, ny;

    /* Unused parameter */
    (void)m;

    /* Pick a location */
    while (true)
    {
        /* Pick a new location */
        ny = rand_spread(y, d);
        nx = rand_spread(x, d);

        /* Ignore annoying locations */
        if (!in_bounds_fully(ny, nx))
            continue;

        /* Ignore "excessively distant" locations */
        if ((d > 1) && (distance(y, x, ny, nx) > d))
            continue;

        /* Require "line of sight" */
        if (los(y, x, ny, nx))
            break;
    }

    /* Save the location */
    (*yp) = ny;
    (*xp) = nx;
}
