$ErrorActionPreference = 'Stop'
$learningRepo = Split-Path -Parent $PSScriptRoot
$learningDir = Join-Path $learningRepo 'build-standard/monster-learning-test'
$learningOldPath = $env:PATH
Push-Location $learningRepo
try {
    New-Item -ItemType Directory -Force $learningDir | Out-Null
    $env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;$learningOldPath"
    & C:\msys64\mingw64\bin\gcc.exe -std=c17 -O1 -Wall -Wextra -Werror -DUSE_SDL `
        -I src -I external/SDL/include scripts/tests/monster-learning-test.c `
        src/monster/monster-ai.c -o "$learningDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster learning test compilation failed.' }
    & "$learningDir/test.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Monster learning regression test failed.' }
}
finally {
    $env:PATH = $learningOldPath
    Pop-Location
}
