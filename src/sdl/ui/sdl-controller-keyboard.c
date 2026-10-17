#include "angband.h"
#include "sdl/main-sdl-private.h"

/* This keyboard also works outside Steam.  It feeds the existing UTF-8 text
 * editor, so names, inscriptions and searches keep the same edit semantics. */
enum { KEYBOARD_COLUMNS = 10, KEYBOARD_LETTER_ROWS = 4,
    KEYBOARD_ACTION_FIRST = 40, KEYBOARD_KEYS = 45 };

typedef struct controller_keyboard_state {
    bool session;
    bool visible;
    bool upper;
    bool symbols;
    int selected;
    int first_row;
    int axis_direction[4];
} controller_keyboard_state;
static controller_keyboard_state keyboard;

typedef struct controller_keyboard_layout {
    SDL_Rect panel;
    SDL_FRect keys[KEYBOARD_KEYS];
    SDL_FRect hints;
    int font_px;
    int first_row;
    int rows;
    int action_rows;
} controller_keyboard_layout;

static cptr keyboard_characters[2][KEYBOARD_LETTER_ROWS] = {
    { "1234567890", "qwertyuiop", "asdfghjkl;", "zxcvbnm,./" },
    { "!@#$%^&*()", "[]{}<>\\|~`", "-_=+;:'\"? ", ".,/0123456" }
};

static cptr keyboard_hints(void)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    if (screen.h < 520 || screen.w < 600)
        return "<A> Type   <B> Cancel   <X> Delete";
    return "<A> Type   <B> Cancel   <X> Delete   <Y> Case\n"
        "<Menu> Done   <LB>/<RB> Letters / symbols";
}

bool sdl_controller_keyboard_active(void)
{
    return keyboard.session && config.gamepad_enabled
        && (keyboard.visible || steamdeck_controls_active());
}

void sdl_controller_keyboard_begin(void)
{
    keyboard = (controller_keyboard_state){ .session = true, .selected = 20 };
    keyboard.visible = config.gamepad_enabled && steamdeck_controls_active();
    if (keyboard.visible) {
        sdl_gamepad_reset_movement_controls();
        sdl_gamepad_reset_modifiers();
        sdl_gamepad_context_focus_clear();
    }
    g_state.need_present = true;
}

void sdl_controller_keyboard_end(void)
{
    if (keyboard.visible) {
        sdl_gamepad_reset_movement_controls();
        sdl_gamepad_reset_modifiers();
    }
    memset(&keyboard, 0, sizeof(keyboard));
    g_state.need_present = true;
}

static bool keyboard_layout(controller_keyboard_layout* layout)
{
    SDL_Rect screen = sdl_get_layout_screen_rect();
    if (!sdl_controller_keyboard_active() || screen.w <= 0 || screen.h <= 0)
        return false;
    memset(layout, 0, sizeof(*layout));
    layout->font_px = (int)SDL_ceilf(sdl_ui_density_scale()
        * sdl_menu_font_px(24));
    TTF_Font* font = sdl_story_font_for_height_slot(layout->font_px,
        SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return false;
    float pad = MAX(6.0f, layout->font_px * 0.3f);
    float gap = MAX(3.0f, layout->font_px * 0.15f);
    float row_h = MAX(36.0f, TTF_GetFontHeight(font) * 1.4f);
    int hint_w = 0, hint_h = 0;
    TTF_GetStringSizeWrapped(font, keyboard_hints(), 0,
        MAX(1, screen.w - (int)(2 * pad)), &hint_w, &hint_h);
    cptr actions[] = {"ABC", "#+=", "Space", "Clear", "Done"};
    layout->action_rows = 1;
    for (int key = 0; key < 5; ++key) {
        int width = 0;
        TTF_GetStringSize(font, actions[key], 0, &width, NULL);
        if (width + pad * 2 > (screen.w - pad * 2) / 5 - gap)
            layout->action_rows = 2;
    }
    /* Short landscape windows page through the letter rows without reducing
     * the chosen text size.  The action row stays reachable and visible. */
    int available_rows = (int)((screen.h * 0.60f - hint_h - 3 * pad)
        / (row_h + gap)) - layout->action_rows;
    layout->rows = MAX(1, MIN(KEYBOARD_LETTER_ROWS, available_rows));
    if (keyboard.selected < KEYBOARD_ACTION_FIRST) {
        int row = keyboard.selected / KEYBOARD_COLUMNS;
        keyboard.first_row = MIN(keyboard.first_row, row);
        keyboard.first_row = MAX(keyboard.first_row, row - layout->rows + 1);
    }
    keyboard.first_row = MAX(0, MIN(keyboard.first_row,
        KEYBOARD_LETTER_ROWS - layout->rows));
    layout->first_row = keyboard.first_row;
    int height = (int)((layout->rows + layout->action_rows) * (row_h + gap)
        + hint_h + 3 * pad);
    layout->panel = (SDL_Rect){ screen.x, screen.y + screen.h - height,
        screen.w, height };
    float cell_w = (screen.w - 2 * pad) / KEYBOARD_COLUMNS;
    for (int row = 0; row < layout->rows; ++row) {
        for (int col = 0; col < KEYBOARD_COLUMNS; ++col) {
            int key = (row + layout->first_row) * KEYBOARD_COLUMNS + col;
            layout->keys[key] = (SDL_FRect){ screen.x + pad + col * cell_w,
                layout->panel.y + pad + row * (row_h + gap),
                cell_w - gap, row_h };
        }
    }
    for (int col = 0; col < 5; ++col) {
        int action_row = layout->action_rows == 2 && col >= 3 ? 1 : 0;
        int columns = layout->action_rows == 1 ? 5 : action_row == 0 ? 3 : 2;
        int column = action_row ? col - 3 : col;
        float action_w = (screen.w - 2 * pad) / columns;
        layout->keys[KEYBOARD_ACTION_FIRST + col] = (SDL_FRect){
            screen.x + pad + column * action_w,
            layout->panel.y + pad + (layout->rows + action_row) * (row_h + gap),
            action_w - gap, row_h };
    }
    layout->hints = (SDL_FRect){ screen.x + pad,
        layout->panel.y + pad + (layout->rows + layout->action_rows)
            * (row_h + gap) + pad,
        screen.w - 2 * pad, hint_h };
    return true;
}

SDL_Rect sdl_controller_keyboard_rect(void)
{
    controller_keyboard_layout layout;
    if (keyboard_layout(&layout))
        return layout.panel;
    return (SDL_Rect){0};
}

static void keyboard_move(int dx, int dy)
{
    controller_keyboard_layout layout;
    int action_rows = keyboard_layout(&layout) ? layout.action_rows : 1;
    int row = keyboard.selected / KEYBOARD_COLUMNS;
    int col = keyboard.selected % KEYBOARD_COLUMNS;
    if (action_rows == 2 && keyboard.selected >= 43) {
        row = 5;
        col = keyboard.selected - 43;
    }
    int columns = row < 4 ? 10 : action_rows == 1 ? 5 : row == 4 ? 3 : 2;
    if (dx)
        col = (col + dx + columns) % columns;
    if (dy) {
        int count = 4 + action_rows;
        int next_row = (row + dy + count) % count;
        int next_columns = next_row < 4 ? 10
            : action_rows == 1 ? 5 : next_row == 4 ? 3 : 2;
        col = (col * 2 + 1) * next_columns / (columns * 2);
        row = next_row;
    }
    keyboard.selected = row < 4 ? row * 10 + col
        : row == 4 ? KEYBOARD_ACTION_FIRST + col : 43 + col;
    g_state.need_present = true;
}

static void keyboard_activate(void)
{
    switch (keyboard.selected) {
    case 40: keyboard.upper = !keyboard.upper; break;
    case 41: keyboard.symbols = !keyboard.symbols; break;
    case 42: Term_keypress(' '); break;
    case 43: Term_keypress(0x15); break;
    case 44: Term_keypress('\r'); break;
    default: {
        int row = keyboard.selected / KEYBOARD_COLUMNS;
        int col = keyboard.selected % KEYBOARD_COLUMNS;
        unsigned char ch = keyboard_characters[keyboard.symbols][row][col];
        if (keyboard.upper) ch = (unsigned char)toupper(ch);
        Term_keypress(ch);
        break;
    }
    }
    g_state.need_present = true;
}

bool sdl_controller_keyboard_handle_button(const SDL_GamepadButtonEvent* ev)
{
    if (!keyboard.session || !config.gamepad_enabled || !ev)
        return false;
    keyboard.visible = true;
    g_state.need_present = true;
    if (!ev->down)
        return true;
    switch (ev->button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: keyboard_move(0, -1); break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: keyboard_move(0, 1); break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: keyboard_move(-1, 0); break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: keyboard_move(1, 0); break;
    case SDL_GAMEPAD_BUTTON_SOUTH: keyboard_activate(); break;
    case SDL_GAMEPAD_BUTTON_EAST: Term_keypress(ESCAPE); break;
    case SDL_GAMEPAD_BUTTON_WEST: Term_keypress('\b'); break;
    case SDL_GAMEPAD_BUTTON_NORTH: keyboard.upper = !keyboard.upper; break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        keyboard.symbols = !keyboard.symbols; break;
    case SDL_GAMEPAD_BUTTON_START: Term_keypress('\r'); break;
    case SDL_GAMEPAD_BUTTON_BACK: Term_keypress('\t'); break;
    default: break;
    }
    return true;
}

bool sdl_controller_keyboard_handle_axis(const SDL_GamepadAxisEvent* ev)
{
    if (!keyboard.session || !config.gamepad_enabled || !ev)
        return false;
    if (ev->axis > SDL_GAMEPAD_AXIS_RIGHTY)
        return true;
    int deadzone = MAX(8000, config.gamepad_deadzone);
    int direction = ev->value < -deadzone ? -1 : ev->value > deadzone ? 1 : 0;
    if (direction && direction != keyboard.axis_direction[ev->axis]) {
        keyboard.visible = true;
        keyboard_move(ev->axis % 2 ? 0 : direction,
            ev->axis % 2 ? direction : 0);
    }
    keyboard.axis_direction[ev->axis] = direction;
    return true;
}

bool sdl_controller_keyboard_handle_pointer(const SDL_Event* ev)
{
    controller_keyboard_layout layout;
    float x, y;
    bool activate = false;
    if (!sdl_controller_keyboard_active() || !keyboard_layout(&layout))
        return false;
    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION: x = ev->motion.x; y = ev->motion.y; break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        x = ev->button.x; y = ev->button.y;
        activate = ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN
            && ev->button.button == SDL_BUTTON_LEFT;
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return false;
        activate = ev->type == SDL_EVENT_FINGER_DOWN;
        break;
    default: return false;
    }
    SDL_FPoint point = {x, y};
    SDL_FRect panel = {layout.panel.x, layout.panel.y,
        layout.panel.w, layout.panel.h};
    if (!SDL_PointInRectFloat(&point, &panel))
        return false;
    for (int key = 0; key < KEYBOARD_KEYS; ++key) {
        if (layout.keys[key].w > 0
            && SDL_PointInRectFloat(&point, &layout.keys[key])) {
            keyboard.selected = key;
            if (activate) keyboard_activate();
            g_state.need_present = true;
            break;
        }
    }
    return true;
}

static void keyboard_draw_text(TTF_Font* font, cptr text,
    SDL_FRect rect, bool wrapped)
{
    int w = 0, h = 0;
    SDL_Color color = {240, 240, 240, 255};
    SDL_Texture* texture = wrapped
        ? sdl_ui_wrapped_text_texture(font, text, (int)rect.w, color, &w, &h)
        : sdl_ui_text_texture(font, text, color, &w, &h);
    if (!texture)
        return;
    SDL_FRect dst = {rect.x + MAX(0, (rect.w - w) * 0.5f),
        rect.y + MAX(0, (rect.h - h) * 0.5f), w, h};
    SDL_Rect clip = {(int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h};
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    sdl_ui_render_text_texture(texture, NULL, &dst);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

void sdl_controller_keyboard_render(void)
{
    controller_keyboard_layout layout;
    if (!g_state.renderer || !keyboard_layout(&layout))
        return;
    TTF_Font* font = sdl_story_font_for_height_slot(layout.font_px,
        SDL_STORY_FONT_SLOT_MENU);
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_FRect panel = {layout.panel.x, layout.panel.y,
        layout.panel.w, layout.panel.h};
    SDL_SetRenderDrawColor(g_state.renderer, 18, 22, 28, 255);
    SDL_RenderFillRect(g_state.renderer, &panel);
    SDL_SetRenderDrawColor(g_state.renderer, 125, 145, 165, 255);
    SDL_RenderRect(g_state.renderer, &panel);
    for (int key = 0; key < KEYBOARD_KEYS; ++key) {
        SDL_FRect rect = layout.keys[key];
        if (rect.w <= 0) continue;
        bool selected = key == keyboard.selected;
        SDL_SetRenderDrawColor(g_state.renderer, selected ? 50 : 35,
            selected ? 100 : 42, selected ? 145 : 52, 255);
        SDL_RenderFillRect(g_state.renderer, &rect);
        SDL_SetRenderDrawColor(g_state.renderer, selected ? 240 : 90,
            selected ? 205 : 105, selected ? 100 : 120, 255);
        SDL_RenderRect(g_state.renderer, &rect);
        char letter[2] = {0};
        cptr label;
        if (key < KEYBOARD_ACTION_FIRST) {
            letter[0] = keyboard_characters[keyboard.symbols][key / 10][key % 10];
            if (keyboard.upper) letter[0] = (char)toupper((unsigned char)letter[0]);
            label = letter[0] == ' ' ? "_" : letter;
        } else {
            cptr actions[] = {keyboard.upper ? "abc" : "ABC",
                keyboard.symbols ? "abc" : "#+=", "Space", "Clear", "Done"};
            label = actions[key - KEYBOARD_ACTION_FIRST];
        }
        keyboard_draw_text(font, label, rect, false);
    }
    if (layout.rows < KEYBOARD_LETTER_ROWS) {
        SDL_FRect viewport = {panel.x, panel.y, panel.w,
            layout.keys[KEYBOARD_ACTION_FIRST].y - panel.y};
        sdl_ui_render_scroll_indicator(viewport, layout.first_row,
            KEYBOARD_LETTER_ROWS - layout.rows, layout.rows);
    }
    keyboard_draw_text(font, keyboard_hints(), layout.hints, true);
}
