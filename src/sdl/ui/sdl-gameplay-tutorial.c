#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "tutorial/tutorial.h"
#include "support/input.h"
#include "ui/menu-click.h"
#include "ui/command-reference.h"

/* This owner lives in the normal SDL event/present loop: it never runs a
 * nested renderer or changes the cost of the command demonstrated by a card. */
static unsigned int tutorial_seen_revision;
static bool tutorial_was_active;
static Uint64 tutorial_input_barrier;
static bool tutorial_blocked_keys[SDL_SCANCODE_COUNT];
static bool tutorial_blocked_buttons[SDL_GAMEPAD_BUTTON_COUNT];
static bool tutorial_blocked_axes[SDL_GAMEPAD_AXIS_COUNT];
static SDL_MouseButtonFlags tutorial_blocked_mouse;
static SDL_FingerID tutorial_blocked_fingers[32];
static int tutorial_blocked_finger_count;
static int tutorial_focus;
static int tutorial_scroll;
static int tutorial_max_scroll;
static SDL_FRect tutorial_card;
static SDL_FRect tutorial_buttons[3];
static bool tutorial_pointer_gameplay;
static SDL_FingerID tutorial_pointer_finger;
static bool tutorial_pointer_mouse;
static int tutorial_pressed_button = -1;
static bool tutorial_scrolling;
static float tutorial_scroll_y;
static bool tutorial_reading;
static bool tutorial_force_sync;
static unsigned int tutorial_input_epoch;
static bool tutorial_menu_preview_available;
static bool tutorial_menu_preview_shown;
static char tutorial_menu_preview_control[64];
enum tutorial_input_kind { TUTORIAL_INPUT_UNKNOWN, TUTORIAL_INPUT_KEYBOARD,
    TUTORIAL_INPUT_MOUSE, TUTORIAL_INPUT_TOUCH, TUTORIAL_INPUT_CONTROLLER };
static enum tutorial_input_kind tutorial_last_input;

void sdl_gameplay_tutorial_set_menu_preview(bool available, bool shown)
{
    if (available!=tutorial_menu_preview_available || shown!=tutorial_menu_preview_shown)
        g_state.need_present=true;
    tutorial_menu_preview_available=available;
    tutorial_menu_preview_shown=shown;
    if (!available) tutorial_menu_preview_control[0]='\0';
}

void sdl_gameplay_tutorial_set_menu_preview_control(const char *label)
{
    const char *value=label?label:"";
    if (strcmp(value,tutorial_menu_preview_control)) g_state.need_present=true;
    SDL_strlcpy(tutorial_menu_preview_control,value,sizeof(tutorial_menu_preview_control));
}

static enum tutorial_input_kind tutorial_input_kind(void)
{
    if (config.input_ui_mode==SDL_INPUT_UI_MODE_CONTROLLER) return TUTORIAL_INPUT_CONTROLLER;
    if (tutorial_last_input!=TUTORIAL_INPUT_UNKNOWN
        && !(config.input_ui_mode==SDL_INPUT_UI_MODE_PLATFORM
            && tutorial_last_input==TUTORIAL_INPUT_CONTROLLER)) return tutorial_last_input;
    if (sdl_touch_only_device_active()) return TUTORIAL_INPUT_TOUCH;
    if (config.input_ui_mode!=SDL_INPUT_UI_MODE_PLATFORM && steamdeck_controls_active())
        return TUTORIAL_INPUT_CONTROLLER;
    return TUTORIAL_INPUT_KEYBOARD;
}

static void tutorial_note_input(const SDL_Event *event)
{
    enum tutorial_input_kind kind=tutorial_last_input;
    if (event->common.timestamp<tutorial_input_barrier) return;
    if (event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat)
        kind=TUTORIAL_INPUT_KEYBOARD;
    else if (event->type==SDL_EVENT_FINGER_DOWN) kind=TUTORIAL_INPUT_TOUCH;
    else if ((event->type==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.which!=SDL_TOUCH_MOUSEID)
        || (event->type==SDL_EVENT_MOUSE_WHEEL && event->wheel.which!=SDL_TOUCH_MOUSEID)
        || (event->type==SDL_EVENT_MOUSE_MOTION && event->motion.which!=SDL_TOUCH_MOUSEID
            && (event->motion.xrel || event->motion.yrel))) kind=TUTORIAL_INPUT_MOUSE;
    else if (event->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN
        || (event->type==SDL_EVENT_GAMEPAD_AXIS_MOTION
            && abs(event->gaxis.value)>MAX(config.gamepad_deadzone,4000))) kind=TUTORIAL_INPUT_CONTROLLER;
    if (kind!=tutorial_last_input) { tutorial_last_input=kind; g_state.need_present=true; }
}

static bool tutorial_menu_owns_input(void)
{
    return character_icky || inkey_prompt_input_active()
        || g_touch_pane_yes_no_prompt_active
        || g_touch_pane_reset_confirm_active
        || sdl_question_menu_captures_pointer()
        || sdl_hint_quest_menu_active() || g_main_menu_overlay_active
        || g_player_action_menu.active || g_player_exchange_target.active
        || g_minimap.active || g_unified_look_active;
}

void sdl_gameplay_tutorial_sync(void)
{
    unsigned int revision = tutorial_revision();
    bool active = tutorial_is_active();
    int count = 0;
    const bool *keys;
    if (!tutorial_force_sync && revision == tutorial_seen_revision && active == tutorial_was_active)
        return;
    tutorial_force_sync=false;
    tutorial_seen_revision = revision;
    if (!active && !tutorial_was_active)
        return;
    tutorial_was_active = active;
    tutorial_input_barrier = SDL_GetTicksNS();
    ++tutorial_input_epoch;
    tutorial_focus = 0;
    tutorial_scroll = tutorial_max_scroll = 0;
    tutorial_pressed_button = -1;
    tutorial_pointer_gameplay = false;
    tutorial_scrolling = false;
    tutorial_reading = false;
    keys = SDL_GetKeyboardState(&count);
    for (int i = 0; i < SDL_SCANCODE_COUNT; ++i)
        tutorial_blocked_keys[i] = i < count && keys[i];
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
        tutorial_blocked_buttons[i] = false;
        for (int p = 0; p < g_gamepad_state.pad_count; ++p)
            if (SDL_GetGamepadButton(g_gamepad_state.pads[p].pad,
                    (SDL_GamepadButton)i))
                tutorial_blocked_buttons[i] = true;
    }
    for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
        tutorial_blocked_axes[i] = false;
        for (int p = 0; p < g_gamepad_state.pad_count; ++p)
            if (abs(SDL_GetGamepadAxis(g_gamepad_state.pads[p].pad,
                    (SDL_GamepadAxis)i)) > MAX(config.gamepad_deadzone, 4000))
                tutorial_blocked_axes[i] = true;
    }
    tutorial_blocked_mouse = SDL_GetMouseState(NULL, NULL);
    tutorial_blocked_finger_count = 0;
    {
        int device_count = 0;
        SDL_TouchID *devices = SDL_GetTouchDevices(&device_count);
        for (int d = 0; d < device_count; ++d) {
            int finger_count = 0;
            SDL_Finger **fingers = SDL_GetTouchFingers(devices[d], &finger_count);
            for (int f = 0; f < finger_count && tutorial_blocked_finger_count < 32; ++f)
                tutorial_blocked_fingers[tutorial_blocked_finger_count++] = fingers[f]->id;
            SDL_free(fingers);
        }
        SDL_free(devices);
    }
    /* Do not SDL_FlushEvents(): releases, quit, resize and lifecycle events
     * must still be handled. Discard old input by timestamp below instead. */
    /* Term_flush() also calls TERM_XTRA_FLUSH and recursively dispatches SDL
     * events. Clear only byte queues here: the outer event loop must retain
     * ownership of release/lifecycle events and present the new card first. */
    if (Term) Term->key_head=Term->key_tail=0;
    if (term_screen) term_screen->key_head=term_screen->key_tail=0;
    inkey_next_set(NULL);
    (void)ui_menu_click_take_action(NULL, NULL);
    sdl_gamepad_reset_movement_controls();
    sdl_gamepad_clear_pending_confirm();
    sdl_gamepad_clear_pending_shoulder();
    g_gamepad_state.left_shoulder_down = false;
    g_gamepad_state.right_shoulder_down = false;
    g_gamepad_state.left_trigger_down = false;
    g_gamepad_state.right_trigger_down = false;
    sdl_touch_cancel_all_inputs();
    sdl_touch_thumb_cancel_press();
    sdl_menu_touch_cancel();
    sdl_menu_scroll_cancel();
    sdl_screen_back_gesture_cancel_touch_inputs();
    sdl_screen_back_touch_cancel();
    g_screen_back_right_button_pending = false;
    sdl_main_menu_button_cancel_input();
    sdl_mouse_path_cancel();
    sdl_pointer_aim_cancel_touch_press();
    sdl_pointer_attack_clear_hover();
    sdl_player_action_menu_cancel_press();
    sdl_log_pane_menu_clear_long_press();
    sdl_side_pane_menu_clear_long_press();
    g_state.need_present = true;
}

unsigned int sdl_gameplay_tutorial_input_epoch(void)
{
    return tutorial_input_epoch;
}

static void tutorial_set_reading(bool reading)
{
    int scroll=tutorial_scroll;
    if (reading==tutorial_reading) return;
    tutorial_force_sync=true;
    sdl_gameplay_tutorial_sync();
    tutorial_reading=reading;
    tutorial_scroll=scroll;
}

static int tutorial_command(const tutorial_view *view)
{
    const char *a = view->action;
    if (!strcmp(a, "open-menu")) {
        const char *menu=view->action_subject;
        if (!strcmp(menu,"abilities")) return 'y';
        if (!strcmp(menu,"skills")) return 'H';
        if (!strcmp(menu,"equipment")) return 'e';
        if (!strcmp(menu,"inventory")) return 'i';
        if (!strcmp(menu,"character")) return 'h';
        if (!strcmp(menu,"songs")) return 's';
        if (!strcmp(menu,"supplies")) return 'j';
        if (!strcmp(menu,"smithing")) return '0';
        if (!strcmp(menu,"look")) return 'l';
        if (!strcmp(menu,"settings")) return 'O';
        return 'm';
    }
    if (strstr(a, "learn-ability")) return 'y';
    if (strstr(a, "song")) return 's';
    if (strstr(a, "examine")) return 'x';
    if (strstr(a, "stealth")) return 'S';
    if (strstr(a, "change-active")) return '\t';
    if (strstr(a, "ready") || strstr(a, "equip")) return 'i';
    if (strstr(a, "use-item")) return 'u';
    if (strstr(a, "ranged") || strstr(a, "fire")) return 'f';
    if (strstr(a, "throw")) return 't';
    if (strstr(a, "pickup")) return 'g';
    if (strstr(a, "rest")) return 'Z';
    if (strstr(a, "open-menu")) return 'm';
    return 0; /* Movement/attacks need the player's chosen map direction. */
}

typedef struct tutorial_controls {
    int command;
    bool native_command;
    bool primary;
    char label[3][80];
    char shortcut[3][64];
    char action_hint[192];
    char controls_hint[192];
    char read_hint[48];
} tutorial_controls;

static const char *tutorial_command_label(int command)
{
    switch (command) {
    case 'i': return "Open inventory";
    case 'e': return "Open equipment";
    case 'j': return "Open supplies";
    case 'y': return "Open abilities";
    case 'H': return "Open skills";
    case 'h': return "Open character";
    case 'm': return "Open main menu";
    case '0': return "Open smithing";
    case 'O': return "Open settings";
    default: break;
    }
    for (int i=0;i<COMMAND_PRIMARY_KEYBIND_COUNT;++i)
        if (command_primary_keybinds[i].key_code==command) return command_primary_keybinds[i].key_name;
    for (int i=0;i<COMMAND_SECONDARY_KEYBIND_COUNT;++i)
        if (command_secondary_keybinds[i].key_code==command) return command_secondary_keybinds[i].key_name;
    return "Choose an action";
}

static void tutorial_build_controls(const tutorial_view *view, tutorial_controls *out,
    bool scrollable)
{
    enum tutorial_input_kind input=tutorial_input_kind();
    bool native=tutorial_menu_owns_input() && !view->can_continue && !tutorial_reading;
    const char *confirm=sdl_gamepad_button_short_label(SDL_GAMEPAD_BUTTON_SOUTH);
    const char *back=sdl_gamepad_button_short_label(SDL_GAMEPAD_BUTTON_EAST);
    const char *read=sdl_gamepad_button_short_label(SDL_GAMEPAD_BUTTON_BACK);
    const char *menu=sdl_gamepad_button_short_label(SDL_GAMEPAD_BUTTON_START);
    char binding[96]="";
    memset(out,0,sizeof(*out));
    SDL_strlcpy(out->label[1],"Skip tutorial",sizeof(out->label[1]));
    SDL_strlcpy(out->label[2],tutorial_mode_name(get_sdl_gameplay_tutorial_mode()),sizeof(out->label[2]));
    if (view->can_continue || tutorial_reading) {
        out->primary=true;
        SDL_strlcpy(out->label[0],view->can_continue?"Continue":"Resume action",sizeof(out->label[0]));
    } else if (native) {
        if (strstr(view->action,"examine") && tutorial_menu_preview_available) {
            if (!tutorial_menu_preview_shown) {
                out->primary=true; out->native_command=true; out->command='x';
                SDL_strlcpy(out->label[0],"Preview item",sizeof(out->label[0]));
                if (input==TUTORIAL_INPUT_KEYBOARD) SDL_strlcpy(binding,"x",sizeof(binding));
                else if (input==TUTORIAL_INPUT_CONTROLLER)
                    SDL_strlcpy(binding,tutorial_menu_preview_control,sizeof(binding));
                strnfmt(out->action_hint,sizeof(out->action_hint),"Preview the selected item%s%s",
                    binding[0]?": ":"",binding);
            } else SDL_strlcpy(out->action_hint,"The selected item's preview is open.",sizeof(out->action_hint));
        } else if (strstr(view->action,"ready"))
            SDL_strlcpy(out->action_hint,"Select the item, then choose Ready.",sizeof(out->action_hint));
        else if (strstr(view->action,"equip"))
            SDL_strlcpy(out->action_hint,"Select the equipment you want to wear or wield.",sizeof(out->action_hint));
        else if (strstr(view->action,"examine"))
            SDL_strlcpy(out->action_hint,"Select an item to preview.",sizeof(out->action_hint));
        else if (strstr(view->action,"use-item"))
            SDL_strlcpy(out->action_hint,"Select the required item and choose Use.",sizeof(out->action_hint));
        else SDL_strlcpy(out->action_hint,"Make the requested selection in the open menu.",sizeof(out->action_hint));
    } else {
        out->command=tutorial_command(view); out->primary=out->command!=0;
        if (out->primary) {
            SDL_strlcpy(out->label[0],tutorial_command_label(out->command),sizeof(out->label[0]));
            if (input==TUTORIAL_INPUT_KEYBOARD)
                help_describe_command_bindings(out->command,binding,sizeof(binding));
            else if (input==TUTORIAL_INPUT_CONTROLLER)
                sdl_gamepad_action_binding_short_label(out->command,binding,sizeof(binding));
            strnfmt(out->action_hint,sizeof(out->action_hint),"%s%s%s",out->label[0],binding[0]?": ":"",binding);
        } else {
            const char *verb=strstr(view->action,"attack")?"Attack":"Move";
            strnfmt(out->action_hint,sizeof(out->action_hint),"%s%s",verb,
                input==TUTORIAL_INPUT_CONTROLLER?" with your movement controls."
                :input==TUTORIAL_INPUT_TOUCH?" by tapping the target square."
                :input==TUTORIAL_INPUT_MOUSE?" by clicking the target square."
                :" with your movement keys.");
        }
    }
    if (input==TUTORIAL_INPUT_KEYBOARD) {
        if (out->native_command) SDL_strlcpy(out->shortcut[0],"x",sizeof(out->shortcut[0]));
        else if (!native && tutorial_focus>=0 && tutorial_focus<3)
            SDL_strlcpy(out->shortcut[tutorial_focus],"Space",sizeof(out->shortcut[0]));
        if (!native) SDL_strlcpy(out->shortcut[1],"Esc",sizeof(out->shortcut[1]));
        SDL_strlcpy(out->controls_hint,view->can_continue||tutorial_reading
            ?(scrollable?"Tab: buttons | PgUp/PgDn: scroll":"Tab: buttons")
            :(scrollable?"Ctrl+Tab: tutorial | PgUp/PgDn: read":"Ctrl+Tab: tutorial"),sizeof(out->controls_hint));
        SDL_strlcpy(out->read_hint,"PgUp/Dn: read",sizeof(out->read_hint));
    } else if (input==TUTORIAL_INPUT_CONTROLLER) {
        if (out->native_command) SDL_strlcpy(out->shortcut[0],binding,sizeof(out->shortcut[0]));
        if (!native) {
            if (tutorial_focus>=0 && tutorial_focus<3)
                SDL_strlcpy(out->shortcut[tutorial_focus],confirm,sizeof(out->shortcut[0]));
            SDL_strlcpy(out->shortcut[1],back,sizeof(out->shortcut[1]));
        }
        if (native) strnfmt(out->controls_hint,sizeof(out->controls_hint),"%s: tutorial buttons | %s: back",menu,back);
        else if (view->can_continue || tutorial_reading)
            strnfmt(out->controls_hint,sizeof(out->controls_hint),"D-pad: %s | %s: choose | %s: skip",
                scrollable?"scroll/buttons":"buttons",confirm,back);
        else if (scrollable) strnfmt(out->controls_hint,sizeof(out->controls_hint),"%s: read card | %s: skip",read,back);
        else strnfmt(out->controls_hint,sizeof(out->controls_hint),"%s: skip",back);
        strnfmt(out->read_hint,sizeof(out->read_hint),"%s: read",native?menu:read);
    } else if (input==TUTORIAL_INPUT_TOUCH) {
        SDL_strlcpy(out->controls_hint,scrollable?"Tap a button | Swipe this card to read":"Tap a button",sizeof(out->controls_hint));
        SDL_strlcpy(out->read_hint,"Swipe: read",sizeof(out->read_hint));
    } else {
        SDL_strlcpy(out->controls_hint,scrollable?"Wheel or drag to read":"",sizeof(out->controls_hint));
        SDL_strlcpy(out->read_hint,"Wheel: read",sizeof(out->read_hint));
    }
    if (!scrollable) out->read_hint[0]='\0';
    if (out->shortcut[2][0]) SDL_strlcat(out->shortcut[2],": change",sizeof(out->shortcut[2]));
    else SDL_strlcpy(out->shortcut[2],input==TUTORIAL_INPUT_TOUCH?"Tap to change"
        :input==TUTORIAL_INPUT_MOUSE?"Click to change":"Change mode",sizeof(out->shortcut[2]));
}

static bool tutorial_has_primary(const tutorial_view *view)
{
    return view->can_continue || tutorial_reading
        || (!tutorial_menu_owns_input() && tutorial_command(view))
        || (tutorial_menu_owns_input() && strstr(view->action,"examine")
            && tutorial_menu_preview_available && !tutorial_menu_preview_shown);
}

static void tutorial_move_focus(int delta,const tutorial_view *view)
{
    tutorial_focus=(tutorial_focus+delta+3)%3;
    if (tutorial_focus==0 && !tutorial_has_primary(view))
        tutorial_focus=delta<0?2:1;
}

static bool tutorial_pane_rect(enum pane_type pane, SDL_FRect *rect)
{
    const sdl_view *v = &g_views[pane];
    if (pane!=PANE_MAIN && !sdl_should_show_supporting_panes()) return false;
    if (!v->term_ready || !v->canvas || v->rect.w <= 0 || v->rect.h <= 0) return false;
    *rect = (SDL_FRect){v->rect.x, v->rect.y, v->rect.w, v->rect.h};
    return true;
}

static bool tutorial_menu_cells_rect(SDL_FRect *rect)
{
    bool found=false;
    const term *main_term=term_screen;
    if (!main_term || !ui_menu_click_is_active()) return false;
    for (int row=0;row<main_term->hgt;++row) {
        int first=-1,last=-1;
        SDL_FRect line;
        for (int col=0;col<main_term->wid;++col) {
            if (!ui_menu_click_has_cell(col,row)) continue;
            if (first<0) first=col;
            last=col;
        }
        if (first<0 || !sdl_main_cell_rect(first,row,last-first+1,1,&line)) continue;
        if (!found) *rect=line;
        else SDL_GetRectUnionFloat(rect,&line,rect);
        found=true;
    }
    return found;
}

static bool tutorial_anchor_rect(const tutorial_view *view, SDL_FRect *rect)
{
    const char *a = view->anchor;
    SDL_Rect r;
    if (!strcmp(a, "none")) return false;
    if (!strcmp(a, "health"))
        return sdl_left_panel_source_cell_rect(COL_HP, ROW_HP, LEFT_PANEL_CONTENT_WID, 1, rect);
    if (!strcmp(a, "voice"))
        return sdl_left_panel_source_cell_rect(COL_SP, ROW_SP, LEFT_PANEL_CONTENT_WID, 1, rect);
    if (!strcmp(a,"light"))
        return sdl_left_panel_source_cell_rect(COL_LIGHT,ROW_LIGHT,LEFT_PANEL_CONTENT_WID,1,rect);
    if (!strcmp(a, "weapon"))
        return sdl_combat_overlay_cell_rect(COL_MEL, ROW_MEL, LEFT_PANEL_CONTENT_WID, 1, rect)
            || sdl_left_panel_source_cell_rect(COL_MEL, ROW_MEL, LEFT_PANEL_CONTENT_WID, 1, rect);
    if (!strcmp(a, "quiver"))
        return sdl_combat_overlay_cell_rect(COL_QUIVER, ROW_QUIVER, LEFT_PANEL_CONTENT_WID, 1, rect)
            || sdl_left_panel_source_cell_rect(COL_QUIVER, ROW_QUIVER, LEFT_PANEL_CONTENT_WID, 1, rect);
    if (!strcmp(a, "status") && sdl_status_pane_current_rect(&r, NULL)) {
        *rect = (SDL_FRect){r.x,r.y,r.w,r.h}; return true;
    }
    if (!strcmp(a, "menu") && sdl_main_menu_pane_current_rect(rect)) return true;
    if ((!strcmp(a, "item") || !strcmp(a, "inventory"))
        && tutorial_pane_rect(PANE_INVENTORY, rect)) return true;
    if (!strcmp(a, "equipment") && tutorial_pane_rect(PANE_WORN, rect)) return true;
    if (!strcmp(a,"supplies") && tutorial_pane_rect(PANE_SUPPLY,rect)) return true;
    if (!strcmp(a,"nearby-monsters") && tutorial_pane_rect(PANE_MONSTERS,rect)) return true;
    if (!strcmp(a,"combat-rolls") && sdl_touch_tutorial_view_rect(PANE_ROLLS,rect)) return true;
    if (!strcmp(a, "quick-access")) {
        sdl_controller_focus_target targets[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT+1];
        int count = sdl_touch_top_panel_collect_controller_focus_targets(targets,N_ELEMENTS(targets));
        if (count > 0) {
            *rect=targets[0].rect;
            for (int i=1;i<count;++i) SDL_GetRectUnionFloat(rect,&targets[i].rect,rect);
            return true;
        }
        if (sdl_touch_pane_current_rect(&r)) {
            *rect = (SDL_FRect){r.x,r.y,r.w,r.h}; return true;
        }
    }
    if (!strcmp(a, "message") && tutorial_pane_rect(PANE_LOG, rect)) return true;
    if (tutorial_menu_owns_input()) {
        if (tutorial_menu_cells_rect(rect)) return true;
        return tutorial_pane_rect(PANE_MAIN, rect);
    }
    if (!strcmp(a, "monster")) {
        int nearest = 0, nearest_distance = 100000;
        for (int i = 1; i < mon_max; ++i) {
            monster_type *m = &mon_list[i];
            int distance;
            char name[160];
            if (!m->r_idx || !m->ml) continue;
            monster_desc(name,sizeof(name),m,0);
            if (view->context.subject[0] && strcmp(name,view->context.subject)) continue;
            distance = abs(m->fy-p_ptr->py) + abs(m->fx-p_ptr->px);
            if (distance < nearest_distance && sdl_map_grid_cell_rect(m->fy,m->fx,rect)) {
                nearest = i; nearest_distance = distance;
            }
        }
        if (nearest) return sdl_map_grid_cell_rect(mon_list[nearest].fy,mon_list[nearest].fx,rect);
        return false;
    }
    if (!strcmp(a,"player") || !strcmp(a,"map"))
        return sdl_map_grid_cell_rect(p_ptr->py, p_ptr->px, rect);
    return false;
}

static bool tutorial_hit(float x, float y, const SDL_FRect *r)
{
    return x >= r->x && y >= r->y && x < r->x+r->w && y < r->y+r->h;
}

static void tutorial_activate(int button, const tutorial_view *view)
{
    if (button==0 && !view->can_continue && tutorial_reading) {
        tutorial_set_reading(false);
        tutorial_focus=0;
        g_state.need_present=true;
        return;
    }
    if (button == 2) {
        tutorial_view next;
        int saved_focus=tutorial_focus, saved_scroll=tutorial_scroll;
        bool saved_reading=tutorial_reading;
        cycle_sdl_gameplay_tutorial_mode();
        /* Choosing Disabled also closes a manual archive replay. Eligible
         * lessons keep their step, focus and scroll when changing other modes. */
        if (get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_DISABLED)
            tutorial_invalidate_context();
        tutorial_checkpoint(true);
        sdl_gameplay_tutorial_sync();
        if (get_sdl_gameplay_tutorial_mode()!=TUTORIAL_MODE_DISABLED
            && tutorial_get_view(&next) && !strcmp(view->id,next.id) && view->step==next.step) {
            tutorial_focus=saved_focus;
            tutorial_scroll=saved_scroll;
            tutorial_reading=saved_reading;
        }
        g_state.need_present=true;
        return;
    }
    else if (button == 1) tutorial_skip();
    else if (view->can_continue) tutorial_continue();
    else {
        tutorial_controls controls;
        tutorial_build_controls(view,&controls,false);
        if (controls.primary && controls.command) {
            /* The button names a semantic command, not a physical keymap
             * trigger. The legacy backslash prefix bypasses user keymaps. */
            if (!controls.native_command) Term_keypress('\\');
            Term_keypress(controls.command);
        }
        return;
    }
    tutorial_checkpoint(true);
    sdl_gameplay_tutorial_sync();
}

/* Match the text renderer's width fitting before reserving vertical space. */
static float tutorial_text_height(const char *text, int font_px, float width)
{
    TTF_Font *font;
    int w=0,h=0;
    if (!text[0]) return 0;
    font=sdl_story_font_for_height_slot(font_px,SDL_STORY_FONT_SLOT_TUTORIAL);
    if (!font || !TTF_GetStringSize(font,text,0,&w,&h)) return font_px;
    return h*(w>width && width>0?width/w:1.0f);
}

void sdl_gameplay_tutorial_render(void)
{
    tutorial_view view;
    tutorial_controls controls;
    SDL_Rect screen = sdl_get_layout_screen_rect();
    SDL_FRect anchor;
    bool has_anchor;
    bool compact_action;
    float margin, width, height, pad, line_h, body_y, body_h;
    float button_h=44, button_pad, gap, footer_h, footer_y, hint_h[2];
    float label_h[3]={0}, shortcut_h[3]={0};
    int label_px, shortcut_px, hint_px[2], first_button, button_count;
    int font_px, line_count, visible_lines;
    char text[3072], heading[256], lines[48][SDL_TOUCH_TUTORIAL_LINE_LEN];
    TTF_Font *font;
    const SDL_Color white = {240,239,231,255}, gold = {244,202,111,255};
    const SDL_Color muted = {182,191,204,255};
    sdl_gameplay_tutorial_sync();
    if (!tutorial_get_view(&view) || screen.w <= 0 || screen.h <= 0) return;
    tutorial_build_controls(&view,&controls,false);
    compact_action=!view.can_continue && !tutorial_reading
        && !tutorial_menu_owns_input() && screen.h<360;
    margin = MAX(8.0f, MIN(screen.w, screen.h)*0.018f);
    pad = margin;
    font_px = sdl_main_menu_pane_font_px();
    /* Keep a readable body row beside the footer on very short displays. */
    font_px = MIN(font_px,MAX(8,(int)((screen.h-2*margin)/10)));
    if (compact_action) pad=4;
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_TUTORIAL);
    if (!font) return;
    line_h = MAX(font_px*1.35f,tutorial_text_height("Ag",font_px,0)+1);
    width = MIN(screen.w-2*margin, MAX(690.0f,font_px*24.0f));
    first_button=controls.primary?0:1;
    button_count=3-first_button;
    label_px=MAX(13,(int)(font_px*0.85f));
    shortcut_px=MAX(11,(int)(font_px*0.65f));
    hint_px[0]=MAX(12,font_px-3);
    hint_px[1]=MAX(11,font_px-4);
    button_pad=MAX(4.0f,font_px*0.15f);
    gap=MAX(4.0f,font_px*0.15f);
    for (int i=first_button;i<3;++i) {
        float text_w=(width-2*pad)/button_count-18;
        label_h[i]=tutorial_text_height(controls.label[i],label_px,text_w);
        shortcut_h[i]=tutorial_text_height(controls.shortcut[i],shortcut_px,text_w);
        button_h=MAX(button_h,2*button_pad+label_h[i]
            +(shortcut_h[i]>0?gap+shortcut_h[i]:0));
    }
    /* Reserve full hint lines, including the scroll counter appended below. */
    hint_h[0]=controls.action_hint[0]?tutorial_text_height("Ag",hint_px[0],width):0;
    hint_h[1]=tutorial_text_height("Ag",hint_px[1],width);
    footer_h=button_h+gap;
    if (!compact_action) footer_h+=hint_h[0]+hint_h[1]+gap*(hint_h[0]>0?2:1);
    if (view.context.text[0] && !strstr(view.body,view.context.text))
        strnfmt(text,sizeof(text),"%s\n%s",view.body,view.context.text);
    else SDL_strlcpy(text,view.body,sizeof(text));
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_TUTORIAL);
    if (!font) return;
    line_count = sdl_touch_tutorial_wrap_lines(text,font,width-2*pad,lines,48);
    /* Fit the complete lesson before resorting to scrolling. Only the small
     * live-action strip deliberately limits how much body text is shown. */
    height=2*pad+line_h*(1.5f+(compact_action?1:MAX(1,line_count)))+footer_h+1;
    height=MIN(screen.h-2*margin,MAX(compact_action?100.0f:210.0f,height));
    body_h = height-2*pad-line_h*1.5f-footer_h;
    visible_lines = MAX(0,(int)(body_h/line_h));
    tutorial_max_scroll = MAX(0,line_count-visible_lines);
    tutorial_scroll = MIN(tutorial_scroll,tutorial_max_scroll);
    tutorial_build_controls(&view,&controls,tutorial_max_scroll>0);
    has_anchor = tutorial_anchor_rect(&view, &anchor);
    tutorial_card = (SDL_FRect){screen.x+(screen.w-width)/2,screen.y+screen.h-height-margin,width,height};
    if (has_anchor && anchor.y+anchor.h/2 > screen.y+screen.h/2)
        tutorial_card.y = screen.y+margin;
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0,0,0,145);
    if (has_anchor) {
        SDL_FRect bounds = {screen.x,screen.y,screen.w,screen.h};
        SDL_FRect clipped;
        if (SDL_GetRectIntersectionFloat(&anchor,&bounds,&clipped)) {
            SDL_FRect shades[4] = {
                {bounds.x,bounds.y,bounds.w,clipped.y-bounds.y},
                {bounds.x,clipped.y+clipped.h,bounds.w,bounds.y+bounds.h-clipped.y-clipped.h},
                {bounds.x,clipped.y,clipped.x-bounds.x,clipped.h},
                {clipped.x+clipped.w,clipped.y,bounds.x+bounds.w-clipped.x-clipped.w,clipped.h}};
            SDL_RenderFillRects(g_state.renderer,shades,4);
        } else SDL_RenderFillRect(g_state.renderer,&bounds);
        SDL_SetRenderDrawColor(g_state.renderer,gold.r,gold.g,gold.b,255);
        for (int i=1;i<=3;++i) {
            SDL_FRect border = {anchor.x-i,anchor.y-i,anchor.w+2*i,anchor.h+2*i};
            SDL_RenderRect(g_state.renderer,&border);
        }
    } else {
        SDL_FRect bounds = {screen.x,screen.y,screen.w,screen.h};
        SDL_RenderFillRect(g_state.renderer,&bounds);
    }
    SDL_SetRenderDrawColor(g_state.renderer,18,24,34,250);
    SDL_RenderFillRect(g_state.renderer,&tutorial_card);
    SDL_SetRenderDrawColor(g_state.renderer,gold.r,gold.g,gold.b,255);
    SDL_RenderRect(g_state.renderer,&tutorial_card);
    strnfmt(heading,sizeof(heading),"%s  %d/%d",view.title,view.step,view.step_count);
    sdl_touch_tutorial_draw_text_line(heading,tutorial_card.x+pad,tutorial_card.y+pad,
        width-2*pad-(compact_action && tutorial_max_scroll?100:0),font_px+2,gold,false);
    if (compact_action && tutorial_max_scroll)
        sdl_touch_tutorial_draw_text_line(controls.read_hint,
            tutorial_card.x+width-pad-94,tutorial_card.y+pad+2,94,12,muted,false);
    body_y = tutorial_card.y+pad+line_h*1.5f;
    for (int i=0;i<visible_lines && i+tutorial_scroll<line_count;++i)
        sdl_touch_tutorial_draw_text_line(lines[i+tutorial_scroll],tutorial_card.x+pad,
            body_y+i*line_h,width-2*pad,font_px,white,false);
    if (tutorial_max_scroll) {
        char progress[48];
        strnfmt(progress,sizeof(progress)," | %d-%d/%d",tutorial_scroll+1,
            MIN(line_count,tutorial_scroll+visible_lines),line_count);
        SDL_strlcat(controls.controls_hint,progress,sizeof(controls.controls_hint));
    }
    if (!compact_action) {
        footer_y=tutorial_card.y+height-pad-footer_h+gap;
        sdl_touch_tutorial_draw_text_line(controls.action_hint,tutorial_card.x+pad,
            footer_y,width-2*pad,hint_px[0],muted,false);
        if (hint_h[0]>0) footer_y+=hint_h[0]+gap;
        sdl_touch_tutorial_draw_text_line(controls.controls_hint,tutorial_card.x+pad,
            footer_y,width-2*pad,hint_px[1],muted,false);
    }
    tutorial_buttons[0]=(SDL_FRect){0};
    for (int i=first_button;i<3;++i) {
        tutorial_buttons[i] = (SDL_FRect){tutorial_card.x+pad+(i-first_button)*(width-2*pad)/button_count,
            tutorial_card.y+height-pad-button_h,(width-2*pad)/button_count-6,button_h};
        SDL_SetRenderDrawColor(g_state.renderer,i==tutorial_focus?66:34,i==tutorial_focus?58:43,47,255);
        SDL_RenderFillRect(g_state.renderer,&tutorial_buttons[i]);
        SDL_SetRenderDrawColor(g_state.renderer,i==tutorial_focus?244:90,i==tutorial_focus?202:103,111,255);
        SDL_RenderRect(g_state.renderer,&tutorial_buttons[i]);
        float text_y=tutorial_buttons[i].y+(button_h-label_h[i]
            -(shortcut_h[i]>0?gap+shortcut_h[i]:0))/2;
        sdl_touch_tutorial_draw_text_line(controls.label[i],tutorial_buttons[i].x+tutorial_buttons[i].w/2,
            text_y,tutorial_buttons[i].w-12,label_px,white,true);
        if (controls.shortcut[i][0]) sdl_touch_tutorial_draw_text_line(controls.shortcut[i],
            tutorial_buttons[i].x+tutorial_buttons[i].w/2,text_y+label_h[i]+gap,
            tutorial_buttons[i].w-12,shortcut_px,gold,true);
    }
}

/* True means input belongs to this owner. Non-input events always continue. */
bool sdl_gameplay_tutorial_handle_event(const SDL_Event *ev)
{
    tutorial_view view;
    bool active, down=false, up=false, pointer=false, mouse=false;
    float x=0,y=0;
    SDL_FingerID finger=0;
    sdl_gameplay_tutorial_sync();
    active = tutorial_get_view(&view);
    tutorial_note_input(ev);
    switch (ev->type) {
    case SDL_EVENT_QUIT:
        if (active) {
            tutorial_invalidate_context();
            sdl_gameplay_tutorial_sync();
        }
        return false;
    case SDL_EVENT_KEY_UP:
        if (ev->key.scancode < SDL_SCANCODE_COUNT) {
            bool blocked = tutorial_blocked_keys[ev->key.scancode];
            tutorial_blocked_keys[ev->key.scancode] = false;
            if (blocked) return true;
        }
        return active && view.can_continue;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (ev->gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
            bool blocked = tutorial_blocked_buttons[ev->gbutton.button];
            tutorial_blocked_buttons[ev->gbutton.button] = false;
            if (blocked) return true;
        }
        return active && view.can_continue;
    case SDL_EVENT_KEY_DOWN:
        if (ev->common.timestamp < tutorial_input_barrier
            || (ev->key.scancode < SDL_SCANCODE_COUNT && tutorial_blocked_keys[ev->key.scancode])) return true;
        if (!active) return false;
        if (ev->key.repeat) return true;
        if (ev->key.key==SDLK_TAB && (view.can_continue || tutorial_reading || (ev->key.mod&SDL_KMOD_CTRL))) {
            if (!view.can_continue) tutorial_set_reading(true);
            tutorial_move_focus(1,&view); break;
        }
        if (ev->key.key==SDLK_PAGEUP || ev->key.key==SDLK_PAGEDOWN) {
            /* A page key is still owned by the card when the body fits, but
             * it must not turn a short action card into a different mode with
             * no text to reveal. */
            if (!view.can_continue && !tutorial_menu_owns_input()
                && tutorial_max_scroll > 0) tutorial_set_reading(true);
            tutorial_scroll=MAX(0,MIN(tutorial_max_scroll,tutorial_scroll+(ev->key.key==SDLK_PAGEUP?-1:1))); break;
        }
        if (view.can_continue || tutorial_reading) {
            if (ev->key.key==SDLK_LEFT || ev->key.key==SDLK_UP) tutorial_move_focus(-1,&view);
            else if (ev->key.key==SDLK_RIGHT || ev->key.key==SDLK_DOWN) tutorial_move_focus(1,&view);
            else if (ev->key.key==SDLK_RETURN || ev->key.key==SDLK_KP_ENTER || ev->key.key==SDLK_SPACE)
                tutorial_activate(tutorial_focus,&view);
            else if (sdl_key_is_escape_or_back(ev->key.key)) tutorial_activate(1,&view);
            break;
        }
        if (!tutorial_menu_owns_input()) {
            if (sdl_key_is_escape_or_back(ev->key.key)) { tutorial_activate(1,&view); break; }
            if (ev->key.key==SDLK_RETURN || ev->key.key==SDLK_KP_ENTER || ev->key.key==SDLK_SPACE) { tutorial_activate(tutorial_focus,&view); break; }
        }
        return false; /* Existing keyboard translation -> semantic engine gate. */
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        int button = ev->gbutton.button;
        if (ev->common.timestamp < tutorial_input_barrier
            || (button < SDL_GAMEPAD_BUTTON_COUNT && tutorial_blocked_buttons[button])) return true;
        if (!active) return false;
        if (!config.gamepad_enabled) return true;
        sdl_gamepad_mark_auto_ui();
        if (!view.can_continue && tutorial_menu_owns_input() && button==SDL_GAMEPAD_BUTTON_START) {
            tutorial_set_reading(!tutorial_reading); tutorial_focus=0; break;
        }
        if (!view.can_continue && !tutorial_reading && tutorial_menu_owns_input()) return false;
        if (!view.can_continue && button==SDL_GAMEPAD_BUTTON_BACK) {
            tutorial_set_reading(!tutorial_reading); tutorial_focus=0; break;
        }
        if (sdl_gamepad_button_is_ui_back((SDL_GamepadButton)button)) tutorial_activate(1,&view);
        else if (sdl_gamepad_button_is_ui_confirm((SDL_GamepadButton)button)) tutorial_activate(tutorial_focus,&view);
        else if (button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button==SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
            if (!view.can_continue) tutorial_set_reading(true);
            tutorial_move_focus(button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER?-1:1,&view);
        }
        else if (button>=SDL_GAMEPAD_BUTTON_DPAD_UP && button<=SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
            if (!view.can_continue && !tutorial_reading) return false;
            if (button==SDL_GAMEPAD_BUTTON_DPAD_UP || button==SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                tutorial_scroll=MAX(0,MIN(tutorial_max_scroll,tutorial_scroll+(button==SDL_GAMEPAD_BUTTON_DPAD_UP?-1:1)));
            else tutorial_move_focus(button==SDL_GAMEPAD_BUTTON_DPAD_LEFT?-1:1,&view);
        } else if (!view.can_continue && !tutorial_reading && button<SDL_GAMEPAD_BUTTON_COUNT) {
            int binding=config.gamepad_button_bindings[button];
            if (binding==GAMEPAD_BIND_SHIFT || binding==GAMEPAD_BIND_CTRL || binding==GAMEPAD_BIND_ALT)
                return false;
            if (binding>0 && binding<256) sdl_gamepad_send_key(binding,false);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (ev->gaxis.axis < SDL_GAMEPAD_AXIS_COUNT && tutorial_blocked_axes[ev->gaxis.axis]) {
            if (abs(ev->gaxis.value)<=MAX(config.gamepad_deadzone,4000)) tutorial_blocked_axes[ev->gaxis.axis]=false;
            return true;
        }
        return ev->common.timestamp < tutorial_input_barrier
            || (active && (view.can_continue || tutorial_reading));
    case SDL_EVENT_TEXT_INPUT:
        return ev->common.timestamp < tutorial_input_barrier
            || (active && (view.can_continue || tutorial_reading));
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (active && ev->button.which==SDL_TOUCH_MOUSEID) return true;
        mouse=pointer=true; down=ev->type==SDL_EVENT_MOUSE_BUTTON_DOWN; up=!down;
        x=ev->button.x; y=ev->button.y;
        if (tutorial_blocked_mouse & SDL_BUTTON_MASK(ev->button.button)) {
            if (up) tutorial_blocked_mouse &= ~SDL_BUTTON_MASK(ev->button.button);
            return true;
        }
        if (active && ev->button.button!=SDL_BUTTON_LEFT) return true;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (active && ev->motion.which==SDL_TOUCH_MOUSEID) return true;
        mouse=pointer=true; x=ev->motion.x; y=ev->motion.y; break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (!active) return ev->common.timestamp < tutorial_input_barrier;
        if (!view.can_continue && !tutorial_menu_owns_input()
            && tutorial_max_scroll > 0) tutorial_set_reading(true);
        tutorial_scroll=MAX(0,MIN(tutorial_max_scroll,tutorial_scroll-(int)ev->wheel.y));
        g_state.need_present=true; return true;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        pointer=true; down=ev->type==SDL_EVENT_FINGER_DOWN;
        up=ev->type==SDL_EVENT_FINGER_UP || ev->type==SDL_EVENT_FINGER_CANCELED;
        finger=ev->tfinger.fingerID;
        for (int i=0;i<tutorial_blocked_finger_count;++i) {
            if (tutorial_blocked_fingers[i] != finger) continue;
            if (up) tutorial_blocked_fingers[i] = tutorial_blocked_fingers[--tutorial_blocked_finger_count];
            return true;
        }
        if (!sdl_finger_event_to_render_coords(&ev->tfinger,&x,&y)) return active;
        break;
    default: return false;
    }
    if (!pointer) { g_state.need_present=true; return true; }
    if (ev->common.timestamp < tutorial_input_barrier) return true;
    if (!active) return false;
    if (down) {
        tutorial_pointer_mouse=mouse; tutorial_pointer_finger=finger;
        tutorial_pressed_button=-1; tutorial_pointer_gameplay=false;
        tutorial_scrolling=false;
        for (int i=0;i<3;++i) if (tutorial_hit(x,y,&tutorial_buttons[i])) {
            tutorial_focus=i; tutorial_pressed_button=i; g_state.need_present=true; return true;
        }
        if (tutorial_hit(x,y,&tutorial_card)) {
            if (!view.can_continue && !tutorial_menu_owns_input()
                && tutorial_max_scroll > 0) {
                tutorial_set_reading(true);
                /* This accepted fresh down starts the reading drag itself. */
                if (mouse) tutorial_blocked_mouse&=~SDL_BUTTON_LMASK;
                else for (int i=0;i<tutorial_blocked_finger_count;++i) {
                    if (tutorial_blocked_fingers[i]!=finger) continue;
                    tutorial_blocked_fingers[i]=tutorial_blocked_fingers[--tutorial_blocked_finger_count];
                    break;
                }
            }
            tutorial_scrolling=true; tutorial_scroll_y=y;
            g_state.need_present=true;
            return true;
        }
        if (!view.can_continue && !tutorial_reading && !tutorial_hit(x,y,&tutorial_card)) {
            int map_y,map_x;
            if (tutorial_menu_owns_input()) {
                /* A modal opened behind the card owns every point outside the
                 * card.  In particular, minimap and look overlays are not
                 * part of the main map rect. */
                tutorial_pointer_gameplay = true;
            } else {
                tutorial_pointer_gameplay = sdl_main_view_point_to_map(x,y,&map_y,&map_x);
            }
        }
        return !tutorial_pointer_gameplay;
    }
    if (mouse==tutorial_pointer_mouse && (mouse || finger==tutorial_pointer_finger)) {
        if (tutorial_scrolling) {
            int rows=(int)((tutorial_scroll_y-y)/24.0f);
            if (rows) {
                tutorial_scroll=MAX(0,MIN(tutorial_max_scroll,tutorial_scroll+rows));
                tutorial_scroll_y=y;
                g_state.need_present=true;
            }
            if (up) tutorial_scrolling=false;
            return true;
        }
        if (up && tutorial_pressed_button>=0) {
            int button=tutorial_pressed_button;
            tutorial_pressed_button=-1;
            if (ev->type!=SDL_EVENT_FINGER_CANCELED && tutorial_hit(x,y,&tutorial_buttons[button])) tutorial_activate(button,&view);
            return true;
        }
        if (tutorial_pointer_gameplay) {
            if (up) tutorial_pointer_gameplay=false;
            return false;
        }
    }
    return true;
}
