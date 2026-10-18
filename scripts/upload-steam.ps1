# Windows convenience wrapper for the cross-platform SteamPipe uploader.
# This forwards all arguments to scripts/steam_upload.py and never receives a
# Steam password. SteamCMD handles authentication interactively or from its
# saved login state.

$ErrorActionPreference = "Stop"
$scriptPath = Join-Path $PSScriptRoot "steam_upload.py"
$python = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command python -ErrorAction SilentlyContinue
}
if (-not $python) {
    Write-Error "Python was not found on PATH. Install Python 3.11+ or run steam_upload.py with your Python interpreter."
    exit 1
}

& $python.Source $scriptPath @args
exit $LASTEXITCODE
