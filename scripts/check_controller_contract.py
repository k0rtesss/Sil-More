#!/usr/bin/env python3
"""Static regression guard for Sil-More's semantic controller UI contract."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    opening = source.find("{", start)
    if opening < 0:
        raise AssertionError(f"missing function body: {signature}")

    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening : index + 1]
    raise AssertionError(f"unterminated function body: {signature}")


def require(source: str, text: str, description: str) -> None:
    if text not in source:
        raise AssertionError(f"missing {description}: {text}")


def require_order(source: str, descriptions_and_text: list[tuple[str, str]]) -> None:
    last = -1
    for description, text in descriptions_and_text:
        position = source.find(text)
        if position < 0:
            raise AssertionError(f"missing {description}: {text}")
        if position <= last:
            raise AssertionError(f"incorrect controller ownership order at {description}")
        last = position


def main() -> int:
    gamepad = read("src/sdl/input/sdl-gamepad.c")
    actions = read("src/sdl/input/sdl-player-actions.c")
    question = read("src/sdl/ui/sdl-question-menu.c")
    screen_pointer = read("src/sdl/input/sdl-screen-pointer.c")
    screens = read("src/sdl/ui/sdl-screens.c")
    character = read("src/cmd/ui/cmd-ui-character.c")

    confirm = function_body(gamepad, "bool sdl_gamepad_button_is_ui_confirm")
    back = function_body(gamepad, "bool sdl_gamepad_button_is_ui_back")
    require(confirm, "SDL_GAMEPAD_BUTTON_SOUTH", "physical South confirm")
    require(back, "SDL_GAMEPAD_BUTTON_EAST", "physical East back")

    button = function_body(gamepad, "void sdl_gamepad_handle_button")
    runtime_button = button.split("if (!config.gamepad_enabled)", 1)[1]
    require_order(
        runtime_button,
        [
            ("native minimap owner", "sdl_minimap_handle_gamepad_button"),
            ("native welcome owner", "sdl_welcome_screen_handle_gamepad_button"),
            ("native exchange owner", "sdl_player_exchange_handle_gamepad_button"),
            ("native action-wheel owner", "sdl_player_action_menu_handle_gamepad_button"),
            ("spatial contextual focus owner", "sdl_gamepad_context_focus_handle_button"),
        ],
    )

    # Avoid spelling Term_keypress('\r') in the order table, where escaping is
    # easy to misread; replace the unique explanatory comment with a sentinel.
    button_for_order = runtime_button.replace(
        "Terminal-backed prompts must receive the semantic controller action",
        "Terminal_keypress_placeholder",
    )
    require_order(
        button_for_order,
        [
            ("contextual focus", "sdl_gamepad_context_focus_handle_button"),
            ("terminal modal confirm", "Terminal_keypress_placeholder"),
            ("terminal modal back", "sdl_gamepad_back_button_is_modal"),
            ("D-pad routing", "SDL_GAMEPAD_BUTTON_DPAD_UP"),
            ("bound-button fallback", "handle_bound_button:"),
        ],
    )
    require_order(
        runtime_button,
        [
            ("modal D-pad ownership", "if (sdl_movement_input_is_modal())"),
            ("gameplay D-pad setting", "if (!config.gamepad_use_dpad)"),
        ],
    )

    collect = function_body(gamepad, "static int sdl_gamepad_context_focus_collect")
    require_order(
        collect,
        [
            ("floor-popup priority", "sdl_question_menu_context_hint_active"),
            ("dungeon-context gate", "MOVEMENT_INPUT_CONTEXT_DUNGEON"),
            ("Quick Access targets", "sdl_touch_top_panel_collect_controller_focus_targets"),
            ("left-pane targets", "sdl_character_panel_collect_controller_focus_targets"),
        ],
    )

    axis = function_body(gamepad, "void sdl_gamepad_handle_axis")
    require(axis, "g_player_action_menu.active || g_player_exchange_target.active",
        "native-overlay stick ownership")
    require(axis, "g_gamepad_state.left_ui_dir", "left-stick UI edge state")
    require(axis, "g_gamepad_state.right_ui_dir", "right-stick UI edge state")
    require(axis, "sdl_gamepad_send_ui_direction", "terminal modal stick navigation")
    require(axis, "sdl_gamepad_context_focus_move", "right-stick spatial focus")

    action_buttons = function_body(
        actions, "bool sdl_player_action_menu_handle_gamepad_button"
    )
    exchange_buttons = function_body(
        actions, "bool sdl_player_exchange_handle_gamepad_button"
    )
    if "config.gamepad_button_bindings" in action_buttons + exchange_buttons:
        raise AssertionError("native overlays still reinterpret gameplay bindings")
    require(action_buttons, "return true;", "action-wheel input consumption")
    require(exchange_buttons, "return true;", "exchange input consumption")

    require(question, "sdl_question_menu_activate_context_choice",
        "semantic floor-popup activation")
    require(screen_pointer, "SDL_CONTROLLER_FOCUS_LEFT_PANEL_ATTACK",
        "weapon and quiver focus targets")
    require(screen_pointer, "sdl_character_panel_set_controller_attack_focus",
        "weapon and quiver controller highlighting")
    require(screen_pointer, "sdl_hidden_character_panel_collect_controller_focus_targets",
        "hidden status-overlay focus targets")
    require(screen_pointer, "[A] Expand character pane. [B] Back.",
        "collapsed-pane controller action")
    require(screen_pointer, "sdl_status_line_collect_controller_focus_targets",
        "status-line focus targets")
    require(screen_pointer, "sdl_combat_overlay_collect_controller_focus_targets",
        "combat-overlay focus targets")
    require(screens, "sdl_character_sheet_live_handle_gamepad_button",
        "live character-sheet controller owner")

    character_sheet = function_body(character, "void do_cmd_character_sheet")
    if "ch == steamdeck_prev_page_key()" in character_sheet:
        raise AssertionError("L1 still triggers a non-page character-sheet action")

    for name in ("confirm", "back", "prev_page", "next_page", "info", "alt_action", "secondary"):
        semantic = function_body(gamepad, f"int steamdeck_{name}_key")
        if "get_sdl_gamepad_button_binding" in semantic:
            raise AssertionError(f"{name} still depends on a gameplay remap")
    require(collect, "sdl_movement_input_is_modal()", "modal focus exclusion")
    require(gamepad, "g_gamepad_focus_chord_pending", "stickless focus entry")
    require(gamepad, "void sdl_gamepad_context_focus_render", "visible spatial focus")
    require(question, "sdl_enqueue_bypassed_command(choice)", "remap-safe popup action")

    print("Controller semantic contract: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"Controller semantic contract: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
