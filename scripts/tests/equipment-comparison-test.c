#include "angband.h"
#include "log/log.h"
#include "init/init2-internal.h"
#include "object/object-compare.h"
#include "sdl/main-sdl-private.h"
#include <assert.h>

static char output[32768];
static byte colors[32768];
static size_t length;
static term test_term;
static int ring_kind, sword_kind, armor_kind;
static int comparison_sword_kind;
static char selected_name[512], reference_name[512];
static int render_width, render_height;
static cptr modal_keys;
static int modal_step;

static bool overlay_contains(cptr token)
{
    char text[32768];
    size_t at = 0;
    for (int y = 0; y < g_description_overlay.height; y++)
    {
        for (int x = 0; x < g_description_overlay.width; x++)
        {
            char ch = g_description_overlay.chars[y * g_description_overlay.width + x];
            if (!ch || isspace((unsigned char)ch)) ch = ' ';
            if (at + 2 >= sizeof(text)) break;
            if (ch != ' ' || (at && text[at - 1] != ' ')) text[at++] = ch;
        }
        if (at && text[at - 1] != ' ' && at + 2 < sizeof(text)) text[at++] = ' ';
    }
    text[at] = '\0';
    return strstr(text, token) != NULL;
}

char __wrap_inkey(void)
{
    assert(modal_keys && modal_keys[modal_step]);
    bool comparison = (modal_step % 2) != 0;
    assert(sdl_description_overlay_comparison_view() == comparison);
    assert(overlay_contains("Known item changes") == comparison);
    assert(overlay_contains("It can be thrown") != comparison);
    if (comparison)
        assert(overlay_contains(selected_name) && overlay_contains(reference_name));
    assert(g_description_overlay.has_view_toggle);
    assert(sdl_description_overlay_has_footer_action('x'));
    assert(sdl_description_overlay_has_footer_action(' '));
    return modal_keys[modal_step++];
}

SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){ 0, 0, render_width, render_height }; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane, SDL_Rect* rect)
{ (void)pane; *rect = __wrap_sdl_get_layout_screen_rect(); return true; }
void __wrap_sdl_present_if_needed(sdl_view* view) { (void)view; }

void fixture_description(const object_type** objects,
    const char** headings, int count, bool compare);

static void capture(byte attr, cptr text)
{
    size_t size = strlen(text);
    assert(length + size < sizeof(output));
    memcpy(output + length, text, size + 1);
    memset(colors + length, attr, size);
    length += size;
}

static void clear_output(void)
{
    length = 0;
    output[0] = '\0';
    text_out_hook = capture;
}

static void expect(cptr text, byte attr)
{
    cptr at = strstr(output, text);
    if (!at) fprintf(stderr, "Missing '%s' in:\n%s\n", text, output);
    assert(at);
    for (size_t i = 0; i < strlen(text); i++)
        assert(colors[at - output + i] == attr);
}

static object_type item(int kind)
{
    object_type object;
    object_prep(&object, kind);
    object.ident = IDENT_KNOWN | IDENT_HANDLED;
    return object;
}

static void compare(const object_type* selected, const object_type* reference)
{
    const object_type* objects[] = { selected, reference };
    const char* headings[] = { "Selected item", "Body" };
    object_type before = *selected;
    player_type player_before = *p_ptr;
    u64b random_before = Rand_state_export();
    clear_output();
    object_compare_summary_out(objects, headings, 2);
    assert(!memcmp(selected, &before, sizeof(before)));
    assert(!memcmp(p_ptr, &player_before, sizeof(player_before)));
    assert(random_before == Rand_state_export());
}

static void check_numeric_and_properties(void)
{
    object_type selected = item(armor_kind), reference = selected;
    selected.att += 2;
    selected.evn -= 1;
    selected.weight += 3;
    selected.number = 30;
    reference.number = 1;
    compare(&selected, &reference);
    expect("Attack +2", TERM_L_GREEN);
    expect("Evasion -1", TERM_L_RED);
    expect("Weight +0.3 lb each", TERM_WHITE);

    /* Different kind flags, using isolated in-memory template fixtures. */
    object_kind ring_before = k_info[ring_kind];
    object_kind armor_before = k_info[armor_kind];
    selected = item(ring_kind);
    reference = item(armor_kind);
    reference.tval = TV_RING;
    selected.att = reference.att = selected.evn = reference.evn = 0;
    selected.pd = reference.pd = selected.ps = reference.ps = 0;
    k_info[ring_kind].flags2 = TR2_RES_FIRE;
    k_info[armor_kind].flags2 = TR2_RES_COLD | TR2_VUL_POIS;
    compare(&selected, &reference);
    expect("+fire resistance", TERM_L_GREEN);
    expect("-cold resistance", TERM_L_RED);
    expect("-poison vulnerability", TERM_L_GREEN);
    compare(&reference, &selected);
    expect("+poison vulnerability", TERM_L_RED);

    k_info[ring_kind].flags1 = TR1_NEG_STR | TR1_STL | TR1_TUNNEL;
    k_info[armor_kind].flags1 = TR1_STR | TR1_STL | TR1_TUNNEL;
    k_info[ring_kind].flags2 = k_info[armor_kind].flags2 = 0;
    selected.stat_bonus[A_STR] = -2;
    reference.stat_bonus[A_STR] = 1;
    selected.skill_bonus[S_STL] = -1;
    reference.skill_bonus[S_STL] = -3;
    selected.pval = -2;
    reference.pval = 2;
    compare(&selected, &reference);
    expect("Str -3", TERM_L_RED);
    expect("Stealth +2", TERM_L_GREEN);
    expect("Tunneling -4", TERM_L_RED);

    k_info[ring_kind].flags1 = TR1_SHARPNESS2;
    k_info[armor_kind].flags1 = TR1_SHARPNESS;
    k_info[ring_kind].flags3 = TR3_PERMA_CURSE;
    k_info[armor_kind].flags3 = TR3_LIGHT_CURSE;
    compare(&selected, &reference);
    expect("Curse light -> permanent", TERM_L_RED);
    expect("Sharpness half -> full", TERM_L_GREEN);
    k_info[armor_kind].flags1 |= TR1_SHARPNESS2;
    compare(&selected, &reference);
    assert(!strstr(output, "Sharpness"));

    k_info[ring_kind].flags1 = k_info[armor_kind].flags1 = 0;
    k_info[ring_kind].flags3 = k_info[armor_kind].flags3 = 0;
    selected.abilities = 1;
    selected.skilltype[0] = S_MEL;
    selected.abilitynum[0] = MEL_POWER;
    reference.abilities = 0;
    compare(&selected, &reference);
    expect("+Power", TERM_L_GREEN);
    compare(&reference, &selected);
    expect("-Power", TERM_L_RED);

    k_info[ring_kind] = ring_before;
    k_info[armor_kind] = armor_before;
    puts("Signed item values, known gains/losses, negative bonuses and per-item weight PASS");
}

static void check_unknowns(void)
{
    object_type selected = item(ring_kind), reference = selected;
    selected.ident = IDENT_HANDLED;
    selected.att = 17;
    selected.evn = -19;
    selected.pd = 5;
    selected.ps = 7;
    selected.stat_bonus[A_STR] = 4;
    selected.abilities = 1;
    selected.skilltype[0] = S_MEL;
    selected.abilitynum[0] = MEL_POWER;
    compare(&selected, &reference);
    assert(!strstr(output, "Attack") && !strstr(output, "Evasion"));
    assert(!strstr(output, "Protection") && !strstr(output, "Strength"));
    expect("unidentified effects unknown", TERM_SLATE);
    char original[32768];
    SDL_strlcpy(original, output, sizeof(original));
    selected.att = -200;
    selected.pd = 9;
    selected.ps = 90;
    selected.name1 = 1;
    selected.pval = 40;
    compare(&selected, &reference);
    assert(!strcmp(original, output));

    /* Unknown is not evidence that a resistance or ability is absent. */
    artefact_type artifact_before = a_info[1];
    a_info[1].flags2 |= TR2_RES_FIRE;
    reference.name1 = 1;
    reference.abilities = 1;
    reference.skilltype[0] = S_MEL;
    reference.abilitynum[0] = MEL_POWER;
    compare(&selected, &reference);
    assert(!strstr(output, "Power") && !strstr(output, "resistance"));
    compare(&reference, &selected);
    assert(!strstr(output, "Power") && !strstr(output, "resistance"));
    a_info[1] = artifact_before;

    object_kind before = k_info[ring_kind];
    k_info[ring_kind].flags3 |= TR3_EASY_KNOW;
    k_info[ring_kind].aware = true;
    selected = item(ring_kind); reference = selected;
    selected.ident = IDENT_HANDLED;
    selected.att += 3;
    compare(&selected, &reference);
    expect("Attack +3", TERM_L_GREEN);
    assert(!strstr(output, "effects unknown"));
    k_info[ring_kind] = before;

    selected = item(sword_kind);
    reference = selected;
    selected.ident = 0; /* Unhandled floor weapon. */
    selected.att += 5;
    selected.dd += 3;
    compare(&selected, &reference);
    assert(!strstr(output, "Attack") && !strstr(output, "Base damage"));
    selected.ident = IDENT_HANDLED;
    compare(&selected, &reference);
    expect("Attack +5", TERM_L_GREEN);
    assert(strstr(output, "Base damage"));
    puts("Unknown rings, hidden affixes/abilities and unhandled floor stats remain hidden PASS");
}

static void check_dice_and_depth(void)
{
    object_type selected = item(sword_kind), reference = selected;
    selected.dd = reference.dd = 1;
    selected.ds = 8; reference.ds = 7;
    compare(&selected, &reference);
    expect("Base damage 1d7 -> 1d8", TERM_L_GREEN);
    compare(&reference, &selected);
    expect("Base damage 1d8 -> 1d7", TERM_L_RED);
    selected.dd = 2; selected.ds = 4;
    reference.dd = 1; reference.ds = 10;
    compare(&selected, &reference);
    expect("Base damage 1d10 -> 2d4", TERM_WHITE);
    selected.ds = 3; reference.ds = 7;
    compare(&selected, &reference);
    expect("Base damage 1d7 -> 2d3", TERM_WHITE);
    object_kind sword_before = k_info[sword_kind];
    object_kind armor_before = k_info[armor_kind];
    k_info[sword_kind].flags3 |= TR3_HAND_AND_A_HALF;
    reference = item(armor_kind); reference.tval = TV_SWORD;
    reference.dd = selected.dd; reference.ds = selected.ds;
    compare(&selected, &reference);
    expect("Handling one-handed -> hand-and-a-half", TERM_WHITE);
    k_info[sword_kind] = sword_before;
    k_info[armor_kind] = armor_before;

    object_kind before = k_info[armor_kind];
    k_info[armor_kind].flags4 |= TR4_DEPTH_SCALE_PS;
    selected = item(armor_kind); reference = selected;
    selected.ident = IDENT_HANDLED;
    reference.ident = IDENT_HANDLED;
    selected.pd = reference.pd = 1;
    selected.ps = 3; reference.ps = 2;
    p_ptr->depth = 0;
    compare(&selected, &reference);
    expect("Protection 1d2 -> 1d3", TERM_L_GREEN);
    char original[32768]; SDL_strlcpy(original, output, sizeof(original));
    p_ptr->depth = 20;
    compare(&selected, &reference);
    assert(!strcmp(original, output));
    selected.ident |= IDENT_KNOWN;
    reference.ident |= IDENT_KNOWN;
    compare(&selected, &reference);
    expect("Protection 1d6 -> 1d7", TERM_L_GREEN);
    k_info[armor_kind] = before;
    puts("Clear dice gains/losses colored, mixed dice neutral; known depth scaling PASS");
}

static void check_name_colors(void)
{
    object_type selected = item(comparison_sword_kind), reference = item(sword_kind);
    const object_type* objects[] = { &selected, &reference };
    const char* headings[] = { "Selected item", "Wielding" };
    byte old_color = tval_to_attr[TV_SWORD];
    bool old_unique = artifact_unique_color, old_unknown = unidentified_items_slate;
    bool old_selected_aware = k_info[selected.k_idx].aware;
    bool old_reference_aware = k_info[reference.k_idx].aware;
    char name[512];
    k_info[selected.k_idx].aware = k_info[reference.k_idx].aware = true;
    tval_to_attr[TV_SWORD] = TERM_VIOLET;
    artifact_unique_color = unidentified_items_slate = true;
    clear_output(); fixture_description(objects, headings, 2, true);
    object_desc_display(name, sizeof(name), &selected, true, 0);
    expect(name, TERM_VIOLET);
    object_desc_display(name, sizeof(name), &reference, true, 0);
    expect(name, TERM_VIOLET);
    reference.name1 = 1;
    clear_output(); fixture_description(objects, headings, 2, true);
    object_desc_display(name, sizeof(name), &reference, true, 0);
    expect(name, TERM_YELLOW);
    reference.ident = IDENT_HANDLED;
    clear_output(); fixture_description(objects, headings, 2, true);
    object_desc_display(name, sizeof(name), &reference, true, 0);
    expect(name, TERM_SLATE);
    tval_to_attr[TV_SWORD] = old_color;
    artifact_unique_color = old_unique; unidentified_items_slate = old_unknown;
    k_info[selected.k_idx].aware = old_selected_aware;
    k_info[reference.k_idx].aware = old_reference_aware;
    puts("Both names: inventory palette, known artifact gold, unknown slate PASS");
}

static void check_targets_and_limits(void)
{
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    object_type selected = item(ring_kind), floor = selected;
    inventory[INVEN_LEFT] = selected;
    inventory[INVEN_RIGHT] = selected;
    const object_type* objects[] = { &selected, &floor,
        &inventory[INVEN_LEFT], &inventory[INVEN_RIGHT] };
    const char* headings[] = { "Selected item", "On floor", "Left ring", "Right ring" };
    clear_output(); object_compare_summary_out(objects, headings, 4);
    assert(strstr(output, "vs Left ring") && strstr(output, "vs Right ring"));
    assert(!strstr(output, "On floor"));

    selected = item(sword_kind);
    inventory[INVEN_WIELD] = selected;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    objects[0] = &selected; objects[1] = &inventory[INVEN_WIELD];
    headings[1] = "Melee";
    clear_output(); object_compare_summary_out(objects, headings, 2);
    assert(strstr(output, "Melee (Harness)"));

    object_kind before = k_info[sword_kind];
    k_info[sword_kind].flags2 = 0xffffffff;
    selected = item(sword_kind); floor = item(armor_kind); floor.tval = TV_SWORD;
    objects[0] = &selected; objects[1] = &floor;
    clear_output(); object_compare_summary_out(objects, headings, 2);
    assert(strstr(output, "more changes in Full view"));
    assert(strlen(output) < 400);
    k_info[sword_kind] = before;

    compare(&selected, NULL);
    assert(strstr(output, "Base damage none ->"));
    assert(strstr(output, "(empty slot)"));
    object_type food = item(sword_kind); food.tval = TV_FOOD;
    compare(&food, &selected);
    assert(!output[0]);
    puts("Both ring choices, inactive Harness labels, empty slots, compact limit and non-equipment PASS");
}

static void check_description_wrapping(void)
{
    object_type selected = item(sword_kind), reference = item(comparison_sword_kind);
    const object_type* objects[] = { &selected, &reference };
    const char* headings[] = { "Selected item", "Melee" };
    selected.att = reference.att + 2;
    selected.evn = reference.evn - 1;
    char selected_label[512], reference_label[512];
    object_desc_display(selected_label, sizeof(selected_label), &selected, true, 0);
    object_desc_display(reference_label, sizeof(reference_label), &reference, true, 0);
    assert(strcmp(selected_label, reference_label));
    for (int width = 32; width <= 200; width += 56)
    {
        term scratch;
        assert(!term_init(&scratch, width, 255, 16));
        Term_activate(&scratch);
        text_out_hook = text_out_to_screen;
        text_out_wrap = text_out_indent = 0;
        fixture_description(objects, headings, 2, true);
        char flat[32768]; size_t at = 0;
        for (int y = 0; y <= scratch.scr->cy; y++)
        {
            int end = width;
            while (end && (scratch.scr->c[y][end - 1] == ' '
                || !scratch.scr->c[y][end - 1])) end--;
            for (int x = 0; x < end; x++)
            {
                char ch = scratch.scr->c[y][x];
                if (!ch || isspace((unsigned char)ch)) ch = ' ';
                if (ch != ' ' || (at && flat[at - 1] != ' ')) flat[at++] = ch;
            }
            if (at && flat[at - 1] != ' ') flat[at++] = ' ';
        }
        flat[at] = '\0';
        if (!strstr(flat, "Attack +2") || !strstr(flat, "Evasion -1"))
            fprintf(stderr, "Description at %d columns: %s\n", width, flat);
        assert(strstr(flat, "Known item changes"));
        assert(strstr(flat, "Attack +2") && strstr(flat, "Evasion -1"));
        assert(strstr(flat, "Selected item"));
        assert(strstr(flat, selected_label) && strstr(flat, reference_label));
        assert(!strstr(flat, "It can be thrown") && !strstr(flat, "----"));
        Term_clear();
        fixture_description(objects, headings, 2, false);
        assert(!memcmp(scratch.scr->c[0], "Selected item", 13));
        Term_activate(&test_term); term_nuke(&scratch);
    }
    puts("Full descriptions and comparison without prose at 32/88/144/200 columns PASS");
}

static void click_view(bool touch)
{
    description_overlay_layout layout;
    assert(sdl_description_overlay_layout(&layout) && layout.view_toggle);
    SDL_FRect button = layout.view_toggle_rect;
    assert(button.w > 0 && button.h > 0);
    assert(button.y + button.h <= layout.text_y + 1);
    float right = layout.close_button ? layout.close_rect.x - layout.cell_w
        : layout.panel.x + layout.panel.w - (layout.text_x - layout.panel.x);
    assert(SDL_fabsf(button.x + button.w - right) < 1.0f);
    assert(button.w < 14 * layout.cell_w);
    if (layout.close_button)
        assert(button.x + button.w < layout.close_rect.x);
    SDL_Event event = { 0 };
    if (touch)
    {
        event.type = SDL_EVENT_FINGER_DOWN;
        event.tfinger.windowID = SDL_GetWindowID(g_state.window);
        event.tfinger.fingerID = 123;
        event.tfinger.x = (button.x + button.w / 2) / render_width;
        event.tfinger.y = (button.y + button.h / 2) / render_height;
        sdl_handle_event(&g_state, &event);
        event.type = SDL_EVENT_FINGER_UP;
    }
    else
    {
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = button.x + button.w / 2;
        event.button.y = button.y + button.h / 2;
        sdl_handle_event(&g_state, &event);
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    }
    sdl_handle_event(&g_state, &event);
    Term_flush();
}

static void render_description(cptr directory, bool big)
{
    k_info[sword_kind].aware = true;
    k_info[comparison_sword_kind].aware = true;
    object_type selected = item(sword_kind), reference = item(comparison_sword_kind);
    const object_type* objects[] = { &selected, &reference };
    const char* headings[] = { "Selected item", "Melee" };
    selected.att = reference.att + 2;
    selected.evn = reference.evn - 1;
    selected.weight = reference.weight + 3;
    object_desc_display(selected_name, sizeof(selected_name), &selected, true, 0);
    object_desc_display(reference_name, sizeof(reference_name), &reference, true, 0);
    assert(strcmp(selected_name, reference_name));
    selected.dd = 2; selected.ds = 5;
    reference.dd = 1; reference.ds = 8;
    render_width = big ? 1080 : 1280;
    render_height = big ? 2340 : 720;
    sdl_config_set_defaults(&config);
    config.bigger_font = big;
    config.main_view_scale = 2;
    story_object_desc = true;
    strnfmt(config.story_font2, sizeof(config.story_font2),
        "%s/../xtra/font/EBGaramond-Regular.ttf", ANGBAND_DIR_EDIT);
    strnfmt(config.story_font, sizeof(config.story_font),
        "%s/../xtra/font/Cinzel-Medium.ttf", ANGBAND_DIR_EDIT);
    strnfmt(config.monospace_font, sizeof(config.monospace_font),
        "%s/../xtra/font/VictorMono-Medium.ttf", ANGBAND_DIR_EDIT);
    g_state.window = SDL_CreateWindow("Comparison fixture", render_width,
        render_height, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    assert(g_state.renderer);
    g_state.system_scale = big ? 2.75f : 1.0f;
    for (int i = 0; i < 256; i++)
        g_state.palette[i] = (SDL_Color){ angband_color_table[i][1],
            angband_color_table[i][2], angband_color_table[i][3], 255 };
    assert(object_info_overlay_show_multi(objects, headings, 2, true));
    assert(!sdl_description_overlay_comparison_view());
    assert(g_description_overlay.has_view_toggle);
    assert(!overlay_contains("Known item changes"));
    assert(overlay_contains("It can be thrown"));
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 20, 255);
    SDL_RenderClear(g_state.renderer);
    sdl_description_overlay_render();
    SDL_Surface* full_pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    char full_path[1024];
    strnfmt(full_path, sizeof(full_path), "%s/full-%s.png", directory,
        big ? "portrait-big" : "desktop");
    assert(full_pixels && IMG_SavePNG(full_pixels, full_path));
    SDL_DestroySurface(full_pixels);
    click_view(big);
    assert(sdl_description_overlay_comparison_view());
    assert(overlay_contains("Known item changes"));
    assert(overlay_contains(selected_name) && overlay_contains(reference_name));
    assert(!overlay_contains("It can be thrown"));
    description_overlay_layout layout;
    assert(sdl_description_overlay_layout(&layout));
    assert(layout.panel.x >= 0 && layout.panel.y >= 0);
    assert(layout.panel.x + layout.panel.w <= render_width + 1);
    assert(layout.panel.y + layout.panel.h <= render_height + 1);
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 20, 255);
    SDL_RenderClear(g_state.renderer);
    sdl_description_overlay_render();
    SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(pixels);
    /* Check actual gain/loss glyphs, so a missing atlas cannot silently
     * produce a blank screenshot while geometry assertions still pass. */
    int green = 0, red = 0;
    for (int y = (int)layout.text_y;
         y < MIN(render_height, (int)layout.text_y + 6 * layout.cell_h); y++)
        for (int x = (int)layout.text_x;
             x < MIN(render_width, (int)(layout.panel.x + layout.panel.w)); x++)
        {
            Uint8 r, g, b, a;
            assert(SDL_ReadSurfacePixel(pixels, x, y, &r, &g, &b, &a));
            if (g > 80 && g > r * 1.5f && g > b * 1.5f) green++;
            if (r > 80 && r > g * 1.5f && r > b * 1.5f) red++;
        }
    assert(green > 10 && red > 10);
    char path[1024];
    strnfmt(path, sizeof(path), "%s/comparison-%s.png", directory,
        big ? "portrait-big" : "desktop");
    assert(IMG_SavePNG(pixels, path));
    SDL_DestroySurface(pixels);
    g_description_overlay.scroll = 100;
    click_view(big);
    assert(!sdl_description_overlay_comparison_view());
    assert(g_description_overlay.scroll == 0);
    assert(overlay_contains("It can be thrown"));
    assert(!overlay_contains("Known item changes"));

    /* Controller Y is a modal UI action, independent of gameplay remaps. */
    g_description_overlay.interactive = true;
    config.gamepad_enabled = true;
    config.input_ui_mode = SDL_INPUT_UI_MODE_CONTROLLER;
    g_gamepad_state.pad_count = 1;
    config.gamepad_button_bindings[SDL_GAMEPAD_BUTTON_NORTH] = GAMEPAD_BIND_NONE;
    SDL_Event event = { 0 };
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_NORTH;
    event.gbutton.down = true;
    sdl_handle_event(&g_state, &event);
    assert(sdl_description_overlay_comparison_view());
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.down = false;
    sdl_handle_event(&g_state, &event);
    Term_flush();
    click_view(big);
    assert(!sdl_description_overlay_comparison_view());
    assert(g_description_overlay.active);

    /* Opening a different candidate always starts in Full. */
    assert(object_info_overlay_show_multi(objects, headings, 2, true));
    assert(!sdl_description_overlay_comparison_view());
    object_info_overlay_clear();

    config.input_ui_mode = SDL_INPUT_UI_MODE_PLATFORM;
    const object_info_screen_action actions[] = {
        { 'x', "x Equip" }, { ' ', "Space Pick Up" }, { ESCAPE, "Esc Close" } };
    modal_keys = "vVvx"; modal_step = 0;
    long before_turn = turn;
    assert(object_info_screen_multi_with_actions(objects, headings, 2,
        "x Equip  Space Pick Up  Esc Close", actions, N_ELEMENTS(actions), true) == 'x');
    assert(modal_step == 4 && turn == before_turn);
    modal_keys = "\033"; modal_step = 0;
    assert(!object_info_screen_multi_with_actions(objects, headings, 2,
        "x Equip  Space Pick Up  Esc Close", actions, N_ELEMENTS(actions), true));
    modal_keys = NULL; Term_flush();
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window);
    g_state.renderer = NULL; g_state.window = NULL;
}

int main(int argc, char** argv)
{
    assert(argc == 4); setbuf(stdout, NULL); log_set_level(LOG_ERROR);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    assert(!term_init(&test_term, 80, 24, 256));
    angband_term[0] = &test_term; Term_activate(&test_term);
    assert(!init_z_info() && !init_f_info() && !init_k_info() && !init_b_info());
    assert(!init_flavor_info());
    assert(!init_a_info() && !init_e_info() && !init_r_info() && !init_v_info());
    assert(!init_p_info() && !init_c_info() && !init_other());
    flavor_init();
    rp_ptr = &p_info[0]; current_character_profile = &c_info[0];
    for (int i = 1; i < z_info->k_max; i++)
    {
        if (!ring_kind && k_info[i].tval == TV_RING) ring_kind = i;
        if (!sword_kind && k_info[i].tval == TV_SWORD) sword_kind = i;
        if (!armor_kind && k_info[i].tval == TV_SOFT_ARMOR) armor_kind = i;
    }
    assert(ring_kind && sword_kind && armor_kind);
    comparison_sword_kind = lookup_kind(TV_SWORD, SV_LONG_SWORD);
    assert(comparison_sword_kind && comparison_sword_kind != sword_kind);
    /* No implicit easy-know for the synthetic unknown fixtures. */
    k_info[ring_kind].flags3 &= ~TR3_EASY_KNOW;
    k_info[ring_kind].aware = false;
    k_info[sword_kind].aware = false;
    check_numeric_and_properties();
    check_unknowns();
    check_dice_and_depth();
    check_targets_and_limits();
    check_name_colors();
    check_description_wrapping();
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO) && TTF_Init());
    char pref_path[1024];
    strnfmt(pref_path, sizeof(pref_path), "%s/../pref", ANGBAND_DIR_EDIT);
    ANGBAND_DIR_PREF = pref_path;
    reset_visuals(false);
    render_description(argv[3], false);
    render_description(argv[3], true);
    puts("Full default, mouse/touch switch, hidden comparison prose, controller Y, modal V/action/close, reopen and rendered captures PASS");
    TTF_Quit();
    SDL_Quit();
    return 0;
}
