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
#include "main-sdl.h"
#include "sdl-config.h"
#include "log/log.h"
#include "pane.h"
#include "cJSON.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct resolution_profile {
    int width;
    int height;
    const char* name;
    int main_view_scale;
    int aux_view_font_size;
    int pane_count;
    struct {
        enum pane_type type;
        enum pane_placement where;
        int rows;
        int cols;
    } panes[8];
};

// 4. Right pane: if we can fit ≥40 columns (using ~0.6*font_size char width), add right pane
// 5. Bottom pane: if we can fit ≥1 row below main terminal, add bottom pane
static const struct resolution_profile resolution_profiles[] = {
    { .width = 800, .height = 600, .name = "800x600 (SVGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1024, .height = 768, .name = "1024x768 (XGA)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 40 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1152, .height = 864, .name = "1152x864", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1280, .height = 720, .name = "1280x720 (HD 720p)", .main_view_scale = 1, .aux_view_font_size = 9,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1280, .height = 768, .name = "1280x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    { .width = 1280, .height = 800, .name = "1280x800 (WXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 2, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1280, .height = 960, .name = "1280x960", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1280, .height = 1024, .name = "1280x1024 (SXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1360, .height = 768, .name = "1360x768", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    { .width = 1366, .height = 768, .name = "1366x768 (HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 0, .panes = {} },
    
    { .width = 1400, .height = 1050, .name = "1400x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1440, .height = 900, .name = "1440x900", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1536, .height = 864, .name = "1536x864", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1600, .height = 900, .name = "1600x900 (HD+)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1600, .height = 1200, .name = "1600x1200 (UXGA)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1680, .height = 1050, .name = "1680x1050", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 40 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1920, .height = 1080, .name = "1920x1080 (Full HD)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 1920, .height = 1200, .name = "1920x1200 (WUXGA)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 2, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    { .width = 2048, .height = 1152, .name = "2048x1152", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 0, .panes = {} },
    
    { .width = 2256, .height = 1504, .name = "2256x1504 (Surface Laptop 13.5\")", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2160x1440
    { .width = 2160, .height = 1440, .name = "2160x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 2, .panes = { { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2304x1440 (LG UltraFine scaled)
    { .width = 2304, .height = 1440, .name = "2304x1440 (LG UltraFine scaled)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2520x1680 (MacBook Air 13" M2/M3)
    { .width = 2520, .height = 1680, .name = "2520x1680 (MacBook Air 13\" M2/M3)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1080 (Ultrawide)
    { .width = 2560, .height = 1080, .name = "2560x1080 (Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1440 (QHD)
    { .width = 2560, .height = 1440, .name = "2560x1440 (QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1600 (MacBook 13")
    { .width = 2560, .height = 1600, .name = "2560x1600 (MacBook 13\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2560x1700 (Dell XPS 17")
    { .width = 2560, .height = 1700, .name = "2560x1700 (Dell XPS 17\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2736x1824 (Surface Book)
    { .width = 2736, .height = 1824, .name = "2736x1824 (Surface Book)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1620
    { .width = 2880, .height = 1620, .name = "2880x1620", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1800 (MacBook 15")
    { .width = 2880, .height = 1800, .name = "2880x1800 (MacBook 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 2880x1920 (Surface Laptop 15")
    { .width = 2880, .height = 1920, .name = "2880x1920 (Surface Laptop 15\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3000x2000 (Surface Laptop)
    { .width = 3000, .height = 2000, .name = "3000x2000 (Surface Laptop)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3024x1964 (MacBook Pro 14" base)
    { .width = 3024, .height = 1964, .name = "3024x1964 (MacBook Pro 14\" base)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3072x1920 (MacBook Pro 16")
    { .width = 3072, .height = 1920, .name = "3072x1920 (MacBook Pro 16\")", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3200x1800
    { .width = 3200, .height = 1800, .name = "3200x1800", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3240x2160
    { .width = 3240, .height = 2160, .name = "3240x2160", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3440x1440 (Ultrawide QHD)
    { .width = 3440, .height = 1440, .name = "3440x1440 (Ultrawide QHD)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3456x2234 (MacBook Pro 14")
    { .width = 3456, .height = 2234, .name = "3456x2234 (MacBook Pro 14\")", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1080 (Super Ultrawide)
    { .width = 3840, .height = 1080, .name = "3840x1080 (Super Ultrawide)", .main_view_scale = 2, .aux_view_font_size = 16,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1200
    { .width = 3840, .height = 1200, .name = "3840x1200", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1440
    { .width = 3840, .height = 1440, .name = "3840x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x1600
    { .width = 3840, .height = 1600, .name = "3840x1600", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2160 (4K UHD)
    { .width = 3840, .height = 2160, .name = "3840x2160 (4K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 3840x2400 (Dell UltraSharp)
    { .width = 3840, .height = 2400, .name = "3840x2400 (Dell UltraSharp)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4096x2160 (DCI 4K)
    { .width = 4096, .height = 2160, .name = "4096x2160 (DCI 4K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 4480x1440
    { .width = 4480, .height = 1440, .name = "4480x1440", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x1440 (Super Ultrawide)
    { .width = 5120, .height = 1440, .name = "5120x1440 (Super Ultrawide)", .main_view_scale = 3, .aux_view_font_size = 18,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2160 (5K Ultrawide)
    { .width = 5120, .height = 2160, .name = "5120x2160 (5K Ultrawide)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 5120x2880 (5K)
    { .width = 5120, .height = 2880, .name = "5120x2880 (5K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 6016x3384 (6K)
    { .width = 6016, .height = 3384, .name = "6016x3384 (6K)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } },
    
    // 7680x4320 (8K UHD)
    { .width = 7680, .height = 4320, .name = "7680x4320 (8K UHD)", .main_view_scale = 4, .aux_view_font_size = 24,
      .pane_count = 5, .panes = { { PANE_INVENTORY, PLACE_RIGHT, 22, 50 }, { PANE_WORN, PLACE_RIGHT, 17, 0 },
                                  { PANE_INFO, PLACE_RIGHT, 0, 0 }, { PANE_ROLLS, PLACE_BOTTOM, 4, 0 }, { PANE_LOG, PLACE_BOTTOM, 0, 0 } } }
};

#define NUM_RESOLUTION_PROFILES (sizeof(resolution_profiles) / sizeof(resolution_profiles[0]))

static const char* pane_type_to_string(enum pane_type type)
{
    switch (type) {
        case PANE_MAIN: return "MAIN";
        case PANE_INVENTORY: return "INVENTORY";
        case PANE_WORN: return "WORN";
        case PANE_ROLLS: return "ROLLS";
        case PANE_INFO: return "INFO";
        case PANE_CHARACTER: return "CHARACTER";
        case PANE_LOG: return "LOG";
        case PANE_MONSTERS: return "MONSTERS";
        case PANE_TOUCH: return "TOUCH";
        default: return "MAIN";
    }
}

static enum pane_type parse_pane_type(const char* value)
{
    if (!value)
        return PANE_MAIN;
    if (strcmp(value, "MAIN") == 0) return PANE_MAIN;
    if (strcmp(value, "INVENTORY") == 0) return PANE_INVENTORY;
    if (strcmp(value, "WORN") == 0) return PANE_WORN;
    if (strcmp(value, "ROLLS") == 0) return PANE_ROLLS;
    if (strcmp(value, "INFO") == 0) return PANE_INFO;
    if (strcmp(value, "CHARACTER") == 0) return PANE_CHARACTER;
    if (strcmp(value, "LOG") == 0) return PANE_LOG;
    if (strcmp(value, "MONSTERS") == 0) return PANE_MONSTERS;
    if (strcmp(value, "TOUCH") == 0) return PANE_TOUCH;
    return PANE_MAIN;
}

static const char* pane_placement_to_string(enum pane_placement where)
{
    return pane_placement_name(where);
}

static enum pane_placement parse_pane_placement(const char* value)
{
    if (!value)
        return PLACE_RIGHT;
    if (strcmp(value, "BOTTOM") == 0) return PLACE_BOTTOM;
    if (strcmp(value, "DOUBLE_BOTTOM") == 0 || strcmp(value, "DOUBLE BOTTOM") == 0)
        return PLACE_DOUBLE_BOTTOM;
    if (strcmp(value, "RIGHT") == 0) return PLACE_RIGHT;
    if (strcmp(value, "LEFT") == 0) return PLACE_LEFT;
    if (strcmp(value, "DOUBLE_LEFT") == 0 || strcmp(value, "DOUBLE LEFT") == 0)
        return PLACE_DOUBLE_LEFT;
    if (strcmp(value, "DOUBLE_RIGHT") == 0 || strcmp(value, "DOUBLE RIGHT") == 0)
        return PLACE_DOUBLE_RIGHT;
    return PLACE_RIGHT;
}

static const char* min_terminal_mode_to_string(int mode)
{
    switch (mode) {
        case SDL_MIN_TERMINAL_COMPACT: return "COMPACT";
        case SDL_MIN_TERMINAL_NORMAL: return "NORMAL";
        default: return "NORMAL";
    }
}

static int parse_min_terminal_mode(const char* value)
{
    if (!value)
        return SDL_MIN_TERMINAL_NORMAL;
    if (strcmp(value, "COMPACT") == 0) return SDL_MIN_TERMINAL_COMPACT;
    if (strcmp(value, "NORMAL") == 0) return SDL_MIN_TERMINAL_NORMAL;
    return SDL_MIN_TERMINAL_NORMAL;
}

static bool sdl_overlay_density_is_valid(int value)
{
    return value >= SDL_OVERLAY_DENSITY_AUTO
        && value <= SDL_OVERLAY_DENSITY_LARGE;
}

static const char* sdl_overlay_density_to_string(int value)
{
    switch (value) {
    case SDL_OVERLAY_DENSITY_COMPACT:
        return "COMPACT";
    case SDL_OVERLAY_DENSITY_ROOMY:
        return "ROOMY";
    case SDL_OVERLAY_DENSITY_LARGE:
        return "LARGE";
    case SDL_OVERLAY_DENSITY_AUTO:
    default:
        return "AUTO";
    }
}

static int parse_overlay_density(const char* value)
{
    if (!value)
        return SDL_OVERLAY_DENSITY_AUTO;
    if (strcmp(value, "COMPACT") == 0)
        return SDL_OVERLAY_DENSITY_COMPACT;
    if (strcmp(value, "ROOMY") == 0 || strcmp(value, "NORMAL") == 0)
        return SDL_OVERLAY_DENSITY_ROOMY;
    if (strcmp(value, "LARGE") == 0)
        return SDL_OVERLAY_DENSITY_LARGE;
    return SDL_OVERLAY_DENSITY_AUTO;
}

static char* read_file_contents(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        log_debug("Could not open JSON file: %s", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        log_error("Failed to allocate memory for JSON file");
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);
    
    return content;
}

static const u16b movement_direction_order[] = {
    APP_MOVEMENT_DIRECTION_NORTHWEST,
    APP_MOVEMENT_DIRECTION_NORTH,
    APP_MOVEMENT_DIRECTION_NORTHEAST,
    APP_MOVEMENT_DIRECTION_WEST,
    APP_MOVEMENT_DIRECTION_EAST,
    APP_MOVEMENT_DIRECTION_SOUTHWEST,
    APP_MOVEMENT_DIRECTION_SOUTH,
    APP_MOVEMENT_DIRECTION_SOUTHEAST
};

static const char* sdl_config_movement_preset_name(u16b preset_id)
{
    switch (preset_id)
    {
    case APP_MOVEMENT_PRESET_MODERN_ARROWS:
        return "modernArrows";
    case APP_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        return "modernWasdQezc";
    case APP_MOVEMENT_PRESET_VI_KEYS:
        return "viKeys";
    case APP_MOVEMENT_PRESET_CLASSIC_SIL:
        return "classicSil";
    default:
        return "custom";
    }
}

static bool sdl_config_movement_preset_from_name(const char* name,
    u16b* out_preset_id)
{
    u16b preset_id = APP_MOVEMENT_PRESET_NONE;

    if (!name)
        return false;

    if (streq(name, "modernArrows"))
        preset_id = APP_MOVEMENT_PRESET_MODERN_ARROWS;
    else if (streq(name, "modernWasdQezc"))
        preset_id = APP_MOVEMENT_PRESET_MODERN_WASD_QEZC;
    else if (streq(name, "viKeys"))
        preset_id = APP_MOVEMENT_PRESET_VI_KEYS;
    else if (streq(name, "classicSil"))
        preset_id = APP_MOVEMENT_PRESET_CLASSIC_SIL;
    else if (!streq(name, "custom"))
        return false;

    if (out_preset_id)
        *out_preset_id = preset_id;

    return true;
}

static const char* sdl_config_movement_context_name(u16b context)
{
    switch (context)
    {
    case APP_MOVEMENT_CONTEXT_DUNGEON:
        return "dungeon";
    case APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT:
        return "directionPrompt";
    case APP_MOVEMENT_CONTEXT_TARGETING:
        return "targeting";
    case APP_MOVEMENT_CONTEXT_ANY:
    default:
        return "any";
    }
}

static bool sdl_config_movement_context_from_name(const char* name,
    u16b* out_context)
{
    u16b context = APP_MOVEMENT_CONTEXT_ANY;

    if (!name)
        return false;

    if (streq(name, "any"))
        context = APP_MOVEMENT_CONTEXT_ANY;
    else if (streq(name, "dungeon"))
        context = APP_MOVEMENT_CONTEXT_DUNGEON;
    else if (streq(name, "directionPrompt"))
        context = APP_MOVEMENT_CONTEXT_DIRECTION_PROMPT;
    else if (streq(name, "targeting"))
        context = APP_MOVEMENT_CONTEXT_TARGETING;
    else
        return false;

    if (out_context)
        *out_context = context;

    return true;
}

static const char* sdl_config_movement_action_name(u16b action)
{
    switch (action)
    {
    case APP_MOVEMENT_ACTION_MOVE_DIR:
        return "moveDir";
    case APP_MOVEMENT_ACTION_RUN_DIR:
        return "runDir";
    case APP_MOVEMENT_ACTION_INTERACT_DIR:
        return "interactDir";
    case APP_MOVEMENT_ACTION_WAIT:
        return "wait";
    case APP_MOVEMENT_ACTION_REST:
        return "rest";
    default:
        return "none";
    }
}

static bool sdl_config_movement_action_from_name(const char* name,
    u16b* out_action)
{
    u16b action = APP_MOVEMENT_ACTION_NONE;

    if (!name)
        return false;

    if (streq(name, "moveDir"))
        action = APP_MOVEMENT_ACTION_MOVE_DIR;
    else if (streq(name, "runDir"))
        action = APP_MOVEMENT_ACTION_RUN_DIR;
    else if (streq(name, "interactDir"))
        action = APP_MOVEMENT_ACTION_INTERACT_DIR;
    else if (streq(name, "wait"))
        action = APP_MOVEMENT_ACTION_WAIT;
    else if (streq(name, "rest"))
        action = APP_MOVEMENT_ACTION_REST;
    else
        return false;

    if (out_action)
        *out_action = action;

    return true;
}

static const char* sdl_config_movement_direction_name(u16b direction)
{
    switch (direction)
    {
    case APP_MOVEMENT_DIRECTION_CENTER:
        return "center";
    case APP_MOVEMENT_DIRECTION_NORTH:
        return "north";
    case APP_MOVEMENT_DIRECTION_NORTHEAST:
        return "northeast";
    case APP_MOVEMENT_DIRECTION_EAST:
        return "east";
    case APP_MOVEMENT_DIRECTION_SOUTHEAST:
        return "southeast";
    case APP_MOVEMENT_DIRECTION_SOUTH:
        return "south";
    case APP_MOVEMENT_DIRECTION_SOUTHWEST:
        return "southwest";
    case APP_MOVEMENT_DIRECTION_WEST:
        return "west";
    case APP_MOVEMENT_DIRECTION_NORTHWEST:
        return "northwest";
    default:
        return "none";
    }
}

static bool sdl_config_movement_direction_from_name(const char* name,
    u16b* out_direction)
{
    u16b direction = APP_MOVEMENT_DIRECTION_NONE;

    if (!name)
        return false;

    if (streq(name, "none"))
        direction = APP_MOVEMENT_DIRECTION_NONE;
    else if (streq(name, "center"))
        direction = APP_MOVEMENT_DIRECTION_CENTER;
    else if (streq(name, "north"))
        direction = APP_MOVEMENT_DIRECTION_NORTH;
    else if (streq(name, "northeast"))
        direction = APP_MOVEMENT_DIRECTION_NORTHEAST;
    else if (streq(name, "east"))
        direction = APP_MOVEMENT_DIRECTION_EAST;
    else if (streq(name, "southeast"))
        direction = APP_MOVEMENT_DIRECTION_SOUTHEAST;
    else if (streq(name, "south"))
        direction = APP_MOVEMENT_DIRECTION_SOUTH;
    else if (streq(name, "southwest"))
        direction = APP_MOVEMENT_DIRECTION_SOUTHWEST;
    else if (streq(name, "west"))
        direction = APP_MOVEMENT_DIRECTION_WEST;
    else if (streq(name, "northwest"))
        direction = APP_MOVEMENT_DIRECTION_NORTHWEST;
    else
        return false;

    if (out_direction)
        *out_direction = direction;

    return true;
}

static bool sdl_config_movement_binding_equals(
    const app_movement_binding* left, const app_movement_binding* right)
{
    if (!left || !right)
        return false;

    return left->context == right->context
        && left->action == right->action
        && left->direction == right->direction
        && left->device == right->device
        && left->input_type == right->input_type
        && left->required_modifiers == right->required_modifiers
        && left->forbidden_modifiers == right->forbidden_modifiers
        && left->trigger == right->trigger
        && left->trigger_aux == right->trigger_aux;
}

static bool sdl_config_movement_append_binding(struct sdl_config* cfg,
    const app_movement_binding* binding)
{
    u16b i;

    if (!cfg || !binding || !app_movement_binding_is_valid(binding))
        return false;
    if (cfg->movement_binding_count >= SDL_MOVEMENT_BINDING_MAX)
        return false;

    for (i = 0; i < cfg->movement_binding_count; i++)
    {
        if (sdl_config_movement_binding_equals(&cfg->movement_bindings[i],
                binding))
        {
            return true;
        }
    }

    cfg->movement_bindings[cfg->movement_binding_count++] = *binding;
    return true;
}

static void sdl_config_init_keyboard_binding(app_movement_binding* binding,
    u16b action, u16b direction, SDL_Scancode scancode, u16b required_modifiers,
    u16b forbidden_modifiers)
{
    app_movement_binding_clear(binding);
    binding->context = APP_MOVEMENT_CONTEXT_ANY;
    binding->action = action;
    binding->direction = direction;
    binding->device = APP_INPUT_DEVICE_KEYBOARD;
    binding->input_type = APP_INPUT_TYPE_KEY;
    binding->required_modifiers = required_modifiers;
    binding->forbidden_modifiers = forbidden_modifiers;
    binding->trigger = (u32b)scancode;
}

static bool sdl_config_add_keyboard_binding(struct sdl_config* cfg,
    u16b action, u16b direction, SDL_Scancode scancode, u16b required_modifiers,
    u16b forbidden_modifiers)
{
    app_movement_binding binding;

    if (!cfg || scancode == SDL_SCANCODE_UNKNOWN)
        return false;

    sdl_config_init_keyboard_binding(&binding, action, direction, scancode,
        required_modifiers, forbidden_modifiers);
    return sdl_config_movement_append_binding(cfg, &binding);
}

static void sdl_config_add_directional_preset_set(struct sdl_config* cfg,
    const SDL_Scancode* primary_scancodes)
{
    static const SDL_Scancode keypad_scancodes[] = {
        SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
        SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
        SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3
    };
    const u16b plain_forbidden = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT
        | APP_INPUT_MODIFIER_META;
    const u16b shift_forbidden = APP_INPUT_MODIFIER_CTRL
        | APP_INPUT_MODIFIER_ALT | APP_INPUT_MODIFIER_META;
    const u16b ctrl_forbidden = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_ALT | APP_INPUT_MODIFIER_META;
    size_t i;

    for (i = 0; i < N_ELEMENTS(movement_direction_order); i++)
    {
        u16b direction = movement_direction_order[i];

        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_MOVE_DIR, direction, primary_scancodes[i], 0,
            plain_forbidden);
        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_MOVE_DIR, direction, keypad_scancodes[i], 0,
            plain_forbidden);
        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_RUN_DIR, direction, primary_scancodes[i],
            APP_INPUT_MODIFIER_SHIFT, shift_forbidden);
        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_RUN_DIR, direction, keypad_scancodes[i],
            APP_INPUT_MODIFIER_SHIFT, shift_forbidden);
        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_INTERACT_DIR, direction, primary_scancodes[i],
            APP_INPUT_MODIFIER_CTRL, ctrl_forbidden);
        (void)sdl_config_add_keyboard_binding(cfg,
            APP_MOVEMENT_ACTION_INTERACT_DIR, direction, keypad_scancodes[i],
            APP_INPUT_MODIFIER_CTRL, ctrl_forbidden);
    }
}

static void sdl_config_add_wait_rest_bindings(struct sdl_config* cfg,
    SDL_Scancode wait_primary, SDL_Scancode rest_primary,
    SDL_Scancode wait_secondary, SDL_Scancode rest_secondary)
{
    const u16b plain_forbidden = APP_INPUT_MODIFIER_SHIFT
        | APP_INPUT_MODIFIER_CTRL | APP_INPUT_MODIFIER_ALT
        | APP_INPUT_MODIFIER_META;
    const u16b shift_forbidden = APP_INPUT_MODIFIER_CTRL
        | APP_INPUT_MODIFIER_ALT | APP_INPUT_MODIFIER_META;

    (void)sdl_config_add_keyboard_binding(cfg, APP_MOVEMENT_ACTION_WAIT,
        APP_MOVEMENT_DIRECTION_NONE, wait_primary, 0, plain_forbidden);
    (void)sdl_config_add_keyboard_binding(cfg, APP_MOVEMENT_ACTION_WAIT,
        APP_MOVEMENT_DIRECTION_NONE, wait_secondary, 0, plain_forbidden);
    (void)sdl_config_add_keyboard_binding(cfg, APP_MOVEMENT_ACTION_REST,
        APP_MOVEMENT_DIRECTION_NONE, rest_primary, APP_INPUT_MODIFIER_SHIFT,
        shift_forbidden);
    (void)sdl_config_add_keyboard_binding(cfg, APP_MOVEMENT_ACTION_REST,
        APP_MOVEMENT_DIRECTION_NONE, rest_secondary, APP_INPUT_MODIFIER_SHIFT,
        shift_forbidden);
}

void sdl_config_clear_movement_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    cfg->movement_keyboard_present = false;
    cfg->movement_keyboard_preset = APP_MOVEMENT_PRESET_NONE;
    cfg->movement_binding_count = 0;
    memset(cfg->movement_bindings, 0, sizeof(cfg->movement_bindings));
}

void sdl_config_set_default_movement_bindings(struct sdl_config* cfg,
    u16b preset_id)
{
    static const SDL_Scancode classic_scancodes[] = {
        SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9,
        SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_6,
        SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3
    };
    static const SDL_Scancode arrows_scancodes[] = {
        SDL_SCANCODE_HOME, SDL_SCANCODE_UP, SDL_SCANCODE_PAGEUP,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_END, SDL_SCANCODE_DOWN, SDL_SCANCODE_PAGEDOWN
    };
    static const SDL_Scancode wasd_scancodes[] = {
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E,
        SDL_SCANCODE_A, SDL_SCANCODE_D,
        SDL_SCANCODE_Z, SDL_SCANCODE_S, SDL_SCANCODE_C
    };
    static const SDL_Scancode vi_scancodes[] = {
        SDL_SCANCODE_Y, SDL_SCANCODE_K, SDL_SCANCODE_U,
        SDL_SCANCODE_H, SDL_SCANCODE_L,
        SDL_SCANCODE_B, SDL_SCANCODE_J, SDL_SCANCODE_N
    };
    const SDL_Scancode* directional_scancodes = classic_scancodes;

    if (!cfg)
        return;

    switch (preset_id)
    {
    case APP_MOVEMENT_PRESET_MODERN_ARROWS:
        directional_scancodes = arrows_scancodes;
        break;
    case APP_MOVEMENT_PRESET_MODERN_WASD_QEZC:
        directional_scancodes = wasd_scancodes;
        break;
    case APP_MOVEMENT_PRESET_VI_KEYS:
        directional_scancodes = vi_scancodes;
        break;
    case APP_MOVEMENT_PRESET_CLASSIC_SIL:
    default:
        directional_scancodes = classic_scancodes;
        preset_id = APP_MOVEMENT_PRESET_CLASSIC_SIL;
        break;
    }

    sdl_config_clear_movement_bindings(cfg);
    cfg->movement_keyboard_present = true;
    cfg->movement_keyboard_preset = preset_id;
    sdl_config_add_directional_preset_set(cfg, directional_scancodes);

    if (preset_id == APP_MOVEMENT_PRESET_CLASSIC_SIL)
    {
        /* On Windows, Shift+numpad navigation can surface as the navigation
         * cluster rather than keypad scancodes. Keep classic numpad defaults,
         * but accept the navigation-cluster scancodes too so running and
         * directed interact continue to work in deployment builds.
         */
        sdl_config_add_directional_preset_set(cfg, arrows_scancodes);
        sdl_config_add_wait_rest_bindings(cfg, SDL_SCANCODE_KP_5,
            SDL_SCANCODE_KP_5, SDL_SCANCODE_Z, SDL_SCANCODE_Z);
    }
    else
    {
        sdl_config_add_wait_rest_bindings(cfg, SDL_SCANCODE_PERIOD,
            SDL_SCANCODE_PERIOD, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_5);
    }
}

bool sdl_config_has_movement_bindings(const struct sdl_config* cfg)
{
    return cfg && cfg->movement_binding_count > 0;
}

static void sdl_config_load_movement_bindings(cJSON* root,
    struct sdl_config* cfg)
{
    cJSON* movement;
    cJSON* preset;
    cJSON* bindings;
    int binding_count;
    int i;

    if (!root || !cfg)
        return;

    movement = cJSON_GetObjectItemCaseSensitive(root, "movement");
    if (!cJSON_IsObject(movement))
        return;

    cfg->movement_keyboard_present = true;

    preset = cJSON_GetObjectItemCaseSensitive(movement, "keyboardPreset");
    if (cJSON_IsString(preset) && preset->valuestring)
    {
        (void)sdl_config_movement_preset_from_name(preset->valuestring,
            &cfg->movement_keyboard_preset);
    }

    bindings = cJSON_GetObjectItemCaseSensitive(movement, "keyboardBindings");
    if (!cJSON_IsArray(bindings))
        return;

    binding_count = cJSON_GetArraySize(bindings);
    for (i = 0; i < binding_count && i < SDL_MOVEMENT_BINDING_MAX; i++)
    {
        cJSON* item = cJSON_GetArrayItem(bindings, i);
        cJSON* context_item;
        cJSON* action_item;
        cJSON* direction_item;
        cJSON* trigger_item;
        cJSON* trigger_aux_item;
        cJSON* required_item;
        cJSON* forbidden_item;
        app_movement_binding binding;

        if (!cJSON_IsObject(item))
            continue;

        app_movement_binding_clear(&binding);
        binding.device = APP_INPUT_DEVICE_KEYBOARD;
        binding.input_type = APP_INPUT_TYPE_KEY;

        context_item = cJSON_GetObjectItemCaseSensitive(item, "context");
        action_item = cJSON_GetObjectItemCaseSensitive(item, "action");
        direction_item = cJSON_GetObjectItemCaseSensitive(item, "direction");
        trigger_item = cJSON_GetObjectItemCaseSensitive(item, "trigger");
        trigger_aux_item = cJSON_GetObjectItemCaseSensitive(item, "triggerAux");
        required_item = cJSON_GetObjectItemCaseSensitive(item,
            "requiredModifiers");
        forbidden_item = cJSON_GetObjectItemCaseSensitive(item,
            "forbiddenModifiers");

        if (!cJSON_IsString(context_item) || !context_item->valuestring
            || !sdl_config_movement_context_from_name(context_item->valuestring,
                &binding.context))
        {
            continue;
        }

        if (!cJSON_IsString(action_item) || !action_item->valuestring
            || !sdl_config_movement_action_from_name(action_item->valuestring,
                &binding.action))
        {
            continue;
        }

        if (cJSON_IsString(direction_item) && direction_item->valuestring)
        {
            if (!sdl_config_movement_direction_from_name(
                    direction_item->valuestring, &binding.direction))
            {
                continue;
            }
        }

        if (!cJSON_IsNumber(trigger_item) || trigger_item->valueint <= 0)
            continue;

        binding.trigger = (u32b)trigger_item->valueint;
        if (cJSON_IsNumber(trigger_aux_item) && trigger_aux_item->valueint > 0)
            binding.trigger_aux = (u32b)trigger_aux_item->valueint;
        if (cJSON_IsNumber(required_item) && required_item->valueint >= 0)
            binding.required_modifiers = (u16b)required_item->valueint;
        if (cJSON_IsNumber(forbidden_item) && forbidden_item->valueint >= 0)
            binding.forbidden_modifiers = (u16b)forbidden_item->valueint;

        if (!sdl_config_movement_append_binding(cfg, &binding))
        {
            log_warn("Skipping invalid or duplicate movement binding at JSON index %d",
                i);
        }
    }
}

static void sdl_config_save_movement_bindings(cJSON* root,
    const struct sdl_config* cfg)
{
    cJSON* movement;
    cJSON* bindings;
    u16b i;

    if (!root || !cfg)
        return;
    if (!cfg->movement_keyboard_present && cfg->movement_binding_count == 0
        && cfg->movement_keyboard_preset == APP_MOVEMENT_PRESET_NONE)
    {
        return;
    }

    movement = cJSON_CreateObject();
    if (!movement)
        return;

    cJSON_AddNumberToObject(movement, "version", APP_MOVEMENT_FORMAT_VERSION);
    cJSON_AddStringToObject(movement, "keyboardPreset",
        sdl_config_movement_preset_name(cfg->movement_keyboard_preset));

    bindings = cJSON_CreateArray();
    if (bindings)
    {
        for (i = 0; i < cfg->movement_binding_count; i++)
        {
            const app_movement_binding* binding = &cfg->movement_bindings[i];
            cJSON* item;

            if (!app_movement_binding_is_valid(binding))
                continue;

            item = cJSON_CreateObject();
            if (!item)
                continue;

            cJSON_AddStringToObject(item, "context",
                sdl_config_movement_context_name(binding->context));
            cJSON_AddStringToObject(item, "action",
                sdl_config_movement_action_name(binding->action));
            cJSON_AddStringToObject(item, "direction",
                sdl_config_movement_direction_name(binding->direction));
            cJSON_AddNumberToObject(item, "trigger", (double)binding->trigger);
            if (binding->trigger_aux != 0)
            {
                cJSON_AddNumberToObject(item, "triggerAux",
                    (double)binding->trigger_aux);
            }
            cJSON_AddNumberToObject(item, "requiredModifiers",
                (double)binding->required_modifiers);
            cJSON_AddNumberToObject(item, "forbiddenModifiers",
                (double)binding->forbidden_modifiers);
            cJSON_AddItemToArray(bindings, item);
        }

        cJSON_AddItemToObject(movement, "keyboardBindings", bindings);
    }

    cJSON_AddItemToObject(root, "movement", movement);
}

static bool g_app_intro_seen = false;

static const byte app_interface_options[] = {
    OPT_system_beep, OPT_quick_messages, OPT_auto_more, OPT_easy_main_menu,
    OPT_hjkl_movement, OPT_angband_keyset, OPT_space_acts_as_comma,
    OPT_look_objects_sort_by_difficulty, OPT_show_level_generation_debug,
    OPT_NONE
};

static const byte app_text_options[] = {
    OPT_story_lists, OPT_story_lists_inven, OPT_story_lists_inven_pane,
    OPT_story_lists_equip, OPT_story_lists_equip_pane, OPT_story_monster_desc,
    OPT_story_monster_desc_pane, OPT_story_character_sheet,
    OPT_NONE
};

static const byte app_efficiency_options[] = {
    OPT_instant_run, OPT_center_player, OPT_run_avoid_center,
    OPT_NONE
};

static const byte app_gameplay_options[] = {
    OPT_pacifist_attack_warning, OPT_unlock_blitz_mode,
    OPT_NONE
};

static const byte app_visual_options[] = {
    OPT_auto_display_lists, OPT_artifact_unique_color, OPT_hilite_player,
    OPT_hilite_target, OPT_hilite_unwary, OPT_solid_walls, OPT_hybrid_walls,
    OPT_unidentified_items_slate, OPT_stealth_vision, OPT_sleep_icon,
    OPT_show_smithing_difficulty, OPT_show_smithing_difficulty_look,
    OPT_show_elemental_item_rolls,
    OPT_NONE
};

static bool option_list_contains(const byte* ids, int opt)
{
    if (!ids)
        return false;

    for (int i = 0; ids[i] != OPT_NONE; i++) {
        if ((int)ids[i] == opt)
            return true;
    }

    return false;
}

bool option_is_app_persistent(int opt)
{
    /* Multi-value non-bool options saved explicitly in the visual JSON block */
    if (opt == OPT_intro_style || opt == OPT_banner_popup_seconds
        || opt == OPT_hide_left_panel || opt == OPT_min_depth_timer_mode)
        return true;
    return option_list_contains(app_interface_options, opt)
        || option_list_contains(app_text_options, opt)
        || option_list_contains(app_efficiency_options, opt)
        || option_list_contains(app_gameplay_options, opt)
        || option_list_contains(app_visual_options, opt);
}

static void sdl_config_apply_app_option_defaults(void)
{
    if (!op_ptr)
        return;

    op_ptr->delay_factor = 5;
    op_ptr->hitpoint_warn = 3;
    op_ptr->main_combat_rolls = platform_steamdeck_mode() ? 2 : 0;
    op_ptr->narrative_banner_seconds = NARRATIVE_BANNER_SECONDS_DEFAULT;
    op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
#if defined(__ANDROID__) || defined(SIL_IOS)
    op_ptr->ability_desc_mode = 1;
#else
    op_ptr->ability_desc_mode = 0;
#endif
    op_ptr->intro_style = INTRO_STYLE_RANDOM;
}

static void sdl_config_load_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    cJSON* group = cJSON_GetObjectItemCaseSensitive(app_options, group_name);
    if (!op_ptr)
        return;
    if (!cJSON_IsObject(group))
        return;

    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];
        cJSON* item;

        if (!key)
            continue;

        item = cJSON_GetObjectItemCaseSensitive(group, key);
        if (cJSON_IsBool(item))
            op_ptr->opt[opt] = cJSON_IsTrue(item);
    }
}

static void sdl_config_save_app_option_group(cJSON* app_options,
    const char* group_name, const byte* option_ids)
{
    if (!op_ptr)
        return;

    cJSON* group = cJSON_CreateObject();
    if (!group)
        return;

    for (int i = 0; option_ids[i] != OPT_NONE; i++) {
        int opt = option_ids[i];
        cptr key = option_text[opt];

        if (!key)
            continue;

        cJSON_AddBoolToObject(group, key, op_ptr->opt[opt]);
    }

    cJSON_AddItemToObject(app_options, group_name, group);
}

static void sdl_config_load_byte_value(cJSON* parent, const char* key,
    byte* out_value, byte max_value)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (!cJSON_IsNumber(item))
        return;

    if (item->valueint < 0)
        return;

    if (item->valueint > max_value) {
        *out_value = max_value;
        return;
    }

    *out_value = (byte)item->valueint;
}

void sdl_config_load_app_options(const char* filename)
{
    char* content;
    cJSON* root;
    cJSON* app_options;
    cJSON* item;
    bool config_exists = false;

    sdl_config_apply_app_option_defaults();

    if (filename && filename[0])
        config_exists = SDL_GetPathInfo(filename, NULL);

    g_app_intro_seen = config_exists;

    if (!filename || !filename[0]) {
        log_warn("sdl_config_load_app_options: no config filename provided");
        return;
    }

    content = read_file_contents(filename);
    if (!content) {
        log_debug("No app options found in SDL config, using defaults");
        return;
    }

    root = cJSON_Parse(content);
    free(content);

    if (!root) {
        log_warn("sdl_config_load_app_options: failed to parse %s", filename);
        return;
    }

    app_options = cJSON_GetObjectItemCaseSensitive(root, "appOptions");
    if (!cJSON_IsObject(app_options)) {
        cJSON_Delete(root);
        return;
    }

    if (!op_ptr) {
        cJSON_Delete(root);
        return;
    }

    item = cJSON_GetObjectItemCaseSensitive(app_options, "introSeen");
    if (cJSON_IsBool(item))
        g_app_intro_seen = cJSON_IsTrue(item);

    sdl_config_load_app_option_group(app_options, "interface", app_interface_options);
    sdl_config_load_app_option_group(app_options, "text", app_text_options);
    sdl_config_load_app_option_group(app_options, "efficiency", app_efficiency_options);
    sdl_config_load_app_option_group(app_options, "gameplay", app_gameplay_options);
    sdl_config_load_app_option_group(app_options, "visual", app_visual_options);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
    if (cJSON_IsObject(item))
        sdl_config_load_byte_value(item, "hitpointWarning", &op_ptr->hitpoint_warn, 9);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "efficiency");
    if (cJSON_IsObject(item))
        sdl_config_load_byte_value(item, "delayFactor", &op_ptr->delay_factor, 9);

    item = cJSON_GetObjectItemCaseSensitive(app_options, "gameplay");
    if (cJSON_IsObject(item)) {
        sdl_config_load_byte_value(item, "minDepthTimerMode",
            &op_ptr->min_depth_timer_mode, MIN_DEPTH_TIMER_MODE_MAX);
    }

    item = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
    if (cJSON_IsObject(item)) {
        sdl_config_load_byte_value(item, "mainCombatRolls", &op_ptr->main_combat_rolls, 4);
        sdl_config_load_byte_value(item, "bannerPopupSeconds",
            &op_ptr->narrative_banner_seconds, NARRATIVE_BANNER_SECONDS_MAX);
        sdl_config_load_byte_value(item, "abilityDescMode", &op_ptr->ability_desc_mode, 2);
        sdl_config_load_byte_value(item, "introStyle", &op_ptr->intro_style, INTRO_STYLE_RANDOM);
    }

    cJSON_Delete(root);
}

bool platform_intro_should_force_flame(void)
{
    return !g_app_intro_seen;
}

void platform_intro_mark_seen(void)
{
    g_app_intro_seen = true;
}

static void sdl_config_load_touch_binding_array(cJSON* array, int* dst, int max_count)
{
    int count;

    if (!cJSON_IsArray(array) || !dst || max_count <= 0)
        return;

    count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count && i < count; i++) {
        cJSON* binding = cJSON_GetArrayItem(array, i);
        if (cJSON_IsNumber(binding))
            dst[i] = binding->valueint;
    }
}

static void sdl_config_load_touch_label_array(cJSON* array, char dst[][SDL_TOUCH_PANE_LABEL_LEN], int max_count)
{
    int count;

    if (!cJSON_IsArray(array) || !dst || max_count <= 0)
        return;

    count = cJSON_GetArraySize(array);
    for (int i = 0; i < max_count && i < count; i++) {
        cJSON* label = cJSON_GetArrayItem(array, i);
        if (cJSON_IsString(label) && label->valuestring) {
            SDL_strlcpy(dst[i], label->valuestring, SDL_TOUCH_PANE_LABEL_LEN);
        }
    }
}

static cJSON* sdl_config_create_int_array(const int* src, int count)
{
    cJSON* array;

    if (!src || count <= 0)
        return NULL;

    array = cJSON_CreateArray();
    if (!array)
        return NULL;

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateNumber(src[i]));
    }

    return array;
}

static void sdl_config_clear_gamepad_combo_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
        for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++)
            cfg->gamepad_button_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++)
            cfg->gamepad_trigger_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
            cfg->gamepad_left_stick_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
            cfg->gamepad_right_stick_combo_bindings[modifier][i] = GAMEPAD_BIND_NONE;
        }
    }
}

static const char* sdl_config_gamepad_button_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftButtonBindings",
    "ctrlButtonBindings",
    "altButtonBindings",
};

static const char* sdl_config_gamepad_trigger_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftTriggerBindings",
    "ctrlTriggerBindings",
    "altTriggerBindings",
};

static const char* sdl_config_gamepad_left_stick_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftLeftStickBindings",
    "ctrlLeftStickBindings",
    "altLeftStickBindings",
};

static const char* sdl_config_gamepad_right_stick_combo_names[GAMEPAD_MODIFIER_COUNT] = {
    "shiftRightStickBindings",
    "ctrlRightStickBindings",
    "altRightStickBindings",
};

static cJSON* sdl_config_create_string_array(const char src[][SDL_TOUCH_PANE_LABEL_LEN], int count)
{
    cJSON* array;

    if (!src || count <= 0)
        return NULL;

    array = cJSON_CreateArray();
    if (!array)
        return NULL;

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateString(src[i]));
    }

    return array;
}

static void sdl_config_load_overlay_panels(cJSON* root,
    struct sdl_config* cfg)
{
    cJSON* overlays;
    cJSON* item;
    int count = 0;

    if (!root || !cfg)
        return;

    overlays = cJSON_GetObjectItemCaseSensitive(root, "overlayPanels");
    if (!cJSON_IsArray(overlays))
        return;

    cJSON_ArrayForEach(item, overlays) {
        cJSON* id;
        cJSON* pinned;
        cJSON* x;
        cJSON* y;
        struct sdl_overlay_panel_config* overlay;

        if (count >= SDL_OVERLAY_PANEL_CONFIG_MAX)
            break;
        if (!cJSON_IsObject(item))
            continue;

        id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!cJSON_IsString(id) || !id->valuestring || !id->valuestring[0])
            continue;

        overlay = &cfg->overlay_panels[count];
        memset(overlay, 0, sizeof(*overlay));
        SDL_strlcpy(overlay->id, id->valuestring, sizeof(overlay->id));

        pinned = cJSON_GetObjectItemCaseSensitive(item, "pinned");
        overlay->pinned = cJSON_IsBool(pinned) && cJSON_IsTrue(pinned);

        x = cJSON_GetObjectItemCaseSensitive(item, "x");
        if (cJSON_IsNumber(x))
            overlay->x = x->valueint;

        y = cJSON_GetObjectItemCaseSensitive(item, "y");
        if (cJSON_IsNumber(y))
            overlay->y = y->valueint;

        count++;
    }

    cfg->overlay_panel_count = count;
    log_debug("Loaded %d overlay panel layout entries", count);
}

static void sdl_config_save_overlay_panels(cJSON* root,
    const struct sdl_config* cfg)
{
    cJSON* overlays;

    if (!root || !cfg || cfg->overlay_panel_count <= 0)
        return;

    overlays = cJSON_CreateArray();
    if (!overlays)
        return;

    for (int i = 0; i < cfg->overlay_panel_count
        && i < SDL_OVERLAY_PANEL_CONFIG_MAX; i++)
    {
        const struct sdl_overlay_panel_config* overlay = &cfg->overlay_panels[i];
        cJSON* item;

        if (!overlay->id[0])
            continue;

        item = cJSON_CreateObject();
        if (!item)
            continue;

        cJSON_AddStringToObject(item, "id", overlay->id);
        cJSON_AddBoolToObject(item, "pinned", overlay->pinned);
        cJSON_AddNumberToObject(item, "x", overlay->x);
        cJSON_AddNumberToObject(item, "y", overlay->y);
        cJSON_AddItemToArray(overlays, item);
    }

    if (cJSON_GetArraySize(overlays) > 0)
        cJSON_AddItemToObject(root, "overlayPanels", overlays);
    else
        cJSON_Delete(overlays);
}

static void sdl_config_migrate_layout(struct sdl_config* cfg,
    struct pane_config* pane_configs, int pane_count)
{
    if (!cfg)
        return;

    if (cfg->layout_schema_version < 1)
        cfg->layout_schema_version = 1;

    if (!sdl_overlay_density_is_valid(cfg->overlay_density))
        cfg->overlay_density = SDL_OVERLAY_DENSITY_AUTO;

    for (int i = 0; pane_configs && i < pane_count; i++) {
        if (pane_configs[i].rect.rows < 0)
            pane_configs[i].rect.rows = 0;
        if (pane_configs[i].rect.cols < 0)
            pane_configs[i].rect.cols = 0;
        if (pane_configs[i].ratio < 0.0f)
            pane_configs[i].ratio = 0.0f;
        if (pane_configs[i].ratio > 1.0f)
            pane_configs[i].ratio = 1.0f;
    }

    if (cfg->layout_schema_version < SDL_LAYOUT_SCHEMA_VERSION) {
        log_info("Migrated SDL layout schema from %d to %d",
            cfg->layout_schema_version, SDL_LAYOUT_SCHEMA_VERSION);
        cfg->layout_schema_version = SDL_LAYOUT_SCHEMA_VERSION;
    }
}

void sdl_config_load(const char* filename, struct sdl_config* cfg, 
                     struct pane_config* pane_configs, int* pane_count, int max_panes)
{
    log_info("Loading SDL configuration from: %s", filename);
    
    char* content = read_file_contents(filename);
    if (!content) {
        log_debug("Failed to read config file, using defaults");
        return;
    }
    
    log_debug("Config file content length: %zu bytes", strlen(content));
    
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            log_error("JSON parse error before: %s", error_ptr);
        } else {
            log_error("JSON parse error (no error pointer available)");
        }
        return;
    }
    
    log_debug("JSON parsed successfully");
    
    // Parse SDL settings
    cJSON* sdl = cJSON_GetObjectItemCaseSensitive(root, "sdl");
    if (cJSON_IsObject(sdl)) {
        log_debug("Found 'sdl' object in JSON");
        cJSON* item;

        item = cJSON_GetObjectItemCaseSensitive(sdl, "layoutVersion");
        if (cJSON_IsNumber(item)) {
            cfg->layout_schema_version = item->valueint;
            log_debug("Loaded layoutVersion: %d",
                cfg->layout_schema_version);
        } else {
            cfg->layout_schema_version = 1;
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "mainViewScale");
        if (cJSON_IsNumber(item)) {
            cfg->main_view_scale = item->valueint;
            log_debug("Loaded mainViewScale: %d", cfg->main_view_scale);
        } else {
            log_warn("mainViewScale not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "auxViewFontSize");
        if (cJSON_IsNumber(item)) {
            cfg->aux_view_font_size = item->valueint;
            log_debug("Loaded auxViewFontSize: %d", cfg->aux_view_font_size);
        } else {
            log_warn("auxViewFontSize not found or not a number");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "menuPanelFontSize");
        if (cJSON_IsNumber(item)) {
            cfg->menu_panel_font_size = item->valueint;
            log_debug("Loaded menuPanelFontSize: %d",
                cfg->menu_panel_font_size);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "plainMenuFontSize");
        if (cJSON_IsNumber(item)) {
            cfg->plain_menu_font_size = item->valueint;
            log_debug("Loaded plainMenuFontSize: %d",
                cfg->plain_menu_font_size);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "browserMenuFontSize");
        if (cJSON_IsNumber(item)) {
            cfg->browser_menu_font_size = item->valueint;
            log_debug("Loaded browserMenuFontSize: %d",
                cfg->browser_menu_font_size);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl,
            "characterSheetFontSize");
        if (cJSON_IsNumber(item)) {
            cfg->character_sheet_font_size = item->valueint;
            log_debug("Loaded characterSheetFontSize: %d",
                cfg->character_sheet_font_size);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "overlayDensity");
        if (cJSON_IsString(item)) {
            cfg->overlay_density = parse_overlay_density(item->valuestring);
            log_debug("Loaded overlayDensity: %s",
                sdl_overlay_density_to_string(cfg->overlay_density));
        } else if (cJSON_IsNumber(item)) {
            cfg->overlay_density = item->valueint;
            log_debug("Loaded numeric overlayDensity: %d",
                cfg->overlay_density);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "margin");
        if (cJSON_IsNumber(item)) {
            cfg->margin = item->valueint;
            log_debug("Loaded margin: %d", cfg->margin);
        } else {
            log_warn("margin not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "fullscreen");
        if (cJSON_IsBool(item)) {
            cfg->fullscreen = cJSON_IsTrue(item);
            log_debug("Loaded fullscreen: %s", cfg->fullscreen ? "true" : "false");
        } else {
            log_warn("fullscreen not found or not a boolean");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "tiles");
        if (cJSON_IsBool(item)) {
            cfg->tiles = cJSON_IsTrue(item);
            log_debug("Loaded tiles: %s", cfg->tiles ? "true" : "false");
        } else {
            log_warn("tiles not found or not a boolean");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "useUnsafeArea");
        if (cJSON_IsBool(item)) {
            cfg->use_unsafe_area = cJSON_IsTrue(item);
            log_debug("Loaded useUnsafeArea: %s",
                cfg->use_unsafe_area ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "palettePreset");
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
            SDL_strlcpy(cfg->palette_preset, item->valuestring,
                sizeof(cfg->palette_preset));
            log_debug("Loaded palettePreset: %s", cfg->palette_preset);
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "enableRightPanes");
        if (cJSON_IsBool(item)) {
            cfg->enable_right_panes = cJSON_IsTrue(item);
            log_debug("Loaded enableRightPanes: %s", cfg->enable_right_panes ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "enableBottomPanes");
        if (cJSON_IsBool(item)) {
            cfg->enable_bottom_panes = cJSON_IsTrue(item);
            log_debug("Loaded enableBottomPanes: %s", cfg->enable_bottom_panes ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "showPaneBorders");
        if (cJSON_IsBool(item)) {
            cfg->show_pane_borders = cJSON_IsTrue(item);
            log_debug("Loaded showPaneBorders: %s", cfg->show_pane_borders ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "hideLeftPanel");
        if (cJSON_IsBool(item)) {
            cfg->hide_left_panel = cJSON_IsTrue(item);
            log_debug("Loaded hideLeftPanel: %s", cfg->hide_left_panel ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(sdl, "minTerminalMode");
        if (cJSON_IsString(item)) {
            cfg->min_terminal_mode = parse_min_terminal_mode(item->valuestring);
            log_debug("Loaded minTerminalMode: %s", min_terminal_mode_to_string(cfg->min_terminal_mode));
        } else if (cJSON_IsNumber(item)) {
            if (item->valueint == SDL_MIN_TERMINAL_COMPACT)
                cfg->min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
            else
                cfg->min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
            log_debug("Loaded numeric minTerminalMode: %s", min_terminal_mode_to_string(cfg->min_terminal_mode));
        }
        
        // Window position and size for windowed mode
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowX");
        if (cJSON_IsNumber(item)) {
            cfg->window_x = item->valueint;
            log_debug("Loaded windowX: %d", cfg->window_x);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowY");
        if (cJSON_IsNumber(item)) {
            cfg->window_y = item->valueint;
            log_debug("Loaded windowY: %d", cfg->window_y);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowWidth");
        if (cJSON_IsNumber(item)) {
            cfg->window_width = item->valueint;
            log_debug("Loaded windowWidth: %d", cfg->window_width);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowHeight");
        if (cJSON_IsNumber(item)) {
            cfg->window_height = item->valueint;
            log_debug("Loaded windowHeight: %d", cfg->window_height);
        }
        
        // Custom fonts
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(cfg->story_font, item->valuestring, sizeof(cfg->story_font));
            log_debug("Loaded storyFont: %s", cfg->story_font);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monospaceFont");
        if (cJSON_IsString(item)) {
            SDL_strlcpy(cfg->monospace_font, item->valuestring, sizeof(cfg->monospace_font));
            log_debug("Loaded monospaceFont: %s", cfg->monospace_font);
        }
        
        // Monospace font rendering options (with backward compatibility)
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoBold");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontBold");
        if (cJSON_IsBool(item)) {
            cfg->mono_bold = cJSON_IsTrue(item);
            log_debug("Loaded monoBold: %s", cfg->mono_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoItalic");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontItalic");
        if (cJSON_IsBool(item)) {
            cfg->mono_italic = cJSON_IsTrue(item);
            log_debug("Loaded monoItalic: %s", cfg->mono_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoUnderline");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontUnderline");
        if (cJSON_IsBool(item)) {
            cfg->mono_underline = cJSON_IsTrue(item);
            log_debug("Loaded monoUnderline: %s", cfg->mono_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoStrikethrough");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontStrikethrough");
        if (cJSON_IsBool(item)) {
            cfg->mono_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded monoStrikethrough: %s", cfg->mono_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoHinting");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontHinting");
        if (cJSON_IsNumber(item)) {
            cfg->mono_hinting = item->valueint;
            log_debug("Loaded monoHinting: %d", cfg->mono_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoKerning");
        if (!cJSON_IsBool(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontKerning");
        if (cJSON_IsBool(item)) {
            cfg->mono_kerning = cJSON_IsTrue(item);
            log_debug("Loaded monoKerning: %s", cfg->mono_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "monoOutline");
        if (!cJSON_IsNumber(item)) item = cJSON_GetObjectItemCaseSensitive(sdl, "fontOutline");
        if (cJSON_IsNumber(item)) {
            cfg->mono_outline = item->valueint;
            log_debug("Loaded monoOutline: %d", cfg->mono_outline);
        }
        
        // Story font rendering options
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyBold");
        if (cJSON_IsBool(item)) {
            cfg->story_bold = cJSON_IsTrue(item);
            log_debug("Loaded storyBold: %s", cfg->story_bold ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyItalic");
        if (cJSON_IsBool(item)) {
            cfg->story_italic = cJSON_IsTrue(item);
            log_debug("Loaded storyItalic: %s", cfg->story_italic ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyUnderline");
        if (cJSON_IsBool(item)) {
            cfg->story_underline = cJSON_IsTrue(item);
            log_debug("Loaded storyUnderline: %s", cfg->story_underline ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyStrikethrough");
        if (cJSON_IsBool(item)) {
            cfg->story_strikethrough = cJSON_IsTrue(item);
            log_debug("Loaded storyStrikethrough: %s", cfg->story_strikethrough ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyHinting");
        if (cJSON_IsNumber(item)) {
            cfg->story_hinting = item->valueint;
            log_debug("Loaded storyHinting: %d", cfg->story_hinting);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyKerning");
        if (cJSON_IsBool(item)) {
            cfg->story_kerning = cJSON_IsTrue(item);
            log_debug("Loaded storyKerning: %s", cfg->story_kerning ? "true" : "false");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "storyOutline");
        if (cJSON_IsNumber(item)) {
            cfg->story_outline = item->valueint;
            log_debug("Loaded storyOutline: %d", cfg->story_outline);
        }
    } else {
        log_warn("'sdl' object not found in JSON");
    }
    
    // Parse pane configurations
    cJSON* panes = cJSON_GetObjectItemCaseSensitive(root, "panes");
    if (cJSON_IsArray(panes)) {
        int count = 0;
        cJSON* pane_item = NULL;
        int array_size = cJSON_GetArraySize(panes);
        log_debug("Found 'panes' array with %d items", array_size);
        
        cJSON_ArrayForEach(pane_item, panes) {
            if (count >= max_panes) {
                log_warn("Too many panes in config, maximum is %d", max_panes);
                break;
            }
            
            struct pane_config* pc = &pane_configs[count];
            memset(pc, 0, sizeof(*pc));
            pc->pane = PANE_MAIN;
            pc->enabled = true;
            
            cJSON* type = cJSON_GetObjectItemCaseSensitive(pane_item, "type");
            if (cJSON_IsString(type)) {
                pc->pane = parse_pane_type(type->valuestring);
                log_debug("Pane %d: type=%s", count, type->valuestring);
            }
            
            cJSON* where = cJSON_GetObjectItemCaseSensitive(pane_item, "where");
            if (cJSON_IsString(where)) {
                pc->where = parse_pane_placement(where->valuestring);
                log_debug("Pane %d: where=%s", count, where->valuestring);
            }

            cJSON* enabled = cJSON_GetObjectItemCaseSensitive(pane_item, "enabled");
            if (cJSON_IsBool(enabled)) {
                pc->enabled = cJSON_IsTrue(enabled);
                log_debug("Pane %d: enabled=%s", count, pc->enabled ? "true" : "false");
            }
            
            cJSON* rows = cJSON_GetObjectItemCaseSensitive(pane_item, "rows");
            if (cJSON_IsNumber(rows)) {
                pc->rect.rows = rows->valueint;
                log_debug("Pane %d: rows=%d", count, pc->rect.rows);
            }
            
            cJSON* cols = cJSON_GetObjectItemCaseSensitive(pane_item, "cols");
            if (cJSON_IsNumber(cols)) {
                pc->rect.cols = cols->valueint;
                log_debug("Pane %d: cols=%d", count, pc->rect.cols);
            }
            
            cJSON* ratio = cJSON_GetObjectItemCaseSensitive(pane_item, "ratio");
            if (cJSON_IsNumber(ratio)) {
                pc->ratio = (float)ratio->valuedouble;
                log_debug("Pane %d: ratio=%.2f", count, pc->ratio);
            }

            cJSON* font_size = cJSON_GetObjectItemCaseSensitive(pane_item, "fontSize");
            if (cJSON_IsNumber(font_size)) {
                pc->font_size = font_size->valueint;
                if (pc->font_size < 0)
                    pc->font_size = 0;
                if (pc->font_size > 48)
                    pc->font_size = 48;
                log_debug("Pane %d: fontSize=%d", count, pc->font_size);
            }

            if (!pane_type_allows_placement(pc->pane, pc->where)) {
                enum pane_placement fallback = pane_first_allowed_placement(pc->pane);
                log_warn("Pane %d placement %s is invalid for type %s, using %s",
                    count,
                    pane_placement_name(pc->where),
                    pane_type_to_string(pc->pane),
                    pane_placement_name(fallback));
                pc->where = fallback;
            }
            
            count++;
        }
        
        *pane_count = count;
        log_debug("Parsed %d panes from JSON", count);
    } else {
        log_warn("'panes' array not found in JSON");
    }

    sdl_config_load_overlay_panels(root, cfg);
    sdl_config_migrate_layout(cfg, pane_configs, pane_count ? *pane_count : 0);

    // Parse gamepad settings
    cJSON* gamepad = cJSON_GetObjectItemCaseSensitive(root, "gamepad");
    if (cJSON_IsObject(gamepad)) {
        cJSON* item;

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "enabled");
        if (cJSON_IsBool(item)) {
            cfg->gamepad_enabled = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.enabled: %s", cfg->gamepad_enabled ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "autoMode");
        if (cJSON_IsBool(item)) {
            cfg->gamepad_auto_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.autoMode: %s", cfg->gamepad_auto_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "steamdeckMode");
        if (cJSON_IsBool(item)) {
            cfg->steamdeck_mode = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.steamdeckMode: %s", cfg->steamdeck_mode ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useDpad");
        if (cJSON_IsBool(item)) {
            cfg->gamepad_use_dpad = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useDpad: %s", cfg->gamepad_use_dpad ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "useLeftStick");
        if (cJSON_IsBool(item)) {
            cfg->gamepad_use_left_stick = cJSON_IsTrue(item);
            log_debug("Loaded gamepad.useLeftStick: %s", cfg->gamepad_use_left_stick ? "true" : "false");
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "deadzone");
        if (cJSON_IsNumber(item)) {
            cfg->gamepad_deadzone = item->valueint;
            log_debug("Loaded gamepad.deadzone: %d", cfg->gamepad_deadzone);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerThreshold");
        if (cJSON_IsNumber(item)) {
            cfg->gamepad_trigger_threshold = item->valueint;
            log_debug("Loaded gamepad.triggerThreshold: %d", cfg->gamepad_trigger_threshold);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "buttonBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_BUTTON_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    cfg->gamepad_button_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.buttonBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "triggerBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_TRIGGER_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    cfg->gamepad_trigger_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.triggerBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "leftStickBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    cfg->gamepad_left_stick_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.leftStickBindings (%d entries)", count);
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "rightStickBindings");
        if (cJSON_IsArray(item)) {
            int count = cJSON_GetArraySize(item);
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT && i < count; i++) {
                cJSON* binding = cJSON_GetArrayItem(item, i);
                if (cJSON_IsNumber(binding)) {
                    cfg->gamepad_right_stick_bindings[i] = binding->valueint;
                }
            }
            log_debug("Loaded gamepad.rightStickBindings (%d entries)", count);
        }

        for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_button_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    cfg->gamepad_button_combo_bindings[modifier],
                    GAMEPAD_BUTTON_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_trigger_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    cfg->gamepad_trigger_combo_bindings[modifier],
                    GAMEPAD_TRIGGER_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_left_stick_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    cfg->gamepad_left_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
            }

            item = cJSON_GetObjectItemCaseSensitive(gamepad,
                sdl_config_gamepad_right_stick_combo_names[modifier]);
            if (cJSON_IsArray(item)) {
                sdl_config_load_touch_binding_array(item,
                    cfg->gamepad_right_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
            }
        }

        item = cJSON_GetObjectItemCaseSensitive(gamepad, "shoulderComboBinding");
        if (cJSON_IsNumber(item)) {
            cfg->gamepad_shoulder_combo_binding = item->valueint;
            log_debug("Loaded gamepad.shoulderComboBinding: %d", cfg->gamepad_shoulder_combo_binding);
        }

        if (cfg->gamepad_use_dpad) {
            cfg->gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_UP] = GAMEPAD_BIND_NONE;
            cfg->gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_DOWN] = GAMEPAD_BIND_NONE;
            cfg->gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_LEFT] = GAMEPAD_BIND_NONE;
            cfg->gamepad_button_bindings[GAMEPAD_BUTTON_DPAD_RIGHT] = GAMEPAD_BIND_NONE;
        }

        if (cfg->gamepad_use_left_stick) {
            for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                cfg->gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
            }
        }
    } else {
        log_warn("'gamepad' object not found in JSON");
    }

    {
        cJSON* touch_pane = cJSON_GetObjectItemCaseSensitive(root, "touchPane");
        if (cJSON_IsObject(touch_pane)) {
            cJSON* bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "bindings");
            cJSON* labels = cJSON_GetObjectItemCaseSensitive(touch_pane, "labels");
            cJSON* second_bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "secondBindings");
            cJSON* second_labels = cJSON_GetObjectItemCaseSensitive(touch_pane, "secondLabels");
            cJSON* panel_names = cJSON_GetObjectItemCaseSensitive(touch_pane, "panelNames");
            cJSON* swipe_enabled = cJSON_GetObjectItemCaseSensitive(touch_pane, "swipeEnabled");
            cJSON* swipe_bindings = cJSON_GetObjectItemCaseSensitive(touch_pane, "swipeBindings");
            if (cJSON_IsArray(bindings)) {
                int count = cJSON_GetArraySize(bindings);
                if (count == 21) {
                    for (int i = 0; i < count && (i + 3) < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
                        cJSON* binding = cJSON_GetArrayItem(bindings, i);
                        if (cJSON_IsNumber(binding)) {
                            int value = binding->valueint;
                            if (value == ' ')
                                value = INPUT_BIND_CONFIRM;
                            cfg->touch_pane_bindings[i + 3] = value;
                        }
                    }
                    log_info("Migrated legacy touchPane.bindings layout (21 -> %d entries)",
                        SDL_TOUCH_PANE_BUTTON_COUNT);
                } else {
                    sdl_config_load_touch_binding_array(bindings, cfg->touch_pane_bindings,
                        SDL_TOUCH_PANE_BUTTON_COUNT);
                }
                log_debug("Loaded touchPane.bindings (%d entries)", count);
            }

            if (cJSON_IsArray(labels)) {
                int count = cJSON_GetArraySize(labels);
                sdl_config_load_touch_label_array(labels, cfg->touch_pane_labels,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.labels (%d entries)", count);
            }

            if (cJSON_IsArray(second_bindings)) {
                int count = cJSON_GetArraySize(second_bindings);
                sdl_config_load_touch_binding_array(second_bindings, cfg->touch_pane_second_bindings,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.secondBindings (%d entries)", count);
            }

            if (cJSON_IsArray(second_labels)) {
                int count = cJSON_GetArraySize(second_labels);
                sdl_config_load_touch_label_array(second_labels, cfg->touch_pane_second_labels,
                    SDL_TOUCH_PANE_BUTTON_COUNT);
                log_debug("Loaded touchPane.secondLabels (%d entries)", count);
            }

            if (cJSON_IsArray(panel_names)) {
                int count = cJSON_GetArraySize(panel_names);
                for (int i = 0; i < SDL_TOUCH_PANE_PANEL_COUNT && i < count; i++) {
                    cJSON* panel_name = cJSON_GetArrayItem(panel_names, i);
                    if (cJSON_IsString(panel_name) && panel_name->valuestring) {
                        SDL_strlcpy(cfg->touch_pane_panel_names[i], panel_name->valuestring,
                            sizeof(cfg->touch_pane_panel_names[i]));
                    }
                }
                log_debug("Loaded touchPane.panelNames (%d entries)", count);
            }

            if (cJSON_IsBool(swipe_enabled)) {
                cfg->touch_swipe_enabled = cJSON_IsTrue(swipe_enabled);
                log_debug("Loaded touchPane.swipeEnabled: %s",
                    cfg->touch_swipe_enabled ? "true" : "false");
            }

            if (cJSON_IsArray(swipe_bindings)) {
                int count = cJSON_GetArraySize(swipe_bindings);
                sdl_config_load_touch_binding_array(swipe_bindings,
                    cfg->touch_swipe_bindings, GAMEPAD_STICK_DIR_COUNT);
                log_debug("Loaded touchPane.swipeBindings (%d entries)", count);
            }
        } else {
            log_warn("'touchPane' object not found in JSON");
        }
    }

    sdl_config_load_movement_bindings(root, cfg);
    
    cJSON_Delete(root);
    log_debug("Configuration loading complete. Total panes: %d", *pane_count);
}

bool sdl_config_save(const char* filename, const struct sdl_config* cfg,
                     const struct pane_config* pane_configs, int pane_count)
{
    cJSON* root;

    if (!filename || !filename[0] || !cfg
        || (pane_count > 0 && !pane_configs))
    {
        log_error("Invalid SDL configuration save request");
        return false;
    }

    root = cJSON_CreateObject();
    if (!root) {
        log_error("Failed to create JSON root object");
        return false;
    }
    
    // Create SDL settings object
    cJSON* sdl = cJSON_CreateObject();
    if (!sdl) {
        cJSON_Delete(root);
        log_error("Failed to create SDL settings object");
        return false;
    }
    
    cJSON_AddNumberToObject(sdl, "layoutVersion",
        SDL_LAYOUT_SCHEMA_VERSION);
    cJSON_AddNumberToObject(sdl, "mainViewScale", cfg->main_view_scale);
    cJSON_AddStringToObject(sdl, "overlayDensity",
        sdl_overlay_density_to_string(cfg->overlay_density));
    cJSON_AddNumberToObject(sdl, "auxViewFontSize", cfg->aux_view_font_size);
    cJSON_AddNumberToObject(sdl, "menuPanelFontSize",
        cfg->menu_panel_font_size);
    cJSON_AddNumberToObject(sdl, "plainMenuFontSize",
        cfg->plain_menu_font_size);
    cJSON_AddNumberToObject(sdl, "browserMenuFontSize",
        cfg->browser_menu_font_size);
    cJSON_AddNumberToObject(sdl, "characterSheetFontSize",
        cfg->character_sheet_font_size);
    cJSON_AddNumberToObject(sdl, "margin", cfg->margin);
    cJSON_AddBoolToObject(sdl, "fullscreen", cfg->fullscreen);
    cJSON_AddBoolToObject(sdl, "tiles", cfg->tiles);
    cJSON_AddBoolToObject(sdl, "useUnsafeArea", cfg->use_unsafe_area);
    cJSON_AddStringToObject(sdl, "palettePreset",
        (cfg->palette_preset[0]) ? cfg->palette_preset : "classic");
    cJSON_AddBoolToObject(sdl, "enableRightPanes", cfg->enable_right_panes);
    cJSON_AddBoolToObject(sdl, "enableBottomPanes", cfg->enable_bottom_panes);
    cJSON_AddBoolToObject(sdl, "showPaneBorders", cfg->show_pane_borders);
    cJSON_AddBoolToObject(sdl, "hideLeftPanel", cfg->hide_left_panel);
    cJSON_AddStringToObject(sdl, "minTerminalMode", min_terminal_mode_to_string(cfg->min_terminal_mode));
    
    // Save window position and size for windowed mode
    cJSON_AddNumberToObject(sdl, "windowX", cfg->window_x);
    cJSON_AddNumberToObject(sdl, "windowY", cfg->window_y);
    cJSON_AddNumberToObject(sdl, "windowWidth", cfg->window_width);
    cJSON_AddNumberToObject(sdl, "windowHeight", cfg->window_height);
    
    // Save custom fonts
    cJSON_AddStringToObject(sdl, "storyFont", cfg->story_font);
    cJSON_AddStringToObject(sdl, "monospaceFont", cfg->monospace_font);
    
    // Save monospace font rendering options
    cJSON_AddBoolToObject(sdl, "monoBold", cfg->mono_bold);
    cJSON_AddBoolToObject(sdl, "monoItalic", cfg->mono_italic);
    cJSON_AddBoolToObject(sdl, "monoUnderline", cfg->mono_underline);
    cJSON_AddBoolToObject(sdl, "monoStrikethrough", cfg->mono_strikethrough);
    cJSON_AddNumberToObject(sdl, "monoHinting", cfg->mono_hinting);
    cJSON_AddBoolToObject(sdl, "monoKerning", cfg->mono_kerning);
    cJSON_AddNumberToObject(sdl, "monoOutline", cfg->mono_outline);
    
    // Save story font rendering options
    cJSON_AddBoolToObject(sdl, "storyBold", cfg->story_bold);
    cJSON_AddBoolToObject(sdl, "storyItalic", cfg->story_italic);
    cJSON_AddBoolToObject(sdl, "storyUnderline", cfg->story_underline);
    cJSON_AddBoolToObject(sdl, "storyStrikethrough", cfg->story_strikethrough);
    cJSON_AddNumberToObject(sdl, "storyHinting", cfg->story_hinting);
    cJSON_AddBoolToObject(sdl, "storyKerning", cfg->story_kerning);
    cJSON_AddNumberToObject(sdl, "storyOutline", cfg->story_outline);
    
    cJSON_AddItemToObject(root, "sdl", sdl);
    
    // Create panes array
    cJSON* panes = cJSON_CreateArray();
    if (!panes) {
        cJSON_Delete(root);
        log_error("Failed to create panes array");
        return false;
    }
    
    for (int i = 0; i < pane_count; i++) {
        const struct pane_config* pc = &pane_configs[i];
        
        cJSON* pane = cJSON_CreateObject();
        if (!pane) {
            continue;
        }
        
        cJSON_AddStringToObject(pane, "type", pane_type_to_string(pc->pane));
        cJSON_AddStringToObject(pane, "where", pane_placement_to_string(pc->where));
        cJSON_AddBoolToObject(pane, "enabled", pc->enabled);
        
        if (pc->rect.rows > 0) {
            cJSON_AddNumberToObject(pane, "rows", pc->rect.rows);
        }
        
        if (pc->rect.cols > 0) {
            cJSON_AddNumberToObject(pane, "cols", pc->rect.cols);
        }
        
        if (pc->ratio > 0.0f) {
            cJSON_AddNumberToObject(pane, "ratio", pc->ratio);
        }

        if (pc->font_size > 0) {
            cJSON_AddNumberToObject(pane, "fontSize", pc->font_size);
        }
        
        cJSON_AddItemToArray(panes, pane);
    }
    
    cJSON_AddItemToObject(root, "panes", panes);
    sdl_config_save_overlay_panels(root, cfg);

    // Create gamepad settings object
    {
        cJSON* gamepad = cJSON_CreateObject();
        if (gamepad) {
            cJSON* bindings = NULL;
            cJSON* triggers = NULL;
            cJSON* left_stick = NULL;
            cJSON* right_stick = NULL;

            cJSON_AddBoolToObject(gamepad, "enabled", cfg->gamepad_enabled);
            cJSON_AddBoolToObject(gamepad, "autoMode", cfg->gamepad_auto_mode);
            cJSON_AddBoolToObject(gamepad, "steamdeckMode", cfg->steamdeck_mode);
            cJSON_AddBoolToObject(gamepad, "useDpad", cfg->gamepad_use_dpad);
            cJSON_AddBoolToObject(gamepad, "useLeftStick", cfg->gamepad_use_left_stick);
            cJSON_AddNumberToObject(gamepad, "deadzone", cfg->gamepad_deadzone);
            cJSON_AddNumberToObject(gamepad, "triggerThreshold", cfg->gamepad_trigger_threshold);

            bindings = cJSON_CreateArray();
            if (bindings) {
                for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
                    cJSON_AddItemToArray(bindings, cJSON_CreateNumber(cfg->gamepad_button_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "buttonBindings", bindings);
            }

            triggers = cJSON_CreateArray();
            if (triggers) {
                for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
                    cJSON_AddItemToArray(triggers, cJSON_CreateNumber(cfg->gamepad_trigger_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "triggerBindings", triggers);
            }

            left_stick = cJSON_CreateArray();
            if (left_stick) {
                for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                    cJSON_AddItemToArray(left_stick, cJSON_CreateNumber(cfg->gamepad_left_stick_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "leftStickBindings", left_stick);
            }

            right_stick = cJSON_CreateArray();
            if (right_stick) {
                for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
                    cJSON_AddItemToArray(right_stick, cJSON_CreateNumber(cfg->gamepad_right_stick_bindings[i]));
                }
                cJSON_AddItemToObject(gamepad, "rightStickBindings", right_stick);
            }

            for (int modifier = 0; modifier < GAMEPAD_MODIFIER_COUNT; modifier++) {
                cJSON* combo = sdl_config_create_int_array(
                    cfg->gamepad_button_combo_bindings[modifier], GAMEPAD_BUTTON_COUNT);
                if (combo)
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_button_combo_names[modifier], combo);

                combo = sdl_config_create_int_array(
                    cfg->gamepad_trigger_combo_bindings[modifier],
                    GAMEPAD_TRIGGER_COUNT);
                if (combo)
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_trigger_combo_names[modifier], combo);

                combo = sdl_config_create_int_array(
                    cfg->gamepad_left_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
                if (combo)
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_left_stick_combo_names[modifier], combo);

                combo = sdl_config_create_int_array(
                    cfg->gamepad_right_stick_combo_bindings[modifier],
                    GAMEPAD_STICK_DIR_COUNT);
                if (combo)
                    cJSON_AddItemToObject(gamepad,
                        sdl_config_gamepad_right_stick_combo_names[modifier], combo);
            }

            cJSON_AddNumberToObject(gamepad, "shoulderComboBinding", cfg->gamepad_shoulder_combo_binding);

            cJSON_AddItemToObject(root, "gamepad", gamepad);
        }
    }

    {
        cJSON* touch_pane = cJSON_CreateObject();
        if (touch_pane) {
            cJSON* bindings = sdl_config_create_int_array(cfg->touch_pane_bindings,
                SDL_TOUCH_PANE_BUTTON_COUNT);
            cJSON* labels = sdl_config_create_string_array(cfg->touch_pane_labels,
                SDL_TOUCH_PANE_BUTTON_COUNT);
            cJSON* second_bindings = sdl_config_create_int_array(cfg->touch_pane_second_bindings,
                SDL_TOUCH_PANE_BUTTON_COUNT);
            cJSON* second_labels = sdl_config_create_string_array(cfg->touch_pane_second_labels,
                SDL_TOUCH_PANE_BUTTON_COUNT);
            cJSON* panel_names = sdl_config_create_string_array(cfg->touch_pane_panel_names,
                SDL_TOUCH_PANE_PANEL_COUNT);
            cJSON* swipe_bindings = sdl_config_create_int_array(
                cfg->touch_swipe_bindings, GAMEPAD_STICK_DIR_COUNT);
            if (bindings) {
                cJSON_AddItemToObject(touch_pane, "bindings", bindings);
            }
            if (labels) {
                cJSON_AddItemToObject(touch_pane, "labels", labels);
            }
            if (second_bindings) {
                cJSON_AddItemToObject(touch_pane, "secondBindings", second_bindings);
            }
            if (second_labels) {
                cJSON_AddItemToObject(touch_pane, "secondLabels", second_labels);
            }
            if (panel_names) {
                cJSON_AddItemToObject(touch_pane, "panelNames", panel_names);
            }
            cJSON_AddBoolToObject(touch_pane, "swipeEnabled",
                cfg->touch_swipe_enabled);
            if (swipe_bindings) {
                cJSON_AddItemToObject(touch_pane, "swipeBindings",
                    swipe_bindings);
            }
            cJSON_AddItemToObject(root, "touchPane", touch_pane);
        }
    }

    sdl_config_save_movement_bindings(root, cfg);

    /* Create app-wide options object */
    {
        cJSON* app_options = cJSON_CreateObject();
        cJSON* interface = NULL;
        cJSON* efficiency = NULL;
        cJSON* gameplay = NULL;
        cJSON* visual = NULL;

        if (app_options && op_ptr) {
            cJSON_AddBoolToObject(app_options, "introSeen", g_app_intro_seen);

            sdl_config_save_app_option_group(app_options, "interface", app_interface_options);
            sdl_config_save_app_option_group(app_options, "text", app_text_options);
            sdl_config_save_app_option_group(app_options, "efficiency", app_efficiency_options);
            sdl_config_save_app_option_group(app_options, "gameplay", app_gameplay_options);
            sdl_config_save_app_option_group(app_options, "visual", app_visual_options);

            interface = cJSON_GetObjectItemCaseSensitive(app_options, "interface");
            if (cJSON_IsObject(interface)) {
                cJSON_AddNumberToObject(interface, "hitpointWarning", op_ptr->hitpoint_warn);
            }

            efficiency = cJSON_GetObjectItemCaseSensitive(app_options, "efficiency");
            if (cJSON_IsObject(efficiency)) {
                cJSON_AddNumberToObject(efficiency, "delayFactor", op_ptr->delay_factor);
            }

            gameplay = cJSON_GetObjectItemCaseSensitive(app_options, "gameplay");
            if (cJSON_IsObject(gameplay)) {
                cJSON_AddNumberToObject(gameplay, "minDepthTimerMode",
                    op_ptr->min_depth_timer_mode);
            }

            visual = cJSON_GetObjectItemCaseSensitive(app_options, "visual");
            if (cJSON_IsObject(visual)) {
                cJSON_AddNumberToObject(visual, "mainCombatRolls", op_ptr->main_combat_rolls);
                cJSON_AddNumberToObject(visual, "bannerPopupSeconds",
                    op_ptr->narrative_banner_seconds);
                cJSON_AddNumberToObject(visual, "abilityDescMode", op_ptr->ability_desc_mode);
                cJSON_AddNumberToObject(visual, "introStyle", op_ptr->intro_style);
            }

            cJSON_AddItemToObject(root, "appOptions", app_options);
        }
    }
    
    // Print to string and write to file
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        log_error("Failed to print JSON");
        return false;
    }
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        log_error("Could not write JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return false;
    }
    
    bool saved = true;

    if (fprintf(f, "%s\n", json_string) < 0)
    {
        log_error("Failed writing JSON file: %s", filename);
        saved = false;
    }
    if (fclose(f) != 0)
    {
        log_error("Failed closing JSON file after write: %s", filename);
        saved = false;
    }
    cJSON_free(json_string);
    cJSON_Delete(root);

    if (!saved)
        return false;
    
    log_info("Saved SDL configuration to: %s", filename);
    return true;
}

void sdl_config_set_default_gamepad_bindings(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    for (int i = 0; i < GAMEPAD_BUTTON_COUNT; i++) {
        cfg->gamepad_button_bindings[i] = GAMEPAD_BIND_NONE;
    }
    for (int i = 0; i < GAMEPAD_TRIGGER_COUNT; i++) {
        cfg->gamepad_trigger_bindings[i] = GAMEPAD_BIND_NONE;
    }
    for (int i = 0; i < GAMEPAD_STICK_DIR_COUNT; i++) {
        cfg->gamepad_left_stick_bindings[i] = GAMEPAD_BIND_NONE;
        cfg->gamepad_right_stick_bindings[i] = GAMEPAD_BIND_NONE;
    }
    sdl_config_clear_gamepad_combo_bindings(cfg);

    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_SOUTH] = ' ';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_EAST] = 'f';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_WEST] = 'u';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_NORTH] = 's';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_SHOULDER] = 'e';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'i';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_START] = ESCAPE;
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_BACK] = 'h';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_PADDLE1] = 'r';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_PADDLE2] = 'o';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_PADDLE1] = 'q';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_PADDLE2] = '?';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_LEFT_STICK] = 'z';
    cfg->gamepad_button_bindings[GAMEPAD_BUTTON_RIGHT_STICK] = 'j';

    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_RIGHT] = 'x';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_LEFT] = 'a';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_UP] = 'M';
    cfg->gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_DOWN] = 'b';

    cfg->gamepad_trigger_bindings[0] = GAMEPAD_BIND_SHIFT;
    cfg->gamepad_trigger_bindings[1] = GAMEPAD_BIND_CTRL;

    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_SOUTH] = 'Z';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_EAST] = 'F';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_WEST] = 'x';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_NORTH] = 'S';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_LEFT_SHOULDER] = 'M';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_SHIFT][GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'p';

    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_SOUTH] = 'z';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_EAST] = '-';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_WEST] = 'X';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_NORTH] = '0';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_BACK] = '\t';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_LEFT_SHOULDER] = 'a';
    cfg->gamepad_button_combo_bindings[GAMEPAD_MODIFIER_CTRL][GAMEPAD_BUTTON_RIGHT_SHOULDER] = 'j';

    cfg->gamepad_shoulder_combo_binding = 'l';
}

void sdl_config_set_default_touch_pane_bindings(struct sdl_config* cfg)
{
    static const int main_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        ESCAPE, GAMEPAD_BIND_CTRL, GAMEPAD_BIND_SHIFT,
        'e', 'i', 'j',
        'u', 's', 'f',
        '7', '8', '9',
        '4', INPUT_BIND_CONFIRM, '6',
        '1', '2', '3',
        'l', 'x', 'a',
        'M', 'h', '\t',
    };
    static const int second_defaults[SDL_TOUCH_PANE_BUTTON_COUNT] = {
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT, GAMEPAD_BIND_SHIFT,
        '0', '-', 'q',
        'r', 'S', 'F',
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, 'z', TOUCH_PANE_BIND_INHERIT,
        TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT, TOUCH_PANE_BIND_INHERIT,
        'L', 'X', 'p',
        'w', 'b', 'c',
    };
    static const int swipe_defaults[GAMEPAD_STICK_DIR_COUNT] = {
        '8', '2', '4', '6',
    };

    if (!cfg)
        return;

    memcpy(cfg->touch_pane_bindings, main_defaults, sizeof(main_defaults));
    memcpy(cfg->touch_pane_second_bindings, second_defaults, sizeof(second_defaults));
    SDL_strlcpy(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN], "Main",
        sizeof(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_MAIN]));
    SDL_strlcpy(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND], "Shift",
        sizeof(cfg->touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_SECOND]));
    cfg->touch_swipe_enabled = true;
    memcpy(cfg->touch_swipe_bindings, swipe_defaults, sizeof(swipe_defaults));
}

void sdl_config_clear_touch_pane_labels(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    memset(cfg->touch_pane_labels, 0, sizeof(cfg->touch_pane_labels));
    memset(cfg->touch_pane_second_labels, 0, sizeof(cfg->touch_pane_second_labels));
}

void sdl_config_set_defaults(struct sdl_config* cfg)
{
    if (!cfg)
        return;

    cfg->layout_schema_version = SDL_LAYOUT_SCHEMA_VERSION;
    cfg->main_view_scale = 1;
    cfg->overlay_density = SDL_OVERLAY_DENSITY_AUTO;
    cfg->aux_view_font_size = 0;
    cfg->menu_panel_font_size = 0;
    cfg->plain_menu_font_size = 0;
    cfg->browser_menu_font_size = 0;
    cfg->character_sheet_font_size = 0;
    cfg->margin = 4;
    cfg->fullscreen = true;
    cfg->tiles = true;
    cfg->use_unsafe_area = false;
    SDL_strlcpy(cfg->palette_preset, "classic",
        sizeof(cfg->palette_preset));
    cfg->enable_right_panes = true;
    cfg->enable_bottom_panes = true;
    cfg->show_pane_borders = true;
    cfg->hide_left_panel = false;
#if defined(__ANDROID__) || defined(SIL_IOS)
    cfg->min_terminal_mode = SDL_MIN_TERMINAL_COMPACT;
#else
    cfg->min_terminal_mode = SDL_MIN_TERMINAL_NORMAL;
#endif
    
    // Default window position and size (will be overridden by actual screen size)
    cfg->window_x = -1;  // -1 means centered
    cfg->window_y = -1;  // -1 means centered
    cfg->window_width = 0;  // 0 means use default calculation
    cfg->window_height = 0; // 0 means use default calculation
    
    // Default fonts
    SDL_strlcpy(cfg->story_font, "font/Cinzel-Medium.ttf",
        sizeof(cfg->story_font));
    SDL_strlcpy(cfg->monospace_font, "font/VictorMono-Medium.ttf",
        sizeof(cfg->monospace_font));
    
    // Default monospace font rendering options
    cfg->mono_bold = false;
    cfg->mono_italic = false;
    cfg->mono_underline = false;
    cfg->mono_strikethrough = false;
    cfg->mono_hinting = 0;  // TTF_HINTING_NORMAL
    cfg->mono_kerning = true;
    cfg->mono_outline = 0;
    
    // Default story font rendering options
    cfg->story_bold = false;
    cfg->story_italic = false;
    cfg->story_underline = false;
    cfg->story_strikethrough = false;
    cfg->story_hinting = 0;  // TTF_HINTING_NORMAL
    cfg->story_kerning = true;
    cfg->story_outline = 0;

    // Default gamepad settings
    cfg->gamepad_enabled = true;
    cfg->gamepad_auto_mode = true;
    cfg->steamdeck_mode = false;
    cfg->gamepad_use_dpad = true;
    cfg->gamepad_use_left_stick = true;
    cfg->gamepad_deadzone = 12000;
    cfg->gamepad_trigger_threshold = 16000;
    sdl_config_set_default_gamepad_bindings(cfg);
    sdl_config_set_default_touch_pane_bindings(cfg);
    sdl_config_clear_touch_pane_labels(cfg);
    sdl_config_clear_movement_bindings(cfg);
    cfg->overlay_panel_count = 0;
    memset(cfg->overlay_panels, 0, sizeof(cfg->overlay_panels));
}

void sdl_config_set_defaults_for_resolution(struct sdl_config* cfg, 
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height)
{
    // Start with base defaults
    sdl_config_set_defaults(cfg);
    *pane_count = 0;
    
    log_info("Setting resolution-specific defaults for %dx%d", screen_width, screen_height);
    
    // Search for matching resolution profile
    const struct resolution_profile* profile = NULL;
    for (size_t i = 0; i < NUM_RESOLUTION_PROFILES; i++) {
        if (resolution_profiles[i].width == screen_width && 
            resolution_profiles[i].height == screen_height) {
            profile = &resolution_profiles[i];
            break;
        }
    }
    
    if (profile) {
        // Apply resolution-specific settings
        log_info("Detected %s resolution - applying optimized defaults", profile->name);
        
        cfg->main_view_scale = profile->main_view_scale;
        cfg->aux_view_font_size = 0;
        cfg->menu_panel_font_size = 0;
        cfg->plain_menu_font_size = 0;
        cfg->browser_menu_font_size = 0;
        cfg->character_sheet_font_size = 0;
        // Note: margin, fullscreen, tiles, and window position/size use base defaults
        
        // Apply pane configuration
        *pane_count = profile->pane_count;
        if (*pane_count > max_panes) {
            log_warn("Profile has %d panes but max_panes is %d, truncating", 
                     *pane_count, max_panes);
            *pane_count = max_panes;
        }
        
        for (int i = 0; i < *pane_count; i++) {
            pane_configs[i].pane = profile->panes[i].type;
            pane_configs[i].where = profile->panes[i].where;
            pane_configs[i].enabled = true;
            pane_configs[i].rect.rows = profile->panes[i].rows;
            pane_configs[i].rect.cols = profile->panes[i].cols;
            pane_configs[i].font_size = 0;
            pane_configs[i].ratio = 0.0f;
        }
    } else {
        // Unknown resolution - use generic defaults
        log_info("Using generic defaults for %dx%d resolution", screen_width, screen_height);
        // The config already has base defaults from sdl_config_set_defaults()
        // pane_count is 0, so default_pane_config will be used by caller
    }

    if (screen_width == 1280 && screen_height == 800) {
        cfg->steamdeck_mode = true;
        log_info("Detected 1280x800 resolution - enabling Steam Deck UI mode by default");
    }
}

void sdl_config_apply_cmdline(struct sdl_config* cfg, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                int scale = atoi(scale_str);
                if (scale > 0) {
                    cfg->main_view_scale = scale;
                    log_info("Command line: main view scale set to %d", scale);
                }
            }
        } else if (strcmp(argv[i], "--ascii") == 0) {
            cfg->tiles = false;
            log_info("Command line: ASCII mode enabled");
        } else if (strcmp(argv[i], "--windowed") == 0) {
            cfg->fullscreen = false;
            log_info("Command line: windowed mode enabled");
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            cfg->fullscreen = true;
            log_info("Command line: fullscreen mode enabled");
        } else if (strcmp(argv[i], "--tiles") == 0) {
            cfg->tiles = true;
            log_info("Command line: tiles mode enabled");
        } else if (strcmp(argv[i], "--font-size") == 0) {
            if (argc > i + 1) {
                const char* size_str = argv[++i];
                int size = atoi(size_str);
                if (size > 0) {
                    cfg->aux_view_font_size = size;
                    log_info("Command line: auxiliary view font size set to %d", size);
                }
            }
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (argc > i + 1) {
                const char* margin_str = argv[++i];
                int margin = atoi(margin_str);
                if (margin >= 0) {
                    cfg->margin = margin;
                    log_info("Command line: margin set to %d", margin);
                }
            }
        }
    }
}

