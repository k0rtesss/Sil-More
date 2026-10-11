#!/usr/bin/env python3
"""Launch an isolated real game with an SDL virtual gamepad for manual QA.

Build portable first. This links its objects with a wrapper main; production
input/control protocols stay unchanged. Use pad(profile, button=..., value=...)
or pad(profile, axis=..., value=...) and GameControl for PNG observations.
Stick axes rest at 0; virtual trigger axes rest at -32768 and press at 32767.
"""
from pathlib import Path
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from game_control import GameControl, launch

WRAPPER = r'''
#include "angband.h"
#include <SDL3/SDL.h>
#include <stdio.h>
int sil_game_main(int argc, char **argv);
static SDL_Joystick *joystick;
static SDL_AtomicInt stopping;
static int pump(void *unused)
{
    (void)unused;
    while (!SDL_GetAtomicInt(&stopping)) {
        FILE *file = fopen("pad-request.txt", "r");
        if (file) {
            char kind = 0;
            int id = 0, value = 0;
            int count = fscanf(file, " %c %d %d", &kind, &id, &value);
            fclose(file);
            remove("pad-request.txt");
            bool ok = false;
            if (count == 3) {
                if (kind == 'b') ok = SDL_SetJoystickVirtualButton(joystick, id, value != 0);
                if (kind == 'a') ok = SDL_SetJoystickVirtualAxis(joystick, id, (Sint16)value);
            }
            file = fopen("pad-reply.txt", "w");
            if (file) { fprintf(file, "%d", ok); fclose(file); }
        }
        SDL_Delay(5);
    }
    return 0;
}
int main(int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD)) return 90;
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.name = "Sil-More isolated QA virtual gamepad";
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    joystick = SDL_OpenJoystick(id);
    if (!id || !joystick) return 91;
    /* SDL maps virtual signed trigger axes to the gamepad's unsigned range. */
    if (!SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768)
        || !SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768))
        return 92;
    SDL_Thread *thread = SDL_CreateThread(pump, "QA gamepad", NULL);
    if (!thread) return 93;
    int result = sil_game_main(argc, argv);
    SDL_SetAtomicInt(&stopping, 1);
    SDL_WaitThread(thread, NULL);
    return result;
}
'''


def build_and_launch(deadzone: int = 12000, seed_profile: Path | None = None,
                     headless: bool = True) -> dict:
    build = ROOT / "build-portable"
    out = ROOT / "scripts/output/control-playtests" / ("goal-gamepad-" + uuid.uuid4().hex[:8])
    out.mkdir(parents=True)
    deploy = ROOT / "sil-more-windows-sdl3-portable"
    for file in deploy.iterdir():
        if file.is_file() and file.suffix.lower() == ".dll":
            shutil.copy2(file, out / file.name)
    for folder in ("docs", "edit", "help", "pref", "xtra"):
        if (deploy / "lib" / folder).is_dir():
            shutil.copytree(deploy / "lib" / folder, out / "lib" / folder)
    for folder in ("apex/metaruns", "data", "save", "user", "bone"):
        (out / "lib" / folder).mkdir(parents=True, exist_ok=True)
    if seed_profile is not None:
        seed_profile = seed_profile.resolve()
        # Only isolated QA profiles may seed continuation scenarios.
        assert seed_profile.parent == out.parent and seed_profile.name.startswith("goal-gamepad-")
        for folder in ("apex", "save"):
            shutil.copytree(seed_profile / "lib" / folder, out / "lib" / folder, dirs_exist_ok=True)
    (out / "screens").mkdir()
    (out / "lib/user/sil_sdl.json").write_text(json.dumps({
        "sdl": {"inputUiMode": "CONTROLLER", "desktopInputChoice": "CONTROLLER"},
        "gamepad": {"enabled": True, "deadzone": deadzone}}))
    source = out / "wrapper.c"
    source.write_text(WRAPPER)
    objects = shlex.split((build / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    rsp = out / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects))
    env = os.environ.copy()
    env["PATH"] = "C:/msys64/mingw64/bin;C:/msys64/usr/bin;" + env["PATH"]
    cc = "C:/msys64/mingw64/bin/cc.exe"
    flags = ["-DUSE_SDL", "-DSIL_USE_LOCAL_DATA", "-std=c17", "-O0",
             "@CMakeFiles/sil-more.dir/includes_C.rsp"]
    main_object = out / "main.obj"
    subprocess.run([cc, *flags, "-Dmain=sil_game_main", "-c", str(ROOT / "src/main.c"),
                    "-o", str(main_object)], cwd=build, env=env, check=True)
    exe = out / "sil-more.exe"
    subprocess.run([cc, *flags, str(source), str(main_object), "@" + str(rsp),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=build, env=env, check=True)
    control = GameControl(out / "control", timeout=30)
    result = launch(control, exe, headless=headless, wizard=False)
    result.update(profile=str(out), control=str(control.directory))
    (out / "playtest.json").write_text(json.dumps(result, indent=2))
    return result


def pad(profile: str | Path, *, button: int | None = None,
        axis: int | None = None, value: int = 1, settle: float = .12) -> None:
    profile = Path(profile)
    reply = profile / "pad-reply.txt"
    reply.unlink(missing_ok=True)
    request = profile / "pad-request.txt"
    temp = profile / "pad-temp.txt"
    temp.write_text(f"{'b' if button is not None else 'a'} {button if button is not None else axis} {value}")
    temp.replace(request)
    deadline = time.monotonic() + 5
    while not reply.exists():
        if time.monotonic() > deadline:
            raise TimeoutError("Virtual controller did not acknowledge input")
        time.sleep(.01)
    assert reply.read_text() == "1"
    time.sleep(settle)


def tap(profile: str | Path, button: int) -> None:
    pad(profile, button=button)
    pad(profile, button=button, value=0)


if __name__ == "__main__":
    print(json.dumps(build_and_launch(int(sys.argv[1]) if len(sys.argv) > 1 else 12000), indent=2))

