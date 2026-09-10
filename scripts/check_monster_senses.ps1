$ErrorActionPreference = 'Stop'
$senseRepo = Split-Path -Parent $PSScriptRoot
$senseBuild = Join-Path $senseRepo 'build-standard'
$senseTestDir = Join-Path $senseBuild 'monster-senses-test'
$senseOldPath = $env:PATH
Push-Location $senseRepo
try {
    New-Item -ItemType Directory -Force $senseTestDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$senseBuild\_deps\SDL;$senseOldPath"
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include `
        scripts/tests/monster-senses-test.c src/variable.c src/tables.c src/rng.c `
        src/monster/monster-senses.c src/cmd/movement/cmd-run.c `
        src/melee/melee-util.c src/melee/melee-process.c src/melee/melee-movement-path.c `
        src/ui/targeting/direction.c `
        src/cave/cave-flow.c src/cave/cave-water.c src/cave/cave-geometry.c `
        build-standard/_deps/SDL/libSDL3.dll.a -o "$senseTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster senses test compilation failed.' }
    & "$senseTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster senses regression test failed.' }
}
finally {
    $env:PATH = $senseOldPath
    Pop-Location
}
