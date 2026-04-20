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
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$ArgumentList
    )

    $stdoutFile = [System.IO.Path]::GetTempFileName()
    $stderrFile = [System.IO.Path]::GetTempFileName()

    try {
        $process = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList `
            -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $stdoutFile `
            -RedirectStandardError $stderrFile

        if (Test-Path $stdoutFile) {
            Get-Content -Path $stdoutFile | ForEach-Object { Write-Host $_ }
        }
        if (Test-Path $stderrFile) {
            Get-Content -Path $stderrFile | ForEach-Object { Write-Host $_ }
        }

        return $process.ExitCode
    }
    finally {
        Remove-Item -Path $stdoutFile, $stderrFile -Force -ErrorAction SilentlyContinue
    }
}

function Resolve-JavaHome {
    if ($env:JAVA_HOME -and (Test-Path (Join-Path $env:JAVA_HOME 'bin\java.exe'))) {
        return $env:JAVA_HOME
    }

    $studioJbr = 'C:\Program Files\Android\Android Studio\jbr'
    if (Test-Path (Join-Path $studioJbr 'bin\java.exe')) {
        return $studioJbr
    }

    return $null
}

$androidDir = Join-Path $PSScriptRoot 'android'
if (-not (Test-Path $androidDir)) {
    throw "Android project folder not found: $androidDir"
}

Push-Location $androidDir
try {
    $task = if ($Config -eq 'Release') { 'assembleRelease' } else { 'assembleDebug' }

    $javaHome = Resolve-JavaHome
    if ($javaHome) {
        $env:JAVA_HOME = $javaHome
        $env:Path = "$javaHome\bin;$env:Path"
    }

    if (Test-Path '.\\gradlew.bat') {
        $wrapperExitCode = Invoke-NativeCommand '.\\gradlew.bat' $task
        if ($wrapperExitCode -ne 0) {
            throw "Gradle wrapper failed with exit code $wrapperExitCode"
        }
        $apkPath = if ($Config -eq 'Release') {
            Join-Path $androidDir 'app\build\outputs\apk\release\app-release.apk'
        } else {
            Join-Path $androidDir 'app\build\outputs\apk\debug\app-debug.apk'
        }
        Write-Host "APK build completed via gradlew ($task)." -ForegroundColor Green
        if (Test-Path $apkPath) {
            Write-Host "APK: $apkPath" -ForegroundColor Green
        }
        return
    }

    if (Get-Command gradle -ErrorAction SilentlyContinue) {
        $gradleExitCode = Invoke-NativeCommand 'gradle' $task
        if ($gradleExitCode -ne 0) {
            throw "Gradle CLI failed with exit code $gradleExitCode"
        }
        $apkPath = if ($Config -eq 'Release') {
            Join-Path $androidDir 'app\build\outputs\apk\release\app-release.apk'
        } else {
            Join-Path $androidDir 'app\build\outputs\apk\debug\app-debug.apk'
        }
        Write-Host "APK build completed via gradle CLI ($task)." -ForegroundColor Green
        if (Test-Path $apkPath) {
            Write-Host "APK: $apkPath" -ForegroundColor Green
        }
        return
    }

    throw @"
No Gradle CLI found and no gradle wrapper present.

Use Android Studio to build the APK:
1) Open android/ project.
2) Let Gradle sync complete.
3) Build > Build APK(s).

Then install with:
  .\install-android-apk.ps1 -Config $Config
"@
}
finally {
    Pop-Location
}
