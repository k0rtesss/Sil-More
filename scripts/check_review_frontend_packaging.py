#!/usr/bin/env python3
"""Check malformed SDL config lifetimes and actual release asset staging.

Requires current Windows build objects. Uses isolated generated files only;
does not run the release builders or modify installed game data.
"""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/review-frontend-packaging"

CONFIG = r'''
#include "angband.h"
#include "sdl-config.h"
#include "log/log.h"
#include <assert.h>
static void *released_input;
static bool saw_parse_error;
/* Poison logical releases but defer deallocation until the call returns.
 * This exposes a freed-buffer diagnostic without executing undefined reads. */
static void checked_free(void *ptr)
{
    assert(ptr && !released_input);
    released_input = ptr;
    memset(ptr, 'Z', strlen(ptr));
}
#define free checked_free
#include "sdl-config.c"
#undef free
static void observe_log(log_Event *ev)
{
    if (strcmp(ev->fmt, "JSON parse error before: %s")) return;
    assert(!released_input);
    va_list ap;
    va_copy(ap, ev->ap);
    const char *error = va_arg(ap, const char *);
    va_end(ap);
    assert(error && !strncmp(error, "INVALID", 7));
    saw_parse_error = true;
}
int main(void)
{
    struct sdl_config cfg;
    log_set_quiet(true);
    log_add_callback(observe_log, NULL, LOG_ERROR);
    for (int i = 0; i < 20; ++i) {
        FILE *file = fopen("settings.json", "wb");
        assert(file);
        fputs("{\"sdl\":INVALID}", file);
        fclose(file);
        released_input = NULL;
        saw_parse_error = false;
        assert(sdl_config_load("settings.json", &cfg, NULL, 0, NULL)
            == SDL_CONFIG_LOAD_PARSE_FAILED);
        assert(saw_parse_error && released_input);
        free(released_input);

        file = fopen("settings.json", "wb");
        assert(file);
        fputs("{\"sdl\":{\"mainViewScale\":3}}", file);
        fclose(file);
        released_input = NULL;
        saw_parse_error = false;
        assert(sdl_config_load("settings.json", &cfg, NULL, 0, NULL)
            == SDL_CONFIG_LOAD_OK);
        assert(!saw_parse_error && released_input && cfg.main_view_scale == 3);
        free(released_input);
    }
    puts("SDL config: malformed-input diagnostic before release, valid-input reload, repeated recovery PASS");
    return 0;
}
'''

STAGING = r'''
param([string]$Builder, [string]$SourceRoot, [string]$Destination,
      [string]$FixtureRoot)
$ErrorActionPreference = 'Stop'
$resolvedRoot = [IO.Path]::GetFullPath($FixtureRoot).TrimEnd('\') + '\'
foreach ($candidate in @($SourceRoot, $Destination)) {
    $resolved = [IO.Path]::GetFullPath($candidate)
    if (-not $resolved.StartsWith($resolvedRoot,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Fixture path escapes its temporary workspace: $resolved"
    }
}
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    $Builder, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw 'Release builder has syntax errors' }
$removeWav = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Remove-WavFilesRecursive'
}, $false)
$copyLoop = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.ForEachStatementAst] -and
        $node.Variable.VariablePath.UserPath -eq 'folder' -and
        $node.Condition.Extent.Text -eq '$libFoldersToCopy'
}, $false)
if (-not $removeWav -or -not $copyLoop) { throw 'Release asset staging not found' }
# Execute the production staging loop with isolated fixtures, including its
# real cleanup. Exclude build/deployment/cover-art/archive side effects.
Invoke-Expression $removeWav.Extent.Text
$libSourceRoot = $SourceRoot
$releaseLibPath = $Destination
$libPath = $Destination
$libFoldersToCopy = @('xtra')
$foldersCopied = 0
$musicFilesCopied = 0
New-Item -ItemType Directory -Path $Destination | Out-Null
Invoke-Expression $copyLoop.Extent.Text
'''


def check_config():
    source = OUT / "config.c"
    source.write_text(CONFIG, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl-config.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *(str(BUILD / "_deps" / name) for name in
          ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), env["PATH"]])
    exe = OUT / "config.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp",
                    str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                   cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=15)


def check_staging():
    runner = OUT / "stage.ps1"
    runner.write_text(STAGING, encoding="utf-8")
    original = ROOT / "lib/xtra/graf"
    with tempfile.TemporaryDirectory(prefix="assets-", dir=OUT) as temporary:
        fixture = Path(temporary).resolve()
        assert fixture.is_relative_to(OUT.resolve())
        src = fixture / "source"
        shutil.copytree(original, src / "xtra/graf")
        for name in ("font/InputMono-Bold.ttf", "font/OFL-test.ttf",
                     "sound/packs/reference/sample.ogg", "sound/runtime.ogg",
                     "sound/legacy.wav"):
            path = src / "xtra" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture")
        for builder in ("create-release-build.ps1", "create-portable-release-build.ps1"):
            dst = fixture / builder.removesuffix(".ps1")
            subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                            "-File", str(runner), "-Builder", str(ROOT / builder),
                            "-SourceRoot", str(src), "-Destination", str(dst),
                            "-FixtureRoot", str(fixture)], check=True,
                           capture_output=True, text=True, timeout=30)
            for path in original.rglob("*"):
                if path.is_file():
                    assert (dst / "xtra/graf" / path.relative_to(original)).read_bytes() == path.read_bytes(), path.name
            assert not (dst / "xtra/font/InputMono-Bold.ttf").exists()
            assert (dst / "xtra/font/OFL-test.ttf").exists()
            assert not (dst / "xtra/sound/packs").exists()
            assert not (dst / "xtra/sound/legacy.wav").exists()
            assert (dst / "xtra/sound/runtime.ogg").exists()
            print(f"{builder}: complete graphics/license bytes retained; existing font/audio exclusions PASS")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    check_config()
    check_staging()


if __name__ == "__main__":
    main()
