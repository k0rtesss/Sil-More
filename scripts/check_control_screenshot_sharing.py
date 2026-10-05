#!/usr/bin/env python3
"""Exercise actual GameControl screenshot publication with Windows reader locks.

Runs the production SDL endpoint in an isolated software-renderer harness.
--baseline removes only screenshot rename retries to reproduce the reported error.
"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import os
import shlex
import subprocess
import struct
import tempfile
import threading
import time
import check_game_control as runner

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/control-screenshot-sharing'

def build(baseline):
    OUT.mkdir(parents=True,exist_ok=True)
    harness=runner.HARNESS.replace('    int last = 0, choice', '    int input_count = 0;\n    int last = 0, choice',1)
    harness=harness.replace('"LAST=%d MENU=%d CONFIRM=%d CUSTOM=%d NAME=%s"',
        '"COUNT=%d LAST=%d MENU=%d CONFIRM=%d CUSTOM=%d NAME=%s"',1)
    harness=harness.replace('            last, choice, confirm, custom, name);',
        '            input_count, last, choice, confirm, custom, name);',1)
    harness=harness.replace('        last = (unsigned char)inkey();',
        '        last = (unsigned char)inkey();\n        input_count++;',1)
    source=OUT/'check.c';source.write_text(harness,encoding='utf-8')
    control=ROOT/'src/sdl/control/sdl-control.c'
    if baseline:
        text=control.read_text(encoding='utf-8')
        call='IMG_SavePNG(surface, temp) && control_publish_screenshot(temp, path)'
        assert text.count(call)==1
        control=OUT/'control-baseline.c'
        control.write_text(text.replace(call,'IMG_SavePNG(surface, temp) && SDL_RenamePath(temp, path)',1),encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects
        if not p.endswith(('/src/main.c.obj','/src/sdl/control/sdl-control.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env.pop('SIL_MORE_CONTROL_DIR',None)
    env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p)for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    env.update(SDL_VIDEO_DRIVER='dummy',SDL_RENDER_DRIVER='software',SDL_AUDIO_DRIVER='dummy',CONTROL_TEST_ASSETS=str(ROOT/'lib'))
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',
        str(source),str(control),'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-o',str(exe)],cwd=BUILD,env=env,check=True)
    return exe,env

def reader(path):
    """An ordinary reader that permits reads/writes, but not delete/rename."""
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,
        wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    kernel.CreateFileW.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE];kernel.CloseHandle.restype=wintypes.BOOL
    handle=kernel.CreateFileW(str(path),0x80000000,0x1|0x2,None,3,0x80,None)
    if handle==ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    return lambda: kernel.CloseHandle(handle)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def count(response,number):
    assert f'COUNT={number} ' in runner.terminal(response),response

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
    if os.name!='nt':raise SystemExit('This regression exercises Windows file-sharing behavior.')
    exe,env=build(args.baseline)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT)as data:
        profile=Path(data).resolve();env['CONTROL_TEST_PROFILE']=str(profile)
        control=runner.GameControl(profile/'control',timeout=10)
        process=subprocess.Popen([str(exe),'--windowed','--control-dir',str(control.directory)],cwd=ROOT,env=env,
            stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try:
            runner.wait_ready(control,process)
            first=control.observe();assert first['ok'];count(first,0)
            screenshot=control.directory/'screenshot.png';temp=control.directory/'screenshot.png.tmp'
            before=digest(screenshot)
            close=reader(screenshot)
            release=threading.Timer(0.15,close);release.start()
            try:
                response=control.key('x',output=profile/'transient.png')
            finally:release.join()
            assert response['ok'] and response['input_applied'],response
            count(response,1);assert digest(screenshot)!=before and not temp.exists()
            assert (profile/'transient.png').read_bytes().startswith(b'\x89PNG\r\n\x1a\n')
            assert struct.unpack('>II',(profile/'transient.png').read_bytes()[16:24]) == (
                response['state']['width'],response['state']['height'])
            observation=control.observe(capture=False);assert observation['ok'];count(observation,1)
            before=digest(screenshot);close=reader(screenshot)
            try:
                started=time.monotonic();response=control.key('a');elapsed=time.monotonic()-started
                assert not response['ok'] and response['input_applied'],response
                assert 'Screenshot capture failed' in response['error'] and 'screenshot' not in response,response
                count(response,2);assert 0.20<=elapsed<5 and digest(screenshot)==before and not temp.exists()
            finally:close()
            recovery=control.observe();assert recovery['ok'] and not recovery['input_applied'];count(recovery,2)
            screenshot.unlink();screenshot.mkdir()
            try:
                started=time.monotonic();failure=control.observe();elapsed=time.monotonic()-started
                assert not failure['ok'] and not failure['input_applied'],failure
                assert elapsed<5 and screenshot.is_dir() and not temp.exists()
                count(failure,2)
            finally:screenshot.rmdir()
            recovery=control.observe();assert recovery['ok'];count(recovery,2)
            assert control.key('q',capture=False)['closed'];process.wait(timeout=10)
            print('Actual GameControl: transient reader publication recovers; permanent lock/directory fails boundedly; fresh PNG and exactly-once input preserved PASS.')
        finally:
            if process.poll()is None:
                try:control.key('q',capture=False);process.wait(timeout=5)
                except Exception:process.kill();process.wait(timeout=5)

if __name__=='__main__':main()
