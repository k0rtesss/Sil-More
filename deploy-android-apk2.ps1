param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('Sideload','Play')]
    [string]$Delivery = 'Sideload',

    [string]$AdbPath,

    [string]$Serial,

    [string]$KeystorePath = $env:SIL_MORE_RELEASE_STORE_FILE,

    [string]$KeystoreAlias = $env:SIL_MORE_RELEASE_KEY_ALIAS,

    [switch]$AllowDowngrade,

    [switch]$LaunchApp
)

$ErrorActionPreference = 'Stop'

$deployScript = Join-Path $PSScriptRoot 'deploy-android.ps1'
if (-not (Test-Path -LiteralPath $deployScript)) {
    throw "Missing script: $deployScript"
}

$deployParams = @{
    Config   = $Config
    Delivery = $Delivery
    Apk2     = $true
}
if ($AdbPath) {
    $deployParams['AdbPath'] = $AdbPath
}
if ($Serial) {
    $deployParams['Serial'] = $Serial
}
if ($KeystorePath) {
    $deployParams['KeystorePath'] = $KeystorePath
}
if ($KeystoreAlias) {
    $deployParams['KeystoreAlias'] = $KeystoreAlias
}
if ($AllowDowngrade) {
    $deployParams['AllowDowngrade'] = $true
}
if ($LaunchApp) {
    $deployParams['LaunchApp'] = $true
}

& $deployScript @deployParams
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
    throw "APK2 Android deployment failed with exit code $LASTEXITCODE"
}

Write-Host 'APK2 Android app deployed as a separate package from the primary app.' -ForegroundColor Green
