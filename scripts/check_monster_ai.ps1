$ErrorActionPreference = 'Stop'
$aiRepo = Split-Path -Parent $PSScriptRoot
$aiBuild = Join-Path $aiRepo 'build-standard'
$aiTestDir = Join-Path $aiBuild 'monster-ai-test'
$aiOldPath = $env:PATH
Push-Location $aiRepo
try {
    New-Item -ItemType Directory -Force $aiTestDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$aiBuild\_deps\SDL;$aiOldPath"
    # Link real engine modules; LTO omits unrelated game and UI entry points.
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include `
        scripts/tests/monster-ai-test.c src/variable.c src/tables.c src/rng.c `
        src/melee/melee-util.c src/melee/melee-movement-path.c `
        src/cave/cave-flow.c src/cave/cave-water.c `
        src/cave/cave-geometry.c src/ui/targeting/direction.c src/cmd/movement/cmd-run.c `
        build-standard/_deps/SDL/libSDL3.dll.a -o "$aiTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster AI test compilation failed.' }
    & "$aiTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster AI regression test failed.' }
}
finally {
    $env:PATH = $aiOldPath
    Pop-Location
}
