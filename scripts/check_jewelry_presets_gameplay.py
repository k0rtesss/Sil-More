#!/usr/bin/env python3
"""Exercise Jewelry Sets shortcuts through real SDL desktop key events."""

from contextlib import contextmanager

import check_player_systems_gameplay as gameplay


OUT = gameplay.ROOT / "scripts/output/jewelry-presets-gameplay"
WORN = [
    {"kind": 150, "slot": 27},  # Ring of Secrets, left hand
    {"kind": 156, "slot": 28},  # Ring of Frost, right hand
    {"kind": 135, "slot": 29},  # Amulet of Regeneration
]
WORN_LABELS = ("Ring of Secrets", "Ring of Frost", "Amulet of Regeneration")


@contextmanager
def scene(name, spec, executable, env):
    profile = gameplay.start(name, spec, executable, env)
    print(f"{name}: {profile}", flush=True)

    def act(*keys):
        if not keys:
            return gameplay.act(profile)
        for key in keys:
            record = gameplay.act(profile, "key", key)
        return record

    try:
        yield profile, act
    finally:
        process = gameplay._fixture_processes.pop(profile)
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=10)


def state(record):
    return record["response"]["state"]


def terminal(record):
    return "\n".join(state(record)["terminal"])


def open_presets(act):
    record = act("i", "j", "Down")
    assert "[Jewelry]" in terminal(record) and "Jewelry Sets" in terminal(record)
    assert "s save" in terminal(record)
    return record


def main():
    gameplay.OUT = OUT
    default_bindings = (
        "sdl_config_set_default_movement_bindings(&config,SDL_MOVEMENT_PRESET_CLASSIC_SIL);"
    )
    assert gameplay.FIXTURE.count(default_bindings) == 2, "Fixture input setup changed"
    fixture = gameplay.FIXTURE.replace(
        default_bindings,
        'sdl_config_set_default_movement_bindings(&config, number(spec,"wasd",0)\n'
        '        ? SDL_MOVEMENT_PRESET_MODERN_WASD_QEZC : SDL_MOVEMENT_PRESET_CLASSIC_SIL);\n'
        '    op_ptr->main_combat_rolls=3;',
    )
    executable, env = gameplay.build(fixture=fixture)

    spec = dict(items=WORN, skills=50, targets=[dict(race=11, y=12, x=13, hp=200)])
    with scene("desktop", spec, executable, env) as (profile, act):
        initial = act()["telemetry"]
        assert "Set 1: <empty>" in terminal(open_presets(act))
        saved = act("s")
        assert "Jewelry set 1 saved." in state(saved)["messages"], "First lowercase s must save"
        assert "Set 1 [equipped]" in terminal(saved)
        assert all(label in terminal(saved) for label in WORN_LABELS)

        saved = act("Down", "s")
        assert "Jewelry set 2 saved." in state(saved)["messages"]
        assert "Set 2 [equipped]" in terminal(saved)

        # The first name key works even after returning focus to the groups.
        prompt = act("Up", "Left", "n")
        assert state(prompt)["text_input"] or "Jewelry set name" in terminal(prompt)
        gameplay.act(profile, "text", "Night watch")
        renamed = act("Enter")
        assert "1 Night watch [equipped]" in terminal(renamed)

        # Cancel and accept the existing replacement confirmation.
        canceled = act("Left", "s", "Escape")
        assert "1 Night watch [equipped]" in terminal(canceled)
        assert "Night watch saved." not in state(canceled)["messages"]
        replaced = act("s", "y")
        assert "Night watch saved." in state(replaced)["messages"]

        # Cancel and confirm clear while leaving the second preset intact.
        canceled = act("Left", "c", "Escape")
        assert "1 Night watch [equipped]" in terminal(canceled)
        cleared = act("c", "y")
        assert "Night watch cleared." in state(cleared)["messages"]
        assert "1 Night watch: <empty>" in terminal(cleared)
        assert "Set 2 [equipped]" in terminal(cleared)

        closed = act("Escape")
        assert not state(closed)["modal"]
        assert closed["telemetry"]["player_turn"] == initial["player_turn"]
        assert closed["telemetry"]["items"] == initial["items"]
        act("Ctrl+s")
        checkpoint = profile / "save/Player_Audit"
        assert checkpoint.is_file()

        assert "[Equipped]" in terminal(act("e"))
        assert not state(act("Escape"))["modal"]
        assert "[Inventory]" in terminal(act("i"))
        assert not state(act("Escape"))["modal"]
        assert state(act("l"))["modal"]
        assert not state(act("Escape"))["modal"]
        combat = act("Right")
        assert combat["telemetry"]["targets"][0]["hp"] < initial["targets"][0]["hp"]
        assert "Combat" in terminal(act("Ctrl+q"))
        assert not state(act("Escape"))["modal"]

        with scene("reloaded", dict(load_from=str(checkpoint)), executable, env) as (_, reload_act):
            restored = terminal(open_presets(reload_act))
            assert "1 Night watch: <empty>" in restored
            assert "Set 2 [equipped]" in restored
            assert all(label in restored for label in WORN_LABELS)

    with scene("incomplete", dict(items=WORN[:2]), executable, env) as (_, act):
        open_presets(act)
        rejected = act("s")
        assert ("Wear two rings and an amulet before saving a jewelry set."
                in state(rejected)["messages"])
        assert "Set 1: <empty>" in terminal(rejected)

    with scene("wasd", dict(items=WORN, wasd=1), executable, env) as (_, act):
        open_presets(act)
        assert "Jewelry set 1 saved." in state(act("s"))["messages"]

    print("Jewelry Sets save (Classic and WASD), rename, replace, clear, reload, and incomplete-set checks passed.")


if __name__ == "__main__":
    main()
