#!/usr/bin/env python3
"""Check complete save writes and failed-save recovery in temporary profiles."""
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
static bool fail_activation;
static bool active_save_at_commit;
static unsigned activation_attempts;

bool __real_fd_move(cptr from, cptr to);
bool __wrap_fd_move(cptr from, cptr to)
{
    if (!strcmp(from, "flush-save.new") && !strcmp(to, "flush-save")) {
        activation_attempts++;
        active_save_at_commit = SDL_GetPathInfo(to, NULL);
        if (fail_activation)
            return false;
    }
    return __real_fd_move(from, to);
}

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
    SDL_free(kept_save);
    SDL_PathInfo info;
    assert(!SDL_GetPathInfo("flush-save.new",&info));

    fail_flush=false; SDL_ClearError();
    character_saved = false;
    fail_activation = true;
    assert(!save_player());
    assert(activation_attempts == 2);
    assert(active_save_at_commit);
    assert(!character_saved);
    kept_save = SDL_LoadFile(savefile, &kept_size);
    assert(kept_save && kept_size == old_size && !memcmp(kept_save, old_save, old_size));
    SDL_free(kept_save);

    ++turn;
    fail_activation = false;
    assert(save_player() && flush_diagnostics == 1);
    assert(activation_attempts == 3 && active_save_at_commit && character_saved);
    size_t new_size = 0;
    void* new_save = SDL_LoadFile(savefile, &new_size);
    assert(new_save && (new_size != old_size || memcmp(new_save, old_save, old_size)));
    assert(!SDL_GetPathInfo("flush-save.new", &info));
    assert(!SDL_GetPathInfo("flush-save.old", &info));
    SDL_free(new_save);
    SDL_free(old_save);

    puts("Save recovery: complete first save, failed flush and activation preserve the live save, and retry replaces it: PASS");
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
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl,--wrap=SDL_FlushIO,--wrap=fd_move",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), profile],
                       cwd=profile, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
