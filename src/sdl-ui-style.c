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

#define ANGBAND_NO_IO_COMPAT
#include "fs/io_sdl.h"
#undef ANGBAND_NO_IO_COMPAT
#include "fs/resource.h"
#include "sdl-main-internal.h"

typedef struct sdl_ui_font_cache {
    TTF_Font* font;
    int pixel_height;
    int style_signature;
    char path[1024];
} sdl_ui_font_cache;

static sdl_ui_font_cache g_sdl_ui_font_cache;

#define SDL_UI_RGBA(r, g, b, a) (SDL_Color){ (r), (g), (b), (a) }

static const sdl_ui_style g_sdl_ui_style_default = {
    "default",
    "basalt-iron",
    "ui/backdrop/default",
    "ui/header/default",
    SDL_UI_RGBA(7, 9, 10, 255),
    SDL_UI_RGBA(14, 16, 16, 236),
    SDL_UI_RGBA(22, 22, 20, 224),
    SDL_UI_RGBA(93, 79, 55, 220),
    SDL_UI_RGBA(54, 50, 42, 188),
    SDL_UI_RGBA(68, 61, 49, 176),
    SDL_UI_RGBA(0, 0, 0, 110),
    SDL_UI_RGBA(226, 181, 95, 255),
    SDL_UI_RGBA(83, 63, 36, 132),
    SDL_UI_RGBA(118, 87, 42, 156),
    SDL_UI_RGBA(17, 17, 16, 128),
    SDL_UI_RGBA(234, 226, 205, 255),
    SDL_UI_RGBA(164, 156, 134, 255),
    SDL_UI_RGBA(111, 106, 94, 255),
    SDL_UI_RGBA(78, 76, 70, 255),
    SDL_UI_RGBA(220, 169, 82, 255),
    SDL_UI_RGBA(156, 123, 67, 255),
    SDL_UI_RGBA(105, 84, 53, 255),
    SDL_UI_RGBA(126, 190, 126, 255),
    SDL_UI_RGBA(226, 181, 95, 255),
    SDL_UI_RGBA(217, 91, 79, 255),
    SDL_UI_RGBA(172, 132, 205, 255),
    SDL_UI_RGBA(118, 178, 190, 255),
    24.0f, 24.0f, 18.0f, 16.0f, 2.0f, 12.0f, 10.0f,
    24.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 10.0f
};

static const sdl_ui_style g_sdl_ui_style_hub = {
    "hub",
    "engraved-stone",
    "ui/backdrop/hub",
    "ui/header/hub",
    SDL_UI_RGBA(9, 10, 10, 255),
    SDL_UI_RGBA(18, 18, 16, 238),
    SDL_UI_RGBA(30, 27, 22, 226),
    SDL_UI_RGBA(118, 93, 55, 226),
    SDL_UI_RGBA(62, 53, 41, 198),
    SDL_UI_RGBA(82, 68, 47, 190),
    SDL_UI_RGBA(0, 0, 0, 120),
    SDL_UI_RGBA(236, 188, 94, 255),
    SDL_UI_RGBA(96, 69, 34, 140),
    SDL_UI_RGBA(130, 89, 38, 166),
    SDL_UI_RGBA(17, 16, 14, 136),
    SDL_UI_RGBA(238, 229, 205, 255),
    SDL_UI_RGBA(171, 160, 132, 255),
    SDL_UI_RGBA(116, 108, 92, 255),
    SDL_UI_RGBA(80, 76, 68, 255),
    SDL_UI_RGBA(228, 175, 79, 255),
    SDL_UI_RGBA(161, 119, 59, 255),
    SDL_UI_RGBA(110, 82, 47, 255),
    SDL_UI_RGBA(126, 190, 126, 255),
    SDL_UI_RGBA(228, 175, 79, 255),
    SDL_UI_RGBA(219, 92, 77, 255),
    SDL_UI_RGBA(175, 133, 203, 255),
    SDL_UI_RGBA(111, 171, 185, 255),
    28.0f, 24.0f, 22.0f, 20.0f, 3.0f, 16.0f, 12.0f,
    30.0f, 12.0f, 12.0f, 5.0f, 4.0f, 1.0f, 2.0f, 12.0f
};

static const sdl_ui_style g_sdl_ui_style_compact_overlay = {
    "compact-overlay",
    "dark-glass-iron",
    "ui/backdrop/overlay",
    "ui/header/overlay",
    SDL_UI_RGBA(7, 9, 10, 255),
    SDL_UI_RGBA(10, 12, 12, 236),
    SDL_UI_RGBA(18, 19, 18, 226),
    SDL_UI_RGBA(91, 82, 63, 214),
    SDL_UI_RGBA(48, 47, 42, 176),
    SDL_UI_RGBA(64, 60, 50, 164),
    SDL_UI_RGBA(0, 0, 0, 116),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(78, 60, 36, 126),
    SDL_UI_RGBA(112, 82, 42, 152),
    SDL_UI_RGBA(14, 14, 13, 132),
    SDL_UI_RGBA(231, 224, 205, 255),
    SDL_UI_RGBA(156, 150, 132, 255),
    SDL_UI_RGBA(105, 101, 90, 255),
    SDL_UI_RGBA(74, 72, 68, 255),
    SDL_UI_RGBA(218, 168, 82, 255),
    SDL_UI_RGBA(145, 115, 67, 255),
    SDL_UI_RGBA(96, 80, 55, 255),
    SDL_UI_RGBA(121, 184, 124, 255),
    SDL_UI_RGBA(224, 176, 88, 255),
    SDL_UI_RGBA(214, 86, 74, 255),
    SDL_UI_RGBA(166, 128, 195, 255),
    SDL_UI_RGBA(108, 168, 180, 255),
    22.0f, 22.0f, 16.0f, 14.0f, 2.0f, 10.0f, 9.0f,
    20.0f, 9.0f, 9.0f, 4.0f, 2.0f, 1.0f, 2.0f, 8.0f
};

static const sdl_ui_style g_sdl_ui_style_browser = {
    "browser",
    "stone-table",
    "ui/backdrop/browser",
    "ui/header/browser",
    SDL_UI_RGBA(8, 10, 10, 255),
    SDL_UI_RGBA(13, 15, 15, 238),
    SDL_UI_RGBA(20, 21, 19, 228),
    SDL_UI_RGBA(84, 76, 61, 212),
    SDL_UI_RGBA(48, 47, 41, 180),
    SDL_UI_RGBA(68, 62, 50, 184),
    SDL_UI_RGBA(0, 0, 0, 104),
    SDL_UI_RGBA(220, 172, 86, 255),
    SDL_UI_RGBA(68, 54, 35, 128),
    SDL_UI_RGBA(102, 74, 38, 150),
    SDL_UI_RGBA(14, 14, 13, 130),
    SDL_UI_RGBA(232, 225, 205, 255),
    SDL_UI_RGBA(158, 151, 132, 255),
    SDL_UI_RGBA(105, 101, 91, 255),
    SDL_UI_RGBA(75, 73, 68, 255),
    SDL_UI_RGBA(216, 166, 80, 255),
    SDL_UI_RGBA(147, 116, 65, 255),
    SDL_UI_RGBA(96, 80, 55, 255),
    SDL_UI_RGBA(123, 187, 127, 255),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(214, 88, 74, 255),
    SDL_UI_RGBA(170, 130, 198, 255),
    SDL_UI_RGBA(108, 168, 182, 255),
    22.0f, 14.0f, 18.0f, 14.0f, 2.0f, 12.0f, 16.0f,
    30.0f, 12.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 0.0f
};

static const sdl_ui_style g_sdl_ui_style_document = {
    "document",
    "parchment-shadow",
    "ui/backdrop/document",
    "ui/header/document",
    SDL_UI_RGBA(8, 9, 8, 255),
    SDL_UI_RGBA(20, 18, 15, 240),
    SDL_UI_RGBA(30, 26, 20, 230),
    SDL_UI_RGBA(103, 82, 51, 224),
    SDL_UI_RGBA(61, 51, 38, 190),
    SDL_UI_RGBA(78, 64, 44, 182),
    SDL_UI_RGBA(0, 0, 0, 118),
    SDL_UI_RGBA(226, 176, 84, 255),
    SDL_UI_RGBA(84, 62, 32, 128),
    SDL_UI_RGBA(120, 80, 34, 152),
    SDL_UI_RGBA(17, 15, 13, 132),
    SDL_UI_RGBA(238, 228, 202, 255),
    SDL_UI_RGBA(172, 158, 127, 255),
    SDL_UI_RGBA(120, 108, 87, 255),
    SDL_UI_RGBA(78, 72, 64, 255),
    SDL_UI_RGBA(222, 166, 76, 255),
    SDL_UI_RGBA(151, 112, 58, 255),
    SDL_UI_RGBA(98, 73, 44, 255),
    SDL_UI_RGBA(126, 188, 125, 255),
    SDL_UI_RGBA(224, 174, 86, 255),
    SDL_UI_RGBA(215, 86, 72, 255),
    SDL_UI_RGBA(170, 128, 196, 255),
    SDL_UI_RGBA(112, 170, 180, 255),
    28.0f, 24.0f, 24.0f, 20.0f, 3.0f, 16.0f, 12.0f,
    32.0f, 12.0f, 12.0f, 5.0f, 4.0f, 1.0f, 2.0f, 12.0f
};

static const sdl_ui_style g_sdl_ui_style_item_browser = {
    "item-browser",
    "iron-ledger",
    "ui/backdrop/items",
    "ui/header/items",
    SDL_UI_RGBA(7, 9, 9, 255),
    SDL_UI_RGBA(12, 14, 14, 240),
    SDL_UI_RGBA(18, 20, 19, 230),
    SDL_UI_RGBA(81, 78, 64, 216),
    SDL_UI_RGBA(46, 48, 43, 184),
    SDL_UI_RGBA(65, 63, 51, 186),
    SDL_UI_RGBA(0, 0, 0, 108),
    SDL_UI_RGBA(214, 170, 88, 255),
    SDL_UI_RGBA(62, 58, 39, 132),
    SDL_UI_RGBA(94, 82, 44, 152),
    SDL_UI_RGBA(13, 14, 13, 132),
    SDL_UI_RGBA(231, 225, 205, 255),
    SDL_UI_RGBA(158, 154, 134, 255),
    SDL_UI_RGBA(104, 103, 92, 255),
    SDL_UI_RGBA(73, 73, 68, 255),
    SDL_UI_RGBA(210, 166, 86, 255),
    SDL_UI_RGBA(143, 116, 70, 255),
    SDL_UI_RGBA(92, 81, 57, 255),
    SDL_UI_RGBA(121, 185, 126, 255),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(211, 87, 76, 255),
    SDL_UI_RGBA(166, 128, 195, 255),
    SDL_UI_RGBA(116, 174, 182, 255),
    22.0f, 14.0f, 18.0f, 14.0f, 2.0f, 12.0f, 14.0f,
    28.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 0.0f
};

static const sdl_ui_style g_sdl_ui_style_character = {
    "character-sheet",
    "engraved-ledger",
    "ui/backdrop/character",
    "ui/header/character",
    SDL_UI_RGBA(8, 9, 8, 255),
    SDL_UI_RGBA(15, 16, 15, 240),
    SDL_UI_RGBA(24, 23, 19, 230),
    SDL_UI_RGBA(100, 82, 54, 218),
    SDL_UI_RGBA(58, 52, 42, 188),
    SDL_UI_RGBA(76, 66, 48, 184),
    SDL_UI_RGBA(0, 0, 0, 108),
    SDL_UI_RGBA(226, 177, 86, 255),
    SDL_UI_RGBA(72, 58, 37, 128),
    SDL_UI_RGBA(106, 76, 38, 150),
    SDL_UI_RGBA(14, 14, 13, 132),
    SDL_UI_RGBA(235, 227, 205, 255),
    SDL_UI_RGBA(166, 156, 132, 255),
    SDL_UI_RGBA(111, 105, 91, 255),
    SDL_UI_RGBA(78, 75, 68, 255),
    SDL_UI_RGBA(222, 170, 82, 255),
    SDL_UI_RGBA(150, 118, 64, 255),
    SDL_UI_RGBA(99, 78, 50, 255),
    SDL_UI_RGBA(124, 188, 126, 255),
    SDL_UI_RGBA(224, 176, 88, 255),
    SDL_UI_RGBA(214, 86, 74, 255),
    SDL_UI_RGBA(172, 132, 200, 255),
    SDL_UI_RGBA(116, 172, 184, 255),
    26.0f, 18.0f, 20.0f, 16.0f, 2.0f, 14.0f, 12.0f,
    28.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 0.0f
};

static const sdl_ui_style g_sdl_ui_style_crafting = {
    "crafting",
    "forge-iron",
    "ui/backdrop/forge",
    "ui/header/forge",
    SDL_UI_RGBA(8, 8, 7, 255),
    SDL_UI_RGBA(16, 14, 12, 240),
    SDL_UI_RGBA(27, 22, 17, 228),
    SDL_UI_RGBA(116, 75, 45, 220),
    SDL_UI_RGBA(62, 45, 34, 190),
    SDL_UI_RGBA(82, 56, 39, 184),
    SDL_UI_RGBA(0, 0, 0, 112),
    SDL_UI_RGBA(230, 157, 83, 255),
    SDL_UI_RGBA(86, 50, 29, 136),
    SDL_UI_RGBA(130, 69, 34, 158),
    SDL_UI_RGBA(16, 13, 11, 132),
    SDL_UI_RGBA(238, 225, 204, 255),
    SDL_UI_RGBA(170, 150, 128, 255),
    SDL_UI_RGBA(118, 98, 84, 255),
    SDL_UI_RGBA(79, 70, 64, 255),
    SDL_UI_RGBA(228, 151, 78, 255),
    SDL_UI_RGBA(160, 97, 55, 255),
    SDL_UI_RGBA(103, 67, 43, 255),
    SDL_UI_RGBA(124, 185, 126, 255),
    SDL_UI_RGBA(230, 157, 83, 255),
    SDL_UI_RGBA(218, 84, 70, 255),
    SDL_UI_RGBA(172, 126, 196, 255),
    SDL_UI_RGBA(120, 168, 178, 255),
    22.0f, 14.0f, 18.0f, 14.0f, 2.0f, 12.0f, 14.0f,
    28.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 0.0f
};

static const sdl_ui_style g_sdl_ui_style_map_recall = {
    "map-recall",
    "dark-glass-cold-light",
    "ui/backdrop/map",
    "ui/header/recall",
    SDL_UI_RGBA(6, 9, 10, 255),
    SDL_UI_RGBA(10, 14, 15, 238),
    SDL_UI_RGBA(15, 22, 22, 226),
    SDL_UI_RGBA(68, 96, 93, 218),
    SDL_UI_RGBA(42, 58, 56, 186),
    SDL_UI_RGBA(52, 72, 70, 184),
    SDL_UI_RGBA(0, 0, 0, 112),
    SDL_UI_RGBA(129, 195, 188, 255),
    SDL_UI_RGBA(37, 67, 64, 130),
    SDL_UI_RGBA(44, 98, 92, 154),
    SDL_UI_RGBA(12, 15, 15, 132),
    SDL_UI_RGBA(225, 232, 220, 255),
    SDL_UI_RGBA(150, 166, 158, 255),
    SDL_UI_RGBA(99, 116, 110, 255),
    SDL_UI_RGBA(70, 81, 78, 255),
    SDL_UI_RGBA(128, 190, 184, 255),
    SDL_UI_RGBA(82, 130, 126, 255),
    SDL_UI_RGBA(55, 90, 88, 255),
    SDL_UI_RGBA(124, 188, 126, 255),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(214, 88, 74, 255),
    SDL_UI_RGBA(168, 130, 196, 255),
    SDL_UI_RGBA(128, 190, 184, 255),
    24.0f, 18.0f, 18.0f, 16.0f, 2.0f, 12.0f, 10.0f,
    24.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 10.0f
};

static const sdl_ui_style g_sdl_ui_style_chrome = {
    "chrome",
    "hud-iron",
    "ui/backdrop/chrome",
    "ui/header/chrome",
    SDL_UI_RGBA(0, 0, 0, 255),
    SDL_UI_RGBA(4, 5, 5, 232),
    SDL_UI_RGBA(12, 13, 12, 216),
    SDL_UI_RGBA(48, 48, 42, 198),
    SDL_UI_RGBA(32, 33, 30, 170),
    SDL_UI_RGBA(48, 48, 42, 160),
    SDL_UI_RGBA(0, 0, 0, 96),
    SDL_UI_RGBA(218, 170, 82, 255),
    SDL_UI_RGBA(54, 48, 32, 124),
    SDL_UI_RGBA(82, 62, 34, 148),
    SDL_UI_RGBA(8, 8, 8, 128),
    SDL_UI_RGBA(230, 224, 206, 255),
    SDL_UI_RGBA(152, 150, 134, 255),
    SDL_UI_RGBA(100, 100, 92, 255),
    SDL_UI_RGBA(72, 72, 68, 255),
    SDL_UI_RGBA(214, 166, 80, 255),
    SDL_UI_RGBA(142, 116, 66, 255),
    SDL_UI_RGBA(92, 80, 56, 255),
    SDL_UI_RGBA(120, 184, 124, 255),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(210, 84, 72, 255),
    SDL_UI_RGBA(164, 126, 194, 255),
    SDL_UI_RGBA(108, 168, 180, 255),
    0.0f, 0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 4.0f,
    8.0f, 8.0f, 8.0f, 3.0f, 0.0f, 1.0f, 1.0f, 0.0f
};

static const sdl_ui_style g_sdl_ui_style_minimap = {
    "minimap",
    "map-glass",
    "ui/backdrop/map",
    "ui/header/map",
    SDL_UI_RGBA(5, 7, 8, 255),
    SDL_UI_RGBA(8, 10, 11, 238),
    SDL_UI_RGBA(14, 18, 18, 220),
    SDL_UI_RGBA(62, 87, 86, 210),
    SDL_UI_RGBA(38, 52, 52, 180),
    SDL_UI_RGBA(48, 68, 67, 176),
    SDL_UI_RGBA(0, 0, 0, 108),
    SDL_UI_RGBA(128, 190, 184, 255),
    SDL_UI_RGBA(34, 62, 60, 128),
    SDL_UI_RGBA(46, 98, 92, 154),
    SDL_UI_RGBA(10, 12, 12, 132),
    SDL_UI_RGBA(224, 232, 220, 255),
    SDL_UI_RGBA(148, 166, 158, 255),
    SDL_UI_RGBA(98, 116, 110, 255),
    SDL_UI_RGBA(70, 80, 78, 255),
    SDL_UI_RGBA(128, 190, 184, 255),
    SDL_UI_RGBA(82, 130, 126, 255),
    SDL_UI_RGBA(55, 90, 88, 255),
    SDL_UI_RGBA(124, 188, 126, 255),
    SDL_UI_RGBA(222, 174, 88, 255),
    SDL_UI_RGBA(214, 88, 74, 255),
    SDL_UI_RGBA(168, 130, 196, 255),
    SDL_UI_RGBA(128, 190, 184, 255),
    24.0f, 16.0f, 18.0f, 16.0f, 2.0f, 12.0f, 10.0f,
    24.0f, 10.0f, 10.0f, 4.0f, 3.0f, 1.0f, 2.0f, 0.0f
};

static SDL_Color sdl_ui_style_raw_color(byte attr)
{
    byte color = attr & 0x0Fu;

    return SDL_UI_RGBA(angband_color_table[color][1],
        angband_color_table[color][2], angband_color_table[color][3], 255);
}

const sdl_ui_style* sdl_ui_style_for_panel(u16b panel_style)
{
    switch (panel_style)
    {
    case APP_UI_PANEL_STYLE_HUB:
    case APP_UI_PANEL_STYLE_WELCOME:
        return &g_sdl_ui_style_hub;

    case APP_UI_PANEL_STYLE_BROWSER:
        return &g_sdl_ui_style_browser;

    case APP_UI_PANEL_STYLE_ITEM_BROWSER:
        return &g_sdl_ui_style_item_browser;

    case APP_UI_PANEL_STYLE_CHARACTER_SHEET:
        return &g_sdl_ui_style_character;

    case APP_UI_PANEL_STYLE_CRAFTING:
        return &g_sdl_ui_style_crafting;

    case APP_UI_PANEL_STYLE_MAP_RECALL:
    case APP_UI_PANEL_STYLE_OVERLAY_RAIL:
        return &g_sdl_ui_style_map_recall;

    case APP_UI_PANEL_STYLE_DOCUMENT:
        return &g_sdl_ui_style_document;

    case APP_UI_PANEL_STYLE_COMPACT_OVERLAY:
    case APP_UI_PANEL_STYLE_PLAIN:
        return &g_sdl_ui_style_compact_overlay;

    case APP_UI_PANEL_STYLE_STATUS_RAIL:
    case APP_UI_PANEL_STYLE_STRIP:
        return &g_sdl_ui_style_chrome;

    case APP_UI_PANEL_STYLE_MINIMAP:
        return &g_sdl_ui_style_minimap;

    default:
        return &g_sdl_ui_style_default;
    }
}

SDL_Color sdl_ui_style_with_alpha(SDL_Color color, byte alpha)
{
    color.a = alpha;
    return color;
}

SDL_Color sdl_ui_style_accent_for_attr(const sdl_ui_style* style, byte attr)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;

    switch (attr & 0x0Fu)
    {
    case TERM_RED:
    case TERM_L_RED:
        return resolved->danger;
    case TERM_GREEN:
    case TERM_L_GREEN:
        return resolved->success;
    case TERM_ORANGE:
    case TERM_YELLOW:
    case TERM_UMBER:
    case TERM_L_UMBER:
        return resolved->warning;
    case TERM_VIOLET:
        return resolved->magic;
    case TERM_SLATE:
        return resolved->panel_border_soft;
    case TERM_BLUE:
    case TERM_L_BLUE:
    case TERM_WHITE:
    case TERM_L_WHITE:
    case TERM_DARK:
        return resolved->accent;
    default:
        return sdl_ui_style_raw_color(attr);
    }
}

SDL_Color sdl_ui_style_color_for_attr(const sdl_ui_style* style, byte attr)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;

    switch (attr & 0x0Fu)
    {
    case TERM_DARK:
        return resolved->text_subtle;
    case TERM_WHITE:
    case TERM_L_WHITE:
        return resolved->text;
    case TERM_SLATE:
        return resolved->text_muted;
    case TERM_L_DARK:
        return resolved->text_disabled;
    case TERM_BLUE:
    case TERM_L_BLUE:
        return resolved->accent;
    case TERM_ORANGE:
    case TERM_YELLOW:
    case TERM_UMBER:
    case TERM_L_UMBER:
        return resolved->warning;
    case TERM_RED:
    case TERM_L_RED:
        return resolved->danger;
    case TERM_GREEN:
    case TERM_L_GREEN:
        return resolved->success;
    case TERM_VIOLET:
        return resolved->magic;
    default:
        return sdl_ui_style_raw_color(attr);
    }
}

static void sdl_ui_style_fill_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(g_state.renderer, rect);
}

static void sdl_ui_style_draw_rect(const SDL_FRect* rect, SDL_Color color)
{
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderRect(g_state.renderer, rect);
}

static SDL_Color sdl_ui_style_alpha(SDL_Color color, int alpha)
{
    color.a = (byte)MAX(0, MIN(255, alpha));
    return color;
}

static SDL_Color sdl_ui_style_mix(SDL_Color a, SDL_Color b, int b_weight,
    int total_weight)
{
    int a_weight;

    if (total_weight <= 0)
        return a;
    if (b_weight < 0)
        b_weight = 0;
    if (b_weight > total_weight)
        b_weight = total_weight;

    a_weight = total_weight - b_weight;
    a.r = (byte)((int)a.r * a_weight / total_weight
        + (int)b.r * b_weight / total_weight);
    a.g = (byte)((int)a.g * a_weight / total_weight
        + (int)b.g * b_weight / total_weight);
    a.b = (byte)((int)a.b * a_weight / total_weight
        + (int)b.b * b_weight / total_weight);
    a.a = (byte)((int)a.a * a_weight / total_weight
        + (int)b.a * b_weight / total_weight);
    return a;
}

static unsigned sdl_ui_style_slot_hash(cptr text)
{
    unsigned hash = 2166136261u;

    if (!text)
        return hash;

    while (*text)
    {
        hash ^= (unsigned char)*text++;
        hash *= 16777619u;
    }

    return hash;
}

static void sdl_ui_style_draw_material_pixels(const sdl_ui_style* style,
    int canvas_w, int canvas_h)
{
    int step;
    int dot;
    int line_w;
    unsigned hash;
    SDL_Color fleck;
    SDL_Color rule;

    if (!style || canvas_w <= 0 || canvas_h <= 0)
        return;

    step = MAX(10, sdl_ui_scale_px(34.0f));
    dot = MAX(1, sdl_ui_scale_px(1.0f));
    line_w = MAX(1, sdl_ui_scale_px(1.0f));
    hash = sdl_ui_style_slot_hash(
        (style->backdrop_slot && style->backdrop_slot[0])
            ? style->backdrop_slot
            : style->material);
    fleck = sdl_ui_style_alpha(style->panel_border_soft, 24);
    rule = sdl_ui_style_alpha(style->panel_fill_alt, 20);

    for (int y = (int)(hash % (unsigned)step); y < canvas_h; y += step)
    {
        int x_offset = (int)((hash >> ((y / step) % 11)) % (unsigned)step);

        for (int x = x_offset; x < canvas_w; x += step * 2)
        {
            sdl_ui_style_fill_rect(&(SDL_FRect){ (float)x, (float)y,
                (float)dot, (float)dot }, fleck);
        }
    }

    for (int y = step * 2; y < canvas_h; y += step * 3)
    {
        sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, (float)y,
            (float)canvas_w, (float)line_w }, rule);
    }
}

static void sdl_ui_style_draw_vignette(const sdl_ui_style* style, int canvas_w,
    int canvas_h)
{
    int bands;
    int i;

    if (!style || canvas_w <= 0 || canvas_h <= 0)
        return;

    bands = MAX(1, sdl_ui_scale_px(6.0f));
    for (i = 0; i < bands; i++)
    {
        int alpha = 18 + i * 8;
        SDL_Color shade = sdl_ui_style_alpha(style->shadow, alpha);
        float inset = (float)i;

        sdl_ui_style_fill_rect(&(SDL_FRect){ inset, inset,
            (float)canvas_w - inset * 2.0f, 1.0f }, shade);
        sdl_ui_style_fill_rect(&(SDL_FRect){ inset,
            (float)canvas_h - inset - 1.0f,
            (float)canvas_w - inset * 2.0f, 1.0f }, shade);
        sdl_ui_style_fill_rect(&(SDL_FRect){ inset, inset, 1.0f,
            (float)canvas_h - inset * 2.0f }, shade);
        sdl_ui_style_fill_rect(&(SDL_FRect){ (float)canvas_w - inset - 1.0f,
            inset, 1.0f, (float)canvas_h - inset * 2.0f }, shade);
    }
}

static void sdl_ui_style_draw_canvas_accents(const sdl_ui_style* style,
    int canvas_w, int canvas_h, int band_h)
{
    int rail_w;
    int notch;
    int step;
    SDL_Color rail;
    SDL_Color accent;

    if (!style || canvas_w <= 0 || canvas_h <= 0)
        return;

    rail_w = MAX(1, sdl_ui_scale_px(3.0f));
    notch = MAX(3, sdl_ui_scale_px(8.0f));
    step = MAX(notch * 3, sdl_ui_scale_px(72.0f));
    rail = sdl_ui_style_alpha(style->panel_border_soft, 84);
    accent = sdl_ui_style_alpha(style->accent_dim, 112);

    if (rail_w * 2 < canvas_w)
    {
        sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, 0.0f,
            (float)rail_w, (float)canvas_h }, rail);
        sdl_ui_style_fill_rect(&(SDL_FRect){ (float)rail_w, 0.0f,
            1.0f, (float)canvas_h }, sdl_ui_style_alpha(accent, 72));
        sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - rail_w),
            0.0f, (float)rail_w, (float)canvas_h }, rail);
        sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - rail_w - 1),
            0.0f, 1.0f, (float)canvas_h }, sdl_ui_style_alpha(accent, 72));
    }

    for (int x = step / 2; x < canvas_w - notch; x += step)
    {
        sdl_ui_style_fill_rect(&(SDL_FRect){ (float)x,
            (float)MAX(0, band_h - 3), (float)notch, 1.0f }, accent);
    }

    sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, (float)notch, 1.0f },
        accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, 0.0f, 1.0f, (float)notch },
        accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - notch), 0.0f,
        (float)notch, 1.0f }, accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - 1), 0.0f, 1.0f,
        (float)notch }, accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, (float)(canvas_h - 1),
        (float)notch, 1.0f }, accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, (float)(canvas_h - notch),
        1.0f, (float)notch }, accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - notch),
        (float)(canvas_h - 1), (float)notch, 1.0f }, accent);
    sdl_ui_style_fill_rect(&(SDL_FRect){ (float)(canvas_w - 1),
        (float)(canvas_h - notch), 1.0f, (float)notch }, accent);
}

void sdl_ui_style_draw_canvas(const sdl_ui_style* style, int canvas_w,
    int canvas_h)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;
    SDL_FRect rect = { 0.0f, 0.0f, (float)canvas_w, (float)canvas_h };
    int band_h;

    if (canvas_w <= 0 || canvas_h <= 0)
        return;

    sdl_ui_style_fill_rect(&rect, resolved->canvas_fill);
    sdl_ui_style_draw_material_pixels(resolved, canvas_w, canvas_h);

    band_h = sdl_ui_scale_px((resolved->header_slot
        && resolved->header_slot[0]) ? 62.0f : 42.0f);
    if (band_h > 0 && band_h < canvas_h)
    {
        SDL_Color top = sdl_ui_style_mix(resolved->panel_fill_alt,
            resolved->accent_dim, 1, 8);
        SDL_Color bottom = sdl_ui_style_mix(resolved->shadow,
            resolved->panel_fill, 1, 5);

        top.a = MIN(top.a, 82);
        bottom.a = MIN(bottom.a, 82);
        sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, 0.0f,
            (float)canvas_w, (float)band_h }, top);
        sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f,
            (float)(canvas_h - band_h), (float)canvas_w, (float)band_h },
            bottom);

        if (resolved->header_slot && resolved->header_slot[0])
        {
            SDL_Color header_rule = resolved->panel_border_soft;
            SDL_Color header_accent = resolved->accent_dim;

            header_rule.a = MIN(header_rule.a, 116);
            header_accent.a = MIN(header_accent.a, 136);
            sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, (float)(band_h - 2),
                (float)canvas_w, 1.0f }, header_rule);
            sdl_ui_style_fill_rect(&(SDL_FRect){ 0.0f, (float)(band_h - 1),
                (float)canvas_w, 1.0f }, header_accent);
        }
    }

    if (resolved->backdrop_slot && resolved->backdrop_slot[0])
    {
        sdl_ui_style_draw_canvas_accents(resolved, canvas_w, canvas_h, band_h);
    }
    sdl_ui_style_draw_vignette(resolved, canvas_w, canvas_h);
}

void sdl_ui_style_draw_panel_frame(const sdl_ui_style* style,
    const SDL_FRect* rect, bool border)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;
    int shadow_px;
    int border_px;
    int i;

    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    shadow_px = sdl_ui_scale_px(resolved->shadow_px);
    for (i = shadow_px; i > 0; i--)
    {
        SDL_FRect shadow_rect = {
            rect->x + (float)i,
            rect->y + (float)i,
            rect->w,
            rect->h
        };
        SDL_Color shadow = resolved->shadow;

        shadow.a = (byte)MIN(255, (int)shadow.a * i / MAX(1, shadow_px));
        sdl_ui_style_fill_rect(&shadow_rect, shadow);
    }

    sdl_ui_style_fill_rect(rect, resolved->panel_fill);
    if (!border)
        return;

    if (rect->w > 4.0f && rect->h > 4.0f)
    {
        SDL_Color top_rule = resolved->accent_dim;
        SDL_Color inner_rule = resolved->panel_border_soft;
        SDL_Color side_rule = resolved->accent_soft;
        int accent_w = MAX(1, sdl_ui_scale_px(2.0f));

        top_rule.a = MIN(top_rule.a, 142);
        inner_rule.a = MIN(inner_rule.a, 110);
        side_rule.a = MIN(side_rule.a, 94);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f, rect->y + 1.0f,
            MAX(0.0f, rect->w - 2.0f), 1.0f }, top_rule);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f, rect->y + 2.0f,
            (float)accent_w, MAX(0.0f, rect->h - 4.0f) }, side_rule);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f,
            rect->y + rect->h - 2.0f, MAX(0.0f, rect->w - 2.0f), 1.0f },
            inner_rule);
    }

    border_px = MAX(1, sdl_ui_scale_px(resolved->border_px));
    for (i = 0; i < border_px; i++)
    {
        SDL_FRect border_rect = {
            rect->x + (float)i,
            rect->y + (float)i,
            rect->w - (float)(i * 2),
            rect->h - (float)(i * 2)
        };

        if (border_rect.w <= 0.0f || border_rect.h <= 0.0f)
            break;
        sdl_ui_style_draw_rect(&border_rect,
            (i == 0) ? resolved->panel_border : resolved->panel_border_soft);
    }

    if (rect->w > 12.0f && rect->h > 12.0f)
    {
        int corner = MAX(2, sdl_ui_scale_px(5.0f));
        SDL_Color corner_color = sdl_ui_style_alpha(resolved->accent_dim, 132);

        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f, rect->y + 1.0f,
            (float)corner, 1.0f }, corner_color);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f, rect->y + 1.0f,
            1.0f, (float)corner }, corner_color);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + rect->w
            - (float)corner - 1.0f, rect->y + 1.0f, (float)corner, 1.0f },
            corner_color);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + rect->w - 2.0f,
            rect->y + 1.0f, 1.0f, (float)corner }, corner_color);
    }
}

void sdl_ui_style_draw_control_frame(const sdl_ui_style* style,
    const SDL_FRect* rect, u16b state_flags, bool active, bool focused,
    bool pressed)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;
    bool disabled = (state_flags & APP_UI_ITEM_FLAG_DISABLED) != 0;
    SDL_Color fill = resolved->panel_fill_alt;
    SDL_Color border = resolved->panel_border_soft;
    int border_px;
    int focus_px;
    int i;

    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f)
        return;

    if (disabled)
    {
        fill = resolved->disabled_fill;
        border = resolved->panel_border_soft;
    }
    else if (pressed)
    {
        fill = resolved->pressed_fill;
        border = resolved->focus_ring;
    }
    else if (active)
    {
        fill = resolved->selected_fill;
        border = resolved->focus_ring;
    }
    else if (focused)
    {
        fill = resolved->panel_fill_alt;
        fill.a = (byte)MIN(255, MAX((int)fill.a, 108));
        border = resolved->focus_ring;
    }
    else
    {
        fill.a = MIN(fill.a, 88);
        border.a = MIN(border.a, 176);
    }

    sdl_ui_style_fill_rect(rect, fill);

    if (!disabled && (active || focused || pressed) && rect->w > 4.0f
        && rect->h > 4.0f)
    {
        SDL_Color accent = resolved->accent;
        int accent_w = MAX(1, sdl_ui_scale_px(2.0f));
        SDL_Color top = resolved->accent_soft;

        accent.a = (byte)MIN(255, MAX((int)accent.a, 180));
        top.a = (byte)MIN(255, MAX((int)top.a, 112));
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x, rect->y,
            (float)accent_w, rect->h }, accent);
        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + (float)accent_w,
            rect->y, MAX(0.0f, rect->w - (float)accent_w), 1.0f }, top);
    }
    else if (!disabled && rect->w > 4.0f && rect->h > 4.0f)
    {
        SDL_Color top = sdl_ui_style_alpha(resolved->panel_border_soft, 80);

        sdl_ui_style_fill_rect(&(SDL_FRect){ rect->x + 1.0f, rect->y + 1.0f,
            MAX(0.0f, rect->w - 2.0f), 1.0f }, top);
    }

    border_px = MAX(1, sdl_ui_scale_px(resolved->border_px));
    for (i = 0; i < border_px; i++)
    {
        SDL_FRect border_rect = {
            rect->x + (float)i,
            rect->y + (float)i,
            rect->w - (float)(i * 2),
            rect->h - (float)(i * 2)
        };

        if (border_rect.w <= 0.0f || border_rect.h <= 0.0f)
            break;
        sdl_ui_style_draw_rect(&border_rect, border);
    }

    if (!disabled && (focused || pressed))
    {
        focus_px = MAX(1, sdl_ui_scale_px(resolved->focus_px));
        for (i = 0; i < focus_px; i++)
        {
            SDL_FRect focus_rect = {
                rect->x - (float)i,
                rect->y - (float)i,
                rect->w + (float)(i * 2),
                rect->h + (float)(i * 2)
            };

            sdl_ui_style_draw_rect(&focus_rect, resolved->focus_ring);
        }
    }
}

void sdl_ui_style_draw_rule(const sdl_ui_style* style, const SDL_FRect* rect)
{
    const sdl_ui_style* resolved = style ? style : &g_sdl_ui_style_default;

    sdl_ui_style_fill_rect(rect, resolved->divider);
}

static const char* sdl_ui_font_path(void)
{
    static char path[1024];

    if (!resource_resolve_xtra_path(path, sizeof(path),
            config.monospace_font[0] ? config.monospace_font : NULL,
            "font/VictorMono-Medium.ttf"))
    {
        return NULL;
    }

    return path;
}

static int sdl_ui_font_signature(void)
{
    int signature = 0;

    signature |= config.mono_bold ? 0x0001 : 0;
    signature |= config.mono_italic ? 0x0002 : 0;
    signature |= config.mono_underline ? 0x0004 : 0;
    signature |= config.mono_strikethrough ? 0x0008 : 0;
    signature |= (config.mono_hinting & 0xFF) << 8;
    signature |= config.mono_kerning ? 0x10000 : 0;
    signature |= (config.mono_outline & 0xFF) << 17;

    return signature;
}

static void sdl_ui_apply_font_settings(TTF_Font* font)
{
    int style = TTF_STYLE_NORMAL;

    if (!font)
        return;

    if (config.mono_bold)
        style |= TTF_STYLE_BOLD;
    if (config.mono_italic)
        style |= TTF_STYLE_ITALIC;
    if (config.mono_underline)
        style |= TTF_STYLE_UNDERLINE;
    if (config.mono_strikethrough)
        style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL)
        TTF_SetFontStyle(font, style);

    TTF_SetFontHinting(font, config.mono_hinting);
    TTF_SetFontKerning(font, config.mono_kerning);
    if (config.mono_outline > 0)
        TTF_SetFontOutline(font, config.mono_outline);
}

int sdl_ui_scale_px(float logical_value)
{
    float scale = (g_state.system_scale > 0.0f) ? g_state.system_scale : 1.0f;

    return (int)(logical_value * scale + 0.5f);
}

int sdl_ui_font_size_logical(const sdl_view* view)
{
    (void)view;
    return sdl_resolve_menu_panel_font_size(config.menu_panel_font_size);
}

void sdl_ui_font_cache_clear(void)
{
    if (g_sdl_ui_font_cache.font)
    {
        TTF_CloseFont(g_sdl_ui_font_cache.font);
        g_sdl_ui_font_cache.font = NULL;
    }

    g_sdl_ui_font_cache.pixel_height = 0;
    g_sdl_ui_font_cache.style_signature = 0;
    g_sdl_ui_font_cache.path[0] = '\0';
}

TTF_Font* sdl_ui_font_for_height(int pixel_height)
{
    const char* font_path = sdl_ui_font_path();
    int style_signature = sdl_ui_font_signature();

    if (pixel_height <= 0)
        return NULL;
    if (!font_path)
        return NULL;

    if (g_sdl_ui_font_cache.font
        && g_sdl_ui_font_cache.pixel_height == pixel_height
        && g_sdl_ui_font_cache.style_signature == style_signature
        && streq(g_sdl_ui_font_cache.path, font_path))
    {
        return g_sdl_ui_font_cache.font;
    }

    sdl_ui_font_cache_clear();

    {
        ang_file* stream = sdl_fopen(font_path, "rb");
        if (!stream)
        {
            log_warn("SDL UI font open failed for '%s': %s", font_path,
                SDL_GetError());
            return NULL;
        }

        g_sdl_ui_font_cache.font = TTF_OpenFontIO(stream, true,
            (float)pixel_height);
    }
    if (!g_sdl_ui_font_cache.font)
    {
        log_warn("SDL UI font load failed for '%s': %s", font_path,
            SDL_GetError());
        return NULL;
    }

    sdl_ui_apply_font_settings(g_sdl_ui_font_cache.font);
    g_sdl_ui_font_cache.pixel_height = pixel_height;
    g_sdl_ui_font_cache.style_signature = style_signature;
    SDL_strlcpy(g_sdl_ui_font_cache.path, font_path,
        sizeof(g_sdl_ui_font_cache.path));
    return g_sdl_ui_font_cache.font;
}

int sdl_ui_measure_text(TTF_Font* font, cptr text)
{
    int measured_w = 0;

    if (!font || !text || !text[0])
        return 0;

    if (!TTF_MeasureString(font, text, 0, 0, &measured_w, NULL))
        return 0;

    return measured_w;
}

int sdl_ui_text_left_padding(TTF_Font* font, int target_h)
{
    int pad_px = 0;
    int font_h;
    int ch;

    if (!font)
        return 0;

    for (ch = 33; ch < 127; ch++)
    {
        int minx = 0;
        int maxx = 0;
        int miny = 0;
        int maxy = 0;
        int advance = 0;

        if (!TTF_GetGlyphMetrics(font, (Uint32)ch, &minx, &maxx, &miny,
                &maxy, &advance))
        {
            continue;
        }

        if (minx < 0 && -minx > pad_px)
            pad_px = -minx;
    }

    if (pad_px <= 0)
        return 0;

    font_h = TTF_GetFontHeight(font);
    if (target_h > 0 && font_h > target_h)
    {
        pad_px = (int)((float)pad_px * (float)target_h / (float)font_h
            + 0.999f);
    }

    return pad_px;
}

int sdl_ui_text_pair_left_padding(TTF_Font* primary, TTF_Font* secondary,
    int target_h)
{
    return MAX(sdl_ui_text_left_padding(primary, target_h),
        sdl_ui_text_left_padding(secondary, target_h));
}

void sdl_ui_render_text(TTF_Font* font, float x_px, float y_px,
    SDL_Color color, cptr text)
{
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;

    if (!font || !text || !text[0])
        return;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    dst.x = x_px;
    dst.y = y_px;
    dst.w = (float)surface->w;
    dst.h = (float)surface->h;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}
