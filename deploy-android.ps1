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
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    [string]$AdbPath,

    [switch]$AllowDowngrade,

    [switch]$LaunchApp
)

$ErrorActionPreference = 'Stop'

$androidApplicationId = 'com.silqh.silmore.unstable'
$androidLaunchActivity = 'com.silqh.silmore.SilMoreActivity'

$repoRoot = $PSScriptRoot
$buildScript = Join-Path $repoRoot 'build-android-apk.ps1'
$installScript = Join-Path $repoRoot 'install-android-apk.ps1'

if (-not (Test-Path $buildScript)) {
    throw "Missing script: $buildScript"
}
if (-not (Test-Path $installScript)) {
    throw "Missing script: $installScript"
}

& $buildScript -Config $Config

$installParams = @{
    Config = $Config
}
if ($AdbPath) {
    $installParams['AdbPath'] = $AdbPath
}
if ($AllowDowngrade) {
    $installParams['AllowDowngrade'] = $true
}

& $installScript @installParams

if ($LaunchApp) {
    $adb = if ($AdbPath) {
        $AdbPath
    } else {
        $cmd = Get-Command adb -ErrorAction SilentlyContinue
        if ($cmd) {
            $cmd.Source
        } else {
            Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
        }
    }

    if (-not (Test-Path $adb)) {
        throw "adb not found for launch step: $adb"
    }

    & $adb shell am start -n "$androidApplicationId/$androidLaunchActivity"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to launch app via adb (exit code $LASTEXITCODE)"
    }

    Write-Host "Launched $androidApplicationId." -ForegroundColor Green
}

Write-Host "Deploy complete for $Config." -ForegroundColor Green
