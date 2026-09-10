$ErrorActionPreference = 'Stop'
$combatRepo = Split-Path -Parent $PSScriptRoot
$combatDir = Join-Path $combatRepo 'build-standard/monster-ai-combat-test'
$combatOldPath = $env:PATH
Push-Location $combatRepo
try {
    New-Item -ItemType Directory -Force $combatDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$combatRepo\build-standard\_deps\SDL;$combatOldPath"
    foreach ($combatLane in @('PROCESS', 'MELEE')) {
        & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -DUSE_SDL `
            -I src -I external/SDL/include "-DCOMBAT_${combatLane}_SELECTOR" `
            -c scripts/tests/monster-ai-combat-test.c -o "$combatDir/$combatLane.o"
        if ($LASTEXITCODE -ne 0) { throw "Monster AI $combatLane selector compilation failed." }
    }
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include scripts/tests/monster-ai-combat-test.c `
        src/monster/monster-ai.c src/monster/monster-abilities.c src/monster/monster-tactics.c `
        src/player/player-song-monster.c src/support/geometry.c src/cave/cave-geometry.c `
        src/melee/melee-attack-ranged.c "$combatDir/PROCESS.o" "$combatDir/MELEE.o" `
        src/variable.c src/tables.c build-standard/_deps/SDL/libSDL3.dll.a -o "$combatDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster AI combat fixture compilation failed.' }
    & "$combatDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster AI combat fixture failed.' }
}
finally {
    $env:PATH = $combatOldPath
    Pop-Location
}
