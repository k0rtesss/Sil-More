#!/usr/bin/env python3
"""Exercise the real SDL tutorial event owner without opening a player save.

The SDL event gate is compiled unchanged. Game-side services are isolated
stubs; assertions cover ownership, release barriers and semantic key dispatch.
This is not a physical-device or visual replay.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-sdl-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "cJSON.h"
/* Hide only the unused renderer from this event-only executable. */
#define sdl_gameplay_tutorial_render static tutorial_test_unused_render
#include "sdl/ui/sdl-gameplay-tutorial.c"

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];
struct sdl_config config;
gamepad_input_state g_gamepad_state;
player_action_menu_state g_player_action_menu;
player_exchange_target_state g_player_exchange_target;
bool g_main_menu_overlay_active, g_touch_pane_yes_no_prompt_active;
bool g_touch_pane_reset_confirm_active, g_unified_look_active;
minimap_state g_minimap;
bool g_screen_back_right_button_pending;
s16b character_icky;
term *Term;
term *angband_term[ANGBAND_TERM_MAX];
static tutorial_view current;
static unsigned int revision;
static int keys[16], key_count, flush_count, pending_choice;
static bool touch_only, disabled;
static tutorial_mode g_app_gameplay_tutorial_mode=TUTORIAL_MODE_EXTENDED;
static int config_saves;
bool save_pane_config_to_json(void) { ++config_saves; return true; }
const char *tutorial_mode_name(tutorial_mode mode)
{ return mode==TUTORIAL_MODE_DISABLED?"Disabled":mode==TUTORIAL_MODE_NORMAL?"Normal":"Extended"; }
void tutorial_set_mode(tutorial_mode mode) { disabled=mode==TUTORIAL_MODE_DISABLED; ++revision; }
/* CONFIG_MODE_FUNCTIONS */
bool sdl_touch_only_device_active(void) { return touch_only; }
bool steamdeck_controls_active(void) { return config.input_ui_mode==SDL_INPUT_UI_MODE_CONTROLLER; }
const char *sdl_gamepad_button_short_label(int b)
{ return b==SDL_GAMEPAD_BUTTON_SOUTH?"A":b==SDL_GAMEPAD_BUTTON_EAST?"B":b==SDL_GAMEPAD_BUTTON_START?"Menu":"View"; }
void sdl_gamepad_action_binding_short_label(int command,char *buf,size_t size)
{ (void)command; SDL_strlcpy(buf,"X",size); }
void help_describe_command_bindings(int command,char *buf,size_t size)
{ (void)command; SDL_strlcpy(buf,"Alt+x",size); }
size_t strnfmt(char *buf,size_t size,const char *format,...)
{ va_list args; va_start(args,format); int n=SDL_vsnprintf(buf,size,format,args); va_end(args); return n<0?0:(size_t)n; }

unsigned int tutorial_revision(void) { return revision; }
bool tutorial_is_active(void) { return current.active; }
bool tutorial_get_view(tutorial_view *out) { *out=current; return current.active; }
void tutorial_checkpoint(bool safe) { (void)safe; }
void tutorial_continue(void) { current.active=false; ++revision; }
void tutorial_skip(void) { current.active=false; ++revision; }
void tutorial_invalidate_context(void) { current.active=false; ++revision; }
errr Term_keypress(int key) { keys[key_count++]=key; return 0; }
errr Term_flush(void) { ++flush_count; key_count=0; return 0; }
void inkey_next_set(cptr keys) { (void)keys; }
bool inkey_prompt_input_active(void) { return false; }
bool ui_menu_click_take_action(int *choice, int *action)
{ (void)choice; (void)action; bool result=pending_choice; pending_choice=0; return result; }
bool sdl_question_menu_captures_pointer(void) { return false; }
bool sdl_hint_quest_menu_active(void) { return false; }
bool sdl_key_is_escape_or_back(SDL_Keycode key) { return key==SDLK_ESCAPE; }
bool sdl_gamepad_button_is_ui_confirm(SDL_GamepadButton b) { return b==SDL_GAMEPAD_BUTTON_SOUTH; }
bool sdl_gamepad_button_is_ui_back(SDL_GamepadButton b) { return b==SDL_GAMEPAD_BUTTON_EAST; }
void sdl_gamepad_send_key(int key, bool mods) { (void)mods; Term_keypress(key); }
bool sdl_finger_event_to_render_coords(const SDL_TouchFingerEvent *event,float *x,float *y)
{ *x=event->x; *y=event->y; return true; }
bool sdl_main_view_point_to_map(float x,float y,int *my,int *mx)
{ (void)my; (void)mx; return x>500 && y>500; }
#define NOOP(name) void name(void) {}
NOOP(sdl_gamepad_mark_auto_ui)
NOOP(sdl_gamepad_reset_movement_controls)
NOOP(sdl_gamepad_clear_pending_confirm)
NOOP(sdl_gamepad_clear_pending_shoulder)
NOOP(sdl_touch_cancel_all_inputs)
NOOP(sdl_touch_thumb_cancel_press)
NOOP(sdl_menu_touch_cancel)
NOOP(sdl_menu_scroll_cancel)
NOOP(sdl_screen_back_gesture_cancel_touch_inputs)
NOOP(sdl_screen_back_touch_cancel)
NOOP(sdl_main_menu_button_cancel_input)
NOOP(sdl_mouse_path_cancel)
NOOP(sdl_pointer_aim_cancel_touch_press)
NOOP(sdl_pointer_attack_clear_hover)
NOOP(sdl_player_action_menu_cancel_press)
NOOP(sdl_log_pane_menu_clear_long_press)
NOOP(sdl_side_pane_menu_clear_long_press)

static void show(bool info, const char *action, const char *subject)
{
    current=(tutorial_view){.active=true,.can_continue=info};
    SDL_strlcpy(current.action,action,sizeof(current.action));
    SDL_strlcpy(current.action_subject,subject,sizeof(current.action_subject));
    ++revision;
    sdl_gameplay_tutorial_sync();
    /* Physical input on the host must not make this synthetic test flaky. */
    memset(tutorial_blocked_keys,0,sizeof(tutorial_blocked_keys));
    memset(tutorial_blocked_buttons,0,sizeof(tutorial_blocked_buttons));
    key_count=0;
}
static SDL_Event key_event(Uint32 type, SDL_Keycode key, SDL_Scancode scan)
{
    SDL_Event event={0};
    SDL_zero(event);
    event.key.type=type; event.key.timestamp=tutorial_input_barrier+1000000;
    event.key.key=key; event.key.scancode=scan;
    return event;
}
static SDL_Event button_event(Uint32 type, SDL_GamepadButton button)
{
    SDL_Event event={0};
    SDL_zero(event);
    event.gbutton.type=type; event.gbutton.timestamp=tutorial_input_barrier+1000000;
    event.gbutton.button=button; event.gbutton.down=type==SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    return event;
}
int main(void)
{
    SDL_Event e;
    term terminal={0};
    assert(SDL_Init(SDL_INIT_EVENTS));
    Term=&terminal;
    angband_term[0]=&terminal;
    config.gamepad_enabled=true;
    {
        const char *documents[]={"{}","{\"gameplayTutorialEnabled\":true}",
            "{\"gameplayTutorialEnabled\":false}","{\"gameplayTutorialMode\":\"Normal\"}",
            "{\"gameplayTutorialMode\":\"Disabled\"}","{\"gameplayTutorialMode\":\"Extended\"}",
            "{\"gameplayTutorialMode\":\"Normal\",\"gameplayTutorialEnabled\":false}",
            "{\"gameplayTutorialMode\":1}","{\"gameplayTutorialMode\":1.5}",
            "{\"gameplayTutorialMode\":\"invalid\",\"gameplayTutorialEnabled\":false}"};
        tutorial_mode expected[]={2,2,0,1,0,2,1,1,2,0};
        for (int i=0;i<10;++i) {
            cJSON *document=cJSON_Parse(documents[i]); assert(document);
            assert(sdl_config_gameplay_tutorial_mode_from_json(document)==expected[i]);
            cJSON_Delete(document);
        }
        assert(get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_EXTENDED);
        cycle_sdl_gameplay_tutorial_mode(); assert(get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_DISABLED);
        cycle_sdl_gameplay_tutorial_mode(); assert(get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_NORMAL);
        set_sdl_gameplay_tutorial_enabled(true); assert(get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_NORMAL);
        cycle_sdl_gameplay_tutorial_mode(); assert(get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_EXTENDED);
        assert(config_saves==4);
    }
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    pending_choice=1;
    terminal.key_head=3; terminal.key_tail=1;
    show(true,"","");
    assert(pending_choice==0 && terminal.key_head==0 && terminal.key_tail==0);
    assert(flush_count==0); /* No recursive SDL dispatch during transition. */
    character_icky=1; /* Info still owns input above a purchase menu. */
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(sdl_gameplay_tutorial_handle_event(&e) && key_count==0);
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);
    assert(sdl_gameplay_tutorial_handle_event(&e) && !current.active && key_count==0);
    character_icky=0;
    /* Both advertised Space and the existing Enter shortcut confirm cards
     * and dispatch primary tutorial actions. */
    {
        const SDL_Keycode confirm_keys[]={SDLK_SPACE,SDLK_RETURN};
        const SDL_Scancode confirm_scans[]={SDL_SCANCODE_SPACE,SDL_SCANCODE_RETURN};
        for (int i=0;i<2;++i) {
            show(true,"","");
            e=key_event(SDL_EVENT_KEY_DOWN,confirm_keys[i],confirm_scans[i]);
            assert(sdl_gameplay_tutorial_handle_event(&e) && !current.active);
            show(false,"open-menu","inventory");
            e=key_event(SDL_EVENT_KEY_DOWN,confirm_keys[i],confirm_scans[i]);
            assert(sdl_gameplay_tutorial_handle_event(&e));
            assert(key_count==2 && keys[0]=='\\' && keys[1]=='i');
        }
    }
    {
        tutorial_controls labels;
        show(false,"examine","");
        tutorial_last_input=TUTORIAL_INPUT_KEYBOARD;
        tutorial_build_controls(&current,&labels,true);
        assert(labels.primary && !labels.native_command && !strcmp(labels.label[0],"Examine item"));
        assert(strstr(labels.action_hint,"Alt+x") && !strstr(labels.action_hint,"(x)"));
        character_icky=1;
        sdl_gameplay_tutorial_set_menu_preview(true,false);
        sdl_gameplay_tutorial_set_menu_preview_control("Y");
        tutorial_build_controls(&current,&labels,true);
        assert(labels.primary && labels.native_command && labels.command=='x');
        assert(!strcmp(labels.label[0],"Preview item") && !strcmp(labels.shortcut[0],"x"));
        assert(!strstr(labels.action_hint,"Alt+x") && strstr(labels.controls_hint,"Ctrl+Tab"));
        key_count=0; tutorial_activate(0,&current);
        assert(key_count==1 && keys[0]=='x');
        e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
        assert(!sdl_gameplay_tutorial_handle_event(&e)); /* Native keyboard x reaches browser. */
        tutorial_card=(SDL_FRect){10,10,300,200};
        tutorial_buttons[0]=(SDL_FRect){20,150,80,44};
        tutorial_buttons[1]=(SDL_FRect){110,150,80,44};
        tutorial_buttons[2]=(SDL_FRect){200,150,80,44};
        key_count=0;
        e=(SDL_Event){0}; e.button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
        e.button.timestamp=tutorial_input_barrier+1000000;
        e.button.button=SDL_BUTTON_LEFT; e.button.x=40; e.button.y=170;
        assert(sdl_gameplay_tutorial_handle_event(&e));
        assert(tutorial_input_kind()==TUTORIAL_INPUT_MOUSE);
        e.button.type=SDL_EVENT_MOUSE_BUTTON_UP;
        assert(sdl_gameplay_tutorial_handle_event(&e) && key_count==1 && keys[0]=='x');
        key_count=0;
        e=(SDL_Event){0}; e.tfinger.type=SDL_EVENT_FINGER_DOWN;
        e.tfinger.timestamp=tutorial_input_barrier+1000000;
        e.tfinger.fingerID=27; e.tfinger.x=40; e.tfinger.y=170;
        assert(sdl_gameplay_tutorial_handle_event(&e));
        assert(tutorial_input_kind()==TUTORIAL_INPUT_TOUCH);
        e.tfinger.type=SDL_EVENT_FINGER_UP;
        assert(sdl_gameplay_tutorial_handle_event(&e) && key_count==1 && keys[0]=='x');
        e=(SDL_Event){0}; e.motion.type=SDL_EVENT_MOUSE_MOTION;
        e.motion.timestamp=tutorial_input_barrier+1000000;
        e.motion.which=SDL_TOUCH_MOUSEID; e.motion.xrel=4;
        assert(sdl_gameplay_tutorial_handle_event(&e));
        assert(tutorial_input_kind()==TUTORIAL_INPUT_TOUCH); /* Synthetic mouse does not change touch labels. */
        sdl_gameplay_tutorial_set_menu_preview(true,true);
        tutorial_build_controls(&current,&labels,true);
        assert(!labels.primary); key_count=0; tutorial_activate(0,&current); assert(key_count==0);
        sdl_gameplay_tutorial_set_menu_preview(true,false);
        tutorial_last_input=TUTORIAL_INPUT_MOUSE;
        tutorial_build_controls(&current,&labels,true);
        assert(!strstr(labels.controls_hint,"Click") && strstr(labels.controls_hint,"Wheel")
            && !labels.shortcut[0][0] && !strcmp(labels.label[1],"Skip tutorial"));
        tutorial_build_controls(&current,&labels,false);
        assert(!labels.controls_hint[0]);
        tutorial_last_input=TUTORIAL_INPUT_TOUCH;
        tutorial_build_controls(&current,&labels,true);
        assert(strstr(labels.controls_hint,"Tap") && strstr(labels.read_hint,"Swipe"));
        assert(!strstr(labels.controls_hint,"Click") && !strstr(labels.controls_hint,"Enter"));
        config.input_ui_mode=SDL_INPUT_UI_MODE_CONTROLLER;
        tutorial_build_controls(&current,&labels,true);
        assert(!strcmp(labels.shortcut[0],"Y") && !labels.shortcut[1][0]);
        assert(strstr(labels.controls_hint,"Menu") && strstr(labels.controls_hint,"back"));
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_BACK);
        assert(!sdl_gameplay_tutorial_handle_event(&e)); /* Native Preview control remains native. */
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_START);
        assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_reading);
        tutorial_build_controls(&current,&labels,true);
        assert(!strcmp(labels.shortcut[1],"B"));
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_focus==1);
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);
        assert(sdl_gameplay_tutorial_handle_event(&e) && !current.active);
        show(false,"examine","");
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_START);
        assert(sdl_gameplay_tutorial_handle_event(&e));
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        assert(sdl_gameplay_tutorial_handle_event(&e));
        assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_focus==2);
        e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);
        assert(sdl_gameplay_tutorial_handle_event(&e) && disabled);
        assert(!current.active && get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_DISABLED);
        show(true,"",""); /* An archive card can also be opened while Disabled. */
        tutorial_focus=2; tutorial_scroll=3;
        tutorial_activate(2,&current);
        assert(current.active && get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_NORMAL);
        assert(tutorial_focus==2 && tutorial_scroll==3);
        tutorial_build_controls(&current,&labels,true);
        assert(!strcmp(labels.label[2],"Normal") && strstr(labels.shortcut[2],"change"));
        tutorial_activate(2,&current);
        assert(current.active && get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_EXTENDED);
        tutorial_activate(2,&current);
        assert(!current.active && get_sdl_gameplay_tutorial_mode()==TUTORIAL_MODE_DISABLED);
        sdl_gameplay_tutorial_set_menu_preview(false,false);
        character_icky=0; config.input_ui_mode=SDL_INPUT_UI_MODE_AUTO;
    }
    tutorial_blocked_keys[SDL_SCANCODE_X]=true;
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e.type=SDL_EVENT_KEY_UP;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    show(false,"open-menu","inventory");
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);
    assert(sdl_gameplay_tutorial_handle_event(&e));
    assert(key_count==2 && keys[0]=='\\' && keys[1]=='i');
    character_icky=1;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    character_icky=0;
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_UP);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    e.key.repeat=true;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    show(false,"move","");
    /* Page and wheel input are owned by a short action card, but they do not
     * enter reading mode when the rendered body has no overflow. */
    tutorial_max_scroll=0; tutorial_reading=false;
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_PAGEDOWN,SDL_SCANCODE_PAGEDOWN);
    assert(sdl_gameplay_tutorial_handle_event(&e) && !tutorial_reading);
    e=(SDL_Event){0}; e.wheel.type=SDL_EVENT_MOUSE_WHEEL;
    e.wheel.timestamp=tutorial_input_barrier+1000000; e.wheel.y=-1;
    assert(sdl_gameplay_tutorial_handle_event(&e) && !tutorial_reading);
    /* A modal opened behind an action card keeps pointer ownership, including
     * points outside the main map (for example a minimap overlay). */
    g_minimap.active=true;
    e=(SDL_Event){0}; e.button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.timestamp=tutorial_input_barrier+1000000; e.button.button=SDL_BUTTON_LEFT;
    e.button.x=400; e.button.y=400;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    e.button.type=SDL_EVENT_MOUSE_BUTTON_UP;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    g_minimap.active=false;
    e=(SDL_Event){0}; e.gaxis.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.timestamp=tutorial_input_barrier+1000000;
    e.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX; e.gaxis.value=20000;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    character_icky=1;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    character_icky=0;
    tutorial_blocked_axes[SDL_GAMEPAD_AXIS_LEFTX]=true;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e.gaxis.value=0;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e.gaxis.value=20000;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    unsigned int epoch=sdl_gameplay_tutorial_input_epoch();
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_BACK);
    assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_reading);
    assert(sdl_gameplay_tutorial_input_epoch()>epoch);
    e=(SDL_Event){0}; e.gaxis.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.timestamp=tutorial_input_barrier+1000000;
    e.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX; e.gaxis.value=20000;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    tutorial_max_scroll=5;
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_scroll==1 && key_count==0);
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    assert(sdl_gameplay_tutorial_handle_event(&e) && key_count==0);
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_SOUTH);
    assert(sdl_gameplay_tutorial_handle_event(&e) && !tutorial_reading && key_count==0);
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_UP);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    character_icky=1;
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_BACK);
    assert(!sdl_gameplay_tutorial_handle_event(&e) && !tutorial_reading);
    character_icky=0;
    show(true,"","");
    e=(SDL_Event){0}; e.gaxis.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.timestamp=tutorial_input_barrier+1000000;
    e.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX; e.gaxis.value=20000;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e=(SDL_Event){0}; e.type=SDL_EVENT_WINDOW_RESIZED;
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    e.type=SDL_EVENT_QUIT;
    assert(!sdl_gameplay_tutorial_handle_event(&e) && !current.active);
    tutorial_blocked_buttons[SDL_GAMEPAD_BUTTON_DPAD_UP]=true;
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_UP);
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e.type=SDL_EVENT_GAMEPAD_BUTTON_UP;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    e=button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_UP);
    assert(!sdl_gameplay_tutorial_handle_event(&e));
    tutorial_blocked_mouse=SDL_BUTTON_LMASK;
    e=(SDL_Event){0}; e.button.type=SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.timestamp=SDL_GetTicksNS()+1; e.button.button=SDL_BUTTON_LEFT;
    assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_blocked_mouse==0);
    tutorial_blocked_fingers[0]=42; tutorial_blocked_finger_count=1;
    e=(SDL_Event){0}; e.tfinger.type=SDL_EVENT_FINGER_UP;
    e.tfinger.timestamp=SDL_GetTicksNS()+1; e.tfinger.fingerID=42;
    assert(sdl_gameplay_tutorial_handle_event(&e) && tutorial_blocked_finger_count==0);
    e=key_event(SDL_EVENT_KEY_DOWN,SDLK_X,SDL_SCANCODE_X);
    e.common.timestamp=tutorial_input_barrier-1;
    assert(sdl_gameplay_tutorial_handle_event(&e));
    SDL_Quit();
    puts("Gameplay tutorial SDL gate: PASS (info/menu ownership, required commands, keymaps, fresh keyboard/gamepad/mouse/touch, resize, quit)");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    config_source = (ROOT / "src/sdl-config.c").read_text()
    functions = []
    for signature in ["static tutorial_mode sdl_config_gameplay_tutorial_mode_from_json(",
                      "bool get_sdl_gameplay_tutorial_enabled(", "void set_sdl_gameplay_tutorial_enabled(",
                      "tutorial_mode get_sdl_gameplay_tutorial_mode(", "void set_sdl_gameplay_tutorial_mode(",
                      "void cycle_sdl_gameplay_tutorial_mode("]:
        start = config_source.index(signature)
        end = config_source.index("{", start)
        depth = 1
        while depth:
            end += 1
            depth += (config_source[end] == "{") - (config_source[end] == "}")
        functions.append(config_source[start:end + 1])
    source.write_text(HARNESS.replace("/* CONFIG_MODE_FUNCTIONS */", "\n".join(functions)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "-Wno-unused-function", "-Wno-old-style-declaration",
                    "-O2", "-ffunction-sections", "-fdata-sections",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(ROOT / "src/cJSON.c"),
                    "_deps/SDL/libSDL3.dll.a", "-Wl,--gc-sections",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
