#include "angband.h"
#include "log/log.h"
#include "init/init2-internal.h"
#include "object/object-compare.h"
#include "object/object-info.h"
#include "player/player-upkeep-internal.h"
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
static cptr render_directory;
static bool render_big;

static void item_label(char* name, size_t size, const object_type* object)
{
    object_desc(name, size, object, true, 3);
    name[0] = (char)toupper((unsigned char)name[0]);
}

static void check_controls_and_flow(const description_overlay_layout* layout)
{
    const SDL_FRect* controls[] = { &layout->view_toggle_rect, &layout->close_rect };
    for (int i = 0; i < (layout->close_button ? 2 : 1); i++)
    {
        const SDL_FRect* rect = controls[i];
        assert(rect->x >= layout->panel.x && rect->y >= layout->panel.y);
        assert(rect->x + rect->w <= layout->panel.x + layout->panel.w + 1);
        assert(rect->y + rect->h <= layout->panel.y + layout->panel.h + 1);
    }
    assert(layout->text_y - layout->panel.y <= layout->cell_h / 2 + 1);
    assert(layout->view_toggle_rect.y == layout->text_y);
    assert(layout->fixed_rows > 0);
    assert(g_description_overlay.chars[0] == 'S');
    for (int row = 0; row < layout->fixed_rows; row++)
    {
        char line[1024];
        int used = g_description_overlay.width;
        while (used && isspace((unsigned char)g_description_overlay.chars[
                row * g_description_overlay.width + used - 1]))
            used--;
        assert(used < sizeof(line));
        memcpy(line, g_description_overlay.chars + row * g_description_overlay.width, used);
        line[used] = '\0';
        bool story = g_description_overlay.story
            && (g_description_overlay.story[row * g_description_overlay.width] & STORY_FLAG_USE);
        int width = story ? sdl_description_overlay_story_text_width(line,
            used, STORY_FONT_SLOT_SECONDARY) : used * layout->cell_w;
        if (width > layout->view_toggle_rect.x - layout->text_x + 1)
            fprintf(stderr,"Header text overlaps controls: row=%d width=%d available=%.1f %s\n",
                row,width,layout->view_toggle_rect.x-layout->text_x,line);
        assert(width <= layout->view_toggle_rect.x - layout->text_x + 1);
    }
}

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
    if (modal_step < 2)
    {
        description_overlay_layout layout;
        assert(sdl_description_overlay_layout(&layout));
        check_controls_and_flow(&layout);
        SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 20, 255);
        SDL_RenderClear(g_state.renderer);
        sdl_description_overlay_render();
        SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
        char path[1024];
        strnfmt(path, sizeof(path), "%s/modal-%s-%s.png", render_directory,
            comparison ? "comparison" : "full", render_big ? "portrait-big" : "desktop");
        assert(pixels && IMG_SavePNG(pixels, path));
        SDL_DestroySurface(pixels);
        SDL_Rect detail_rect = {
            (int)layout.panel.x, (int)layout.panel.y,
            (int)SDL_ceilf(layout.panel.w), (int)SDL_ceilf(layout.panel.h) };
        pixels = SDL_RenderReadPixels(g_state.renderer, &detail_rect);
        strnfmt(path, sizeof(path), "%s/modal-%s-detail-%s.png", render_directory,
            comparison ? "comparison" : "full", render_big ? "portrait-big" : "desktop");
        assert(pixels && IMG_SavePNG(pixels, path));
        SDL_DestroySurface(pixels);
    }
    return modal_keys[modal_step++];
}

SDL_Rect __wrap_sdl_get_layout_screen_rect(void)
{ return (SDL_Rect){ 0, 0, render_width, render_height }; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane, SDL_Rect* rect)
{ (void)pane; *rect = __wrap_sdl_get_layout_screen_rect(); return true; }
void __wrap_sdl_present_if_needed(sdl_view* view) { (void)view; }
bool __wrap_sdl_mobile_portrait_layout_active(void)
{ return g_startup_device_class == SDL_STARTUP_DEVICE_MOBILE_TOUCH
    && render_width < render_height; }
bool __wrap_sdl_touch_only_device_active(void)
{ return g_startup_device_class == SDL_STARTUP_DEVICE_MOBILE_TOUCH; }

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
    object_type inventory_before[INVEN_TOTAL];
    memcpy(inventory_before, inventory, sizeof(inventory_before));
    u64b random_before = Rand_state_export();
    clear_output();
    object_compare_summary_out(objects, headings, 2);
    assert(!strstr(output, "Item attack") && !strstr(output, "Item evasion")
        && !strstr(output, "Base damage"));
    assert(!memcmp(selected, &before, sizeof(before)));
    assert(!memcmp(p_ptr, &player_before, sizeof(player_before)));
    assert(!memcmp(inventory, inventory_before, sizeof(inventory_before)));
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
    expect("Attack +17", TERM_L_GREEN);
    expect("Evasion -19", TERM_L_RED);
    assert(strstr(output, "Protection") && !strstr(output, "Str +4"));
    expect("unidentified effects unknown", TERM_SLATE);
    char original[32768];
    SDL_strlcpy(original, output, sizeof(original));
    /* Identical observable numbers, arbitrarily different hidden effects. */
    selected.name1 = 1;
    selected.pval = 40;
    selected.stat_bonus[A_STR] = 12;
    selected.skill_bonus[S_MEL] = 20;
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
    expect("Attack +5", TERM_L_GREEN);
    assert(strstr(output, "Damage"));
    selected.ident = IDENT_HANDLED;
    compare(&selected, &reference);
    expect("Attack +5", TERM_L_GREEN);
    assert(strstr(output, "Damage"));
    puts("Unknown/floor combat stats visible; hidden identity, bonuses and abilities stay hidden PASS");
}

static void check_dice_and_depth(void)
{
    int old_food = p_ptr->food;
    p_ptr->food = PY_FOOD_FULL;
    object_type selected = item(sword_kind), reference = selected;
    selected.dd = reference.dd = 1;
    selected.ds = 8; reference.ds = 7;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 1d8", TERM_L_GREEN);
    compare(&reference, &selected);
    expect("Damage 1d8 -> 1d7", TERM_L_RED);
    selected.dd = 2; selected.ds = 4;
    reference.dd = 1; reference.ds = 10;
    compare(&selected, &reference);
    expect("Damage 1d10 -> 2d4", TERM_WHITE);
    selected.ds = 3; reference.ds = 7;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 2d3", TERM_WHITE);
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
    p_ptr->depth = 20;
    compare(&selected, &reference);
    expect("Protection 1d6 -> 1d7", TERM_L_GREEN);
    selected.ident |= IDENT_KNOWN;
    reference.ident |= IDENT_KNOWN;
    compare(&selected, &reference);
    expect("Protection 1d6 -> 1d7", TERM_L_GREEN);
    k_info[armor_kind] = before;
    p_ptr->food = old_food;
    puts("Clear dice gains/losses colored, mixed dice neutral; observable depth armor before ID PASS");
}

static void learn(int skill, int ability)
{
    p_ptr->innate_ability[skill][ability] = true;
    p_ptr->have_ability[skill][ability] = true;
    p_ptr->active_ability[skill][ability] = true;
}

static void check_loadout_math(void)
{
    player_type saved_player = *p_ptr;
    object_type saved_inventory[INVEN_TOTAL];
    memcpy(saved_inventory, inventory, sizeof(saved_inventory));
    object_kind saved_sword = k_info[sword_kind];
    object_kind saved_other = k_info[comparison_sword_kind];
    object_kind saved_ring = k_info[ring_kind];
    object_kind saved_armor = k_info[armor_kind];
    u32b saved_character_flags = current_character_profile->flags_u;
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->stat_base[A_STR] = 5;
    p_ptr->stat_base[A_DEX] = 2;
    p_ptr->skill_base[S_MEL] = p_ptr->skill_base[S_EVN] = 10;
    k_info[sword_kind].flags1 = k_info[sword_kind].flags2 = 0;
    k_info[sword_kind].flags3 = k_info[sword_kind].flags4 = 0;
    k_info[comparison_sword_kind].flags1 = k_info[comparison_sword_kind].flags2 = 0;
    k_info[comparison_sword_kind].flags3 = k_info[comparison_sword_kind].flags4 = 0;
    k_info[ring_kind].flags1 = k_info[ring_kind].flags2 = 0;
    k_info[ring_kind].flags3 = k_info[ring_kind].flags4 = 0;
    k_info[armor_kind].flags1 = k_info[armor_kind].flags2 = 0;
    k_info[armor_kind].flags3 = k_info[armor_kind].flags4 = 0;
    object_type selected = item(sword_kind), reference = item(comparison_sword_kind);
    selected.dd = reference.dd = 1;
    selected.ds = reference.ds = 8;
    selected.att = reference.att = selected.evn = reference.evn = 0;
    selected.weight = 50; reference.weight = 10;
    compare(&selected, &reference);
    expect("Damage 1d9 -> 1d13", TERM_L_GREEN);
    p_ptr->stat_base[A_STR] = -5;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 1d3", TERM_L_RED);

    p_ptr->stat_base[A_STR] = 5;
    learn(S_MEL, MEL_RAPID_ATTACK);
    compare(&selected, &reference);
    expect("Damage 1d9 -> 1d10", TERM_L_GREEN);
    p_ptr->innate_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] = false;
    p_ptr->have_ability[S_MEL][MEL_RAPID_ATTACK] = false;

    p_ptr->stat_base[A_STR] = 2;
    selected.weight = reference.weight = 30;
    selected.ds = reference.ds = 5;
    k_info[sword_kind].flags3 = TR3_HAND_AND_A_HALF;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 1d9", TERM_L_GREEN);
    current_character_profile->flags_u |= UNQ_MEL_MAEDHROS;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 1d10", TERM_L_GREEN);
    current_character_profile->flags_u = saved_character_flags;
    learn(S_MEL, MEL_CONTROL);
    compare(&selected, &reference);
    assert(strstr(output, "Damage 1d7 (unchanged)"));
    p_ptr->innate_ability[S_MEL][MEL_CONTROL] = false;
    p_ptr->active_ability[S_MEL][MEL_CONTROL] = false;
    p_ptr->have_ability[S_MEL][MEL_CONTROL] = false;

    int shield_kind = lookup_kind(TV_SHIELD, SV_ROUND_SHIELD);
    assert(shield_kind);
    inventory[INVEN_ARM] = item(shield_kind);
    inventory[INVEN_ARM].pd = 1; inventory[INVEN_ARM].ps = 4;
    inventory[INVEN_ARM].evn = 2;
    compare(&selected, &reference);
    assert(strstr(output, "Damage 1d7 (unchanged)"));
    k_info[sword_kind].flags3 = TR3_TWO_HANDED;
    compare(&selected, &reference);
    expect("Evasion -2", TERM_L_RED);
    expect("Armor 1-4 -> 0-0", TERM_L_RED);
    object_wipe(&inventory[INVEN_ARM]);
    k_info[sword_kind].flags3 = 0;

    /* Newly equipped grants apply; a learned, intentionally disabled grant
     * must stay disabled. Removing a known grant also removes its damage. */
    selected.abilities = 1;
    selected.skilltype[0] = S_MEL; selected.abilitynum[0] = MEL_POWER;
    compare(&selected, &reference);
    expect("Damage 1d7 -> 1d8", TERM_L_GREEN);
    learn(S_MEL, MEL_POWER);
    p_ptr->active_ability[S_MEL][MEL_POWER] = false;
    compare(&selected, &reference);
    assert(strstr(output, "Damage 1d7 (unchanged)"));
    p_ptr->innate_ability[S_MEL][MEL_POWER] = false;
    p_ptr->have_ability[S_MEL][MEL_POWER] = false;
    inventory[INVEN_WIELD] = selected;
    p_ptr->active_ability[S_MEL][MEL_POWER] = true;
    calc_bonuses_for_preview();
    compare(&reference, &inventory[INVEN_WIELD]);
    expect("Damage 1d8 -> 1d7", TERM_L_RED);

    /* Ring replacement recalculates damage through strength and the weapon
     * cap, and distinguishes the two actual ring destinations. */
    selected.abilities = 0;
    inventory[INVEN_WIELD] = selected;
    inventory[INVEN_WIELD].weight = 50;
    object_type ring = item(ring_kind);
    ring.att = ring.evn = ring.pd = ring.ps = 0;
    ring.stat_bonus[A_STR] = 2;
    k_info[ring_kind].flags1 = TR1_STR;
    inventory[INVEN_LEFT] = ring;
    inventory[INVEN_LEFT].stat_bonus[A_STR] = 1;
    inventory[INVEN_RIGHT] = ring;
    inventory[INVEN_RIGHT].stat_bonus[A_STR] = 3;
    calc_bonuses_for_preview();
    compare(&ring, &inventory[INVEN_LEFT]);
    assert(strstr(output, "Damage 1d10 (unchanged)")); /* already weight-capped */
    p_ptr->stat_base[A_STR] = 0;
    compare(&ring, &inventory[INVEN_LEFT]);
    expect("Damage 1d9 -> 1d10", TERM_L_GREEN);
    compare(&ring, &inventory[INVEN_RIGHT]);
    expect("Damage 1d9 -> 1d8", TERM_L_RED);
    object_wipe(&inventory[INVEN_RIGHT]);
    compare(&inventory[INVEN_LEFT], NULL);
    assert(strstr(output, "Damage 1d6 (unchanged)"));
    inventory[INVEN_LEFT].number = 2;
    compare(&inventory[INVEN_LEFT], NULL);
    expect("Damage 1d6 -> 1d7", TERM_L_GREEN);
    inventory[INVEN_LEFT].number = 1;
    inventory[INVEN_RIGHT] = ring;
    inventory[INVEN_RIGHT].stat_bonus[A_STR] = 3;

    /* Unchanged unidentified gear retains the visible attribute total while
     * its identity and exact bonus remain undisclosed. */
    object_wipe(&inventory[INVEN_LEFT]);
    inventory[INVEN_RIGHT].ident = 0;
    p_ptr->stat_base[A_STR] = 0;
    calc_bonuses_for_preview();
    selected.ds = reference.ds = 5;
    selected.weight = 50; reference.weight = 10;
    compare(&selected, &reference);
    expect("Damage 1d6 -> 1d8", TERM_L_GREEN);
    object_wipe(&inventory[INVEN_RIGHT]);

    /* Dexterity, Parry, Heavy Armour and Blocking change loadout totals. */
    p_ptr->stat_base[A_STR] = 2;
    learn(S_EVN, EVN_PARRY);
    selected.evn = 2;
    compare(&selected, &reference);
    expect("Evasion +4", TERM_L_GREEN);
    p_ptr->innate_ability[S_EVN][EVN_PARRY] = false;
    p_ptr->active_ability[S_EVN][EVN_PARRY] = false;
    p_ptr->have_ability[S_EVN][EVN_PARRY] = false;
    learn(S_EVN, EVN_HEAVY_ARMOUR);
    object_type armor = item(armor_kind), old_armor = armor;
    armor.pd = old_armor.pd = 1; armor.ps = old_armor.ps = 3;
    armor.weight = 150; old_armor.weight = 140;
    compare(&armor, &old_armor);
    expect("Armor 1-3 -> 2-4", TERM_L_GREEN);

    /* Active throwing uses melee skill and thrown damage, never bow fields. */
    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    k_info[sword_kind].flags3 = TR3_THROWING;
    selected.evn = 0;
    inventory[INVEN_WIELD] = selected;
    inventory[INVEN_WIELD].pickup_slot = PICKUP_SLOT_ACTIVE_THROWING;
    ring.stat_bonus[A_STR] = 1;
    compare(&ring, NULL);
    assert(strstr(output, "Throwing attack") && !strstr(output, "Archery attack"));
    expect("Damage 1d7 -> 1d8", TERM_L_GREEN);

    /* Bows use their own strength cap and can deactivate a shortbow shield. */
    object_type bow = item(lookup_kind(TV_BOW, SV_LONG_BOW));
    inventory[INVEN_BOW] = item(lookup_kind(TV_BOW, SV_SHORT_BOW));
    inventory[INVEN_WIELD].pickup_slot = -1;
    bow.dd = inventory[INVEN_BOW].dd = 2;
    bow.ds = inventory[INVEN_BOW].ds = 5;
    bow.weight = 40; inventory[INVEN_BOW].weight = 10;
    bow.att = inventory[INVEN_BOW].att = 0;
    bow.abilities = inventory[INVEN_BOW].abilities = 0;
    inventory[INVEN_ARM] = item(shield_kind);
    inventory[INVEN_ARM].pd = 1; inventory[INVEN_ARM].ps = 4;
    inventory[INVEN_ARM].evn = 2;
    learn(S_ARC, ARC_POINT_BLANK);
    compare(&bow, &inventory[INVEN_BOW]);
    assert(strstr(output, "Archery attack"));
    expect("Damage 2d6 -> 2d7", TERM_L_GREEN);
    expect("Evasion -2", TERM_L_RED);
    expect("Armor 1-4 -> 0-0", TERM_L_RED);

    p_ptr->active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    learn(S_EVN, EVN_BLOCKING);
    p_ptr->previous_action[0] = 5;
    object_type shield = inventory[INVEN_ARM];
    shield.ps = 5;
    compare(&shield, &inventory[INVEN_ARM]);
    expect("Armor 2-8 -> 2-10", TERM_L_GREEN);

    learn(S_MEL, MEL_TWO_WEAPON);
    inventory[INVEN_WIELD] = selected;
    inventory[INVEN_ARM] = reference;
    inventory[INVEN_ARM].ds = 5;
    inventory[INVEN_ARM].weight = 30;
    selected.ds = 6; selected.att = 2;
    compare(&selected, &inventory[INVEN_ARM]);
    expect("Damage 1d4 -> 1d5", TERM_L_GREEN);
    assert(strstr(output, "Off-hand attack"));
    compare(&shield, &inventory[INVEN_ARM]);
    assert(strstr(output, "Off-hand attack") && strstr(output, "Off-hand damage"));
    k_info[sword_kind].flags3 |= TR3_TWO_HANDED;
    compare(&selected, &inventory[INVEN_ARM]);
    assert(!output[0]); /* This weapon cannot replace an offhand. */
    compare(&shield, &inventory[INVEN_WIELD]);
    assert(!output[0]); /* Shields cannot be installed in the main hand. */

    k_info[sword_kind] = saved_sword;
    k_info[comparison_sword_kind] = saved_other;
    k_info[ring_kind] = saved_ring;
    k_info[armor_kind] = saved_armor;
    current_character_profile->flags_u = saved_character_flags;
    *p_ptr = saved_player;
    memcpy(inventory, saved_inventory, sizeof(saved_inventory));
    puts("Strength caps/sign, Rapid Attack, handedness/Subtlety/Maedhros, shields, grants, rings, observed unknown stats, Parry, Heavy Armour and throwing PASS");
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
    item_label(name, sizeof(name), &selected);
    expect(name, TERM_VIOLET);
    item_label(name, sizeof(name), &reference);
    expect(name, TERM_VIOLET);
    reference.name1 = 1;
    clear_output(); fixture_description(objects, headings, 2, true);
    item_label(name, sizeof(name), &reference);
    expect(name, TERM_YELLOW);
    reference.ident = IDENT_HANDLED;
    clear_output(); fixture_description(objects, headings, 2, true);
    item_label(name, sizeof(name), &reference);
    expect(name, TERM_SLATE);
    tval_to_attr[TV_SWORD] = old_color;
    artifact_unique_color = old_unique; unidentified_items_slate = old_unknown;
    k_info[selected.k_idx].aware = old_selected_aware;
    k_info[reference.k_idx].aware = old_reference_aware;
    puts("Both names: inventory palette, known artifact gold, unknown slate PASS");
}

static void check_item_headers(void)
{
    object_type weapon = item(sword_kind), other = item(comparison_sword_kind);
    const object_type* objects[] = { &weapon, &other };
    const char* headings[] = { "Selected item", "Melee" };
    char full_header[4096];
    weapon.att = 1; weapon.dd = 2; weapon.ds = 6; weapon.evn = 1;
    weapon.pd = weapon.ps = 0; weapon.weight = 30;
    clear_output(); object_info_item_header_out(&weapon);
    SDL_strlcpy(full_header, output, sizeof(full_header));
    assert(strstr(full_header, "(+1,2d6)") && strstr(full_header, "[+1]"));
    assert(strstr(full_header, "3.0 lb"));
    clear_output(); fixture_description(objects, headings, 2, true);
    assert(strstr(output, full_header));
    assert(!strstr(output, "protection none") && !strstr(output, "Armor"));
    clear_output(); fixture_description(objects, headings, 2, false);
    assert(strstr(output, full_header));

    object_type armor = item(armor_kind), old_armor = armor;
    armor.pd = old_armor.pd = 1;
    armor.ps = 4; old_armor.ps = 3; armor.evn = -1;
    clear_output(); object_info_item_header_out(&armor);
    assert(strstr(output, "[-1,1d4]"));
    compare(&armor, &old_armor);
    expect("Armor 1-3 -> 1-4", TERM_L_GREEN);

    object_type ring = item(ring_kind);
    ring.ident = IDENT_HANDLED;
    ring.att = 2; ring.evn = -1; ring.pd = 1; ring.ps = 4;
    clear_output(); object_info_item_header_out(&ring);
    assert(strstr(output, "(+2)") && strstr(output, "[-1,1d4]"));
    puts("Shared Full/Comparison combat notation and weight; weapon armor omitted, real/unknown armor visible PASS");
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
    assert(strlen(output) < 900);
    k_info[sword_kind] = before;

    compare(&selected, NULL);
    assert(strstr(output, "Damage none ->"));
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
    item_label(selected_label, sizeof(selected_label), &selected);
    item_label(reference_label, sizeof(reference_label), &reference);
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
    check_controls_and_flow(&layout);
    float right = layout.close_button ? layout.close_rect.x - layout.cell_w
        : layout.panel.x + layout.panel.w - (layout.text_x - layout.panel.x);
    assert(SDL_fabsf(button.x + button.w - right) < 1.0f);
    assert(button.w < 14 * layout.cell_w);
    assert(button.h < 2 * layout.cell_h);
    assert(button.y >= 0);
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

static void check_flow_wrapping(void)
{
    char paragraph[4096] = "Selected item with a deliberately long heading ";
    SDL_strlcat(paragraph,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\n",
        sizeof(paragraph));
    for (int i = 0; i < 15; i++)
        SDL_strlcat(paragraph, "A weapon's normal description should use the whole width below the buttons. ", sizeof(paragraph));
    for (int story = 0; story < 2; story++)
    for (int comparison = 0; comparison < 2; comparison++)
    {
        term scratch;
        term* saved_term = Term;
        story_font_term_state previous_story;
        text_out_wrap_region previous_region;
        int cols = sdl_description_overlay_capture_cols(80, true);
        int rows, narrow_cols, narrow_pixels;
        assert(!term_init(&scratch, 200, 255, 16));
        Term_activate(&scratch);
        story_font_term_push_slot(story, false, STORY_FONT_SLOT_SECONDARY, &previous_story);
        assert(sdl_description_overlay_capture_flow(story, true, comparison,
            cols, &rows, &narrow_cols, &narrow_pixels));
        text_out_wrap = cols; text_out_indent = 0;
        text_out_hook = story ? text_out_to_screen_overlay_story : text_out_to_screen;
        text_out_wrap_region_push(rows, narrow_cols, narrow_pixels, &previous_region);
        text_out_c(TERM_WHITE, paragraph);
        text_out_wrap_region_pop(&previous_region);
        assert(text_out_row_columns(0, 123) == 123 && text_out_row_pixels(0, 456) == 456);
        bool wider_below = false;
        char flattened[4096], expected[4096];
        int at = 0, expected_at = 0;
        for (int i = 0; paragraph[i]; i++)
            if (!isspace((unsigned char)paragraph[i])) expected[expected_at++] = paragraph[i];
        expected[expected_at] = '\0';
        for (int y = 0; y <= scratch.scr->cy; y++)
        {
            int used = scratch.wid;
            while (used && (!scratch.scr->c[y][used - 1]
                    || isspace((unsigned char)scratch.scr->c[y][used - 1]))) used--;
            char line[256]; memcpy(line, scratch.scr->c[y], used); line[used] = '\0';
            for (int i = 0; i < used; i++) if (!line[i]) line[i] = ' ';
            int width = story ? sdl_description_overlay_story_text_width(line, used,
                STORY_FONT_SLOT_SECONDARY) : used;
            int budget = story ? narrow_pixels : narrow_cols;
            if (y < rows) assert(width <= budget);
            else if (width > budget + 1) wider_below = true;
            for (int i = 0; i < used; i++)
                if (!isspace((unsigned char)line[i])) flattened[at++] = line[i];
        }
        flattened[at] = '\0';
        if (strcmp(flattened, expected) || !wider_below)
        {
            int first = 0;
            while (flattened[first] && flattened[first] == expected[first]) first++;
            fprintf(stderr, "Flow mismatch story=%d comparison=%d rows=%d cols=%d pixels=%d wider=%d at=%d lengths=%d/%d\nactual: %.150s\nexpected: %.150s\n",
                story,comparison,rows,narrow_cols,narrow_pixels,wider_below,first,
                at,expected_at,flattened+first,expected+first);
        }
        assert(!strcmp(flattened, expected) && wider_below);
        story_font_term_pop(&previous_story);
        Term_activate(saved_term); term_nuke(&scratch);
        text_out_wrap = text_out_indent = 0;
        text_out_hook = text_out_to_screen;
    }
    puts("Full/Comparison mono/story text flows beside controls then uses full width, without losing long words PASS");
}

static void check_scrolling_header(const object_type* selected,
    const object_type* reference)
{
    int saved_width = render_width, saved_height = render_height;
    /* Exercise an actually scrolling viewport even on a tall phone. */
    if (render_height > render_width)
    {
        render_width = MIN(render_width, 540);
        render_height = MIN(render_height, 960);
    }
    object_type long_item = *selected;
    char note[4096] = "";
    for (int i = 0; i < 35; i++)
        SDL_strlcat(note, "A long inscription fills the description with enough text to scroll while its header and controls remain visible. ", sizeof(note));
    long_item.obj_note = quark_add(note);
    const object_type* objects[] = { &long_item, reference };
    const char* headings[] = { "Selected item", "Melee" };
    assert(object_info_overlay_show_multi(objects, headings, 2, true));
    description_overlay_layout layout;
    assert(sdl_description_overlay_layout(&layout) && layout.max_scroll > 0);
    check_controls_and_flow(&layout);
    SDL_Rect header = { (int)layout.text_x, (int)layout.text_y,
        (int)(layout.view_toggle_rect.x - layout.text_x - layout.cell_w),
        layout.fixed_rows * layout.cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 20, 255);
    SDL_RenderClear(g_state.renderer); sdl_description_overlay_render();
    SDL_Surface* before = SDL_RenderReadPixels(g_state.renderer, &header);
    g_description_overlay.scroll = layout.max_scroll;
    SDL_SetRenderDrawColor(g_state.renderer, 12, 15, 20, 255);
    SDL_RenderClear(g_state.renderer); sdl_description_overlay_render();
    SDL_Surface* after = SDL_RenderReadPixels(g_state.renderer, &header);
    assert(before && after && before->format == after->format);
    for (int y = 0; y < before->h; y++)
        assert(!memcmp((byte*)before->pixels + y * before->pitch,
            (byte*)after->pixels + y * after->pitch,
            before->w * SDL_BYTESPERPIXEL(before->format)));
    SDL_DestroySurface(before); SDL_DestroySurface(after);
    object_info_overlay_clear();
    render_width = saved_width; render_height = saved_height;
    puts("Scrolling leaves the wrapped item header beside internal controls unchanged PASS");
}

static void render_description(cptr directory, bool big)
{
    render_directory = directory; render_big = big;
    g_startup_device_class = big ? SDL_STARTUP_DEVICE_MOBILE_TOUCH
        : SDL_STARTUP_DEVICE_DESKTOP;
    k_info[sword_kind].aware = true;
    k_info[comparison_sword_kind].aware = true;
    object_type selected = item(sword_kind), reference = item(comparison_sword_kind);
    const object_type* objects[] = { &selected, &reference };
    const char* headings[] = { "Selected item", "Melee" };
    selected.att = reference.att + 2;
    selected.evn = reference.evn - 1;
    selected.weight = reference.weight + 3;
    selected.dd = 2; selected.ds = 5;
    reference.dd = 1; reference.ds = 8;
    item_label(selected_name, sizeof(selected_name), &selected);
    item_label(reference_name, sizeof(reference_name), &reference);
    assert(strcmp(selected_name, reference_name));
    render_width = big ? 1080 : 1280;
    render_height = big ? 2340 : 720;
    sdl_config_set_defaults(&config);
    config.bigger_font = big;
    config.main_view_scale = 2;
    config.mobile_portrait_mode = big;
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
    assert(!overlay_contains("Armor") && !overlay_contains("protection none"));
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
         y < MIN(render_height, (int)layout.text_y + layout.visible_rows * layout.cell_h); y++)
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
    assert(sdl_description_overlay_layout(&layout));
    assert(!sdl_description_overlay_handle_close_pointer(
        layout.close_rect.x + layout.close_rect.w / 2,
        layout.text_y + 3 * layout.cell_h));
    assert(sdl_description_overlay_handle_close_pointer(
        layout.close_rect.x + layout.close_rect.w / 2,
        layout.close_rect.y + layout.close_rect.h / 2));
    char close_key = 0;
    assert(!Term_inkey(&close_key, false, true) && close_key == ESCAPE);
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
    check_flow_wrapping();
    check_scrolling_header(&selected, &reference);
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
    assert(!init_oath_info());
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
    check_loadout_math();
    check_item_headers();
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
