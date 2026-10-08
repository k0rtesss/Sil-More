#!/usr/bin/env python3
"""Real GameControl reply publication under Windows reader contention.

--baseline restores only the old 100ms shutdown retry window. Normal replies
retry without replaying input; shutdown waits up to two seconds or the request
deadline. A reader held beyond that bound can still prevent final delivery.
"""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess
import tempfile
import threading
import time

import check_game_control as runner
from check_control_screenshot_sharing import reader

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/qa9-control-response-sharing'


def build(baseline):
    OUT.mkdir(parents=True, exist_ok=True)
    harness = runner.HARNESS.replace('    int last = 0, choice',
        '    int input_count = 0;\n    int last = 0, choice', 1)
    harness = harness.replace('"LAST=%d MENU=%d CONFIRM=%d CUSTOM=%d NAME=%s"',
        '"COUNT=%d LAST=%d MENU=%d CONFIRM=%d CUSTOM=%d NAME=%s"', 1)
    harness = harness.replace('            last, choice, confirm, custom, name);',
        '            input_count, last, choice, confirm, custom, name);', 1)
    harness = harness.replace('        last = (unsigned char)inkey();',
        '        last = (unsigned char)inkey();\n        input_count++;', 1)
    # Record actual dispatches and close directly, just like the real welcome
    # screen. An extra input-wait poll could prematurely create a non-closed reply.
    harness = harness.replace('            (void)sdl_control_poll(true);', r'''
            char path[1200], count[32];
            assert(path_build(path,sizeof(path),profile,"input-count.txt"));
            strnfmt(count,sizeof(count),"%d",input_count);
            SDL_IOStream* io=SDL_IOFromFile(path,"wb"); assert(io);
            assert(SDL_WriteIO(io,count,strlen(count))==strlen(count));
            assert(SDL_CloseIO(io));''', 1)
    source = OUT / 'check.c'
    source.write_text(harness, encoding='utf-8')
    control = ROOT / 'src/sdl/control/sdl-control.c'
    if baseline:
        current = control.read_text(encoding='utf-8')
        start = current.index('        /* Readers can hold the previous reply across shutdown.')
        end = current.index('            SDL_Delay(10);', start)
        current = current[:start] + '        for (int i = 0; control_response && i < 10; i++) {\n' + current[end:]
        control = OUT / 'control-baseline.c'
        control.write_text(current, encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
        if not p.endswith(('/src/main.c.obj', '/src/sdl/control/sdl-control.c.obj'))))
    env = os.environ.copy()
    env.pop('SIL_MORE_CONTROL_DIR', None)
    env['PATH'] = os.pathsep.join([*(str(BUILD / '_deps' / p) for p in
        ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    env.update(SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software',
        SDL_AUDIO_DRIVER='dummy', CONTROL_TEST_ASSETS=str(ROOT / 'lib'))
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), str(control),
        '@' + str(response), '@CMakeFiles/sil-more.dir/linkLibs.rsp', '-o', str(exe)],
        cwd=BUILD, env=env, check=True)
    return exe, env


def run_case(exe, env, name, release_after, timeout=5, normal=False):
    with tempfile.TemporaryDirectory(prefix=name + '-', dir=OUT) as data:
        profile = Path(data).resolve()
        session_env = dict(env, CONTROL_TEST_PROFILE=str(profile))
        control = runner.GameControl(profile / 'control', timeout=timeout)
        process = subprocess.Popen([str(exe), '--windowed', '--control-dir',
            str(control.directory)], cwd=ROOT, env=session_env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        close = None
        timer = None
        try:
            runner.wait_ready(control, process)
            if normal:
                close = reader(control.directory / 'response.json')
                timer = threading.Timer(.15, close)
                timer.start()
                response = control.key('x', capture=False)
                timer.join(); timer = None; close = None
                assert response['ok'] and response['input_applied']
                assert 'COUNT=1 ' in runner.terminal(response), response
                assert 'COUNT=1 ' in runner.terminal(control.observe(capture=False))
            before = json.loads((control.directory / 'response.json').read_text())
            close = reader(control.directory / 'response.json')
            if release_after is not None:
                timer = threading.Timer(release_after, close)
                timer.start()
            start = time.monotonic()
            if release_after is not None:
                response = control.key('q', capture=False)
                assert response['ok'] and response['closed'] and response['input_applied'], response
                assert response['id'] != before['id']
            else:
                try:
                    control.key('q', capture=False)
                except runner.ControlError as exc:
                    assert 'session has stopped' in str(exc), exc
                else:
                    raise AssertionError('Permanently locked final reply falsely succeeded')
                assert json.loads((control.directory / 'response.json').read_text())['id'] == before['id']
            elapsed = time.monotonic() - start
            process.wait(timeout=3)
            assert int((profile / 'input-count.txt').read_text()) == (2 if normal else 1)
            assert json.loads((control.directory / 'session.json').read_text())['running'] is False
            if release_after is None:
                assert (1.8 <= elapsed < 3.5) if timeout == 5 else (.15 <= elapsed < .8), elapsed
            print(f'{name}: exactly-once dispatch, elapsed={elapsed:.3f}s PASS')
        finally:
            if timer:
                timer.join(); close = None
            if close:
                close()
            if process.poll() is None:
                process.kill(); process.wait(timeout=3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        raise SystemExit('This regression exercises Windows reader sharing.')
    exe, env = build(args.baseline)
    run_case(exe, env, 'normal-and-transient-shutdown', .25, normal=True)
    run_case(exe, env, 'one-second-shutdown-reader', 1.0)
    run_case(exe, env, 'permanent-shutdown-lock', None)
    run_case(exe, env, 'request-deadline', None, timeout=.6)
    print('GameControl final response survives transient reader contention; permanent locks/deadlines remain bounded PASS.')


if __name__ == '__main__':
    main()
