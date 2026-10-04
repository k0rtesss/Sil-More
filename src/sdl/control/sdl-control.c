#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "sdl/control/sdl-control.h"
#include "cJSON.h"
#include <math.h>

/* A single local client atomically publishes request.json. The SDL input pump
 * claims it, routes input through the usual handlers, and publishes a reply at
 * the next input wait. No OS input injection, sockets, or rendering threads. */
enum { CONTROL_REQUEST_MAX = 16384, CONTROL_ID_MAX = 64,
    CONTROL_TEXT_MAX = 128, CONTROL_POLL_MS = 25 };

static char control_dir[1024];
static char control_session[64];
static char control_last_id[CONTROL_ID_MAX + 1];
static cJSON* control_request;
static cJSON* control_response;
static bool control_input_applied;
static Uint32 control_event_type;
static SDL_Event control_events[4];
static int control_event_count;
static int control_event_next;
static bool control_dispatch_complete;
static bool control_shutting_down;
static SDL_Surface* control_modal_frame;
static bool control_modal_wait;

static bool control_ui_busy(void)
{
    return sdl_question_menu_blocks_input()
        || sdl_character_sheet_screen_page_turning()
        || sdl_hint_quest_menu_pending_timeout_ms(SDL_GetTicksNS()) >= 0;
}

static bool control_path(char* out, size_t size, const char* name)
{
    return path_build(out, size, control_dir, name);
}

static bool control_write_json(const char* name, const cJSON* json)
{
    char path[1200], temp[1200], temp_name[96];
    char* data = cJSON_PrintUnformatted(json);
    SDL_IOStream* io;
    bool ok;
    if (!data)
        return false;
    strnfmt(temp_name, sizeof(temp_name), "%s.tmp", name);
    if (!control_path(path, sizeof(path), name)
        || !control_path(temp, sizeof(temp), temp_name))
    {
        cJSON_free(data);
        return false;
    }
    io = SDL_IOFromFile(temp, "wb");
    ok = io && SDL_WriteIO(io, data, strlen(data)) == strlen(data);
    if (io && !SDL_CloseIO(io))
        ok = false;
    cJSON_free(data);
    if (ok)
        ok = SDL_RenamePath(temp, path);
    if (!ok) {
        log_warn("Control interface could not publish %s: %s", name, SDL_GetError());
        (void)SDL_RemovePath(temp);
    }
    return ok;
}

typedef enum control_read_status {
    CONTROL_READ_MISSING,
    CONTROL_READ_BUSY,
    CONTROL_READ_INVALID,
    CONTROL_READ_OK
} control_read_status;

static cJSON* control_read_json(const char* name, control_read_status* status)
{
    char path[1200], data[CONTROL_REQUEST_MAX + 1];
    SDL_IOStream* io;
    size_t size;
    *status = CONTROL_READ_MISSING;
    if (!control_path(path, sizeof(path), name)
        || !SDL_GetPathInfo(path, NULL))
        return NULL;
    *status = CONTROL_READ_BUSY;
    io = SDL_IOFromFile(path, "rb");
    if (!io)
        return NULL;
    Sint64 expected = SDL_GetIOSize(io);
    size = SDL_ReadIO(io, data, sizeof(data));
    bool ok = SDL_GetIOStatus(io) != SDL_IO_STATUS_ERROR;
    if (!SDL_CloseIO(io))
        ok = false;
    if (!ok || expected < 0)
        return NULL;
    *status = CONTROL_READ_INVALID;
    if (expected > CONTROL_REQUEST_MAX || size > CONTROL_REQUEST_MAX)
        return NULL;
    if (size < (size_t)expected) {
        *status = CONTROL_READ_BUSY;
        return NULL;
    }
    data[size] = '\0';
    cJSON* json = cJSON_ParseWithLengthOpts(data, size + 1, NULL, true);
    if (json)
        *status = CONTROL_READ_OK;
    return json;
}

static const char* control_string(const cJSON* json, const char* field)
{
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, field);
    return cJSON_IsString(value) ? value->valuestring : NULL;
}

static bool control_number(const cJSON* json, const char* field,
    double min, double max, double* out)
{
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, field);
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble)
        || value->valuedouble < min || value->valuedouble > max)
        return false;
    *out = value->valuedouble;
    return true;
}

static bool control_deadline_elapsed(void)
{
    double deadline;
    SDL_Time now;
    return control_number(control_request, "expires_at_ms", 1, 1e15, &deadline)
        && SDL_GetCurrentTime(&now) && deadline <= (double)now / 1000000.0;
}

static bool control_publish_session(bool running)
{
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "protocol", 1);
    cJSON_AddStringToObject(json, "session", control_session);
    cJSON_AddStringToObject(json, "directory", control_dir);
    cJSON_AddStringToObject(json, "version", VERSION_STRING);
    cJSON_AddBoolToObject(json, "running", running);
    bool ok = control_write_json("session.json", json);
    cJSON_Delete(json);
    return ok;
}

static cJSON* control_state(bool waiting)
{
    cJSON* state = cJSON_CreateObject();
    cJSON* lines = cJSON_AddArrayToObject(state, "terminal");
    cJSON* messages = cJSON_AddArrayToObject(state, "messages");
    int width = 0, height = 0;
    int queued = Term ? (Term->key_head + Term->key_size - Term->key_tail)
        % Term->key_size : 0;
    (void)SDL_GetRenderOutputSize(g_state.renderer, &width, &height);
    bool busy = control_ui_busy();
    cJSON_AddBoolToObject(state, "waiting_for_input", waiting && !queued && !busy);
    cJSON_AddBoolToObject(state, "ui_busy", busy);
    cJSON_AddStringToObject(state, "input_context", control_modal_wait ? "modal" : "terminal");
    cJSON_AddBoolToObject(state, "text_input", inkey_prompt_input_active());
    cJSON_AddBoolToObject(state, "playing", p_ptr && p_ptr->playing);
    cJSON_AddBoolToObject(state, "dungeon", character_dungeon);
    cJSON_AddBoolToObject(state, "modal", character_icky != 0);
    cJSON_AddBoolToObject(state, "tiles", g_state.use_tiles);
    cJSON_AddBoolToObject(state, "focused", (SDL_GetWindowFlags(g_state.window)
        & SDL_WINDOW_INPUT_FOCUS) != 0);
    cJSON_AddStringToObject(state, "video_driver", SDL_GetCurrentVideoDriver());
    cJSON_AddNumberToObject(state, "queued_keys", queued);
    cJSON_AddNumberToObject(state, "width", width);
    cJSON_AddNumberToObject(state, "height", height);
    cJSON_AddNumberToObject(state, "turn", turn);
    cJSON_AddNumberToObject(state, "player_turn", playerturn);
    if (character_dungeon && p_ptr) {
        cJSON* player = cJSON_AddObjectToObject(state, "player");
        cJSON_AddStringToObject(player, "name", op_ptr->full_name);
        cJSON_AddNumberToObject(player, "x", p_ptr->px);
        cJSON_AddNumberToObject(player, "y", p_ptr->py);
        cJSON_AddNumberToObject(player, "depth", p_ptr->depth);
        cJSON_AddNumberToObject(player, "hp", p_ptr->chp);
        cJSON_AddNumberToObject(player, "max_hp", p_ptr->mhp);
    }
    /* Terminal text is useful for prompts. Graphical tiles are spaces; the PNG
     * is the authoritative visual observation, including native SDL overlays. */
    if (term_screen && term_screen->scr) {
        char* line = SDL_malloc((size_t)term_screen->wid + 1);
        if (line) {
            for (int y = 0; y < term_screen->hgt; y++) {
                for (int x = 0; x < term_screen->wid; x++) {
                    unsigned char c = term_screen->scr->c[y][x];
                    line[x] = c >= 32 && c <= 126 ? (char)c : ' ';
                }
                line[term_screen->wid] = '\0';
                cJSON_AddItemToArray(lines, cJSON_CreateString(line));
            }
            SDL_free(line);
        }
    }
    int count = message_num();
    if (count > 20)
        count = 20;
    for (int i = count - 1; i >= 0; i--)
        cJSON_AddItemToArray(messages, cJSON_CreateString(message_str(i)));
    return state;
}

static bool control_capture(void)
{
    char path[1200], temp[1200];
    SDL_Texture* target = SDL_GetRenderTarget(g_state.renderer);
    SDL_Surface* surface = NULL;
    bool owned = false;
    bool ok = false;
    if (!control_path(path, sizeof(path), "screenshot.png")
        || !control_path(temp, sizeof(temp), "screenshot.png.tmp"))
        return false;
    /* Compose afresh: reading the backbuffer after RenderPresent is undefined
     * on several drivers. This captures the game even when another app covers
     * its window, without any desktop screenshot or focus change. */
    if (control_modal_wait) {
        surface = control_modal_frame;
    } else if (!g_sdl_present_suppressed && sdl_render_current_window_frame()) {
        surface = SDL_RenderReadPixels(g_state.renderer, NULL);
        owned = true;
    }
    if (surface) {
        ok = IMG_SavePNG(surface, temp) && SDL_RenamePath(temp, path);
        if (owned)
            SDL_DestroySurface(surface);
    }
    (void)SDL_SetRenderTarget(g_state.renderer, target);
    if (!ok) {
        log_warn("Control screenshot failed: %s", SDL_GetError());
        (void)SDL_RemovePath(temp);
    }
    return ok;
}

static void control_finish_reply(void)
{
    if (!control_response || !control_write_json("response.json", control_response))
        return;
    cJSON_Delete(control_response);
    control_response = NULL;
    cJSON_Delete(control_request);
    control_request = NULL;
    control_input_applied = false;
    control_event_count = control_event_next = 0;
    control_dispatch_complete = false;
}

static void control_reply(const char* error, bool waiting)
{
    const char* id = control_string(control_request, "id");
    const char* op = control_string(control_request, "op");
    const cJSON* capture = cJSON_GetObjectItemCaseSensitive(control_request, "capture");
    bool screenshot = !error && !control_shutting_down && (!op || strcmp(op, "status") != 0)
        && !cJSON_IsFalse(capture);
    cJSON* reply = cJSON_CreateObject();
    if (screenshot && !control_capture())
        error = "Screenshot capture failed; check the game log";
    cJSON_AddNumberToObject(reply, "protocol", 1);
    cJSON_AddStringToObject(reply, "session", control_session);
    cJSON_AddStringToObject(reply, "id", id ? id : "");
    cJSON_AddBoolToObject(reply, "ok", error == NULL);
    cJSON_AddBoolToObject(reply, "input_applied", control_input_applied && control_event_next > 0);
    cJSON_AddBoolToObject(reply, "closed", control_shutting_down);
    if (error)
        cJSON_AddStringToObject(reply, "error", error);
    else if (screenshot)
        cJSON_AddStringToObject(reply, "screenshot", "screenshot.png");
    cJSON_AddItemToObject(reply, "state", control_state(waiting));
    if (id && strlen(id) <= CONTROL_ID_MAX)
        SDL_strlcpy(control_last_id, id, sizeof(control_last_id));
    /* Windows readers can momentarily prevent a rename. Retain the completed
     * response and retry publication, without recapturing or replaying input. */
    control_response = reply;
    control_finish_reply();
}

static const char* control_validate(void)
{
    const char* id = control_string(control_request, "id");
    const char* session = control_string(control_request, "session");
    const char* op = control_string(control_request, "op");
    double protocol, expires;
    SDL_Time now;
    const cJSON* capture = cJSON_GetObjectItemCaseSensitive(control_request, "capture");
    if (!cJSON_IsObject(control_request))
        return "Request must be a JSON object of at most 16384 bytes";
    if (!control_number(control_request, "protocol", 1, 1, &protocol))
        return "Unsupported protocol; expected 1";
    if (!id || !id[0] || strlen(id) > CONTROL_ID_MAX)
        return "id must be a nonempty string of at most 64 bytes";
    if (!session || strcmp(session, control_session) != 0)
        return "Session mismatch; read session.json for the current game";
    if (strcmp(id, control_last_id) == 0)
        return "Duplicate request id; input was not replayed";
    if (!op || (strcmp(op, "observe") && strcmp(op, "status")
        && strcmp(op, "key") && strcmp(op, "text") && strcmp(op, "click")))
        return "op must be observe, status, key, text, or click";
    if (capture && !cJSON_IsBool(capture))
        return "capture must be a boolean";
    if (!control_number(control_request, "expires_at_ms", 1, 1e15, &expires)
        || !SDL_GetCurrentTime(&now))
        return "A valid expires_at_ms deadline is required";
    double now_ms = (double)now / 1000000.0;
    if (expires <= now_ms)
        return "Request expired; input was not applied";
    if (expires > now_ms + 60000)
        return "Request deadline must be within 60 seconds";
    return NULL;
}

static bool control_modifiers(SDL_Keymod* mod)
{
    const cJSON* values = cJSON_GetObjectItemCaseSensitive(control_request, "modifiers");
    const cJSON* value;
    *mod = SDL_KMOD_NONE;
    if (!values)
        return true;
    if (!cJSON_IsArray(values) || cJSON_GetArraySize(values) > 3)
        return false;
    cJSON_ArrayForEach(value, values) {
        if (!cJSON_IsString(value))
            return false;
        if (strcmp(value->valuestring, "ctrl") == 0)
            *mod |= SDL_KMOD_CTRL;
        else if (strcmp(value->valuestring, "shift") == 0)
            *mod |= SDL_KMOD_SHIFT;
        else if (strcmp(value->valuestring, "alt") == 0)
            *mod |= SDL_KMOD_ALT;
        else
            return false;
    }
    return true;
}

static const char* control_key(void)
{
    const char* name = control_string(control_request, "key");
    SDL_Event event = {0};
    SDL_Keymod mod;
    SDL_Keycode key = SDLK_UNKNOWN;
    static char text[2]; /* Lives until the queued TEXT_INPUT is consumed. */
    SDL_zero(event);
    if (!name || !name[0] || strlen(name) > 64 || !control_modifiers(&mod))
        return "Provide key and optional modifiers: ctrl, shift, alt";
    if (name[1] == '\0' && (unsigned char)name[0] >= 32
        && (unsigned char)name[0] <= 126)
    {
        key = (unsigned char)name[0];
        if (key >= 'A' && key <= 'Z') {
            key = SDL_tolower(key);
            mod |= SDL_KMOD_SHIFT;
        } else {
            /* Match the frontend's physical Shift mapping. In particular,
             * '>' must be Shift+Period rather than an unmodified Period
             * scancode that a movement preset could interpret as Wait. */
            const char* unshifted = "1234567890-=[]\\;',./`";
            const char* shifted = "!@#$%^&*()_+{}|:\"<>?~";
            const char* match = strchr(shifted, (char)key);
            if (match) {
                key = (unsigned char)unshifted[match - shifted];
                mod |= SDL_KMOD_SHIFT;
            }
        }
    } else {
        key = SDL_GetKeyFromName(name);
        if (strcmp(name, "Enter") == 0)
            key = SDLK_RETURN;
        if (strcmp(name, "Space") == 0)
            key = SDLK_SPACE;
    }
    if (key == SDLK_UNKNOWN)
        return "Unknown SDL key name";
    /* Leave enough space for a complete macro, never enqueue a partial one. */
    if (!Term || (Term->key_head + Term->key_size - Term->key_tail)
        % Term->key_size > Term->key_size - 32)
        return "Terminal input queue is full";
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(g_state.window);
    event.key.timestamp = SDL_GetTicksNS();
    event.key.key = key;
    event.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    event.key.mod = mod;
    event.key.down = true;
    event.key.repeat = false;
    control_events[control_event_count++] = event;
    /* Word-entry prompts ignore printable KEY_DOWN in favor of TEXT_INPUT.
     * Normal menus/gameplay get just KEY_DOWN, so nothing is duplicated. */
    if (inkey_prompt_input_active() && SDL_TextInputActive(g_state.window)
        && !(mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) && key >= 32 && key <= 126)
    {
        text[0] = (char)key;
        if (mod & SDL_KMOD_SHIFT) {
            const char* unshifted = "1234567890-=[]\\;',./`";
            const char* shifted = "!@#$%^&*()_+{}|:\"<>?~";
            const char* match = strchr(unshifted, (char)key);
            text[0] = match ? shifted[match - unshifted] : (char)SDL_toupper(key);
        }
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = SDL_GetWindowID(g_state.window);
        event.text.text = text;
        control_events[control_event_count++] = event;
    }
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(g_state.window);
    event.key.timestamp = SDL_GetTicksNS();
    event.key.key = key;
    event.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    event.key.mod = mod;
    event.key.down = false;
    event.key.repeat = false;
    control_events[control_event_count++] = event;
    return NULL;
}

static const char* control_text(void)
{
    const char* text = control_string(control_request, "text");
    SDL_Event event = {0};
    SDL_zero(event);
    if (!text || !text[0] || strlen(text) > CONTROL_TEXT_MAX)
        return "text must contain 1 to 128 UTF-8 bytes";
    if (!inkey_prompt_input_active())
        return "text is only accepted in a word-entry prompt; use key for commands";
    int queued = (Term->key_head + Term->key_size - Term->key_tail) % Term->key_size;
    if (strlen(text) > (size_t)(Term->key_size - queued - 1))
        return "Terminal input queue cannot hold the text";
    for (const unsigned char* p = (const unsigned char*)text; *p; p++)
        if (*p < 32 || *p == 127)
            return "text cannot contain control characters; send Enter separately";
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = SDL_GetWindowID(g_state.window);
    event.text.timestamp = SDL_GetTicksNS();
    event.text.text = text;
    control_events[control_event_count++] = event;
    return NULL;
}

static const char* control_click(void)
{
    double x, y;
    int width, height;
    float wx, wy;
    const char* button = control_string(control_request, "button");
    const char* error = "Click must be inside the screenshot, with left or right button";
    Uint8 code = SDL_BUTTON_LEFT;
    SDL_Event event = {0};
    SDL_zero(event);
    if (!config.mouse_enabled)
        return "Mouse input is disabled in the game's input settings";
    if (!SDL_GetRenderOutputSize(g_state.renderer, &width, &height)
        || !control_number(control_request, "x", 0, width - 1, &x)
        || !control_number(control_request, "y", 0, height - 1, &y))
        return error;
    if (button && strcmp(button, "right") == 0)
        code = SDL_BUTTON_RIGHT;
    else if (button && strcmp(button, "left") != 0)
        return error;
    if (!SDL_RenderCoordinatesToWindow(g_state.renderer, (float)x, (float)y, &wx, &wy))
        return "Cannot convert screenshot coordinates to window coordinates";
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(g_state.window);
    event.motion.timestamp = SDL_GetTicksNS();
    event.motion.x = wx;
    event.motion.y = wy;
    event.motion.xrel = event.motion.yrel = 0.0f;
    control_events[control_event_count++] = event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(g_state.window);
    event.button.timestamp = SDL_GetTicksNS();
    event.button.button = code;
    event.button.down = true;
    event.button.clicks = 1;
    event.button.x = wx;
    event.button.y = wy;
    control_events[control_event_count++] = event;
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    control_events[control_event_count++] = event;
    return NULL;
}

void sdl_control_init(int argc, char** argv)
{
    const char* dir = SDL_getenv("SIL_MORE_CONTROL_DIR");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--control-dir") == 0) {
            if (++i >= argc || !argv[i][0])
                quit("--control-dir requires an absolute directory path");
            dir = argv[i];
        }
    }
    if (!dir || !dir[0])
        return;
    /* main() changes cwd to the installation. Require an absolute path so the
     * game and client always agree on the mailbox location. */
    if (!path_parse(control_dir, sizeof(control_dir), dir)
        || strlen(control_dir) > sizeof(control_dir) - 64)
        quit("Invalid local control directory path");
    if (control_dir[0] != '/' && control_dir[0] != '\\'
        && !(SDL_isalpha(control_dir[0]) && control_dir[1] == ':'
            && (control_dir[2] == '/' || control_dir[2] == '\\')))
        quit("--control-dir must be an absolute directory path");
    if (!SDL_CreateDirectory(control_dir))
        quit("Cannot create the local control directory");
    control_read_status status;
    cJSON* previous = control_read_json("session.json", &status);
    if (status != CONTROL_READ_MISSING
        && (!previous || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(previous, "running")))) {
        cJSON_Delete(previous);
        quit("Control directory is already in use; choose a fresh directory after a crash");
    }
    cJSON_Delete(previous);
    SDL_Time now = 0;
    (void)SDL_GetCurrentTime(&now);
    SDL_snprintf(control_session, sizeof(control_session), "%016" SDL_PRIx64 "-%016" SDL_PRIx64,
        (Uint64)now, SDL_GetTicksNS());
    control_last_id[0] = '\0';
    control_event_type = SDL_RegisterEvents(1);
    if (!control_event_type)
        quit("Cannot register the local control event");
    if (!control_publish_session(true))
        quit("Cannot publish the local control session");
    log_info("Local control interface enabled in %s (session %s)", control_dir, control_session);
}

void sdl_control_shutdown(void)
{
    if (!control_session[0])
        return;
    control_shutting_down = true;
    if (control_request)
    {
        if (!control_response)
            control_reply(control_input_applied && control_event_next > 0
                ? NULL : "Game is shutting down", false);
        for (int i = 0; control_response && i < 10; i++) {
            SDL_Delay(10);
            control_finish_reply();
        }
        cJSON_Delete(control_response);
        cJSON_Delete(control_request);
        control_response = control_request = NULL;
    }
    control_publish_session(false);
    control_session[0] = '\0';
    control_dir[0] = '\0';
    control_shutting_down = false;
    SDL_DestroySurface(control_modal_frame);
    control_modal_frame = NULL;
}

bool sdl_control_present_modal(SDL_Renderer* renderer)
{
    /* Standalone tutorial/coach loops draw extra content outside the ordinary
     * compositor. Keep their complete frame before Present invalidates it. */
    if (control_session[0]) {
        SDL_DestroySurface(control_modal_frame);
        control_modal_frame = SDL_RenderReadPixels(renderer, NULL);
    }
    return SDL_RenderPresent(renderer);
}

bool sdl_control_wait_modal_event(SDL_Event* event, Uint64 accept_after_ns)
{
    if (!control_session[0])
        return SDL_WaitEvent(event);
    for (;;) {
        control_modal_wait = true;
        (void)sdl_control_poll(SDL_GetTicksNS() >= accept_after_ns);
        control_modal_wait = false;
        if (!SDL_WaitEventTimeout(event, CONTROL_POLL_MS))
            continue;
        if (sdl_control_handle_event(event))
            continue;
        return true;
    }
}

bool sdl_control_handle_event(const SDL_Event* event)
{
    if (!control_event_type || event->type != control_event_type)
        return false;
    control_dispatch_complete = true;
    return true;
}

static void control_enqueue_events(void)
{
    int remaining = control_event_count - control_event_next;
    if (remaining > 0) {
        /* Keep any not-yet-enqueued suffix for the next pump if SDL's queue is
         * temporarily full. Never resend the already queued prefix. */
        int added = SDL_PeepEvents(control_events + control_event_next,
            remaining, SDL_ADDEVENT, 0, 0);
        if (added > 0)
            control_event_next += added;
    }
}

int sdl_control_wait_timeout(int timeout_ms)
{
    if (control_session[0] && (timeout_ms < 0 || timeout_ms > CONTROL_POLL_MS))
        return CONTROL_POLL_MS;
    return timeout_ms;
}

bool sdl_control_enabled(void)
{
    return control_session[0] != '\0';
}

bool sdl_control_poll(bool waiting)
{
    const char* error;
    const char* op;
    if (!control_session[0])
        return false;
    if (control_response) {
        control_finish_reply();
        return false;
    }
    if (control_request && control_input_applied) {
        control_enqueue_events();
        /* Only report completion after the command has reached another input
         * wait. The reply can be a direction/confirmation prompt, not just the
         * dungeon command prompt. A mere queue acknowledgement is insufficient. */
        if (control_dispatch_complete && waiting && Term && Term->key_head == Term->key_tail
            && !control_ui_busy())
            control_reply(NULL, true);
        else if (control_dispatch_complete && control_deadline_elapsed())
            control_reply(NULL, false);
        return false;
    }
    if (!control_request) {
        control_read_status status;
        char path[1200];
        control_request = control_read_json("request.json", &status);
        /* A sharing violation is not malformed JSON. Leave the request in
         * place until it is readable; claiming it now would lose valid input. */
        if (status == CONTROL_READ_MISSING || status == CONTROL_READ_BUSY)
            return false;
        /* Claim before any side effects; a flush or subsequent poll cannot
         * replay this request. Only fixed filenames inside the selected dir. */
        if (!control_path(path, sizeof(path), "request.json") || !SDL_RemovePath(path)) {
            cJSON_Delete(control_request);
            control_request = NULL;
            return false;
        }
        if (!control_request) {
            control_request = cJSON_CreateObject();
            control_reply("Invalid JSON or request exceeds 16384 bytes", waiting);
            return false;
        }
    }
    error = control_validate();
    if (error) {
        control_reply(error, waiting);
        return false;
    }
    op = control_string(control_request, "op");
    if (strcmp(op, "observe") == 0 || strcmp(op, "status") == 0) {
        control_reply(NULL, waiting);
        return false;
    }
    /* Inputs wait for the ordinary blocking input pump. Do not inject keys
     * into intro fades, delays, flushes, or a command still being resolved. */
    if (!waiting || control_ui_busy())
        return false;
    if (strcmp(op, "key") == 0)
        error = control_key();
    else if (strcmp(op, "text") == 0)
        error = control_text();
    else
        error = control_click();
    if (error) {
        control_reply(error, waiting);
        return false;
    }
    control_input_applied = true;
    /* A marker behind the input distinguishes delivery from completion. SDL
     * event handlers may enter nested inkey loops: queue releases and this
     * marker ahead of time so those loops can consume them without recursively
     * dispatching the same request. Its text stays alive until the reply. */
    SDL_Event marker = {0};
    SDL_zero(marker);
    marker.type = control_event_type;
    control_events[control_event_count++] = marker;
    control_enqueue_events();
    g_state.need_present = true;
    return true;
}
