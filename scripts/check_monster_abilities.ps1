$ErrorActionPreference = 'Stop'
$abilityRepo = Split-Path -Parent $PSScriptRoot
$abilityBuild = Join-Path $abilityRepo 'build-standard'
$abilityTestDir = Join-Path $abilityBuild 'monster-abilities-test'
$abilityOldPath = $env:PATH
Push-Location $abilityRepo
try {
    New-Item -ItemType Directory -Force $abilityTestDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$abilityBuild\_deps\SDL;$abilityOldPath"
    foreach ($abilityLane in @('WRITER', 'READER')) {
        & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -DUSE_SDL `
            -I src -I external/SDL/include "-DABILITY_SAVE_$abilityLane" `
            -c scripts/tests/monster-abilities-test.c -o "$abilityTestDir/$abilityLane.o"
        if ($LASTEXITCODE -ne 0) { throw "Monster save $abilityLane fixture compilation failed." }
    }
    # Exercise the production state machine without linking the frontend.
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include `
        scripts/tests/monster-abilities-test.c src/monster/monster-abilities.c `
        src/monster/monster-ai.c `
        src/variable.c src/tables.c src/rng.c `
        "$abilityTestDir/WRITER.o" "$abilityTestDir/READER.o" `
        build-standard/_deps/SDL/libSDL3.dll.a -o "$abilityTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster ability test compilation failed.' }
    & "$abilityTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster ability regression test failed.' }
}
finally {
    $env:PATH = $abilityOldPath
    Pop-Location
}
