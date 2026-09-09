$ErrorActionPreference = 'Stop'
$monsterRepo = Split-Path -Parent $PSScriptRoot
$monsterBuild = Join-Path $monsterRepo 'build-standard'
$monsterTestDir = Join-Path $monsterBuild 'monster-sound-test'
$monsterOldPath = $env:PATH

Push-Location $monsterRepo
try {
    python scripts/check_monster_sound_assignments.py
    if ($LASTEXITCODE -ne 0) { throw 'Monster sound assignments do not match monster data.' }
    New-Item -ItemType Directory -Force $monsterTestDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$monsterBuild\_deps\SDL;$monsterBuild\_deps\SDL_mixer;$monsterOldPath"
    # Whole-program LTO omits unrelated game/UI entry points from this harness.
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include -I external/SDL_mixer/include `
        scripts/tests/monster-sound-test.c src/support/feedback.c src/cJSON.c src/sound-config.c `
        build-standard/_deps/SDL/libSDL3.dll.a `
        build-standard/_deps/SDL_mixer/libSDL3_mixer.dll.a `
        -o "$monsterTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster sound test compilation failed.' }
    $monsterFiles = Get-ChildItem lib/xtra/sound/monsters -Recurse -File -Filter '*.ogg' |
        ForEach-Object FullName
    & "$monsterTestDir/test.exe" @monsterFiles
    if ($LASTEXITCODE -ne 0) { throw 'Monster sound test failed.' }
}
finally {
    $env:PATH = $monsterOldPath
    Pop-Location
}
