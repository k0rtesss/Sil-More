#include "angband.h"
#include "sdl/main-sdl-private.h"

/*
 * Native memorial surface for the Halls of Mandos.
 *
 * The score layer owns ordering, paging, and commands.  It feeds this renderer
 * semantic fields so SDL can compose them in pixels without inheriting any
 * terminal rows or columns.
 */

enum {
    SDL_HALLS_MAX_ENTRIES = 12,
    SDL_HALLS_MAX_ACTIONS = 8,
    SDL_HALLS_TEXT_LEN = 256
};

typedef struct sdl_halls_entry {
    int choice;
    byte attr;
    bool selected;
    char rank[16];
    char name[64];
    char score[32];
    char outcome[SDL_HALLS_TEXT_LEN];
    char details[SDL_HALLS_TEXT_LEN];
    char honors[96];
    char score_increases[SDL_HALLS_TEXT_LEN];
    char score_decreases[SDL_HALLS_TEXT_LEN];
    SDL_FRect hit_rect;
} sdl_halls_entry;

typedef struct sdl_halls_action {
    int choice;
    byte attr;
    bool enabled;
    char label[64];
    SDL_FRect hit_rect;
} sdl_halls_action;

typedef struct sdl_halls_state {
    bool active;
    bool detailed;
    bool touch_press_active;
    bool touch_press_dragged;
    SDL_FingerID touch_press_finger_id;
    int touch_press_choice;
    float touch_press_start_x;
    float touch_press_start_y;
    int entry_count;
    int action_count;
    int hover_choice;
    int outside_choice;
    char subtitle[160];
    char page_status[160];
    char empty_text[160];
    sdl_halls_entry entries[SDL_HALLS_MAX_ENTRIES];
    sdl_halls_action actions[SDL_HALLS_MAX_ACTIONS];
} sdl_halls_state;

typedef struct sdl_halls_layout {
    float margin_x;
    float margin_top;
    float margin_bottom;
    float content_w;
    float content_x;
    float header_h;
    float header_body_gap;
    float body_footer_gap;
    float footer_h;
    float body_top;
    float body_h;
    float gap;
} sdl_halls_layout;

static sdl_halls_state g_sdl_halls;

static float sdl_halls_clampf(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int sdl_halls_clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static bool sdl_halls_mobile_layout(void)
{
#if SIL_SDL_MOBILE_BUILD
    return true;
#else
    return false;
#endif
}

static float sdl_halls_short_side(const SDL_Rect* canvas)
{
    float short_side;

    if (!canvas)
        return 720.0f;

    short_side = (float)MIN(canvas->w, canvas->h);
    return (short_side > 0.0f) ? short_side : 720.0f;
}

static void sdl_halls_measure_layout(const SDL_Rect* canvas,
    sdl_halls_layout* layout)
{
    bool mobile = sdl_halls_mobile_layout();
    float short_side;

    if (!canvas || !layout)
        return;

    memset(layout, 0, sizeof(*layout));
    short_side = sdl_halls_short_side(canvas);
    if (mobile)
    {
        layout->margin_x = sdl_halls_clampf((float)canvas->w * 0.055f,
            18.0f, 92.0f);
        layout->margin_top = sdl_halls_clampf((float)canvas->h * 0.025f,
            10.0f, 32.0f);
        layout->margin_bottom = sdl_halls_clampf((float)canvas->h * 0.022f,
            9.0f, 28.0f);
        layout->content_w = MIN((float)canvas->w
            - layout->margin_x * 2.0f, 1600.0f);
        layout->header_h = sdl_halls_clampf((float)canvas->h * 0.27f,
            148.0f, 300.0f);
        layout->header_body_gap = sdl_halls_clampf(short_side * 0.030f,
            12.0f, 28.0f);
        layout->body_footer_gap = 0.0f;
        layout->footer_h = sdl_halls_clampf((float)canvas->h * 0.12f,
            64.0f, 112.0f);
    }
    else
    {
        /*
         * Desktop Halls use physical pixels.  Keep every limit proportional
         * to the live canvas so high-DPI and large-window layouts continue to
         * grow instead of freezing at a small fixed design size.
         */
        layout->margin_x = MAX((float)canvas->w * 0.060f, 24.0f);
        layout->margin_top = MAX(short_side * 0.030f, 16.0f);
        layout->margin_bottom = layout->margin_top;
        layout->content_w = MIN((float)canvas->w
            - layout->margin_x * 2.0f,
            MIN((float)canvas->w * 0.700f, short_side * 1.30f));
        layout->header_h = MAX(short_side * 0.170f, 136.0f);
        layout->header_body_gap = MAX(short_side * 0.022f, 16.0f);
        layout->body_footer_gap = layout->header_body_gap;
        layout->footer_h = MAX(short_side * 0.058f, 52.0f);
    }
    if (layout->content_w < 1.0f)
        layout->content_w = 1.0f;
    layout->content_x = (float)canvas->x
        + ((float)canvas->w - layout->content_w) * 0.5f;
    layout->body_top = (float)canvas->y + layout->margin_top
        + layout->header_h + layout->header_body_gap;
    layout->body_h = (float)canvas->h - layout->margin_top
        - layout->margin_bottom - layout->header_h
        - layout->header_body_gap - layout->body_footer_gap
        - layout->footer_h;
    if (layout->body_h < 1.0f)
        layout->body_h = 1.0f;
    layout->gap = mobile
        ? sdl_halls_clampf(layout->body_h * 0.015f, 5.0f, 13.0f)
        : MAX(short_side * 0.010f, 6.0f);
}

static float sdl_halls_target_card_h(const SDL_Rect* canvas, bool detailed)
{
    float short_side = sdl_halls_short_side(canvas);

    if (sdl_halls_mobile_layout())
    {
        return detailed
            ? sdl_halls_clampf(short_side * 0.300f, 210.0f, 480.0f)
            : sdl_halls_clampf(short_side * 0.090f, 72.0f, 220.0f);
    }

    return detailed ? MAX(short_side * 0.200f, 150.0f)
                    : MAX(short_side * 0.080f, 72.0f);
}

static int sdl_halls_capacity_for_layout(const SDL_Rect* canvas,
    const sdl_halls_layout* layout, bool detailed)
{
    float target_card_h;
    int capacity;

    if (!canvas || !layout)
        return 1;

    target_card_h = sdl_halls_target_card_h(canvas, detailed);
    capacity = (int)((layout->body_h + layout->gap)
        / (target_card_h + layout->gap));
    return sdl_halls_clampi(capacity, 1, SDL_HALLS_MAX_ENTRIES);
}

static bool sdl_halls_rect_has_area(const SDL_FRect* r)
{
    return r && r->w > 0.0f && r->h > 0.0f;
}

static bool sdl_halls_point_in_rect(float x, float y, const SDL_FRect* r)
{
    return sdl_halls_rect_has_area(r)
        && x >= r->x && x < r->x + r->w
        && y >= r->y && y < r->y + r->h;
}

static SDL_Color sdl_halls_color(byte attr, byte alpha)
{
    SDL_Color color;
    int safe_attr = (attr < 16) ? attr : TERM_WHITE;

    color = g_state.palette[safe_attr];
    color.a = alpha;
    return color;
}

static SDL_FRect sdl_halls_draw_text(TTF_Font* font, cptr text, byte attr,
    SDL_FRect box, int alignment)
{
    SDL_FRect dst = { 0 };
    SDL_Texture* texture;
    SDL_Color color;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || box.w <= 0.0f || box.h <= 0.0f)
        return dst;

    color = sdl_halls_color(attr, 255);
    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture || text_w <= 0 || text_h <= 0)
        return dst;

    if ((float)text_w > box.w)
        scale = box.w / (float)text_w;
    if ((float)text_h * scale > box.h)
        scale = box.h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst.w = (float)text_w * scale;
    dst.h = (float)text_h * scale;
    dst.x = box.x;
    if (alignment == 0)
        dst.x += (box.w - dst.w) * 0.5f;
    else if (alignment > 0)
        dst.x += box.w - dst.w;
    dst.y = box.y + (box.h - dst.h) * 0.5f;
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    return dst;
}

static TTF_Font* sdl_halls_fitted_font(cptr text, SDL_FRect box,
    int min_px, int max_px, int slot)
{
    TTF_Font* chosen = NULL;
    int low = MAX(1, min_px);
    int high = MAX(low, max_px);

    while (text && text[0] && box.w > 0.0f && box.h > 0.0f && low <= high)
    {
        int px = low + (high - low) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px, slot);
        int text_w = 0;
        int text_h = 0;

        if (font && TTF_GetStringSize(font, text, 0, &text_w, &text_h)
            && (float)text_w <= box.w + 1.0f
            && (float)text_h <= box.h + 1.0f)
        {
            chosen = font;
            low = px + 1;
        }
        else
            high = px - 1;
    }

    return chosen ? chosen : sdl_story_font_for_height_slot(MAX(1, min_px),
        slot);
}

static SDL_FRect sdl_halls_draw_fitted_text(cptr text, byte attr,
    SDL_FRect box, int alignment, int min_px, int max_px, int slot)
{
    TTF_Font* font = sdl_halls_fitted_font(text, box, min_px, max_px, slot);

    return sdl_halls_draw_text(font, text, attr, box, alignment);
}

static TTF_Font* sdl_halls_wrapped_font(cptr text, SDL_FRect box,
    int min_px, int max_px)
{
    TTF_Font* chosen = NULL;
    int low = min_px;
    int high = MAX(min_px, max_px);
    int wrap_width = MAX(1, (int)(box.w + 0.5f));

    while (low <= high)
    {
        int px = low + (high - low) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_NARRATIVE);
        int text_w = 0;
        int text_h = 0;

        if (font && TTF_GetStringSizeWrapped(font, text, 0, wrap_width,
                &text_w, &text_h)
            && (float)text_w <= box.w + 1.0f
            && (float)text_h <= box.h + 1.0f)
        {
            chosen = font;
            low = px + 1;
        }
        else
            high = px - 1;
    }

    return chosen ? chosen : sdl_story_font_for_height_slot(min_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
}

static void sdl_halls_draw_wrapped_text(cptr text, byte attr, SDL_FRect box,
    int min_px, int max_px)
{
    TTF_Font* font;
    SDL_Texture* texture;
    SDL_Color color;
    SDL_FRect dst;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!text || !text[0] || box.w <= 0.0f || box.h <= 0.0f)
        return;

    font = sdl_halls_wrapped_font(text, box, min_px, max_px);
    if (!font)
        return;
    color = sdl_halls_color(attr, 255);
    texture = sdl_ui_wrapped_text_texture(font, text,
        MAX(1, (int)(box.w + 0.5f)), color, &text_w, &text_h);
    if (!texture || text_w <= 0 || text_h <= 0)
        return;

    if ((float)text_w > box.w)
        scale = box.w / (float)text_w;
    if ((float)text_h * scale > box.h)
        scale = box.h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst = (SDL_FRect){ box.x, box.y, (float)text_w * scale,
        (float)text_h * scale };
    dst.y += (box.h - dst.h) * 0.5f;
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
}

static void sdl_halls_draw_rule(float x, float y, float w, byte attr,
    byte alpha)
{
    SDL_Color color = sdl_halls_color(attr, alpha);
    SDL_FRect line = { x, y, w, 1.0f };

    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        color.a);
    SDL_RenderFillRect(g_state.renderer, &line);
}

static void sdl_halls_clear_hits(void)
{
    int i;

    for (i = 0; i < g_sdl_halls.entry_count; i++)
        g_sdl_halls.entries[i].hit_rect = (SDL_FRect){ 0 };
    for (i = 0; i < g_sdl_halls.action_count; i++)
        g_sdl_halls.actions[i].hit_rect = (SDL_FRect){ 0 };
}

bool sdl_halls_screen_active(void)
{
    return g_sdl_halls.active;
}

void sdl_halls_screen_begin(cptr subtitle, cptr page_status,
    bool detailed, int outside_choice)
{
    memset(&g_sdl_halls, 0, sizeof(g_sdl_halls));
    g_sdl_halls.active = true;
    g_sdl_halls.detailed = detailed;
    g_sdl_halls.hover_choice = INT_MIN;
    g_sdl_halls.outside_choice = outside_choice;
    SDL_strlcpy(g_sdl_halls.subtitle, subtitle ? subtitle : "",
        sizeof(g_sdl_halls.subtitle));
    SDL_strlcpy(g_sdl_halls.page_status, page_status ? page_status : "",
        sizeof(g_sdl_halls.page_status));
    g_state.need_present = true;
}

int sdl_halls_screen_page_capacity(bool detailed)
{
    SDL_Rect canvas = sdl_get_layout_screen_rect();
    sdl_halls_layout layout;

    if (canvas.w <= 0 || canvas.h <= 0)
        canvas = sdl_get_window_pixel_rect();
    if (canvas.w <= 0 || canvas.h <= 0)
        canvas = (SDL_Rect){ 0, 0, 1280, 720 };
    sdl_halls_measure_layout(&canvas, &layout);
    return sdl_halls_capacity_for_layout(&canvas, &layout, detailed);
}

void sdl_halls_screen_add_entry(int choice, cptr rank, cptr name,
    cptr score, cptr outcome, cptr details, cptr honors,
    cptr score_increases, cptr score_decreases, byte attr, bool selected)
{
    sdl_halls_entry* entry;

    if (!g_sdl_halls.active
        || g_sdl_halls.entry_count >= SDL_HALLS_MAX_ENTRIES)
    {
        return;
    }

    entry = &g_sdl_halls.entries[g_sdl_halls.entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->choice = choice;
    entry->attr = attr;
    entry->selected = selected;
    SDL_strlcpy(entry->rank, rank ? rank : "", sizeof(entry->rank));
    SDL_strlcpy(entry->name, name ? name : "", sizeof(entry->name));
    SDL_strlcpy(entry->score, score ? score : "", sizeof(entry->score));
    SDL_strlcpy(entry->outcome, outcome ? outcome : "", sizeof(entry->outcome));
    SDL_strlcpy(entry->details, details ? details : "", sizeof(entry->details));
    SDL_strlcpy(entry->honors, honors ? honors : "", sizeof(entry->honors));
    SDL_strlcpy(entry->score_increases,
        score_increases ? score_increases : "",
        sizeof(entry->score_increases));
    SDL_strlcpy(entry->score_decreases,
        score_decreases ? score_decreases : "",
        sizeof(entry->score_decreases));
    g_state.need_present = true;
}

void sdl_halls_screen_set_empty(cptr text)
{
    if (!g_sdl_halls.active)
        return;
    SDL_strlcpy(g_sdl_halls.empty_text, text ? text : "",
        sizeof(g_sdl_halls.empty_text));
    g_state.need_present = true;
}

void sdl_halls_screen_add_action(int choice, cptr label, byte attr,
    bool enabled)
{
    sdl_halls_action* action;

    if (!g_sdl_halls.active
        || g_sdl_halls.action_count >= SDL_HALLS_MAX_ACTIONS)
    {
        return;
    }

    action = &g_sdl_halls.actions[g_sdl_halls.action_count++];
    memset(action, 0, sizeof(*action));
    action->choice = choice;
    action->attr = attr;
    action->enabled = enabled;
    SDL_strlcpy(action->label, label ? label : "", sizeof(action->label));
    g_state.need_present = true;
}

void sdl_halls_screen_hide(void)
{
    if (!g_sdl_halls.active)
        return;
    g_sdl_halls.active = false;
    g_sdl_halls.hover_choice = INT_MIN;
    sdl_halls_clear_hits();
    g_state.need_present = true;
}

static void sdl_halls_render_header(const SDL_Rect* canvas, float content_x,
    float content_w, float top, float header_h)
{
    bool mobile = sdl_halls_mobile_layout();
    float short_side = sdl_halls_short_side(canvas);
    int title_px = mobile
        ? sdl_halls_clampi((int)((float)canvas->h * 0.085f), 40, 88)
        : MAX((int)(short_side * 0.050f), 28);
    int body_px = mobile
        ? sdl_halls_clampi((int)((float)canvas->h * 0.065f), 30, 64)
        : MAX((int)(short_side * 0.029f), 24);
    int title_min_px = mobile ? 28
        : MAX((int)(short_side * 0.030f), 28);
    int body_min_px = mobile ? 24
        : MAX((int)(short_side * 0.019f), 24);
    float title_h = header_h * 0.37f;
    float rule_y = top + title_h + header_h * 0.07f;
    float meta_h = header_h * 0.22f;
    float meta_gap = header_h * 0.02f;
    SDL_FRect box;

    box = (SDL_FRect){ content_x, top, content_w, title_h };
    (void)sdl_halls_draw_fitted_text(
        "H A L L S   O F   M A N D O S", TERM_YELLOW, box, 0,
        title_min_px, title_px, SDL_STORY_FONT_SLOT_DEFAULT);
    sdl_halls_draw_rule(content_x + content_w * 0.33f, rule_y + 4.0f,
        content_w * 0.34f, TERM_YELLOW, 120);

    box = (SDL_FRect){ content_x, rule_y + header_h * 0.07f, content_w,
        meta_h };
    (void)sdl_halls_draw_fitted_text(g_sdl_halls.subtitle, TERM_L_WHITE,
        box, 0, body_min_px, body_px, SDL_STORY_FONT_SLOT_NARRATIVE);
    box.y += meta_h + meta_gap;
    (void)sdl_halls_draw_fitted_text(g_sdl_halls.page_status, TERM_SLATE,
        box, 0, body_min_px, body_px, SDL_STORY_FONT_SLOT_NARRATIVE);
}

static char* sdl_halls_trim_segment(char* text)
{
    char* end;

    if (!text)
        return NULL;
    while (*text == ' ')
        text++;
    end = text + strlen(text);
    while (end > text && end[-1] == ' ')
        end--;
    *end = '\0';
    return text;
}

static void sdl_halls_draw_details(TTF_Font* font, cptr text,
    SDL_FRect box)
{
    char buffer[SDL_HALLS_TEXT_LEN];
    char* separator1;
    char* separator2;
    char* turns;
    char* depth;
    char* date;
    float gap;
    float usable_w;
    SDL_FRect field;

    if (!font || !text || !text[0])
        return;

    SDL_strlcpy(buffer, text, sizeof(buffer));
    separator1 = strchr(buffer, '|');
    separator2 = separator1 ? strchr(separator1 + 1, '|') : NULL;
    if (!separator1 || !separator2)
    {
        (void)sdl_halls_draw_text(font, text, TERM_SLATE, box, -1);
        return;
    }

    *separator1 = '\0';
    *separator2 = '\0';
    turns = sdl_halls_trim_segment(buffer);
    depth = sdl_halls_trim_segment(separator1 + 1);
    date = sdl_halls_trim_segment(separator2 + 1);
    if (prefix(depth, "deepest descent "))
        depth += strlen("deepest descent ");

    gap = sdl_halls_clampf(box.w * 0.018f, 5.0f, 12.0f);
    usable_w = box.w - gap * 2.0f;
    field = (SDL_FRect){ box.x, box.y, usable_w * 0.37f, box.h };
    (void)sdl_halls_draw_text(font, turns, TERM_SLATE, field, -1);
    field.x += field.w + gap;
    field.w = usable_w * 0.26f;
    (void)sdl_halls_draw_text(font, depth, TERM_SLATE, field, 0);
    field.x += field.w + gap;
    field.w = box.x + box.w - field.x;
    (void)sdl_halls_draw_text(font, date, TERM_SLATE, field, 1);
}

static void sdl_halls_render_entry(sdl_halls_entry* entry, SDL_FRect card,
    int canvas_short_side)
{
    bool mobile = sdl_halls_mobile_layout();
    bool hovered = (entry->choice == g_sdl_halls.hover_choice);
    bool focused = entry->selected || hovered;
    SDL_Color border = sdl_halls_color(focused ? TERM_YELLOW : TERM_BLUE,
        focused ? 220 : 100);
    SDL_Color accent = sdl_halls_color(entry->attr, focused ? 255 : 185);
    float short_side = (float)MAX(canvas_short_side, 1);
    float pad = mobile
        ? sdl_halls_clampf(card.h * 0.13f, 8.0f, 19.0f)
        : sdl_halls_clampf(short_side * 0.018f, 8.0f,
            MAX(8.0f, card.h * 0.10f));
    float rank_w = mobile
        ? sdl_halls_clampf(card.w * 0.064f, 42.0f, 82.0f)
        : MIN(MAX(short_side * 0.055f, 42.0f), card.w * 0.15f);
    float score_w = mobile
        ? sdl_halls_clampf(card.w * 0.18f, 96.0f, 210.0f)
        : MIN(MAX(card.w * 0.16f, 96.0f),
            MAX(short_side * 0.24f, 96.0f));
    float text_x = card.x + pad + rank_w;
    float text_w = card.w - pad * 2.0f - rank_w;
    int name_px = mobile
        ? sdl_halls_clampi((int)(card.h
            * (g_sdl_halls.detailed ? 0.40f : 0.44f)), 28, 68)
        : MAX((int)(short_side * 0.040f), 21);
    int body_px = mobile
        ? sdl_halls_clampi((int)(card.h
            * (g_sdl_halls.detailed ? 0.32f : 0.36f)), 24, 56)
        : MAX((int)(short_side * 0.032f), 18);
    int meta_px = mobile
        ? sdl_halls_clampi((int)(card.h * 0.27f), 24, 48)
        : MAX((int)(short_side * 0.025f), 15);
    int name_min_px = mobile ? 28
        : MAX((int)(short_side * 0.024f), 21);
    int body_min_px = mobile ? 24
        : MAX((int)(short_side * 0.019f), 18);
    int meta_min_px = mobile ? 24
        : MAX((int)(short_side * 0.015f), 15);
    int factor_min_px = mobile ? 20
        : MAX((int)(short_side * 0.014f), 15);
    TTF_Font* meta_font = sdl_story_font_for_height_slot(meta_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    SDL_FRect strip = {
        card.x, card.y,
        mobile ? (focused ? 5.0f : 2.0f)
               : short_side * (focused ? 0.0040f : 0.0018f),
        card.h
    };
    SDL_FRect box;

    SDL_SetRenderDrawColor(g_state.renderer, focused ? 9 : 3,
        focused ? 16 : 7, focused ? 27 : 13, focused ? 250 : 232);
    SDL_RenderFillRect(g_state.renderer, &card);
    SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g, border.b,
        border.a);
    SDL_RenderRect(g_state.renderer, &card);
    SDL_SetRenderDrawColor(g_state.renderer, accent.r, accent.g, accent.b,
        accent.a);
    SDL_RenderFillRect(g_state.renderer, &strip);

    if (mobile)
    {
        box = (SDL_FRect){ card.x + pad, card.y + pad,
            rank_w - pad * 0.5f, card.h - pad * 2.0f };
        (void)sdl_halls_draw_fitted_text(entry->rank,
            focused ? TERM_YELLOW : TERM_SLATE, box, -1,
            meta_min_px, meta_px, SDL_STORY_FONT_SLOT_NARRATIVE);

        box = (SDL_FRect){ text_x,
            card.y + pad * (g_sdl_halls.detailed ? 0.35f : 0.55f),
            text_w - score_w - pad,
            card.h * (g_sdl_halls.detailed ? 0.19f : 0.31f) };
        (void)sdl_halls_draw_fitted_text(entry->name,
            focused ? TERM_YELLOW : entry->attr, box, -1,
            name_min_px, name_px, SDL_STORY_FONT_SLOT_DEFAULT);
        box.x = card.x + card.w - pad - score_w;
        box.w = score_w;
        (void)sdl_halls_draw_fitted_text(entry->score,
            focused ? TERM_YELLOW : TERM_L_WHITE, box, 1,
            name_min_px, name_px, SDL_STORY_FONT_SLOT_DEFAULT);

        box = (SDL_FRect){ text_x,
            card.y + card.h * (g_sdl_halls.detailed ? 0.22f : 0.36f),
            text_w, card.h * (g_sdl_halls.detailed ? 0.16f : 0.42f) };
        (void)sdl_halls_draw_fitted_text(entry->outcome,
            focused ? TERM_WHITE : TERM_L_WHITE, box, -1,
            body_min_px, body_px, SDL_STORY_FONT_SLOT_NARRATIVE);

        if (g_sdl_halls.detailed)
        {
            float honors_w = entry->honors[0]
                ? sdl_halls_clampf(card.w * 0.22f, 110.0f, 250.0f)
                : 0.0f;

            box = (SDL_FRect){ text_x, card.y + card.h * 0.385f,
                text_w - honors_w - (honors_w > 0.0f ? pad : 0.0f),
                card.h * 0.14f };
            sdl_halls_draw_details(meta_font, entry->details, box);
            if (honors_w > 0.0f)
            {
                box.x = card.x + card.w - pad - honors_w;
                box.w = honors_w;
                (void)sdl_halls_draw_fitted_text(entry->honors, TERM_ORANGE,
                    box, 1, meta_min_px, meta_px,
                    SDL_STORY_FONT_SLOT_NARRATIVE);
            }

            sdl_halls_draw_rule(text_x, card.y + card.h * 0.54f, text_w,
                TERM_BLUE, 80);
            box = (SDL_FRect){ text_x, card.y + card.h * 0.55f,
                text_w, card.h * 0.25f };
            sdl_halls_draw_wrapped_text(entry->score_increases,
                prefix(entry->score_increases, "Score increases: none")
                    ? TERM_SLATE : TERM_L_GREEN,
                box, factor_min_px, meta_px);
            box.y = card.y + card.h * 0.805f;
            box.h = card.h * 0.18f;
            sdl_halls_draw_wrapped_text(entry->score_decreases,
                prefix(entry->score_decreases, "Score decreases: none")
                    ? TERM_SLATE : TERM_L_RED,
                box, factor_min_px, meta_px);
        }
        else if (entry->honors[0])
        {
            box = (SDL_FRect){ card.x + card.w - pad - score_w,
                card.y + card.h * 0.66f, score_w, card.h * 0.26f };
            (void)sdl_halls_draw_fitted_text(entry->honors, TERM_ORANGE,
                box, 1, meta_min_px, meta_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
        }
    }
    else
    {
        float row_gap = MAX(short_side * 0.008f, 6.0f);
        float top_h = MIN(card.h * (g_sdl_halls.detailed ? 0.17f : 0.28f),
            (float)name_px * 1.30f);
        float outcome_h = MIN(
            card.h * (g_sdl_halls.detailed ? 0.14f : 0.34f),
            (float)body_px * 1.30f);
        float content_y = card.y + pad;

        box = (SDL_FRect){ card.x + pad, content_y,
            rank_w - pad * 0.5f, top_h };
        (void)sdl_halls_draw_fitted_text(entry->rank,
            focused ? TERM_YELLOW : TERM_SLATE, box, -1,
            meta_min_px, meta_px, SDL_STORY_FONT_SLOT_NARRATIVE);
        box = (SDL_FRect){ text_x, content_y,
            text_w - score_w - pad, top_h };
        (void)sdl_halls_draw_fitted_text(entry->name,
            focused ? TERM_YELLOW : entry->attr, box, -1,
            name_min_px, name_px, SDL_STORY_FONT_SLOT_DEFAULT);
        box.x = card.x + card.w - pad - score_w;
        box.w = score_w;
        (void)sdl_halls_draw_fitted_text(entry->score,
            focused ? TERM_YELLOW : TERM_L_WHITE, box, 1,
            name_min_px, name_px, SDL_STORY_FONT_SLOT_DEFAULT);

        content_y += top_h + row_gap;
        box = (SDL_FRect){ text_x, content_y, text_w, outcome_h };
        if (!g_sdl_halls.detailed && entry->honors[0])
            box.w -= score_w + pad;
        (void)sdl_halls_draw_fitted_text(entry->outcome,
            focused ? TERM_WHITE : TERM_L_WHITE, box, -1,
            body_min_px, body_px, SDL_STORY_FONT_SLOT_NARRATIVE);

        if (g_sdl_halls.detailed)
        {
            float details_h = MIN(card.h * 0.12f,
                (float)meta_px * 1.40f);
            float honors_w = entry->honors[0]
                ? MAX(110.0f, MIN(card.w * 0.22f,
                    short_side * 0.28f))
                : 0.0f;
            float rule_y;
            float factor_top;
            float factor_bottom;
            float factor_gap;
            float factor_h;

            content_y += outcome_h + row_gap;
            box = (SDL_FRect){ text_x, content_y,
                text_w - honors_w - (honors_w > 0.0f ? pad : 0.0f),
                details_h };
            sdl_halls_draw_details(meta_font, entry->details, box);
            if (honors_w > 0.0f)
            {
                box.x = card.x + card.w - pad - honors_w;
                box.w = honors_w;
                (void)sdl_halls_draw_fitted_text(entry->honors, TERM_ORANGE,
                    box, 1, meta_min_px, meta_px,
                    SDL_STORY_FONT_SLOT_NARRATIVE);
            }

            rule_y = content_y + details_h + row_gap * 0.5f;
            sdl_halls_draw_rule(text_x, rule_y, text_w, TERM_BLUE, 80);
            factor_top = rule_y + row_gap;
            factor_bottom = card.y + card.h - pad;
            factor_gap = row_gap * 0.65f;
            factor_h = MAX(1.0f, factor_bottom - factor_top - factor_gap);

            box = (SDL_FRect){ text_x, factor_top, text_w,
                factor_h * 0.55f };
            sdl_halls_draw_wrapped_text(entry->score_increases,
                prefix(entry->score_increases, "Score increases: none")
                    ? TERM_SLATE : TERM_L_GREEN,
                box, factor_min_px, meta_px);
            box.y += box.h + factor_gap;
            box.h = MAX(1.0f, factor_bottom - box.y);
            sdl_halls_draw_wrapped_text(entry->score_decreases,
                prefix(entry->score_decreases, "Score decreases: none")
                    ? TERM_SLATE : TERM_L_RED,
                box, factor_min_px, meta_px);
        }
        else if (entry->honors[0])
        {
            box = (SDL_FRect){ card.x + card.w - pad - score_w,
                content_y, score_w, outcome_h };
            (void)sdl_halls_draw_fitted_text(entry->honors, TERM_ORANGE,
                box, 1, meta_min_px, meta_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
        }
    }

    entry->hit_rect = card;
}

static TTF_Font* sdl_halls_actions_font(float action_w, float inset,
    float h, int min_px, int max_px)
{
    TTF_Font* chosen = NULL;
    int low = MAX(1, min_px);
    int high = MAX(low, max_px);
    float available_w = MAX(1.0f, action_w - inset * 2.0f);

    while (low <= high)
    {
        int px = low + (high - low) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_MENU);
        bool fits = (font != NULL);

        for (int i = 0; fits && i < g_sdl_halls.action_count; i++)
        {
            int text_w = 0;
            int text_h = 0;

            if (!g_sdl_halls.actions[i].enabled)
                continue;
            if (!TTF_GetStringSize(font, g_sdl_halls.actions[i].label, 0,
                    &text_w, &text_h)
                || (float)text_w > available_w + 1.0f
                || (float)text_h > h + 1.0f)
            {
                fits = false;
            }
        }

        if (fits)
        {
            chosen = font;
            low = px + 1;
        }
        else
            high = px - 1;
    }

    return chosen ? chosen : sdl_story_font_for_height_slot(MAX(1, min_px),
        SDL_STORY_FONT_SLOT_MENU);
}

static void sdl_halls_render_actions(const SDL_Rect* canvas, float content_x,
    float content_w, float y, float h)
{
    bool mobile = sdl_halls_mobile_layout();
    float short_side = sdl_halls_short_side(canvas);
    float gap = mobile
        ? sdl_halls_clampf(content_w * 0.010f, 5.0f, 14.0f)
        : MAX(short_side * 0.008f, 7.0f);
    float action_w;
    int max_font_px = mobile
        ? sdl_halls_clampi((int)(h * 0.50f), 24, 48)
        : MAX((int)(short_side * 0.024f), 18);
    int min_font_px = mobile ? 18
        : MAX((int)(short_side * 0.016f), 13);
    TTF_Font* font;
    int enabled_count = 0;
    int i;

    for (i = 0; i < g_sdl_halls.action_count; i++)
        if (g_sdl_halls.actions[i].enabled)
            enabled_count++;
    if (enabled_count <= 0)
        return;

    action_w = (content_w - gap * (float)(enabled_count - 1))
        / (float)enabled_count;
    font = sdl_halls_actions_font(action_w, gap, h, min_font_px,
        max_font_px);
    for (i = 0; i < g_sdl_halls.action_count; i++)
    {
        sdl_halls_action* action = &g_sdl_halls.actions[i];
        bool hovered;
        SDL_Color border;
        SDL_FRect box;

        if (!action->enabled)
            continue;
        hovered = (action->choice == g_sdl_halls.hover_choice);
        action->hit_rect = (SDL_FRect){ content_x, y, action_w, h };
        content_x += action_w + gap;
        border = sdl_halls_color(hovered ? TERM_YELLOW : TERM_BLUE,
            hovered ? 230 : 115);
        SDL_SetRenderDrawColor(g_state.renderer, hovered ? 12 : 3,
            hovered ? 20 : 8, hovered ? 32 : 15, 240);
        SDL_RenderFillRect(g_state.renderer, &action->hit_rect);
        SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
            border.b, border.a);
        SDL_RenderRect(g_state.renderer, &action->hit_rect);
        box = action->hit_rect;
        box.x += gap;
        box.w -= gap * 2.0f;
        (void)sdl_halls_draw_text(font, action->label,
            hovered ? TERM_YELLOW : action->attr, box, 0);
    }
}

void sdl_halls_screen_render(void)
{
    SDL_Rect window;
    SDL_Rect canvas;
    sdl_halls_layout layout;
    bool mobile;
    float short_side;
    float header_top;
    float body_top;
    float visible_body_h;
    float footer_y;
    float card_h = 0.0f;
    int i;

    if (!g_sdl_halls.active || !g_state.window || !g_state.renderer)
        return;

    window = sdl_get_window_pixel_rect();
    canvas = sdl_get_layout_screen_rect();
    if (canvas.w <= 0 || canvas.h <= 0)
        canvas = window;
    if (window.w <= 0 || window.h <= 0 || canvas.w <= 0 || canvas.h <= 0)
        return;

    sdl_halls_clear_hits();
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    sdl_halls_measure_layout(&canvas, &layout);
    mobile = sdl_halls_mobile_layout();
    short_side = sdl_halls_short_side(&canvas);
    header_top = (float)canvas.y + layout.margin_top;
    body_top = layout.body_top;
    visible_body_h = layout.body_h;
    footer_y = (float)(canvas.y + canvas.h) - layout.margin_bottom
        - layout.footer_h;

    if (g_sdl_halls.entry_count > 0)
    {
        int slot_count = sdl_halls_capacity_for_layout(&canvas, &layout,
            g_sdl_halls.detailed);

        if (slot_count < g_sdl_halls.entry_count)
            slot_count = g_sdl_halls.entry_count;
        card_h = (layout.body_h - layout.gap * (float)(slot_count - 1))
            / (float)slot_count;
        if (!mobile)
        {
            float available_h = (float)canvas.h - layout.margin_top
                - layout.margin_bottom;
            float composition_h;

            visible_body_h = card_h * (float)g_sdl_halls.entry_count
                + layout.gap * (float)(g_sdl_halls.entry_count - 1);
            composition_h = layout.header_h + layout.header_body_gap
                + visible_body_h + layout.body_footer_gap + layout.footer_h;
            header_top += MAX(0.0f, (available_h - composition_h) * 0.35f);
            body_top = header_top + layout.header_h
                + layout.header_body_gap;
            footer_y = body_top + visible_body_h + layout.body_footer_gap;
        }
    }
    else if (!mobile)
    {
        float available_h = (float)canvas.h - layout.margin_top
            - layout.margin_bottom;
        float composition_h;

        visible_body_h = MIN(layout.body_h, MAX(short_side * 0.22f, 180.0f));
        composition_h = layout.header_h + layout.header_body_gap
            + visible_body_h + layout.body_footer_gap + layout.footer_h;
        header_top += MAX(0.0f, (available_h - composition_h) * 0.35f);
        body_top = header_top + layout.header_h + layout.header_body_gap;
        footer_y = body_top + visible_body_h + layout.body_footer_gap;
    }

    sdl_halls_render_header(&canvas, layout.content_x, layout.content_w,
        header_top, layout.header_h);

    if (g_sdl_halls.entry_count > 0)
    {
        float y = body_top;

        for (i = 0; i < g_sdl_halls.entry_count; i++)
        {
            SDL_FRect card = { layout.content_x, y, layout.content_w,
                card_h };

            sdl_halls_render_entry(&g_sdl_halls.entries[i], card,
                (int)short_side);
            y += card_h + layout.gap;
        }
    }
    else
    {
        int empty_px = mobile
            ? sdl_halls_clampi((int)((float)canvas.h * 0.033f), 19, 40)
            : MAX((int)(short_side * 0.034f), 19);
        int empty_min_px = mobile ? 19
            : MAX((int)(short_side * 0.020f), 19);
        SDL_FRect box = { layout.content_x, body_top,
            layout.content_w, visible_body_h };

        (void)sdl_halls_draw_fitted_text(
            g_sdl_halls.empty_text[0] ? g_sdl_halls.empty_text
                                      : "No recorded heroes yet.",
            TERM_SLATE, box, 0, empty_min_px, empty_px,
            SDL_STORY_FONT_SLOT_NARRATIVE);
    }

    sdl_halls_render_actions(&canvas, layout.content_x, layout.content_w,
        footer_y, layout.footer_h);
}

static int sdl_halls_hit_at(float x, float y)
{
    int i;

    for (i = 0; i < g_sdl_halls.action_count; i++)
    {
        if (g_sdl_halls.actions[i].enabled
            && sdl_halls_point_in_rect(x, y,
                &g_sdl_halls.actions[i].hit_rect))
        {
            return g_sdl_halls.actions[i].choice;
        }
    }
    for (i = 0; i < g_sdl_halls.entry_count; i++)
    {
        if (sdl_halls_point_in_rect(x, y,
                &g_sdl_halls.entries[i].hit_rect))
        {
            return g_sdl_halls.entries[i].choice;
        }
    }
    return INT_MIN;
}

static bool sdl_halls_pointer_motion(float x, float y)
{
    int choice = sdl_halls_hit_at(x, y);
    bool wake = false;

    if (choice != g_sdl_halls.hover_choice)
    {
        g_sdl_halls.hover_choice = choice;
        g_state.need_present = true;
    }

    if (choice != INT_MIN)
    {
        if (ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER,
                &wake) && wake)
        {
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
    }
    else if (ui_menu_click_clear_hover(&wake) && wake)
    {
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    }
    return true;
}

static bool sdl_halls_pointer_press(float x, float y, int action)
{
    int choice = sdl_halls_hit_at(x, y);

    if (choice == INT_MIN)
        choice = g_sdl_halls.outside_choice;
    if (choice == INT_MIN)
        return true;

    g_sdl_halls.hover_choice = choice;
    if (ui_menu_click_handle_choice_action(choice, action, NULL))
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    g_state.need_present = true;
    return true;
}

static void sdl_halls_touch_press_cancel(void)
{
    g_sdl_halls.touch_press_active = false;
    g_sdl_halls.touch_press_dragged = false;
    g_sdl_halls.touch_press_finger_id = 0;
    g_sdl_halls.touch_press_choice = INT_MIN;
    g_sdl_halls.touch_press_start_x = 0.0f;
    g_sdl_halls.touch_press_start_y = 0.0f;
}

static bool sdl_halls_touch_press_begin(float x, float y,
    SDL_FingerID finger_id)
{
    int choice;

    if (g_sdl_halls.touch_press_active)
        return true;

    choice = sdl_halls_hit_at(x, y);
    if (choice == INT_MIN)
        choice = g_sdl_halls.outside_choice;

    g_sdl_halls.touch_press_active = true;
    g_sdl_halls.touch_press_dragged = false;
    g_sdl_halls.touch_press_finger_id = finger_id;
    g_sdl_halls.touch_press_choice = choice;
    g_sdl_halls.touch_press_start_x = x;
    g_sdl_halls.touch_press_start_y = y;
    g_sdl_halls.hover_choice = choice;
    g_state.need_present = true;
    return true;
}

static bool sdl_halls_touch_press_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    int choice;

    if (!g_sdl_halls.touch_press_active
        || g_sdl_halls.touch_press_finger_id != finger_id)
    {
        return true;
    }

    dx = x - g_sdl_halls.touch_press_start_x;
    dy = y - g_sdl_halls.touch_press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;
    if (dx > sdl_touch_swipe_threshold_px()
        || dy > sdl_touch_swipe_threshold_px())
    {
        g_sdl_halls.touch_press_dragged = true;
    }

    choice = sdl_halls_hit_at(x, y);
    if (choice == INT_MIN)
        choice = g_sdl_halls.outside_choice;
    if (choice != g_sdl_halls.hover_choice)
    {
        g_sdl_halls.hover_choice = choice;
        g_state.need_present = true;
    }
    return true;
}

static bool sdl_halls_touch_press_finish(float x, float y,
    SDL_FingerID finger_id)
{
    bool dragged;
    int pressed_choice;
    int release_choice;

    if (!g_sdl_halls.touch_press_active
        || g_sdl_halls.touch_press_finger_id != finger_id)
    {
        return true;
    }

    dragged = g_sdl_halls.touch_press_dragged;
    pressed_choice = g_sdl_halls.touch_press_choice;
    release_choice = sdl_halls_hit_at(x, y);
    if (release_choice == INT_MIN)
        release_choice = g_sdl_halls.outside_choice;
    sdl_halls_touch_press_cancel();

    if (dragged || release_choice == INT_MIN
        || release_choice != pressed_choice)
    {
        g_sdl_halls.hover_choice = INT_MIN;
        g_state.need_present = true;
        return true;
    }

    return sdl_halls_pointer_press(x, y, UI_MENU_CLICK_PRIMARY);
}

bool sdl_halls_screen_handle_pointer_event(const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev || !g_sdl_halls.active)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which != SDL_TOUCH_MOUSEID)
            return sdl_halls_pointer_motion((float)ev->motion.x,
                (float)ev->motion.y);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return true;
        if (ev->button.button == SDL_BUTTON_LEFT)
            return sdl_halls_pointer_press((float)ev->button.x,
                (float)ev->button.y, UI_MENU_CLICK_PRIMARY);
        if (ev->button.button == SDL_BUTTON_RIGHT)
            return sdl_halls_pointer_press((float)ev->button.x,
                (float)ev->button.y, UI_MENU_CLICK_SECONDARY);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
        if (ev->wheel.y > 0.0f)
            Term_keypress('p');
        else if (ev->wheel.y < 0.0f)
            Term_keypress('n');
        return true;

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        sdl_note_touch_event_device(ev->tfinger.touchID);
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return sdl_halls_touch_press_begin(x, y,
                ev->tfinger.fingerID);
        return true;

    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return sdl_halls_touch_press_motion(x, y,
                ev->tfinger.fingerID);
        return true;

    case SDL_EVENT_FINGER_UP:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return sdl_halls_touch_press_finish(x, y,
                ev->tfinger.fingerID);
        sdl_halls_touch_press_cancel();
        return true;

    case SDL_EVENT_FINGER_CANCELED:
        if (g_sdl_halls.touch_press_active
            && g_sdl_halls.touch_press_finger_id == ev->tfinger.fingerID)
        {
            sdl_halls_touch_press_cancel();
            g_sdl_halls.hover_choice = INT_MIN;
            g_state.need_present = true;
        }
        return true;

    default:
        return false;
    }
}
