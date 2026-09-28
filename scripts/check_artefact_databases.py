"""Exercise the production artefact database openers in temporary directories."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/artefact-databases"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    for name, module, defines in (
        ("score", "score/score_artefact", []),
        ("memory", "metarun/metarun-artefact-memory", ["-DTEST_MEMORY_DB"]),
    ):
        rsp = OUT / f"{name}.rsp"
        rsp.write_text("\n".join('"' + p + '"' for p in objects
                       if not p.endswith(("/src/main.c.obj", f"/src/{module}.c.obj"))))
        exe = OUT / f"{name}.exe"
        subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                        *defines, "@CMakeFiles/sil-more.dir/includes_C.rsp",
                        str(ROOT / "scripts/tests/artefact-database-test.c"), "@" + str(rsp),
                        "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-o", str(exe)],
                       cwd=BUILD, env=env, check=True)
        with tempfile.TemporaryDirectory(dir=OUT) as temporary:
            subprocess.run([str(exe), temporary], cwd=ROOT, env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()
