#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "ui/menu-click.h"

static int g_menu_font_render_context = -2;
static SDL_FRect g_character_font_button_avoid[8];
static int g_character_font_button_avoid_count;

void sdl_menu_font_button_clear_avoid_rects(void)
{
    g_character_font_button_avoid_count = 0;
}

void sdl_menu_font_button_avoid_rect(SDL_FRect rect)
{
    if (g_character_font_button_avoid_count < (int)N_ELEMENTS(g_character_font_button_avoid))
        g_character_font_button_avoid[g_character_font_button_avoid_count++] = rect;
}

int sdl_menu_font_set_render_context(int menu)
{
    int previous = g_menu_font_render_context;
    g_menu_font_render_context = menu;
    return previous;
}

enum sdl_menu_font sdl_character_sheet_menu_font(void)
{
    int context = g_sdl_character_sheet_screen.context;
    if (context == SDL_CHARACTER_SHEET_LIVE
        || context == SDL_CHARACTER_SHEET_DEBUG)
        return SDL_MENU_FONT_CHARACTER;
    if (context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        || context == SDL_CHARACTER_SHEET_NARRATIVE)
        return g_sdl_character_sheet_screen.font_menu;
    return SDL_MENU_FONT_BIRTH;
}

enum sdl_menu_font sdl_menu_font_current(void)
{
    if (g_menu_font_render_context != -2)
        return g_menu_font_render_context;
    /* Match modal input order. The context hint is part of gameplay. */
    if (g_touch_pane_yes_no_prompt_active || g_touch_pane_reset_confirm_active)
        return SDL_MENU_FONT_QUESTIONS;
    if (g_question_menu.active && !g_question_menu.context_hint)
        return g_question_menu.font_menu;
    if (g_main_menu_overlay_active)
        return SDL_MENU_FONT_MAIN;
    if (g_log_pane_menu.active)
        return SDL_MENU_FONT_LOG_OPTIONS;
    if (g_side_pane_menu.active)
        return SDL_MENU_FONT_PANE_OPTIONS;
    if (g_song_menu.active)
        return SDL_MENU_FONT_SONGS;
    if (sdl_halls_screen_active())
        return SDL_MENU_FONT_SCORES;
    if (sdl_hint_quest_menu_active())
        return SDL_MENU_FONT_QUESTS;
    if (sdl_character_sheet_screen_active())
        return sdl_character_sheet_menu_font();
    if (sdl_pause_text_screen_active() || sdl_tale_screen_active()
        || sdl_poetry_screen_active())
        return SDL_MENU_FONT_NARRATIVE;
    if (sdl_welcome_screen_active())
        return SDL_MENU_FONT_MAIN;
    if (g_unified_look_prompt.active || g_unified_look_sidebar.active)
        return SDL_MENU_FONT_LOOK;
    return sdl_terminal_menu_font();
}

bool get_sdl_menu_bigger_font_for(enum sdl_menu_font menu)
{
    return menu >= 0 && menu < SDL_MENU_FONT_COUNT
        && config.menu_bigger_font[menu];
}

bool get_sdl_menu_bigger_font(void)
{
    enum sdl_menu_font menu = sdl_menu_font_current();
    return menu == SDL_MENU_FONT_NONE ? config.bigger_font
        : get_sdl_menu_bigger_font_for(menu);
}

/* Normal menus use the normal UI profile's base sizes even while the larger
 * gameplay layout is active. Its map scale and auxiliary font can differ. */
static const struct sdl_pane_profile* sdl_menu_normal_profile(void)
{
    if (!config.bigger_font)
        return NULL;
    int orientation = SDL_PANE_ORIENTATION_LANDSCAPE;
#if SIL_SDL_MOBILE_BUILD
    if (config.mobile_portrait_mode)
        orientation = SDL_PANE_ORIENTATION_PORTRAIT;
#endif
    int index = SDL_PANE_FONT_PROFILE_INDEX(false, orientation,
        config.min_terminal_mode);
    return g_pane_profiles[index].main_view_scale > 0
        ? &g_pane_profiles[index] : NULL;
}

int sdl_menu_base_aux_font_size(void)
{
    const struct sdl_pane_profile* normal = sdl_menu_normal_profile();
    return normal ? normal->aux_view_font_size : config.aux_view_font_size;
}

int sdl_menu_auto_font_size_from_main(int numerator, int denominator)
{
    const struct sdl_pane_profile* normal = sdl_menu_normal_profile();
    if (!normal)
        return sdl_auto_font_size_from_main(numerator, denominator);
    float density = g_state.system_scale > 0 ? g_state.system_scale : 1.0f;
    int scale = sdl_clamp_main_view_scale_platform_bounds(normal->main_view_scale,
        config.min_terminal_mode);
    int main_size = (int)(scale * TILE_SIZE / density + 0.5f);
    int size = (main_size * numerator + denominator - 1) / denominator;
    if (size >= main_size && main_size > SDL_AUX_VIEW_FONT_MIN)
        size = main_size - 1;
    return MAX(SDL_AUX_VIEW_FONT_MIN, MIN(SDL_AUX_VIEW_FONT_MAX, size));
}

void set_sdl_menu_bigger_font(enum sdl_menu_font menu, bool value)
{
    if (menu < 0 || menu >= SDL_MENU_FONT_COUNT)
        return;
    config.menu_bigger_font[menu] = value;
    if (sdl_terminal_menu_font() == menu)
        sdl_refresh_terminal_menu_scale();
    if (menu == SDL_MENU_FONT_CHARACTER)
        config.debug_character_sheet = false;
    g_state.need_present = true;
}

void set_sdl_all_bigger_fonts(bool value)
{
    set_sdl_bigger_font(value);
    for (int menu = 0; menu < SDL_MENU_FONT_COUNT; menu++)
        set_sdl_menu_bigger_font(menu, value);
}

bool get_sdl_show_menu_font_button(void)
{
    return config.show_menu_font_button;
}

void set_sdl_show_menu_font_button(bool value)
{
    config.show_menu_font_button = value;
    g_state.need_present = true;
}

static bool sdl_menu_native_surface_active(void)
{
    return sdl_character_sheet_screen_active()
        || g_question_menu.active || g_song_menu.active
        || sdl_halls_screen_active() || sdl_hint_quest_menu_active()
        || sdl_welcome_screen_active() || sdl_pause_text_screen_active()
        || sdl_tale_screen_active() || sdl_poetry_screen_active()
        || g_main_menu_overlay_active || g_log_pane_menu.active
        || g_side_pane_menu.active || g_unified_look_prompt.active
        || g_unified_look_sidebar.active || g_touch_pane_yes_no_prompt_active
        || g_touch_pane_reset_confirm_active;
}

bool sdl_menu_scale_controls_active(void)
{
    /* A native screen can be opened inside a terminal-menu scope. Only the
     * visible terminal owner may expose or change that scope's scale. */
    return sdl_terminal_menu_font() != SDL_MENU_FONT_NONE
        && !sdl_menu_native_surface_active() && !g_description_overlay.active
        && !g_minimap.active && !sdl_controller_keyboard_active();
}

static bool sdl_menu_big_control_active(void)
{
    return config.show_menu_font_button
        && sdl_menu_font_current() != SDL_MENU_FONT_NONE;
}

static int sdl_menu_corner_control_count(void)
{
    int count = 0;

    if (sdl_menu_big_control_active())
        count++;
    if (sdl_menu_scale_controls_active())
        count += 2;
    return count;
}

static float sdl_menu_corner_control_size(void)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    float size = MAX(28.0f, 40.0f * sdl_ui_density_scale());

    return MIN(size, MIN(screen.w, screen.h) * 0.18f);
}

static float sdl_menu_corner_control_gap(void)
{
    return MAX(4.0f * sdl_ui_density_scale(),
        sdl_ui_min_tap_px() - sdl_menu_corner_control_size());
}

static SDL_FRect sdl_menu_font_control_blocked_rect(void)
{
    if (g_question_menu.active && !g_question_menu.context_hint)
        return g_question_menu.header_controls;
    if (sdl_welcome_screen_active())
        return g_sdl_welcome_screen.quit_rect;
    return (SDL_FRect){0};
}

static SDL_FRect sdl_menu_control_rail_rect_avoiding(SDL_FRect blocked)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    float pad = MAX(2.0f, 4.0f * sdl_ui_density_scale());
    float size = sdl_menu_corner_control_size();
    float gap = sdl_menu_corner_control_gap();
    int count = sdl_menu_corner_control_count();
    float width;
    SDL_FRect rail;
    SDL_FRect hit;

    if (count <= 0 || size <= 0.0f || screen.w <= 0 || screen.h <= 0)
        return (SDL_FRect){0};

    if (sdl_menu_scale_controls_active()
        && sdl_touch_menu_font_control_rail(count, size, gap, &rail))
        return rail;

    width = count * size + (count - 1) * gap;
    rail = (SDL_FRect){screen.x + screen.w - width - pad,
        screen.y + pad, width, size};
    hit = (SDL_FRect){rail.x - pad, rail.y - pad,
        rail.w + 2 * pad, rail.h + 2 * pad};
    if (blocked.w > 0 && SDL_HasRectIntersectionFloat(&hit, &blocked))
        rail.x = MAX(screen.x + pad, blocked.x - width - 2 * pad);

    if (sdl_menu_font_current() == SDL_MENU_FONT_CHARACTER) {
        /* Keep the floating controls out of title/heading ink when the sheet
         * publishes a preferred gap. The sheet itself never moves. */
        for (int pass = 0; pass < g_character_font_button_avoid_count; pass++) {
            bool moved = false;

            hit = (SDL_FRect){rail.x - pad, rail.y - pad,
                rail.w + 2 * pad, rail.h + 2 * pad};
            for (int i = 0; i < g_character_font_button_avoid_count; i++) {
                SDL_FRect text = g_character_font_button_avoid[i];
                if (!SDL_HasRectIntersectionFloat(&hit, &text))
                    continue;
                float left = text.x - width - 2 * pad;
                if (left >= screen.x + pad) {
                    rail.x = left;
                    moved = true;
                    break;
                }
                /* Preserve the compact native B button in the sheet's
                 * top-right gap when no full-width header gap is available. */
                float top = screen.y + screen.h;
                float right = screen.x + screen.w - pad;
                for (int j = 0; j < g_character_font_button_avoid_count; j++) {
                    SDL_FRect occupied = g_character_font_button_avoid[j];
                    if (occupied.x + occupied.w > right - width)
                        top = MIN(top, occupied.y);
                }
                float compact = MIN(size,
                    MAX(16.0f, top - screen.y - pad - 1.0f));
                return (SDL_FRect){right - compact, screen.y + pad,
                    compact, compact};
            }
            if (!moved)
                break;
        }
    }
    return rail;
}

static SDL_FRect sdl_menu_corner_control_rect(SDL_FRect rail, int index)
{
    float gap = sdl_menu_corner_control_gap();
    float size = rail.h;

    return (SDL_FRect){rail.x + index * (size + gap), rail.y, size, size};
}

static SDL_FRect sdl_menu_control_hit_rect(SDL_FRect button)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    float hit_size = MAX(button.w, (float)sdl_ui_min_tap_px());
    SDL_FRect hit = {button.x + (button.w - hit_size) / 2.0f,
        button.y + (button.h - hit_size) / 2.0f, hit_size, hit_size};

    hit.x = MAX((float)screen.x, MIN(hit.x, screen.x + screen.w - hit.w));
    hit.y = MAX((float)screen.y, MIN(hit.y, screen.y + screen.h - hit.h));
    return hit;
}

static bool sdl_menu_control_contains(SDL_FRect button, float x, float y)
{
    SDL_FRect hit = sdl_menu_control_hit_rect(button);

    return x >= hit.x && x < hit.x + hit.w
        && y >= hit.y && y < hit.y + hit.h;
}

static int sdl_menu_corner_control_at(SDL_FRect rail, float x, float y)
{
    int count = sdl_menu_corner_control_count();

    if (rail.w <= 0 || rail.h <= 0)
        return -1;
    for (int i = 0; i < count; i++) {
        /* Split the enlarged targets at each gap's midpoint so adjacent
         * minus/plus targets never claim the same pixel. */
        float split = rail.x + i * (rail.h + sdl_menu_corner_control_gap())
            - sdl_menu_corner_control_gap() / 2.0f;
        float next = split + rail.h + sdl_menu_corner_control_gap();
        if ((i > 0 && x < split) || (i < count - 1 && x >= next))
            continue;
        if (sdl_menu_control_contains(
                sdl_menu_corner_control_rect(rail, i), x, y))
            return i;
    }
    return -1;
}

static int sdl_menu_scale_control_first_index(void)
{
    return sdl_menu_big_control_active() ? 1 : 0;
}

static SDL_FRect sdl_menu_font_button_rect_avoiding(SDL_FRect blocked)
{
    SDL_FRect rail = sdl_menu_control_rail_rect_avoiding(blocked);

    if (!sdl_menu_big_control_active())
        return (SDL_FRect){0};
    return sdl_menu_corner_control_rect(rail, 0);
}

static SDL_FRect sdl_menu_font_button_rect(void)
{
    return sdl_menu_font_button_rect_avoiding(
        sdl_menu_font_control_blocked_rect());
}

SDL_FRect sdl_menu_font_button_bounds_avoiding(SDL_FRect blocked)
{
    if (sdl_menu_font_current() == SDL_MENU_FONT_NONE
        && !sdl_menu_scale_controls_active())
        return (SDL_FRect){0};
    return sdl_menu_control_rail_rect_avoiding(blocked);
}

SDL_Rect sdl_menu_content_rect(SDL_Rect rect)
{
    /* The corner shortcut is an overlay. Showing it never changes the canvas. */
    return rect;
}

SDL_FRect sdl_menu_font_button_bounds(void)
{
    if (sdl_menu_font_current() == SDL_MENU_FONT_NONE
        && !sdl_menu_scale_controls_active())
        return (SDL_FRect){0};
    if (!sdl_menu_scale_controls_active() && sdl_menu_big_control_active())
        return sdl_menu_font_button_rect();
    return sdl_menu_control_rail_rect_avoiding(
        sdl_menu_font_control_blocked_rect());
}

int sdl_menu_font_button_available_columns(int row, int columns)
{
    SDL_FRect cell;
    SDL_FRect button = sdl_menu_font_button_bounds();
    SDL_Rect screen = sdl_get_layout_screen_rect();
    SDL_FRect visible = {screen.x, screen.y, screen.w, screen.h};

    if (columns < 1)
        return 1;
    if (button.w <= 0 || !SDL_HasRectIntersectionFloat(&button, &visible)
        || !sdl_main_cell_rect(0, row, 1, 1, &cell) || cell.w <= 0)
        return columns;
    SDL_FRect line = {cell.x, cell.y, cell.w * columns, cell.h};
    if (!SDL_HasRectIntersectionFloat(&button, &line))
        return columns;
    return MAX(1, MIN(columns, (int)SDL_floorf((button.x - cell.x) / cell.w)));
}

void sdl_menu_font_button_render(void)
{
    if (sdl_menu_corner_control_count() <= 0 || !g_state.renderer)
        return;
    SDL_FRect rail = sdl_menu_control_rail_rect_avoiding(
        sdl_menu_font_control_blocked_rect());
    SDL_Color ink = {255, 255, 255, 192};
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < sdl_menu_corner_control_count(); i++) {
        SDL_FRect button = sdl_menu_corner_control_rect(rail, i);
        int scale_first = sdl_menu_scale_control_first_index();
        const char* label = i == 0 && sdl_menu_big_control_active() ? "B"
            : i == scale_first ? "-" : "+";
        SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 192);
        SDL_RenderRect(g_state.renderer, &button);
        TTF_Font* font = sdl_story_font_for_height_slot(
            MAX(12, (int)(button.h * 0.62f)), SDL_STORY_FONT_SLOT_MENU);
        if (font) {
            int width = 0, height = 0;
            SDL_Texture* text = sdl_ui_text_texture(font, label, ink,
                &width, &height);
            if (text) {
                SDL_FRect dst = {button.x + (button.w - width) * 0.5f,
                    button.y + (button.h - height) * 0.5f, width, height};
                sdl_ui_render_text_texture(text, NULL, &dst);
            }
        }
    }
}

bool sdl_menu_font_button_handle_event(const SDL_Event* ev)
{
    static bool pressed;
    static bool touch_pressed;
    static SDL_FingerID finger;
    static enum sdl_menu_font pressed_menu = SDL_MENU_FONT_NONE;
    static int pressed_control = -1;
    static bool pressed_is_big;
    float x = 0, y = 0;
    bool down = false, up = false, pointer = false;
    if (ev->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        pressed = false;
        pressed_control = -1;
        return false;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN
        || ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (ev->button.which == SDL_TOUCH_MOUSEID
            || ev->button.button != SDL_BUTTON_LEFT)
            return false;
        if (pressed && touch_pressed)
            return false;
        x = ev->button.x;
        y = ev->button.y;
        down = ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        up = !down;
        pointer = true;
    } else if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return false;
        x = ev->motion.x;
        y = ev->motion.y;
        pointer = true;
    } else if (ev->type == SDL_EVENT_FINGER_DOWN
        || ev->type == SDL_EVENT_FINGER_UP
        || ev->type == SDL_EVENT_FINGER_MOTION
        || ev->type == SDL_EVENT_FINGER_CANCELED) {
        if (pressed && (!touch_pressed || ev->tfinger.fingerID != finger))
            return false;
        if (pressed && ev->type == SDL_EVENT_FINGER_CANCELED) {
            pressed = false;
            pressed_control = -1;
            return true;
        }
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return false;
        down = ev->type == SDL_EVENT_FINGER_DOWN;
        up = ev->type == SDL_EVENT_FINGER_UP
            || ev->type == SDL_EVENT_FINGER_CANCELED;
        pointer = true;
    }
    if (!pointer)
        return false;
    SDL_FRect rail = sdl_menu_control_rail_rect_avoiding(
        sdl_menu_font_control_blocked_rect());
    int control = sdl_menu_corner_control_at(rail, x, y);
    int scale_first = sdl_menu_scale_control_first_index();
    int released_control = pressed_control;
    bool big_control = sdl_menu_big_control_active() && control == 0;
    enum sdl_menu_font menu = big_control ? sdl_menu_font_current()
        : sdl_terminal_menu_font();
    bool inside = control >= 0;
    if (down && inside) {
        pressed = true;
        pressed_control = control;
        pressed_is_big = big_control;
        pressed_menu = menu;
        touch_pressed = ev->type == SDL_EVENT_FINGER_DOWN;
        if (touch_pressed) {
            finger = ev->tfinger.fingerID;
            sdl_note_touch_event_device(ev->tfinger.touchID);
        }
        return true;
    }
    if (pressed && (up || !down)) {
        if (!up)
            return true;
        pressed = false;
        pressed_control = -1;
        if (inside && control == released_control && menu == pressed_menu
            && big_control == pressed_is_big
            && ev->type != SDL_EVENT_FINGER_CANCELED) {
            if (pressed_is_big) {
                set_sdl_menu_bigger_font(menu,
                    !get_sdl_menu_bigger_font_for(menu));
                /* Do not redraw gameplay over the open menu. Wake its own
                 * loop so it rebuilds wrapping, pagination and hitboxes. */
                bool native = sdl_menu_native_surface_active();
                if (!native)
                    sdl_apply_config_no_redraw();
                save_pane_config_to_json();
                if (menu == SDL_MENU_FONT_SETTINGS
                    && sdl_character_sheet_screen_active()
                    && g_sdl_character_sheet_screen.select_menu_style
                    && !g_question_menu.active && !g_touch_pane_yes_no_prompt_active
                    && !g_touch_pane_reset_confirm_active) {
                    /* Reuse the menu's harmless hover redraw path. */
                    bool wake = false;
                    ui_menu_click_clear_hover(NULL);
                    ui_menu_click_handle_choice_action(
                        g_sdl_character_sheet_screen.focus_choice,
                        UI_MENU_CLICK_HOVER, &wake);
                    if (wake)
                        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
                } else if (menu == SDL_MENU_FONT_CHARACTER || !native) {
                    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
                }
            } else if (released_control == scale_first
                || released_control == scale_first + 1) {
                int old_value = get_sdl_terminal_menu_scale_offset_for(menu);
                int value = old_value
                    + (released_control == scale_first + 1 ? 1 : -1);
                value = MAX(SDL_TERMINAL_MENU_SCALE_OFFSET_MIN,
                    MIN(SDL_TERMINAL_MENU_SCALE_OFFSET_MAX, value));
                if (value != old_value) {
                    set_sdl_terminal_menu_scale_offset_for(menu, value);
                    sdl_apply_config_no_redraw();
                    save_pane_config_to_json();
                    /* The terminal menu loop rebuilds its wrapping and
                     * hitboxes. ui_menu_click state must not select a row. */
                    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
                }
            }
        }
        return true;
    }
    return inside;
}
