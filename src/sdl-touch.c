#include "angband.h"
#include "sdl-main-internal.h"

typedef struct touch_pane_slot_info {
    const char* slot_name;
    const char* default_label;
    int default_binding;
} touch_pane_slot_info;

typedef struct touch_pane_press_state {
    bool active;
    bool mouse;
    SDL_FingerID finger_id;
    int panel;
    int slot;
    Uint64 start_time;
} touch_pane_press_state;

static const touch_pane_slot_info g_touch_pane_slots[SDL_TOUCH_PANE_BUTTON_COUNT] = {
    { "Esc", "Esc", ESCAPE },
    { "Ctrl", "Ctrl", GAMEPAD_BIND_CTRL },
    { "Shift", "Shift", GAMEPAD_BIND_SHIFT },
    { "Worn", "Worn", 'e' },
    { "Inv", "Inv", 'i' },
    { "Supply", "Supply", 'j' },
    { "Use", "Use", 'u' },
    { "Sing", "Sing", 's' },
    { "Shoot", "Shoot", 'f' },
    { "Northwest", NULL, '7' },
    { "North", NULL, '8' },
    { "Northeast", NULL, '9' },
    { "West", NULL, '4' },
    { "Center", "Space", INPUT_BIND_CONFIRM },
    { "East", NULL, '6' },
    { "Southwest", NULL, '1' },
    { "South", NULL, '2' },
    { "Southeast", NULL, '3' },
    { "Staff", "View", 'l' },
    { "Desc", "Desc", 'x' },
    { "Drop", "Staff", 'a' },
    { "Map", "Map", 'M' },
    { "Hero", "Hero", 'h' },
    { "Ability", "Ability", '\t' },
};

static int g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_BUTTON_COUNT];
static char g_default_touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
static bool g_default_touch_pane_bindings_ready = false;
static int g_touch_pane_flash_slot = -1;
static Uint64 g_touch_pane_flash_until = 0;
static int g_touch_pane_pressed_slot = -1;
static bool g_touch_pane_second_panel = false;
static bool g_touch_pane_ctrl_toggle = false;
static bool g_touch_pane_reset_confirm_active = false;
static touch_pane_press_state g_touch_pane_press;

static bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot);

static int sdl_touch_pane_active_panel(void)
{
    return g_touch_pane_second_panel ? SDL_TOUCH_PANE_PANEL_SECOND : SDL_TOUCH_PANE_PANEL_MAIN;
}

static int sdl_touch_pane_other_panel(int panel)
{
    return (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? SDL_TOUCH_PANE_PANEL_MAIN
        : SDL_TOUCH_PANE_PANEL_SECOND;
}

bool sdl_touch_pane_panel_is_valid(int panel)
{
    return (panel >= 0 && panel < SDL_TOUCH_PANE_PANEL_COUNT);
}

int sdl_touch_pane_raw_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        return config.touch_pane_second_bindings[index];

    return config.touch_pane_bindings[index];
}

static int sdl_touch_pane_effective_binding_for_panel(int panel, int index)
{
    int binding = sdl_touch_pane_raw_binding_for_panel(panel, index);

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && binding == TOUCH_PANE_BIND_INHERIT)
        return config.touch_pane_bindings[index];

    return binding;
}

static bool sdl_touch_pane_confirm_binding(int binding);

static bool sdl_touch_pane_binding_is_direction(int binding)
{
    switch (binding) {
    case '1':
    case '2':
    case '3':
    case '4':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;
    default:
        return false;
    }
}

static bool sdl_touch_pane_slot_uses_long_press(int slot, int binding)
{
    return (slot == 0)
        || sdl_touch_pane_binding_is_direction(binding)
        || (binding == 'z')
        || sdl_touch_pane_confirm_binding(binding);
}

static bool sdl_touch_pane_confirm_binding(int binding)
{
    return (binding == INPUT_BIND_CONFIRM || binding == ' ' || binding == '\r');
}

static void sdl_touch_pane_begin_reset_confirm(void)
{
    g_touch_pane_reset_confirm_active = true;
    g_state.need_present = true;
}

static void sdl_touch_pane_finish_reset_confirm(bool confirmed)
{
    g_touch_pane_reset_confirm_active = false;

    if (confirmed) {
        sdl_touch_pane_reset_bindings_to_default();
        msg_print("Touch controls reset to defaults.");
    }

    g_state.need_present = true;
}

static void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return;
    if (slot < 0)
        return;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    if (slot == 0 || binding == ESCAPE) {
        sdl_touch_pane_finish_reset_confirm(false);
    } else if (sdl_touch_pane_confirm_binding(binding)) {
        sdl_touch_pane_finish_reset_confirm(true);
    }
}

static bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects)
{
    float gap;
    float usable_w;
    float usable_h;
    float button_from_w;
    float button_from_h;
    float button_size;
    float grid_w;
    float grid_h;
    float start_x;
    float start_y;

    if (!pane_rect || !slot_rects || pane_rect->w <= 0 || pane_rect->h <= 0)
        return false;

    gap = (float)((pane_rect->w < pane_rect->h) ? pane_rect->w : pane_rect->h) / 40.0f;
    if (gap < 4.0f)
        gap = 4.0f;
    if (gap > 12.0f)
        gap = 12.0f;

    usable_w = (float)pane_rect->w - gap * 2.0f;
    usable_h = (float)pane_rect->h - gap * 2.0f;
    button_from_w = (usable_w - gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1)) / SDL_TOUCH_PANE_BUTTON_COLS;
    button_from_h = (usable_h - gap * (SDL_TOUCH_PANE_BUTTON_ROWS - 1)) / SDL_TOUCH_PANE_BUTTON_ROWS;
    button_size = (button_from_w < button_from_h) ? button_from_w : button_from_h;
    if (button_size < 12.0f)
        return false;

    grid_w = button_size * SDL_TOUCH_PANE_BUTTON_COLS + gap * (SDL_TOUCH_PANE_BUTTON_COLS - 1);
    grid_h = button_size * SDL_TOUCH_PANE_BUTTON_ROWS + gap * (SDL_TOUCH_PANE_BUTTON_ROWS - 1);
    start_x = (float)pane_rect->x + ((float)pane_rect->w - grid_w) * 0.5f;
    start_y = (float)pane_rect->y + ((float)pane_rect->h - grid_h) * 0.5f;

    for (int row = 0; row < SDL_TOUCH_PANE_BUTTON_ROWS; row++) {
        for (int col = 0; col < SDL_TOUCH_PANE_BUTTON_COLS; col++) {
            int idx = row * SDL_TOUCH_PANE_BUTTON_COLS + col;
            slot_rects[idx] = (SDL_FRect){
                .x = start_x + col * (button_size + gap),
                .y = start_y + row * (button_size + gap),
                .w = button_size,
                .h = button_size,
            };
        }
    }

    return true;
}

static bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    const SDL_Rect* pane = &g_pane_rects[PANE_TOUCH];

    if (out_slot)
        *out_slot = -1;

    if (pane->w <= 0 || pane->h <= 0)
        return false;
    if (!sdl_touch_pane_compute_layout(pane, slot_rects))
        return false;

    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
        const SDL_FRect* rect = &slot_rects[i];
        if (x >= rect->x && x < rect->x + rect->w && y >= rect->y && y < rect->y + rect->h) {
            if (out_slot)
                *out_slot = i;
            return true;
        }
    }

    if (x >= pane->x && x < pane->x + pane->w && y >= pane->y && y < pane->y + pane->h)
        return true;

    return false;
}

static void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color)
{
    float dx = 0.0f;
    float dy = 0.0f;
    float px = 0.0f;
    float py = 0.0f;
    float cx;
    float cy;
    float body_len;
    float head_len;
    float tail_x;
    float tail_y;
    float tip_x;
    float tip_y;

    if (!rect)
        return;

    switch (binding) {
    case '7': dx = -0.70710677f; dy = -0.70710677f; break;
    case '8': dx = 0.0f; dy = -1.0f; break;
    case '9': dx = 0.70710677f; dy = -0.70710677f; break;
    case '4': dx = -1.0f; dy = 0.0f; break;
    case '6': dx = 1.0f; dy = 0.0f; break;
    case '1': dx = -0.70710677f; dy = 0.70710677f; break;
    case '2': dx = 0.0f; dy = 1.0f; break;
    case '3': dx = 0.70710677f; dy = 0.70710677f; break;
    default:
        return;
    }

    px = -dy;
    py = dx;
    cx = rect->x + rect->w * 0.5f;
    cy = rect->y + rect->h * 0.5f;
    body_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.28f;
    head_len = ((rect->w < rect->h) ? rect->w : rect->h) * 0.16f;
    tail_x = cx - dx * body_len;
    tail_y = cy - dy * body_len;
    tip_x = cx + dx * body_len;
    tip_y = cy + dy * body_len;

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(g_state.renderer, tail_x, tail_y, tip_x, tip_y);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len + px * head_len * 0.55f,
        tip_y - dy * head_len + py * head_len * 0.55f);
    SDL_RenderLine(g_state.renderer, tip_x, tip_y,
        tip_x - dx * head_len - px * head_len * 0.55f,
        tip_y - dy * head_len - py * head_len * 0.55f);
}

static void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color)
{
    SDL_Surface* name_surface = NULL;
    SDL_Surface* symbol_surface = NULL;
    SDL_Texture* name_texture = NULL;
    SDL_Texture* symbol_texture = NULL;
    TTF_Font* name_font = NULL;
    TTF_Font* symbol_font = NULL;
    bool have_name;
    bool have_symbol;
    float name_max_w;
    float name_max_h;
    float symbol_max_w;
    float symbol_max_h;
    float gap;
    SDL_FRect name_dst;
    SDL_FRect symbol_dst;
    int name_font_px;
    int symbol_font_px;

    if (!rect)
        return;

    have_name = (name && name[0]);
    have_symbol = (symbol && symbol[0]);
    if (!have_name && !have_symbol)
        return;

    name_font_px = (int)(rect->h * 0.18f);
    if (name_font_px < 10)
        name_font_px = 10;

    symbol_font_px = (int)(rect->h * (have_name ? 0.22f : 0.28f));
    if (symbol_font_px < 12)
        symbol_font_px = 12;

    if (have_name) {
        name_font = sdl_story_font_for_height(name_font_px);
        if (name_font)
            name_surface = TTF_RenderText_Blended(name_font, name, 0, color);
    }

    if (have_symbol) {
        symbol_font = sdl_story_font_for_height(symbol_font_px);
        if (symbol_font)
            symbol_surface = TTF_RenderText_Blended(symbol_font, symbol, 0, color);
    }

    if (!name_surface && !symbol_surface)
        return;

    if (name_surface)
        name_texture = SDL_CreateTextureFromSurface(g_state.renderer, name_surface);
    if (symbol_surface)
        symbol_texture = SDL_CreateTextureFromSurface(g_state.renderer, symbol_surface);

    if (name_surface && !name_texture) {
        SDL_DestroySurface(name_surface);
        name_surface = NULL;
    }
    if (symbol_surface && !symbol_texture) {
        SDL_DestroySurface(symbol_surface);
        symbol_surface = NULL;
    }

    if (!name_surface && !symbol_surface)
        return;

    gap = rect->h * 0.03f;
    if (gap < 2.0f)
        gap = 2.0f;

    if (name_surface && symbol_surface) {
        float total_h;
        float avail_h;
        float name_scale_w;
        float name_scale_h;
        float name_scale;
        float symbol_scale_w;
        float symbol_scale_h;
        float symbol_scale;
        float scale;
        float name_h;
        float symbol_h;
        float start_y;

        name_max_w = rect->w * 0.82f;
        symbol_max_w = rect->w * 0.82f;
        avail_h = rect->h * 0.68f;
        name_max_h = rect->h * 0.22f;
        symbol_max_h = rect->h * 0.30f;

        name_scale_w = (name_surface->w > 0) ? (name_max_w / (float)name_surface->w) : 1.0f;
        name_scale_h = (name_surface->h > 0) ? (name_max_h / (float)name_surface->h) : 1.0f;
        name_scale = (name_scale_w < name_scale_h) ? name_scale_w : name_scale_h;
        if (name_scale > 1.0f)
            name_scale = 1.0f;

        symbol_scale_w = (symbol_surface->w > 0) ? (symbol_max_w / (float)symbol_surface->w) : 1.0f;
        symbol_scale_h = (symbol_surface->h > 0) ? (symbol_max_h / (float)symbol_surface->h) : 1.0f;
        symbol_scale = (symbol_scale_w < symbol_scale_h) ? symbol_scale_w : symbol_scale_h;
        if (symbol_scale > 1.0f)
            symbol_scale = 1.0f;

        total_h = (float)name_surface->h * name_scale + gap + (float)symbol_surface->h * symbol_scale;
        scale = 1.0f;
        if (total_h > avail_h && total_h > 0.0f)
            scale = avail_h / total_h;

        name_scale *= scale;
        symbol_scale *= scale;
        name_h = (float)name_surface->h * name_scale;
        symbol_h = (float)symbol_surface->h * symbol_scale;
        start_y = rect->y + (rect->h - (name_h + gap + symbol_h)) * 0.5f;

        name_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)name_surface->w * name_scale) * 0.5f,
            .y = start_y,
            .w = (float)name_surface->w * name_scale,
            .h = name_h,
        };
        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)symbol_surface->w * symbol_scale) * 0.5f,
            .y = start_y + name_h + gap,
            .w = (float)symbol_surface->w * symbol_scale,
            .h = symbol_h,
        };

        SDL_SetTextureBlendMode(name_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(symbol_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, name_texture, NULL, &name_dst);
        SDL_RenderTexture(g_state.renderer, symbol_texture, NULL, &symbol_dst);
    } else {
        SDL_Surface* only_surface = name_surface ? name_surface : symbol_surface;
        SDL_Texture* only_texture = name_surface ? name_texture : symbol_texture;
        float max_w = rect->w * 0.82f;
        float max_h = rect->h * 0.38f;
        float scale_w = (only_surface->w > 0) ? (max_w / (float)only_surface->w) : 1.0f;
        float scale_h = (only_surface->h > 0) ? (max_h / (float)only_surface->h) : 1.0f;
        float scale = (scale_w < scale_h) ? scale_w : scale_h;

        if (scale > 1.0f)
            scale = 1.0f;

        symbol_dst = (SDL_FRect){
            .x = rect->x + (rect->w - (float)only_surface->w * scale) * 0.5f,
            .y = rect->y + (rect->h - (float)only_surface->h * scale) * 0.5f,
            .w = (float)only_surface->w * scale,
            .h = (float)only_surface->h * scale,
        };

        SDL_SetTextureBlendMode(only_texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, only_texture, NULL, &symbol_dst);
    }

    if (name_texture)
        SDL_DestroyTexture(name_texture);
    if (symbol_texture)
        SDL_DestroyTexture(symbol_texture);
    if (name_surface)
        SDL_DestroySurface(name_surface);
    if (symbol_surface)
        SDL_DestroySurface(symbol_surface);
}

static void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case INPUT_BIND_CONFIRM:
    case ' ':
        SDL_strlcpy(buf, "Space", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Tab", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Esc", buflen);
        return;
    default:
        break;
    }

    if (binding >= 32 && binding <= 126) {
        buf[0] = (char)binding;
        buf[1] = '\0';
    }
}

static bool sdl_touch_pane_label_is_symbol_only(const char* label)
{
    return (label && label[0] == '\x01' && label[1] == '\0');
}

static bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol)
{
    if (!name || !name[0] || !symbol || !symbol[0])
        return false;

    return (SDL_strcasecmp(name, symbol) == 0);
}

void sdl_touch_pane_render_reset_prompt(void)
{
    int window_w = 0;
    int window_h = 0;
    SDL_FRect rect;
    SDL_Color frame = g_state.palette[TERM_L_BLUE];
    SDL_Color text = g_state.palette[TERM_WHITE];

    if (!g_touch_pane_reset_confirm_active)
        return;

    SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0)
        return;

    rect = (SDL_FRect){
        .x = window_w * 0.10f,
        .y = window_h * 0.04f,
        .w = window_w * 0.80f,
        .h = (window_h < 600) ? 54.0f : 68.0f,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 10, 10, 10, 235);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 220);
    SDL_RenderRect(g_state.renderer, &rect);
    sdl_touch_pane_draw_button_text(&rect, "Reset touch controls to defaults?",
        "Confirm: Space/Enter button. Cancel: Esc.", text);
}

static void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    int binding;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);

    if (raw_binding == TOUCH_PANE_BIND_INHERIT)
        return;

    if (binding == GAMEPAD_BIND_NONE) {
        SDL_strlcpy(buf, "Off", buflen);
        return;
    }

    if (binding == GAMEPAD_BIND_SHIFT) {
        int other_panel = sdl_touch_pane_other_panel(panel);

        sdl_touch_pane_load_default_bindings();
        if (config.touch_pane_panel_names[other_panel][0]) {
            SDL_strlcpy(buf, config.touch_pane_panel_names[other_panel], buflen);
        } else {
            SDL_strlcpy(buf, g_default_touch_pane_panel_names[other_panel], buflen);
        }
        return;
    }

    if (binding == INPUT_BIND_CONFIRM) {
        SDL_strlcpy(buf, "Space", buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_MAIN
        && binding == g_touch_pane_slots[index].default_binding
        && g_touch_pane_slots[index].default_label
        && g_touch_pane_slots[index].default_label[0]) {
        SDL_strlcpy(buf, g_touch_pane_slots[index].default_label, buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND) {
        switch (index) {
        case 3:
            if (binding == '0') {
                SDL_strlcpy(buf, "Smith", buflen);
                return;
            }
            break;
        case 18:
            if (binding == 'L') {
                SDL_strlcpy(buf, "AltView", buflen);
                return;
            }
            break;
        case 7:
            if (binding == 'S') {
                SDL_strlcpy(buf, "Stealth", buflen);
                return;
            }
            break;
        case 8:
            if (binding == 'F') {
                SDL_strlcpy(buf, "Shoot 2", buflen);
                return;
            }
            break;
        case 19:
            if (binding == 'X') {
                SDL_strlcpy(buf, "Exch", buflen);
                return;
            }
            break;
        case 20:
            if (binding == 'p') {
                SDL_strlcpy(buf, "Play", buflen);
                return;
            }
            break;
        default:
            break;
        }
    }

    binding_action_short(binding, buf, buflen);
}

static void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    int raw_binding;
    const char* custom_label = NULL;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    raw_binding = sdl_touch_pane_raw_binding_for_panel(panel, index);
    custom_label = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels[index]
        : config.touch_pane_labels[index];

    if (sdl_touch_pane_label_is_symbol_only(custom_label)) {
        sdl_touch_pane_binding_symbol(sdl_touch_pane_effective_binding_for_panel(panel, index),
            buf, buflen);
        return;
    }

    if (custom_label[0]) {
        SDL_strlcpy(buf, custom_label, buflen);
        return;
    }

    if (panel == SDL_TOUCH_PANE_PANEL_SECOND && raw_binding == TOUCH_PANE_BIND_INHERIT) {
        sdl_touch_pane_base_label_for_slot(SDL_TOUCH_PANE_PANEL_MAIN, index, buf, buflen);
        return;
    }

    sdl_touch_pane_default_label_for_panel_slot(panel, index, buf, buflen);
}

static void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);
}

static void sdl_touch_pane_send_confirm_action(void)
{
    if (character_dungeon) {
        sdl_submit_legacy_input_byte(' ');
        return;
    }

    sdl_submit_legacy_input_byte('\r');
}

static void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press)
{
    if (binding == GAMEPAD_BIND_NONE)
        return;

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_touch_pane_second_panel = !g_touch_pane_second_panel;
        g_state.need_present = true;
        return;
    }

    if (binding == GAMEPAD_BIND_CTRL) {
        g_touch_pane_ctrl_toggle = !g_touch_pane_ctrl_toggle;
        sdl_gamepad_apply_modifier(binding, g_touch_pane_ctrl_toggle);
        return;
    }

    if (binding == GAMEPAD_BIND_ALT) {
        sdl_gamepad_apply_modifier(binding, true);
        sdl_gamepad_apply_modifier(binding, false);
        return;
    }

    if (sdl_touch_pane_confirm_binding(binding)) {
        if (long_press && character_dungeon) {
            sdl_submit_legacy_input_byte('z');
        } else {
            sdl_touch_pane_send_confirm_action();
        }
        return;
    }

    if (sdl_touch_pane_binding_is_direction(binding)) {
        bool shift = ((!long_press) && second_panel) || sdl_gamepad_shift_active();
        bool ctrl = long_press || sdl_gamepad_ctrl_active();
        bool alt = sdl_gamepad_alt_active();
        int dir = binding - '0';

        if (sdl_submit_directional_movement(dir, shift, ctrl, alt,
                APP_INPUT_DEVICE_TOUCH, APP_INPUT_TYPE_POINTER_BUTTON, 0,
                APP_INPUT_FLAG_PRESS, (u32b)binding, 0))
        {
            return;
        }
        return;
    }

    if (binding == 'z' && long_press) {
        sdl_submit_legacy_input_byte('Z');
        return;
    }

    sdl_gamepad_send_key(binding, false);
}

static void sdl_touch_pane_send_slot(int panel, int index, bool long_press)
{
    int binding;

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    binding = sdl_touch_pane_effective_binding_for_panel(panel, index);
    sdl_touch_pane_send_binding(binding, panel == SDL_TOUCH_PANE_PANEL_SECOND, long_press);
}

void sdl_touch_pane_cancel_press(void)
{
    if (!g_touch_pane_press.active && g_touch_pane_pressed_slot < 0)
        return;

    g_touch_pane_press.active = false;
    g_touch_pane_pressed_slot = -1;
    g_state.need_present = true;
}

int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_touch_pane_press.active)
        return -1;

    elapsed = now_ns - g_touch_pane_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;

    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_touch_pane_flush_pending_press(Uint64 now_ns)
{
    int slot;

    if (!g_touch_pane_press.active)
        return false;
    if (now_ns - g_touch_pane_press.start_time < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return false;

    slot = g_touch_pane_press.slot;
    sdl_touch_pane_cancel_press();
    if (slot == 0) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        int panel = g_touch_pane_press.panel;
        sdl_touch_pane_send_slot(panel, slot, true);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
    }
    return true;
}

static bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return false;
    if (slot < 0)
        return true;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    if (sdl_touch_pane_slot_uses_long_press(slot, binding)) {
        sdl_touch_pane_cancel_press();
        g_touch_pane_press.active = true;
        g_touch_pane_press.mouse = mouse;
        g_touch_pane_press.finger_id = finger_id;
        g_touch_pane_press.panel = panel;
        g_touch_pane_press.slot = slot;
        g_touch_pane_press.start_time = SDL_GetTicksNS();
        g_touch_pane_pressed_slot = slot;
        g_state.need_present = true;
        return true;
    }

    sdl_touch_pane_send_slot(panel, slot, false);
    g_touch_pane_flash_slot = slot;
    g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
    return true;
}

static void sdl_touch_pane_handle_pointer_up(bool mouse, SDL_FingerID finger_id)
{
    Uint64 press_time;
    bool ctrl_direction_override;
    int slot;
    int panel;

    if (!g_touch_pane_press.active)
        return;
    if (g_touch_pane_press.mouse != mouse)
        return;
    if (!mouse && g_touch_pane_press.finger_id != finger_id)
        return;

    press_time = SDL_GetTicksNS() - g_touch_pane_press.start_time;
    ctrl_direction_override = (press_time >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL);
    slot = g_touch_pane_press.slot;
    panel = g_touch_pane_press.panel;
    sdl_touch_pane_cancel_press();
    if (slot == 0 && ctrl_direction_override) {
        sdl_touch_pane_begin_reset_confirm();
    } else {
        sdl_touch_pane_send_slot(panel, slot, ctrl_direction_override);
        g_touch_pane_flash_slot = slot;
        g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
        g_state.need_present = true;
    }
}

void sdl_touch_pane_load_default_bindings(void)
{
    struct sdl_config defaults;

    if (g_default_touch_pane_bindings_ready)
        return;

    sdl_config_set_defaults(&defaults);
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_MAIN], defaults.touch_pane_bindings,
        sizeof(defaults.touch_pane_bindings));
    memcpy(g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_SECOND], defaults.touch_pane_second_bindings,
        sizeof(defaults.touch_pane_second_bindings));
    memcpy(g_default_touch_pane_panel_names, defaults.touch_pane_panel_names,
        sizeof(g_default_touch_pane_panel_names));
    g_default_touch_pane_bindings_ready = true;
}

void sdl_touch_pane_reset_input_state(void)
{
    if (g_touch_pane_ctrl_toggle) {
        g_touch_pane_ctrl_toggle = false;
        sdl_gamepad_apply_modifier(GAMEPAD_BIND_CTRL, false);
    }

    g_touch_pane_second_panel = false;
    g_touch_pane_reset_confirm_active = false;
    sdl_touch_pane_cancel_press();
    g_touch_pane_flash_slot = -1;
    g_touch_pane_flash_until = 0;
    g_touch_pane_pressed_slot = -1;
    g_state.need_present = true;
}

bool sdl_touch_pane_handle_event(const SDL_Event* ev)
{
    if (!ev)
        return false;

    if (g_touch_pane_reset_confirm_active) {
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.key == SDLK_ESCAPE) {
                sdl_touch_pane_finish_reset_confirm(false);
            } else if (ev->key.key == SDLK_RETURN || ev->key.key == SDLK_KP_ENTER
                || ev->key.key == SDLK_SPACE) {
                sdl_touch_pane_finish_reset_confirm(true);
            }
            return true;
        }
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev->button.button == SDL_BUTTON_LEFT && ev->button.which != SDL_TOUCH_MOUSEID)
                sdl_touch_pane_handle_reset_prompt_pointer((float)ev->button.x, (float)ev->button.y);
            return true;
        }
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP)
            return true;
        if (ev->type == SDL_EVENT_FINGER_DOWN) {
            int window_w = 0;
            int window_h = 0;

            if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
                return true;
            SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
            sdl_touch_pane_handle_reset_prompt_pointer(ev->tfinger.x * (float)window_w,
                ev->tfinger.y * (float)window_h);
            return true;
        }
        if (ev->type == SDL_EVENT_FINGER_UP || ev->type == SDL_EVENT_FINGER_CANCELED)
            return true;
        return true;
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (ev->button.button != SDL_BUTTON_LEFT || ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        return sdl_touch_pane_handle_pointer_down((float)ev->button.x, (float)ev->button.y, true, 0);
    }

    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (ev->button.button != SDL_BUTTON_LEFT || ev->button.which == SDL_TOUCH_MOUSEID)
            return false;
        sdl_touch_pane_handle_pointer_up(true, 0);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_DOWN) {
        int window_w = 0;
        int window_h = 0;
        float x;
        float y;

        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;

        SDL_GetWindowSizeInPixels(g_state.window, &window_w, &window_h);
        x = ev->tfinger.x * (float)window_w;
        y = ev->tfinger.y * (float)window_h;
        return sdl_touch_pane_handle_pointer_down(x, y, false, ev->tfinger.fingerID);
    }

    if (ev->type == SDL_EVENT_FINGER_UP) {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        sdl_touch_pane_handle_pointer_up(false, ev->tfinger.fingerID);
        return true;
    }

    if (ev->type == SDL_EVENT_FINGER_CANCELED) {
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (g_touch_pane_press.active && !g_touch_pane_press.mouse
            && g_touch_pane_press.finger_id == ev->tfinger.fingerID) {
            sdl_touch_pane_cancel_press();
        }
        return true;
    }

    return false;
}

void sdl_touch_pane_render(void)
{
    SDL_FRect slot_rects[SDL_TOUCH_PANE_BUTTON_COUNT];
    SDL_FRect pane_rect;
    SDL_Color frame = g_state.palette[TERM_WHITE];
    SDL_Color accent = g_state.palette[TERM_L_BLUE];
    SDL_Color muted = g_state.palette[TERM_SLATE];
    const SDL_Rect* pane = &g_pane_rects[PANE_TOUCH];
    int panel = sdl_touch_pane_active_panel();

    if (pane->w <= 0 || pane->h <= 0)
        return;
    if (!sdl_touch_pane_compute_layout(pane, slot_rects))
        return;

    pane_rect = (SDL_FRect){
        .x = (float)pane->x,
        .y = (float)pane->y,
        .w = (float)pane->w,
        .h = (float)pane->h,
    };

    SDL_SetRenderDrawColor(g_state.renderer, 12, 12, 12, 255);
    SDL_RenderFillRect(g_state.renderer, &pane_rect);
    SDL_SetRenderDrawColor(g_state.renderer, frame.r, frame.g, frame.b, 180);
    SDL_RenderRect(g_state.renderer, &pane_rect);

    for (int i = 0; i < SDL_TOUCH_PANE_BUTTON_COUNT; i++) {
        SDL_Color text_color;
        SDL_Color border_color;
        SDL_FRect shadow;
        char label[64];
        char symbol[32];
        int binding = sdl_touch_pane_effective_binding_for_panel(panel, i);
        bool flashed = (i == g_touch_pane_flash_slot);
        bool pressed = (i == g_touch_pane_pressed_slot);
        bool toggled = ((binding == GAMEPAD_BIND_SHIFT && g_touch_pane_second_panel)
            || (binding == GAMEPAD_BIND_CTRL && sdl_gamepad_ctrl_active()));

        shadow = slot_rects[i];
        shadow.x += 2.0f;
        shadow.y += 2.0f;

        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(g_state.renderer, &shadow);

        if (binding == GAMEPAD_BIND_NONE) {
            SDL_SetRenderDrawColor(g_state.renderer, 26, 26, 26, 255);
            text_color = muted;
            border_color = muted;
        } else if (toggled) {
            SDL_SetRenderDrawColor(g_state.renderer, 48, 58, 44, 255);
            text_color = accent;
            border_color = accent;
        } else if (pressed || flashed) {
            SDL_SetRenderDrawColor(g_state.renderer, 54, 66, 86, 255);
            text_color = accent;
            border_color = accent;
        } else {
            SDL_SetRenderDrawColor(g_state.renderer, 34, 34, 34, 255);
            text_color = frame;
            border_color = frame;
        }

        SDL_RenderFillRect(g_state.renderer, &slot_rects[i]);
        SDL_SetRenderDrawColor(g_state.renderer, border_color.r, border_color.g, border_color.b, 220);
        SDL_RenderRect(g_state.renderer, &slot_rects[i]);

        if (sdl_touch_pane_binding_is_direction(binding)) {
            sdl_touch_pane_draw_arrow(&slot_rects[i], binding, text_color);
            continue;
        }

        sdl_touch_pane_display_label_for_slot(panel, i, label, sizeof(label));
        sdl_touch_pane_binding_symbol(binding, symbol, sizeof(symbol));
        if (sdl_touch_pane_should_hide_symbol(label, symbol))
            symbol[0] = '\0';
        sdl_touch_pane_draw_button_text(&slot_rects[i], label, symbol, text_color);
    }

    if (g_touch_pane_flash_slot >= 0 && SDL_GetTicksNS() >= g_touch_pane_flash_until) {
        g_touch_pane_flash_slot = -1;
        g_touch_pane_flash_until = 0;
    }
}

int get_sdl_touch_pane_binding(int index)
{
    return get_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

int get_sdl_touch_pane_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    return sdl_touch_pane_raw_binding_for_panel(panel, index);
}

void set_sdl_touch_pane_binding(int index, int binding)
{
    set_sdl_touch_pane_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, binding);
}

void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;
    if (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        config.touch_pane_second_bindings[index] = binding;
    else
        config.touch_pane_bindings[index] = binding;
}

int get_sdl_touch_pane_default_binding(int index)
{
    return get_sdl_touch_pane_default_binding_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

int get_sdl_touch_pane_default_binding_for_panel(int panel, int index)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return GAMEPAD_BIND_NONE;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return GAMEPAD_BIND_NONE;
    sdl_touch_pane_load_default_bindings();
    return g_default_touch_pane_bindings[panel][index];
}

void sdl_touch_pane_reset_bindings_to_default(void)
{
    sdl_touch_pane_reset_input_state();
    sdl_config_set_default_touch_pane_bindings(&config);
    sdl_config_clear_touch_pane_labels(&config);
}

cptr get_sdl_touch_pane_slot_name(int index)
{
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return "";
    return g_touch_pane_slots[index].slot_name;
}

void get_sdl_touch_pane_button_label(int index, char* buf, size_t buflen)
{
    get_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, buf, buflen);
}

void set_sdl_touch_pane_button_label(int index, cptr label)
{
    set_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index, label);
}

void clear_sdl_touch_pane_button_label(int index)
{
    clear_sdl_touch_pane_button_label_for_panel(SDL_TOUCH_PANE_PANEL_MAIN, index);
}

void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen)
{
    if (!sdl_touch_pane_panel_is_valid(panel)) {
        if (buf && buflen)
            buf[0] = '\0';
        return;
    }

    sdl_touch_pane_base_label_for_slot(panel, index, buf, buflen);
}

void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label)
{
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    labels = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels
        : config.touch_pane_labels;

    if (!label || !label[0]) {
        labels[index][0] = '\x01';
        labels[index][1] = '\0';
        return;
    }

    SDL_strlcpy(labels[index], label, SDL_TOUCH_PANE_LABEL_LEN);
}

void clear_sdl_touch_pane_button_label_for_panel(int panel, int index)
{
    char (*labels)[SDL_TOUCH_PANE_LABEL_LEN];

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;
    if (index < 0 || index >= SDL_TOUCH_PANE_BUTTON_COUNT)
        return;

    labels = (panel == SDL_TOUCH_PANE_PANEL_SECOND)
        ? config.touch_pane_second_labels
        : config.touch_pane_labels;
    labels[index][0] = '\0';
}

void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!sdl_touch_pane_panel_is_valid(panel))
        return;

    sdl_touch_pane_load_default_bindings();
    if (config.touch_pane_panel_names[panel][0]) {
        SDL_strlcpy(buf, config.touch_pane_panel_names[panel], buflen);
    } else {
        SDL_strlcpy(buf, g_default_touch_pane_panel_names[panel], buflen);
    }
}

void set_sdl_touch_pane_panel_name(int panel, cptr name)
{
    if (!sdl_touch_pane_panel_is_valid(panel))
        return;

    if (!name || !name[0]) {
        config.touch_pane_panel_names[panel][0] = '\0';
        return;
    }

    SDL_strlcpy(config.touch_pane_panel_names[panel], name,
        sizeof(config.touch_pane_panel_names[panel]));
}
