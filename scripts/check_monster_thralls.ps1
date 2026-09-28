$ErrorActionPreference = 'Stop'
$thrallRepo = Split-Path -Parent $PSScriptRoot
$thrallBuild = Join-Path $thrallRepo 'build-standard'
$thrallTestDir = Join-Path $thrallBuild 'monster-thrall-test'
$thrallOldPath = $env:PATH
Push-Location $thrallRepo
try {
    New-Item -ItemType Directory -Force $thrallTestDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$thrallBuild\_deps\SDL;$thrallOldPath"
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include `
        scripts/tests/monster-thrall-test.c src/monster/monster-thrall.c `
        src/variable.c src/tables.c src/rng.c src/ui/targeting/direction.c `
        build-standard/_deps/SDL/libSDL3.dll.a -o "$thrallTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Thrall test compilation failed.' }
    & "$thrallTestDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Thrall regression test failed.' }
}
finally {
    $env:PATH = $thrallOldPath
    Pop-Location
}
