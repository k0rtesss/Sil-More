#!/usr/bin/env python3
"""Production SDL checks for generic floor and wall material edges.

The C harness uses the real map_info() and SDL tile renderer with a software
renderer.  It covers old and imported source coordinates, every floor/wall
pair, visibility gates, RNG hygiene, actor ordering, dirty redraws, and a
rendered contact sheet.  Build first with build-incremental.ps1; this script
does not modify game state outside its isolated test process.
"""

import check_cave_floor_tiles as floors
import check_idle_animation as idle


TESTS = r'''
static const int MATERIAL_TEST_Y = 10;
static const int MATERIAL_TEST_X = 11;
static const int MATERIAL_NEIGHBOUR_X = 12;
static const int MATERIAL_STYLE_OLD = 13;
static const int MATERIAL_STYLE_IMPORTED = 44;
static const int MATERIAL_STYLE_ACCENT = 45;

static void material_set_cell(int y, int x, byte feat, int style, u16b info)
{
    cave_feat[y][x] = feat;
    cave_info[y][x] = info;
    cave_color[y][x] = (byte)(COLOR_STYLE_BASE + (style & (COLOR_STYLE_SLOT_MAX - 1)));
    cave_light[y][x] = 2;
}

static void material_reset(int style)
{
    cave_fixtures_clear();
    sdl_idle_animation_clear_cells();
    use_bigtile = false;
    p_ptr->wx = p_ptr->wy = 0;
    p_ptr->blind = p_ptr->rage = p_ptr->image = 0;
    p_ptr->is_dead = false;
    p_ptr->py = p_ptr->px = 15;
    g_labyrinth_view_active = false;
    g_state.use_tiles = true;
    f_info[FEAT_CHASM].x_attr = TILE_FLAG | 34;
    f_info[FEAT_CHASM].x_char = (char)TILE_FLAG;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
        {
            material_set_cell(y, x, FEAT_FLOOR, style,
                CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
            cave_m_idx[y][x] = cave_o_idx[y][x] = 0;
            cave_rewired[y][x] = 0;
        }
}

static void material_styles(void)
{
    style_type* old = &style_info[MATERIAL_STYLE_OLD];
    style_type* imported = &style_info[MATERIAL_STYLE_IMPORTED];
    style_type* accent = &style_info[MATERIAL_STYLE_ACCENT];

    assert(z_info && style_info && z_info->style_max > MATERIAL_STYLE_ACCENT);

    /* Rows 15/23 represent the older atlas artwork; rows 33/36 are the
     * imported hand-drawn cells.  Each style has two deterministic floor
     * variants so equal-family identity can be checked independently of the
     * selected source cell. */
    old->name = imported->name = accent->name = 1;

    old->wall_row = 15; old->wall_col = 14;
    old->floor_row = 23; old->floor_col = 0;
    old->floor_count = 2; old->floor_tiled = true;
    old->floor_rowv[0] = old->floor_rowv[1] = 23;
    old->floor_colv[0] = 0; old->floor_colv[1] = 2;
    old->door_row = 0; old->door_col = 10; old->door_count = 1;
    old->door_rowv[0] = 0; old->door_colv[0] = 10;

    imported->wall_row = 33; imported->wall_col = 6;
    imported->floor_row = 36; imported->floor_col = 8;
    imported->floor_count = 2; imported->floor_tiled = true;
    imported->floor_rowv[0] = imported->floor_rowv[1] = 36;
    imported->floor_colv[0] = 8; imported->floor_colv[1] = 10;
    imported->door_row = 33; imported->door_col = 10; imported->door_count = 1;
    imported->door_rowv[0] = 33; imported->door_colv[0] = 10;

    accent->wall_row = 33; accent->wall_col = 0;
    accent->floor_row = 35; accent->floor_col = 0;
    accent->floor_count = 2; accent->floor_tiled = true;
    accent->floor_rowv[0] = accent->floor_rowv[1] = 35;
    accent->floor_colv[0] = 0; accent->floor_colv[1] = 2;
    accent->door_row = 33; accent->door_col = 2; accent->door_count = 1;
    accent->door_rowv[0] = 33; accent->door_colv[0] = 2;

    /* The isolated harness only seeds floor/wall feature visuals.  Add the
     * door visuals needed to exercise the logical wall class for open,
     * broken, and warded doors. */
    const byte door_features[] = {
        FEAT_OPEN, FEAT_BROKEN, FEAT_WARDED, FEAT_WARDED2, FEAT_WARDED3,
    };
    for (unsigned i = 0; i < sizeof(door_features) / sizeof(door_features[0]); i++)
    {
        byte feat = door_features[i];
        f_info[feat].mimic = feat;
        f_info[feat].x_attr = TILE_FLAG;
        f_info[feat].x_char = (char)(TILE_FLAG | 10);
    }
}

static int material_tile(int y, int x)
{
    byte a, ta;
    char c, tc;
    map_info(y, x, &a, &c, &ta, &tc);
    assert((ta & TILE_FLAG) && (((byte)tc) & TILE_FLAG));
    return ((int)(ta & TILE_INDEX_MASK) << 8)
        | ((byte)tc & TILE_INDEX_MASK);
}

static SDL_Surface* material_render_cell(int y, int x, byte override_a,
    char override_c, bool override_base)
{
    byte a, ta;
    char c, tc;
    SDL_Texture* previous;
    SDL_Texture* target;
    SDL_Surface* result;
    SDL_FRect dst = {0, 0, TILE_SIZE, TILE_SIZE};

    map_info(y, x, &a, &c, &ta, &tc);
    if (override_base)
    {
        a = override_a;
        c = override_c;
    }
    previous = SDL_GetRenderTarget(g_state.renderer);
    target = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, TILE_SIZE, TILE_SIZE);
    assert(target);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(g_state.renderer, target);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    sdl_draw_map_tile_layers_at(y, x, a, c, ta, tc, &dst);
    result = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(result);
    SDL_SetRenderTarget(g_state.renderer, previous);
    SDL_DestroyTexture(target);
    return result;
}

static unsigned material_diff_bounds(SDL_Surface* a, SDL_Surface* b,
    int* min_x, int* min_y, int* max_x, int* max_y)
{
    unsigned count = 0;
    *min_x = *min_y = TILE_SIZE;
    *max_x = *max_y = -1;
    assert(a->w == b->w && a->h == b->h && a->format == b->format);
    for (int y = 0; y < a->h; y++)
        for (int x = 0; x < a->w; x++)
        {
            Uint32 pa = ((Uint32*)((byte*)a->pixels + y * a->pitch))[x];
            Uint32 pb = ((Uint32*)((byte*)b->pixels + y * b->pitch))[x];
            if (pa == pb)
                continue;
            count++;
            if (x < *min_x) *min_x = x;
            if (y < *min_y) *min_y = y;
            if (x > *max_x) *max_x = x;
            if (y > *max_y) *max_y = y;
            assert(x < 4 || x >= TILE_SIZE - 4
                || y < 4 || y >= TILE_SIZE - 4);
        }
    return count;
}

static void material_assert_center(SDL_Surface* a, SDL_Surface* b)
{
    Uint8 ar, ag, ab, aa, br, bg, bb, ba;
    assert(SDL_ReadSurfacePixel(a, 8, 8, &ar, &ag, &ab, &aa));
    assert(SDL_ReadSurfacePixel(b, 8, 8, &br, &bg, &bb, &ba));
    assert(ar == br && ag == bg && ab == bb && aa == ba);
}

static void material_pair_setup(byte current_feat, int current_style,
    byte neighbour_feat, int neighbour_style)
{
    material_reset(current_style);
    material_set_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X, current_feat,
        current_style, CAVE_MARK | CAVE_SEEN | CAVE_GLOW
            | ((current_feat >= FEAT_DOOR_HEAD
                && current_feat <= FEAT_WALL_TAIL)
                ? CAVE_WALL : 0));
    material_set_cell(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X, neighbour_feat,
        neighbour_style, CAVE_MARK | CAVE_SEEN | CAVE_GLOW
            | ((neighbour_feat >= FEAT_DOOR_HEAD
                && neighbour_feat <= FEAT_WALL_TAIL)
                ? CAVE_WALL : 0));
}

static void material_pair_test(const char* label, byte current_feat,
    int current_style, byte neighbour_feat, int neighbour_style,
    int expected_current_row, int expected_current_col,
    int expected_neighbour_row, int expected_neighbour_col)
{
    SDL_Surface* plain;
    SDL_Surface* edged;
    int min_x, min_y, max_x, max_y;
    int current_tile;
    int neighbour_tile;

    material_pair_setup(current_feat, current_style, neighbour_feat,
        current_style);
    current_tile = material_tile(MATERIAL_TEST_Y, MATERIAL_TEST_X);
    neighbour_tile = material_tile(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X);
    assert((current_tile >> 8) == expected_current_row);
    assert((current_tile & 255) == expected_current_col
        || (current_tile & 255) == expected_current_col + 2);
    assert((neighbour_tile >> 8) == expected_current_row);
    assert((neighbour_tile & 255) == expected_current_col
        || (neighbour_tile & 255) == expected_current_col + 2);
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    plain = material_render_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X, 0, 0,
        false);

    material_set_cell(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X, neighbour_feat,
        neighbour_style, CAVE_MARK | CAVE_SEEN | CAVE_GLOW
            | ((neighbour_feat >= FEAT_DOOR_HEAD
                && neighbour_feat <= FEAT_WALL_TAIL)
                ? CAVE_WALL : 0));
    neighbour_tile = material_tile(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X);
    assert((neighbour_tile >> 8) == expected_neighbour_row);
    assert((neighbour_tile & 255) == expected_neighbour_col
        || (neighbour_tile & 255) == expected_neighbour_col + 2);
    assert(sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    edged = material_render_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X, 0, 0,
        false);
    assert(material_diff_bounds(plain, edged, &min_x, &min_y, &max_x, &max_y) > 0);
    material_assert_center(plain, edged);
    SDL_DestroySurface(plain);
    SDL_DestroySurface(edged);
    printf("Material pair %-16s: PASS\n", label);
}

static void material_same_family_tests(void)
{
    int first = -1;
    int second = -1;
    material_reset(MATERIAL_STYLE_OLD);
    for (int x = 2; x < 27; x++)
    {
        material_set_cell(MATERIAL_TEST_Y, x, FEAT_FLOOR,
            MATERIAL_STYLE_OLD, CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
        if (x > 2 && material_tile(MATERIAL_TEST_Y, x)
            != material_tile(MATERIAL_TEST_Y, x - 1))
        {
            first = x - 1;
            second = x;
            break;
        }
    }
    assert(first >= 0 && second >= 0);
    assert(material_tile(MATERIAL_TEST_Y, first)
        != material_tile(MATERIAL_TEST_Y, second));
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, first));
    puts("Same-family floor variants keep different source cells without an edge: PASS");
}

static void material_pair_tests(void)
{
    material_pair_test("floor/floor", FEAT_FLOOR, MATERIAL_STYLE_OLD,
        FEAT_FLOOR, MATERIAL_STYLE_IMPORTED, 23, 0, 36, 8);
    material_pair_test("floor/wall", FEAT_FLOOR, MATERIAL_STYLE_OLD,
        FEAT_WALL_EXTRA, MATERIAL_STYLE_IMPORTED, 23, 0, 33, 6);
    material_pair_test("wall/wall", FEAT_WALL_EXTRA, MATERIAL_STYLE_OLD,
        FEAT_WALL_INNER, MATERIAL_STYLE_IMPORTED, 15, 14, 33, 6);
    material_same_family_tests();

    /* All of these interesting features use a floor terrain underlay in
     * map_info().  They must remain in the floor material family for edges. */
    const byte special_floor[] = {
        FEAT_TRAP_PIT, FEAT_STAIR_HEAD, FEAT_FORGE_HEAD, FEAT_RUBBLE,
    };
    for (unsigned i = 0; i < sizeof(special_floor) / sizeof(special_floor[0]); i++)
    {
        material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_OLD,
            special_floor[i], MATERIAL_STYLE_IMPORTED);
        cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X]
            = CAVE_MARK | CAVE_SEEN | CAVE_GLOW;
        assert(sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    }
    puts("Trap/stair/forge/rubble floor underlays participate in generic edges: PASS");

    const byte door_features[] = {FEAT_OPEN, FEAT_BROKEN, FEAT_WARDED};
    for (unsigned i = 0; i < sizeof(door_features) / sizeof(door_features[0]); i++)
    {
        material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_OLD,
            door_features[i], MATERIAL_STYLE_IMPORTED);
        cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X]
            = CAVE_MARK | CAVE_SEEN | CAVE_GLOW | CAVE_WALL;
        assert(sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    }
    puts("Open/broken/warded doors participate in the wall material family: PASS");
}

static void material_visibility_tests(void)
{
    u64b rng;
    s32b saved_turn;
    s32b saved_player_turn;

    material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_OLD,
        FEAT_FLOOR, MATERIAL_STYLE_IMPORTED);
    cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X] = 0;
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));

    cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X] = CAVE_MARK;
    cave_light[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X] = 0;
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    cave_light[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X] = 1;
    assert(sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));

    p_ptr->rage = 1;
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    p_ptr->rage = 0;
    g_labyrinth_view_active = true;
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    g_labyrinth_view_active = false;
    cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X]
        = CAVE_MARK | CAVE_SEEN | CAVE_GLOW;

    rng = Rand_state_export();
    saved_turn = turn;
    saved_player_turn = playerturn;
    for (int i = 0; i < 100; i++)
        assert(sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    assert(Rand_state_export() == rng && turn == saved_turn
        && playerturn == saved_player_turn);

    p_ptr->image = 1;
    rng = Rand_state_export();
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    assert(Rand_state_export() == rng && turn == saved_turn
        && playerturn == saved_player_turn);
    p_ptr->image = 0;
    puts("Unknown/remembered-dark/rage/labyrinth gates and hallucination RNG guard: PASS");
}

static SDL_Texture* material_test_atlas(void)
{
    SDL_Surface* surface = SDL_CreateSurface(32 * TILE_SIZE, 37 * TILE_SIZE,
        SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* texture;
    assert(surface);
    for (int row = 0; row < 37; row++)
        for (int col = 0; col < 32; col++)
        {
            SDL_Rect rect = {col * TILE_SIZE, row * TILE_SIZE,
                TILE_SIZE, TILE_SIZE};
            Uint8 r = (Uint8)(24 + (row * 5) % 180);
            Uint8 g = (Uint8)(32 + (col * 7) % 180);
            Uint8 b = (Uint8)(48 + ((row + col) * 9) % 180);
            if (row == 0 && col == 31)
            {
                r = 20; g = 200; b = 60;
            }
            SDL_FillSurfaceRect(surface, &rect,
                SDL_MapSurfaceRGBA(surface, r, g, b, 255));
        }
    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    SDL_DestroySurface(surface);
    assert(texture);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

static void material_actor_tests(void)
{
    SDL_Texture* saved_tileset = g_state.tileset;
    int saved_cols = g_state.tileset_cols;
    SDL_Texture* test_tileset = material_test_atlas();
    SDL_Surface* actor;

    material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_OLD,
        FEAT_FLOOR, MATERIAL_STYLE_IMPORTED);
    g_state.tileset = test_tileset;
    g_state.tileset_cols = 32;
    actor = material_render_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X,
        TILE_FLAG, (char)(TILE_FLAG | 31), true);
    for (int y = 0; y < TILE_SIZE; y++)
        for (int x = 0; x < TILE_SIZE; x++)
        {
            Uint8 r, g, b, a;
            assert(SDL_ReadSurfacePixel(actor, x, y, &r, &g, &b, &a));
            assert(r == 20 && g == 200 && b == 60 && a == 255);
        }
    SDL_DestroySurface(actor);
    g_state.tileset = saved_tileset;
    g_state.tileset_cols = saved_cols;
    SDL_DestroyTexture(test_tileset);
    puts("Opaque actor tile remains above every borrowed material edge: PASS");
}

static void material_redraw_tests(void)
{
    SDL_Surface* before;
    SDL_Surface* incremental;
    SDL_Surface* full;

    material_reset(MATERIAL_STYLE_OLD);
    material_set_cell(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X, FEAT_FLOOR,
        MATERIAL_STYLE_IMPORTED, 0);
    Term->total_erase = true;
    prt_map();
    Term_fresh();
    before = capture(300);

    cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X]
        = CAVE_MARK | CAVE_SEEN | CAVE_GLOW;
    lite_spot(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X);
    Term_fresh();
    incremental = capture(301);
    force_map_redraw();
    Term_fresh();
    full = capture(302);
    assert(!same_surface(before, incremental));
    assert(same_surface(incremental, full));
    SDL_DestroySurface(before);
    SDL_DestroySurface(incremental);
    SDL_DestroySurface(full);

    cave_color[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X]
        = (byte)(COLOR_STYLE_BASE + MATERIAL_STYLE_ACCENT);
    cave_floor_border_redraw_neighbors(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X);
    Term_fresh();
    incremental = capture(303);
    force_map_redraw();
    Term_fresh();
    full = capture(304);
    assert(same_surface(incremental, full));
    SDL_DestroySurface(incremental);
    SDL_DestroySurface(full);

    cave_info[MATERIAL_TEST_Y][MATERIAL_NEIGHBOUR_X] = 0;
    cave_floor_border_redraw_neighbors(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X);
    Term_fresh();
    incremental = capture(305);
    force_map_redraw();
    Term_fresh();
    full = capture(306);
    assert(same_surface(incremental, full));
    SDL_DestroySurface(incremental);
    SDL_DestroySurface(full);
    puts("Material discovery/change/removal incremental repaint matches full repaint: PASS");
}

static void material_panel_cell(int panel, int y, int x)
{
    bool wall = false;
    int style = MATERIAL_STYLE_OLD;
    byte feat = FEAT_FLOOR;

    if (panel == 0)
        style = x < 5 ? MATERIAL_STYLE_OLD : MATERIAL_STYLE_IMPORTED;
    else if (panel == 1)
        style = MATERIAL_STYLE_OLD;
    else if (panel == 2)
    {
        wall = x >= 5;
        style = wall ? MATERIAL_STYLE_IMPORTED : MATERIAL_STYLE_OLD;
        feat = wall ? FEAT_WALL_EXTRA : FEAT_FLOOR;
    }
    else if (panel == 3)
    {
        wall = true;
        style = x < 5 ? MATERIAL_STYLE_OLD : MATERIAL_STYLE_IMPORTED;
        feat = FEAT_WALL_EXTRA;
    }
    else if (panel == 4)
    {
        if (x >= 5 && y < 4) style = MATERIAL_STYLE_IMPORTED;
        else if (x < 5 && y >= 4) style = MATERIAL_STYLE_ACCENT;
        else style = MATERIAL_STYLE_OLD;
        wall = (x == 4 || x == 5) && y >= 2 && y <= 5;
        feat = wall ? FEAT_WALL_INNER : FEAT_FLOOR;
    }
    else
    {
        style = (x < 5) ? MATERIAL_STYLE_IMPORTED : MATERIAL_STYLE_OLD;
        wall = y < 2 || y >= 7 || x < 2 || x >= 8;
        feat = wall ? FEAT_WALL_EXTRA : FEAT_FLOOR;
        if (x >= 4 && x <= 5 && y >= 3 && y <= 4)
            feat = FEAT_CHASM;
    }

    material_set_cell(y + 2, x + 2, feat, style,
        CAVE_MARK | CAVE_SEEN | CAVE_GLOW | (wall ? CAVE_WALL : 0));
}

static void material_label(TTF_Font* font, SDL_Texture* target,
    int x, int y, const char* text)
{
    SDL_Color color = {238, 238, 224, 255};
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect dst;

    if (!font)
        return;
    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return;
    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    dst = (SDL_FRect){(float)x, (float)y, (float)surface->w,
        (float)surface->h};
    SDL_DestroySurface(surface);
    if (!texture)
        return;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    (void)target;
}

static void material_preview(void)
{
    const int panels = 6;
    const int panel_cols = 3;
    const int map_w = 10;
    const int map_h = 8;
    const int scale = 2;
    const int panel_w = map_w * TILE_SIZE * scale;
    const int panel_h = 18 + map_h * TILE_SIZE * scale;
    const char* labels[] = {
        "floor old/new", "floor variants", "floor/wall", "wall old/new",
        "3-way junction", "chasm + mixed",
    };
    SDL_Texture* previous = SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        panel_cols * panel_w, ((panels + panel_cols - 1) / panel_cols) * panel_h);
    TTF_Font* font = TTF_OpenFont("lib/xtra/font/VictorMono-Medium.ttf", 12);
    SDL_Surface* image;

    assert(target && font);
    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(g_state.renderer, target);
    SDL_SetRenderDrawColor(g_state.renderer, 7, 9, 12, 255);
    SDL_RenderClear(g_state.renderer);

    for (int panel = 0; panel < panels; panel++)
    {
        int origin_x = (panel % panel_cols) * panel_w;
        int origin_y = (panel / panel_cols) * panel_h;
        material_reset(MATERIAL_STYLE_OLD);
        for (int y = 0; y < map_h; y++)
            for (int x = 0; x < map_w; x++)
                material_panel_cell(panel, y, x);
        material_label(font, target, origin_x + 4, origin_y + 2, labels[panel]);
        for (int y = 0; y < map_h; y++)
            for (int x = 0; x < map_w; x++)
            {
                byte a, ta;
                char c, tc;
                SDL_FRect dst = {
                    (float)(origin_x + x * TILE_SIZE * scale),
                    (float)(origin_y + 18 + y * TILE_SIZE * scale),
                    TILE_SIZE * scale, TILE_SIZE * scale,
                };
                map_info(y + 2, x + 2, &a, &c, &ta, &tc);
                sdl_draw_map_tile_layers_at(y + 2, x + 2, a, c, ta, tc,
                    &dst);
            }
    }

    image = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(image);
    assert(IMG_SavePNG(image,
        "scripts/output/material-edges-check/contact-sheet.png"));
    SDL_DestroySurface(image);
    TTF_CloseFont(font);
    SDL_SetRenderTarget(g_state.renderer, previous);
    SDL_DestroyTexture(target);
    puts("Rendered labeled floor/wall/chasm material contact sheet: PASS");
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/material-edges-check"
    idle.HARNESS = idle.HARNESS.replace(
        "scripts/output/idle-animation-check", "scripts/output/material-edges-check")
    idle.HARNESS = idle.HARNESS.replace(
        "int main(void) {", floors.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    asynchronous_tests();",
        "    asynchronous_tests();\n"
        "    floor_templates();\n"
        "    material_styles();\n"
        "    material_pair_tests();\n"
        "    material_visibility_tests();\n"
        "    material_actor_tests();\n"
        "    material_redraw_tests();\n"
        "    material_preview();")
    idle.main()


if __name__ == "__main__":
    main()
