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
static style_type material_production_styles[COLOR_STYLE_SLOT_MAX];

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
    assert(z_info->style_max <= COLOR_STYLE_SLOT_MAX);
    memcpy(material_production_styles,style_info,z_info->style_max*sizeof(*style_info));

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
    if (current_feat >= FEAT_WALL_HEAD && current_feat <= FEAT_WALL_TAIL)
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++)
                material_set_cell(y, x, current_feat, current_style,
                    CAVE_MARK | CAVE_SEEN | CAVE_GLOW | CAVE_WALL);
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

    material_pair_setup(current_feat, current_style, current_feat,
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
        assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    }
    puts("Open/broken/warded doors keep their authored silhouette: PASS");
}

static void material_visibility_tests(void)
{
    u64b rng;
    s32b saved_turn;
    s32b saved_player_turn;

    /* Hidden special features must not restore a floor underlay after the
     * rage/labyrinth visibility gate has selected unexplored space. */
    const byte hidden_features[] = {FEAT_TRAP_PIT, FEAT_STAIR_HEAD,
        FEAT_FORGE_HEAD, FEAT_RUBBLE, FEAT_SUNLIGHT};
    byte dark_a = f_info[FEAT_NONE].x_attr;
    char dark_c = f_info[FEAT_NONE].x_char;
    for (int mode = 0; mode < 2; mode++)
        for (unsigned i = 0; i < N_ELEMENTS(hidden_features); i++)
        {
            byte a, ta;
            char c, tc;
            material_reset(MATERIAL_STYLE_OLD);
            material_set_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X,
                hidden_features[i], MATERIAL_STYLE_OLD, CAVE_MARK | CAVE_GLOW);
            p_ptr->rage = mode == 0;
            g_labyrinth_view_active = mode == 1;
            map_info(MATERIAL_TEST_Y, MATERIAL_TEST_X, &a, &c, &ta, &tc);
            assert(a == dark_a && c == dark_c);
            assert(ta == dark_a && tc == dark_c);
            map_info_terrain(MATERIAL_TEST_Y, MATERIAL_TEST_X, &ta, &tc);
            assert(ta == dark_a && tc == dark_c);
        }
    material_reset(MATERIAL_STYLE_OLD);
    const int outside[][2] = {{-1, 0}, {0, -1}, {MAX_DUNGEON_HGT, 0},
        {0, MAX_DUNGEON_WID}, {32, 0}, {0, 32}};
    for (unsigned i = 0; i < N_ELEMENTS(outside); i++)
    {
        byte a, ta;
        char c, tc;
        map_info(outside[i][0], outside[i][1], &a, &c, &ta, &tc);
        assert(a == dark_a && c == dark_c && ta == dark_a && tc == dark_c);
        map_info_terrain(outside[i][0], outside[i][1], &ta, &tc);
        assert(ta == dark_a && tc == dark_c);
    }
    puts("Hidden feature underlays and out-of-map lookups remain dark: PASS");

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

static void material_alias_tests(void)
{
    const int alias = 42;
    style_type saved = style_info[alias];
    style_info[alias] = style_info[MATERIAL_STYLE_OLD];
    /* A reordered set is still the same artwork family. */
    style_info[alias].floor_colv[0] = 2;
    style_info[alias].floor_colv[1] = 0;
    material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_OLD, FEAT_FLOOR, alias);
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X));
    material_pair_setup(FEAT_WALL_EXTRA, MATERIAL_STYLE_OLD,
        FEAT_WALL_EXTRA, alias);
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X));
    style_info[alias] = saved;

    /* The lava bank overrides style 13 with the actual basalt floor sprite.
     * Compare that sprite with real basalt, not the encoded style number. */
    material_reset(63);
    material_set_cell(MATERIAL_TEST_Y, MATERIAL_TEST_X, FEAT_FLOOR,
        MATERIAL_STYLE_OLD, CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
    material_set_cell(MATERIAL_TEST_Y - 1, MATERIAL_TEST_X, FEAT_LAVA, 63,
        CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
    assert(material_tile(MATERIAL_TEST_Y, MATERIAL_TEST_X) == ((35 << 8) | 8));
    assert(!sdl_material_edge_at(MATERIAL_TEST_Y, MATERIAL_TEST_X));

    material_reset(MATERIAL_STYLE_OLD);
    material_set_cell(MATERIAL_TEST_Y - 1, MATERIAL_TEST_X, FEAT_LAVA,
        MATERIAL_STYLE_OLD, CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
    assert(material_tile(MATERIAL_TEST_Y, MATERIAL_TEST_X) == ((35 << 8) | 8));
    assert(material_tile(MATERIAL_TEST_Y + 1, MATERIAL_TEST_X) >> 8 == 23);
    assert(sdl_material_edge_at(MATERIAL_TEST_Y + 1, MATERIAL_TEST_X));
    puts("Artwork aliases, reordered variant families and actual bank overrides: PASS");
}

static void material_diagnostic_color(int tile, Uint8* r, Uint8* g, Uint8* b)
{
    int row = tile >> 8, col = tile & 255;
    *r = (Uint8)(24 + (row * 5) % 180);
    *g = (Uint8)(32 + (col * 7) % 180);
    *b = (Uint8)(48 + ((row + col) * 9) % 180);
}

static void material_mask_tests(void)
{
    const int dy[8] = {-1,-1,0,1,1,1,0,-1};
    const int dx[8] = {0,1,1,1,0,-1,-1,-1};
    SDL_Texture* saved_tileset = g_state.tileset;
    SDL_Texture* diagnostic = material_test_atlas();
    int saved_cols = g_state.tileset_cols;
    unsigned changed_masks = 0;
    g_state.tileset = diagnostic;
    g_state.tileset_cols = 32;

    /* Exhaust the independent raw eight-neighbour masks for both classes.
     * Every changed pixel must come from exactly one visible source color;
     * an overlapping alpha blend cannot satisfy this assertion. Alternating
     * donors exercise three-material corners as well as binary boundaries. */
    for (int wall = 0; wall < 2; wall++)
        for (int mask = 0; mask < 256; mask++)
        {
            byte feat = wall ? FEAT_WALL_EXTRA : FEAT_FLOOR;
            u16b info = CAVE_MARK | CAVE_SEEN | CAVE_GLOW
                | (wall ? CAVE_WALL : 0);
            Uint8 palette[9][3];
            material_pair_setup(feat, MATERIAL_STYLE_OLD, feat,
                MATERIAL_STYLE_OLD);
            for (int i = 0; i < 8; i++)
            {
                int style = mask & (1 << i)
                    ? ((i & 1) ? MATERIAL_STYLE_ACCENT : MATERIAL_STYLE_IMPORTED)
                    : MATERIAL_STYLE_OLD;
                material_set_cell(MATERIAL_TEST_Y + dy[i],
                    MATERIAL_TEST_X + dx[i], feat, style, info);
                material_diagnostic_color(material_tile(MATERIAL_TEST_Y + dy[i],
                    MATERIAL_TEST_X + dx[i]), &palette[i][0],
                    &palette[i][1], &palette[i][2]);
            }
            material_diagnostic_color(material_tile(MATERIAL_TEST_Y,
                MATERIAL_TEST_X), &palette[8][0], &palette[8][1], &palette[8][2]);
            SDL_Surface* result = material_render_cell(MATERIAL_TEST_Y,
                MATERIAL_TEST_X, 0, 0, false);
            unsigned changed = 0;
            for (int y = 0; y < TILE_SIZE; y++)
                for (int x = 0; x < TILE_SIZE; x++)
                {
                    Uint8 r,g,b,a;
                    bool found = false;
                    assert(SDL_ReadSurfacePixel(result,x,y,&r,&g,&b,&a));
                    assert(a == 255);
                    for (int i = 0; i < 9; i++)
                        if (r == palette[i][0] && g == palette[i][1]
                            && b == palette[i][2]) found = true;
                    assert(found);
                    if (r != palette[8][0] || g != palette[8][1]
                        || b != palette[8][2])
                    {
                        changed++;
                        assert(x < 3 || x >= TILE_SIZE - 3
                            || y < 3 || y >= TILE_SIZE - 3);
                    }
                }
            if (!mask) assert(!changed);
            if (changed) changed_masks++;
            SDL_DestroySurface(result);
        }
    assert(changed_masks >= 400);
    g_state.tileset = saved_tileset;
    g_state.tileset_cols = saved_cols;
    SDL_DestroyTexture(diagnostic);
    puts("All 256 floor and 256 wall masks preserve source palette and 3px contours: PASS");
}

static void material_topology_tests(void)
{
    SDL_Texture* saved_tileset = g_state.tileset;
    SDL_Texture* diagnostic = material_test_atlas();
    int saved_cols = g_state.tileset_cols;
    g_state.tileset = diagnostic;
    g_state.tileset_cols = 32;

    const byte blockers[] = {FEAT_WALL_EXTRA, FEAT_WATER, FEAT_OPEN, FEAT_FLOOR};
    for (unsigned blocker = 0; blocker < sizeof(blockers); blocker++)
    {
        material_reset(MATERIAL_STYLE_OLD);
        for (int i = 0; i < 2; i++)
            material_set_cell(MATERIAL_TEST_Y - (i == 0),
                MATERIAL_TEST_X + (i == 1), blockers[blocker], MATERIAL_STYLE_OLD,
                blocker == 3 ? 0 : CAVE_MARK | CAVE_SEEN | CAVE_GLOW
                    | (blocker == 0 ? CAVE_WALL : 0));
        SDL_Surface* plain = material_render_cell(MATERIAL_TEST_Y,
            MATERIAL_TEST_X, 0, 0, false);
        material_set_cell(MATERIAL_TEST_Y - 1, MATERIAL_TEST_X + 1,
            FEAT_FLOOR, MATERIAL_STYLE_IMPORTED,
            CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
        SDL_Surface* diagonal = material_render_cell(MATERIAL_TEST_Y,
            MATERIAL_TEST_X, 0, 0, false);
        assert(same_surface(plain, diagonal));
        SDL_DestroySurface(plain); SDL_DestroySurface(diagonal);
    }

    /* The lower material retains its complete tile; the higher material
     * recedes into it. This prevents two opposing transitions on one seam. */
    material_pair_setup(FEAT_FLOOR, MATERIAL_STYLE_IMPORTED,
        FEAT_FLOOR, MATERIAL_STYLE_IMPORTED);
    SDL_Surface* plain = material_render_cell(MATERIAL_TEST_Y,
        MATERIAL_TEST_X, 0, 0, false);
    material_set_cell(MATERIAL_TEST_Y, MATERIAL_NEIGHBOUR_X, FEAT_FLOOR,
        MATERIAL_STYLE_OLD, CAVE_MARK | CAVE_SEEN | CAVE_GLOW);
    SDL_Surface* lower = material_render_cell(MATERIAL_TEST_Y,
        MATERIAL_TEST_X, 0, 0, false);
    assert(same_surface(plain, lower));
    SDL_DestroySurface(plain); SDL_DestroySurface(lower);

    /* Wall faces may shade their own pixels, but cannot contain floor color. */
    material_pair_setup(FEAT_WALL_EXTRA, MATERIAL_STYLE_IMPORTED,
        FEAT_FLOOR, MATERIAL_STYLE_OLD);
    SDL_Surface* wall = material_render_cell(MATERIAL_TEST_Y,
        MATERIAL_TEST_X, 0, 0, false);
    Uint8 wr,wg,wb;
    material_diagnostic_color(material_tile(MATERIAL_TEST_Y,MATERIAL_TEST_X),
        &wr,&wg,&wb);
    for (int y = 0; y < TILE_SIZE; y++)
        for (int x = 0; x < TILE_SIZE; x++)
        {
            Uint8 r,g,b,a;
            assert(SDL_ReadSurfacePixel(wall,x,y,&r,&g,&b,&a));
            assert(a == 255 && r <= wr && g <= wg && b <= wb);
            /* Neutral modulation permits at most one 8-bit rounding step. */
            assert(abs((int)r * wg - (int)g * wr) <= wr + wg);
            assert(abs((int)r * wb - (int)b * wr) <= wr + wb);
        }
    SDL_DestroySurface(wall);
    g_state.tileset = saved_tileset;
    g_state.tileset_cols = saved_cols;
    SDL_DestroyTexture(diagnostic);
    puts("Occluded diagonal donors, one-sided seams and wall artwork preservation: PASS");
}

static void material_source_sampling_tests(void)
{
    SDL_Texture* saved_tileset = g_state.tileset;
    int saved_cols = g_state.tileset_cols;
    SDL_Surface* atlas = SDL_CreateSurface(32 * TILE_SIZE, 37 * TILE_SIZE,
        SDL_PIXELFORMAT_RGBA32);
    assert(atlas);
    const int rows[] = {23,35,36};
    for (unsigned i = 0; i < sizeof(rows)/sizeof(rows[0]); i++)
        for (int col = 0; col < 32; col++)
            for (int y = 0; y < TILE_SIZE; y++)
                for (int x = 0; x < TILE_SIZE; x++)
                {
                    SDL_Rect pixel = {col*TILE_SIZE+x,rows[i]*TILE_SIZE+y,1,1};
                    SDL_FillSurfaceRect(atlas,&pixel,SDL_MapSurfaceRGBA(atlas,
                        (Uint8)(rows[i]*5), (Uint8)(16+x*11),
                        (Uint8)(16+y*11), 255));
                }
    SDL_Texture* pattern = SDL_CreateTextureFromSurface(g_state.renderer,atlas);
    assert(pattern);
    SDL_DestroySurface(atlas);
    SDL_SetTextureScaleMode(pattern,SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(pattern,SDL_BLENDMODE_BLEND);
    g_state.tileset = pattern; g_state.tileset_cols = 32;
    material_pair_setup(FEAT_FLOOR,MATERIAL_STYLE_OLD,
        FEAT_FLOOR,MATERIAL_STYLE_IMPORTED);
    material_set_cell(MATERIAL_TEST_Y-1,MATERIAL_TEST_X,FEAT_FLOOR,
        MATERIAL_STYLE_ACCENT,CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
    material_set_cell(MATERIAL_TEST_Y-1,MATERIAL_TEST_X+1,FEAT_FLOOR,
        MATERIAL_STYLE_IMPORTED,CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
    SDL_Surface* rendered = material_render_cell(MATERIAL_TEST_Y,
        MATERIAL_TEST_X,0,0,false);
    unsigned donor_pixels = 0;
    for (int y = 0; y < TILE_SIZE; y++)
        for (int x = 0; x < TILE_SIZE; x++)
        {
            Uint8 r,g,b,a;
            assert(SDL_ReadSurfacePixel(rendered,x,y,&r,&g,&b,&a));
            assert(a == 255 && g == 16+x*11 && b == 16+y*11);
            assert(r == 23*5 || r == 35*5 || r == 36*5);
            if (r != 23*5) donor_pixels++;
        }
    assert(donor_pixels > 0);
    SDL_DestroySurface(rendered);
    g_state.tileset = saved_tileset; g_state.tileset_cols = saved_cols;
    SDL_DestroyTexture(pattern);
    puts("Donor texels keep their local coordinates across three-material contours: PASS");
}

static SDL_Surface* material_native_reference(bool expect_native)
{
    SDL_Texture* previous = SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,TILE_SIZE,TILE_SIZE);
    assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);
    SDL_RenderClear(g_state.renderer);
    byte a,ta; char c,tc;
    SDL_FRect dst = {0,0,TILE_SIZE,TILE_SIZE};
    map_info(MATERIAL_TEST_Y,MATERIAL_TEST_X,&a,&c,&ta,&tc);
    sdl_draw_tileset_sprite(ta,tc,&dst,false);
    assert(draw_elemental_transition(MATERIAL_TEST_Y,MATERIAL_TEST_X,&dst)
        == expect_native);
    SDL_Surface* result = SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
    return result;
}

static void material_native_contour_tests(void)
{
    for (int style = 62; style <= 63; style++)
    {
        material_pair_setup(FEAT_FLOOR,style,FEAT_FLOOR,MATERIAL_STYLE_IMPORTED);
        SDL_Surface* expected = material_native_reference(true);
        SDL_Surface* actual = material_render_cell(MATERIAL_TEST_Y,
            MATERIAL_TEST_X,0,0,false);
        assert(same_surface(expected,actual));
        SDL_DestroySurface(expected); SDL_DestroySurface(actual);
    }

    /* A missing optional native atlas must still get a generic contour. */
    material_pair_setup(FEAT_FLOOR,62,FEAT_FLOOR,MATERIAL_STYLE_IMPORTED);
    SDL_Texture* saved = snow_dirt_transition_texture;
    bool saved_attempted = snow_dirt_transition_load_attempted;
    snow_dirt_transition_texture = NULL;
    snow_dirt_transition_load_attempted = true;
    SDL_Surface* plain = material_native_reference(false);
    SDL_Surface* fallback = material_render_cell(MATERIAL_TEST_Y,
        MATERIAL_TEST_X,0,0,false);
    assert(!same_surface(plain,fallback));
    SDL_DestroySurface(plain); SDL_DestroySurface(fallback);
    snow_dirt_transition_texture = saved;
    snow_dirt_transition_load_attempted = saved_attempted;
    puts("Native snow/basalt contours stay exact; unavailable atlas gets generic fallback: PASS");
}

static void material_stress_tests(void)
{
    SDL_Texture* previous = SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,TILE_SIZE,TILE_SIZE);
    assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_FRect dst = {0,0,TILE_SIZE,TILE_SIZE};
    double seconds[2];
    for (int edge = 0; edge < 2; edge++)
    {
        material_pair_setup(FEAT_FLOOR,MATERIAL_STYLE_OLD,FEAT_FLOOR,
            edge ? MATERIAL_STYLE_IMPORTED : MATERIAL_STYLE_OLD);
        if (edge)
            material_set_cell(MATERIAL_TEST_Y-1,MATERIAL_TEST_X,FEAT_FLOOR,
                MATERIAL_STYLE_ACCENT,CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
        byte a,ta; char c,tc;
        map_info(MATERIAL_TEST_Y,MATERIAL_TEST_X,&a,&c,&ta,&tc);
        sdl_draw_map_tile_layers_at(MATERIAL_TEST_Y,MATERIAL_TEST_X,a,c,ta,tc,&dst);
        SDL_Surface* first = SDL_RenderReadPixels(g_state.renderer,NULL);
        assert(first);
        int loads = image_loads, textures = texture_creations, mallocs = allocations;
        u64b rng = Rand_state_export();
        s32b saved_turn = turn, saved_player_turn = playerturn;
        clock_t start = clock();
        for (int i = 0; i < 10000; i++)
        {
            sdl_draw_map_tile_layers_at(MATERIAL_TEST_Y,MATERIAL_TEST_X,a,c,ta,tc,&dst);
            SDL_FlushRenderer(g_state.renderer);
        }
        seconds[edge] = (double)(clock()-start)/CLOCKS_PER_SEC;
        SDL_Surface* last = SDL_RenderReadPixels(g_state.renderer,NULL);
        assert(last && same_surface(first,last));
        assert(Rand_state_export() == rng && turn == saved_turn
            && playerturn == saved_player_turn);
        /* These existing harness counters intercept the optional animation
         * loaders and allocations reached by the layered renderer. SDL's
         * internal renderer allocations are outside this assertion. */
        assert(image_loads == loads && texture_creations == textures
            && allocations == mallocs);
        SDL_DestroySurface(first); SDL_DestroySurface(last);
    }
    SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
    printf("10,000 unchanged floor draws %.3fs; three-material draws %.3fs; stable pixels/RNG, no optional asset loads or animation allocations: PASS\n",
        seconds[0],seconds[1]);
}

static void material_transparency_state_tests(void)
{
    SDL_Texture* saved_tileset = g_state.tileset;
    int saved_cols = g_state.tileset_cols;
    SDL_Texture* previous = SDL_GetRenderTarget(g_state.renderer);
    SDL_Surface* atlas = SDL_CreateSurface(32*TILE_SIZE,37*TILE_SIZE,
        SDL_PIXELFORMAT_RGBA32);
    assert(atlas);
    SDL_FillSurfaceRect(atlas,NULL,SDL_MapSurfaceRGBA(atlas,150,20,10,255));
    for (int col = 8; col <= 10; col += 2)
        for (int y = 0; y < TILE_SIZE; y++)
            for (int x = 0; x < TILE_SIZE; x++)
            {
                SDL_Rect pixel = {col*TILE_SIZE+x,36*TILE_SIZE+y,1,1};
                SDL_FillSurfaceRect(atlas,&pixel,SDL_MapSurfaceRGBA(atlas,
                    40,180,70,(x&1) ? 0 : 255));
            }
    SDL_Texture* pattern = SDL_CreateTextureFromSurface(g_state.renderer,atlas);
    SDL_DestroySurface(atlas);
    SDL_Texture* target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,TILE_SIZE,TILE_SIZE);
    assert(pattern && target);
    SDL_SetTextureScaleMode(pattern,SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(pattern,SDL_BLENDMODE_BLEND);
    g_state.tileset = pattern; g_state.tileset_cols = 32;
    material_pair_setup(FEAT_FLOOR,MATERIAL_STYLE_OLD,
        FEAT_FLOOR,MATERIAL_STYLE_IMPORTED);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);
    SDL_RenderClear(g_state.renderer);
    byte a,ta; char c,tc;
    SDL_FRect dst = {0,0,TILE_SIZE,TILE_SIZE};
    map_info(MATERIAL_TEST_Y,MATERIAL_TEST_X,&a,&c,&ta,&tc);
    sdl_draw_tileset_sprite(ta,tc,&dst,false);

    /* Deliberately non-default state proves the overlay both ignores it for
     * donor ownership and restores it for the caller. */
    SDL_SetTextureBlendMode(pattern,SDL_BLENDMODE_ADD);
    SDL_SetTextureColorMod(pattern,11,22,33);
    SDL_SetTextureAlphaMod(pattern,93);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(g_state.renderer,17,18,19,20);
    assert(sdl_material_edge_draw(MATERIAL_TEST_Y,MATERIAL_TEST_X,ta,tc,&dst,false));
    SDL_BlendMode blend;
    Uint8 r,g,b,alpha;
    assert(SDL_GetTextureBlendMode(pattern,&blend) && blend == SDL_BLENDMODE_ADD);
    assert(SDL_GetTextureColorMod(pattern,&r,&g,&b) && r == 11 && g == 22 && b == 33);
    assert(SDL_GetTextureAlphaMod(pattern,&alpha) && alpha == 93);
    assert(SDL_GetRenderDrawBlendMode(g_state.renderer,&blend) && blend == SDL_BLENDMODE_ADD);
    assert(SDL_GetRenderDrawColor(g_state.renderer,&r,&g,&b,&alpha)
        && r == 17 && g == 18 && b == 19 && alpha == 20);
    SDL_Surface* rendered = SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(rendered);
    for (int y = 0; y < TILE_SIZE; y++)
        for (int x = TILE_SIZE-2; x < TILE_SIZE; x++)
        {
            assert(SDL_ReadSurfacePixel(rendered,x,y,&r,&g,&b,&alpha));
            assert(alpha == 255);
            if (x&1) assert(r == 0 && g == 0 && b == 0);
            else assert(r == 40 && g == 180 && b == 70);
        }
    SDL_DestroySurface(rendered);
    SDL_SetRenderDrawBlendMode(g_state.renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);
    SDL_SetRenderTarget(g_state.renderer,previous);
    g_state.tileset = saved_tileset; g_state.tileset_cols = saved_cols;
    SDL_DestroyTexture(target); SDL_DestroyTexture(pattern);
    puts("Transparent donor texels replace old ownership; texture/render state restored: PASS");
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

static void material_winding_preview(void)
{
    const int width = 28, height = 22, scale = 2, heading = 40;
    style_type test_styles[COLOR_STYLE_SLOT_MAX];
    memcpy(test_styles,style_info,z_info->style_max*sizeof(*style_info));
    memcpy(style_info,material_production_styles,z_info->style_max*sizeof(*style_info));
    material_reset(41);
    const int path_y[28] = {9,9,9,9,9,10,10,10,11,11,10,10,9,9,
        9,10,11,12,12,12,13,13,12,12,12,12,12,12};
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            bool floor = (x >= 2 && x <= 9 && y >= 3 && y <= 12)
                || (x >= 11 && x <= 21 && y >= 3 && y <= 7
                    && x + y > 15 && x - y < 17)
                || (x >= 18 && x <= 25 && y >= 11 && y <= 18)
                || (x >= 3 && x <= 13 && y >= 16 && y <= 19)
                || (x >= 5 && x <= 23 && abs(y-path_y[x]) <= 1)
                || (x >= 11 && x <= 13 && y >= 6 && y <= 17)
                || (x >= 10 && x <= 21 && y >= 17 && y <= 18);
            bool pillar = (x >= 5 && x <= 6 && y >= 5 && y <= 6)
                || (x == 8 && y == 10)
                || (x >= 21 && x <= 22 && y >= 13 && y <= 14)
                || (x == 24 && y == 17)
                || (x == 7 && y == 18);
            if (pillar) floor = false;
            int style = x < 9 ? 41 : (x < 16 ? 42 : (x < 24 ? 44 : 40));
            if (y >= 16 && x < 10) style = 13;
            if (x >= 16 && x < 24 && y < 10) style = 43;
            byte feat = floor ? FEAT_FLOOR : FEAT_WALL_EXTRA;
            if (x >= 18 && x <= 20 && y >= 4 && y <= 5) feat = FEAT_CHASM;
            material_set_cell(y+2,x+2,feat,style,
                CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(floor ? 0 : CAVE_WALL));
        }
    SDL_Texture* previous = SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,
        width*TILE_SIZE*scale,heading+height*TILE_SIZE*scale);
    TTF_Font* font = TTF_OpenFont("lib/xtra/font/VictorMono-Medium.ttf",14);
    assert(target && font);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,7,9,12,255);
    SDL_RenderClear(g_state.renderer);
    material_label(font,target,6,2,"Stock artwork: winding corridors, room corners, pillars and chasm");
    material_label(font,target,6,21,"Masonry / Moss / Brickwork + DirtCave / Flagstone / original stone");
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            byte a,ta; char c,tc;
            SDL_FRect dst = {(float)(x*TILE_SIZE*scale),
                (float)(heading+y*TILE_SIZE*scale),TILE_SIZE*scale,TILE_SIZE*scale};
            map_info(y+2,x+2,&a,&c,&ta,&tc);
            sdl_draw_map_tile_layers_at(y+2,x+2,a,c,ta,tc,&dst);
        }
    SDL_Surface* image = SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(image && IMG_SavePNG(image,
        "scripts/output/material-edges-check/winding-stock-materials.png"));
    SDL_DestroySurface(image);
    TTF_CloseFont(font);
    SDL_SetRenderTarget(g_state.renderer,previous);
    SDL_DestroyTexture(target);
    memcpy(style_info,test_styles,z_info->style_max*sizeof(*style_info));
    puts("Production stock artwork winding-corridor and pillar preview: PASS");
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
        "    material_alias_tests();\n"
        "    material_mask_tests();\n"
        "    material_topology_tests();\n"
        "    material_source_sampling_tests();\n"
        "    material_native_contour_tests();\n"
        "    material_transparency_state_tests();\n"
        "    material_stress_tests();\n"
        "    material_redraw_tests();\n"
        "    material_preview();\n"
        "    material_winding_preview();")
    idle.main()


if __name__ == "__main__":
    main()
