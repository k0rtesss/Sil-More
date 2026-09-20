$ErrorActionPreference = 'Stop'
$eventRepo = Split-Path -Parent $PSScriptRoot
$eventBuild = Join-Path $eventRepo 'build-standard/cave-events-test'
$eventOldPath = $env:PATH
Push-Location $eventRepo
try {
    New-Item -ItemType Directory -Force $eventBuild | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$eventRepo\build-standard\_deps\SDL;$eventOldPath"
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -flto -fwhole-program -DUSE_SDL `
        -I src -I external/SDL/include scripts/tests/cave-events-test.c `
        src/variable.c src/tables.c src/cave/cave-events.c src/cave/cave-water-flow.c `
        build-standard/_deps/SDL/libSDL3.dll.a -o "$eventBuild/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'World sounds test compilation failed.' }
    & "$eventBuild/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'World sounds test failed.' }
} finally { $env:PATH = $eventOldPath; Pop-Location }
