#!/usr/bin/env python3
"""Check complete save writes and failed-flush recovery in temporary profiles."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/save-flush-check"

TESTS = r'''
static bool fail_flush;
static int flush_diagnostics;
bool __real_SDL_FlushIO(SDL_IOStream* stream);
bool __wrap_SDL_FlushIO(SDL_IOStream* stream)
{
    if (fail_flush) {
        SDL_SetError("Simulated save flush failure");
        return false;
    }
    return __real_SDL_FlushIO(stream);
}
static void count_flush_diagnostic(log_Event* event)
{
    if (strstr(event->file,"save.c") && strstr(event->fmt,"flush"))
        flush_diagnostics++;
}
static void check_save_flush(void)
{
    reset_map(1);
    log_set_quiet(true);
    assert(log_add_callback(count_flush_diagnostic,NULL,LOG_WARN)==0);
    SDL_strlcpy(savefile,"flush-save",sizeof(savefile));
    assert(save_player());
    assert(flush_diagnostics==0);
    size_t old_size=0;
    void* old_save=SDL_LoadFile(savefile,&old_size);
    assert(old_save && old_size>1000);

    fail_flush=true;
    assert(!save_player() && flush_diagnostics==1);
    size_t kept_size=0;
    void* kept_save=SDL_LoadFile(savefile,&kept_size);
    assert(kept_save && kept_size==old_size && !memcmp(kept_save,old_save,old_size));
    SDL_free(kept_save); SDL_free(old_save);
    SDL_PathInfo info;
    assert(!SDL_GetPathInfo("flush-save.new",&info));

    fail_flush=false; SDL_ClearError();
    assert(save_player() && flush_diagnostics==1);
    puts("Save flush: successful complete write, failed flush rejected, previous save preserved, broken candidate removed, retry succeeds: PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static const char* guids[]")]
    init = HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + fixture_function("terminal_extra") + "\n" +
                      fixture_function("reset_map") + TESTS + init +
                      "    check_save_flush(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [obj for obj in objects if not obj.endswith(("/src/main.c.obj", "/src/fs/save.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/fs/save.c"), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=SDL_FlushIO",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile],
                       cwd=profile, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
