# Copyright (C) 2025-2026 Sil-More contributors
#
# This file is part of Sil-More.
#
# Sil-More is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Sil-More is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
# for more details.

param(
    [ValidateSet("standard", "portable")]
    [string]$Target = "standard",

    [int]$Jobs = 0,

    [switch]$VerboseBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$cmakeExe = "C:\msys64\mingw64\bin\cmake.exe"
$mingwBin = "C:\msys64\mingw64\bin"
$msysBin = "C:\msys64\usr\bin"
$buildDir = if ($Target -eq "portable") { "build-portable" } else { "build-standard" }
$cachePath = Join-Path $repoRoot $buildDir | Join-Path -ChildPath "CMakeCache.txt"

if (-not (Test-Path $cmakeExe)) {
    Write-Error "MSYS2 CMake was not found at '$cmakeExe'. Install MSYS2 to C:\msys64 or use build-cmake.bat after adjusting the script."
}

if (-not (Test-Path $cachePath)) {
    Write-Error "Missing '$buildDir\\CMakeCache.txt'. Run .\\build-cmake.bat first to configure the build tree."
}

$originalPath = $env:PATH

try {
    # GCC launches helper executables that depend on DLLs under mingw64\bin.
    $env:PATH = "$mingwBin;$msysBin;$originalPath"

    $args = @("--build", (Join-Path $repoRoot $buildDir))

    if ($Jobs -gt 0) {
        $args += @("--parallel", $Jobs.ToString())
    } else {
        $args += "--parallel"
    }

    if ($VerboseBuild) {
        $args += "--verbose"
    }

    & $cmakeExe @args
    exit $LASTEXITCODE
}
finally {
    $env:PATH = $originalPath
}
