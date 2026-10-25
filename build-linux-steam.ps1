[CmdletBinding()]
param(
    [string]$OutputFolder = "scripts/output/linux-release",
    [ValidateRange(1, 128)]
    [int]$Jobs = 8,
    [string]$Image = "registry.gitlab.steamos.cloud/steamrt/steamrt4/sdk:latest",
    [switch]$PullLatest
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $PathValue))
}

function Convert-ToContainerRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$AbsolutePath,
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $relative = [System.IO.Path]::GetRelativePath($RepoRoot, $AbsolutePath)
    if ([System.IO.Path]::IsPathRooted($relative) -or
        $relative -eq ".." -or
        $relative.StartsWith("..$([System.IO.Path]::DirectorySeparatorChar)") -or
        $relative.StartsWith("..$([System.IO.Path]::AltDirectorySeparatorChar)")) {
        throw "Output folder must be inside the repository: $AbsolutePath"
    }

    return $relative.Replace([System.IO.Path]::DirectorySeparatorChar, "/").Replace([System.IO.Path]::AltDirectorySeparatorChar, "/")
}

function Invoke-Docker {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $script:DockerCommand @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Docker command failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$outputPath = Resolve-RepoPath $OutputFolder
$containerOutput = Convert-ToContainerRelativePath -AbsolutePath $outputPath -RepoRoot $repoRoot
$stagePath = Join-Path $outputPath "sil-more-linux-steam"

if (Test-Path -LiteralPath $stagePath) {
    throw "Refusing to overwrite an existing Linux package folder: $stagePath`nUse a fresh -OutputFolder."
}

$docker = Get-Command docker.exe -ErrorAction SilentlyContinue
if (-not $docker) {
    $docker = Get-Command docker -ErrorAction SilentlyContinue
}
if (-not $docker) {
    throw "Docker was not found on PATH. Install and start Docker Desktop, then retry."
}
$script:DockerCommand = if ($docker.Source) { $docker.Source } else { $docker.Name }

if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "external/SDL/CMakeLists.txt"))) {
    throw "Pinned SDL submodules are missing. Run: git submodule update --init --recursive"
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "external/SDL_image/CMakeLists.txt"))) {
    throw "Pinned SDL_image submodule is missing. Run: git submodule update --init --recursive"
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "external/SDL_ttf/CMakeLists.txt"))) {
    throw "Pinned SDL_ttf submodule is missing. Run: git submodule update --init --recursive"
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "external/SDL_mixer/CMakeLists.txt"))) {
    throw "Pinned SDL_mixer submodule is missing. Run: git submodule update --init --recursive"
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "external/steamworks_sdk/sdk/redistributable_bin/linux64/libsteam_api.so"))) {
    throw "Steamworks Linux API library is missing: external/steamworks_sdk/sdk/redistributable_bin/linux64/libsteam_api.so"
}

Write-Host "Steam Runtime 4 Linux build" -ForegroundColor Cyan
Write-Host "Repository: $repoRoot"
Write-Host "Output:     $outputPath"
Write-Host "Jobs:       $Jobs"
Write-Host "Image:      $Image"
Write-Host ""

if ($PullLatest) {
    Write-Host "Pulling latest Steam Runtime 4 SDK image..." -ForegroundColor Yellow
    Invoke-Docker @("pull", "--platform", "linux/amd64", $Image)
} else {
    & $script:DockerCommand image inspect $Image *> $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Steam Runtime 4 SDK image is not cached; pulling it..." -ForegroundColor Yellow
        Invoke-Docker @("pull", "--platform", "linux/amd64", $Image)
    }
}

$mount = "$repoRoot`:/work"
$buildCommand = "python3 scripts/build_linux_steam.py --jobs $Jobs --output '$containerOutput'"

Write-Host "Running Linux packager inside Steam Runtime 4..." -ForegroundColor Yellow
& $script:DockerCommand run --rm -it --platform linux/amd64 `
    -v $mount `
    -w /work `
    $Image `
    bash -lc $buildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Linux Steam package build failed with exit code $LASTEXITCODE"
}

$zip = Get-ChildItem -LiteralPath $outputPath -File -Filter "*-steam-linux-x86_64.zip" -ErrorAction SilentlyContinue | Select-Object -First 1
$tar = Get-ChildItem -LiteralPath $outputPath -File -Filter "*-steam-linux-x86_64.tar.gz" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $zip -or -not $tar) {
    throw "The Linux packager exited successfully but did not produce both expected archives in $outputPath"
}

Write-Host ""
Write-Host "Linux Steam package complete." -ForegroundColor Green
Write-Host "ZIP:     $($zip.FullName)"
Write-Host "Tarball: $($tar.FullName)"
Write-Host ""
Write-Host "Preview the upload from PowerShell with:" -ForegroundColor Yellow
Write-Host ".\scripts\upload-steam.ps1 --platform linux --linux-content-root `"$stagePath`" --preview --account YOUR_STEAM_LOGIN_NAME"
