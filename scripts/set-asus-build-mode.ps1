param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("FullSpeed", "Whisper")]
    [string]$Mode
)

$ErrorActionPreference = "Stop"

$deviceId = [uint32]0x00110019
$modeValues = @{
    FullSpeed = [uint32]3
    Whisper   = [uint32]1
}

try {
    $bios = Get-ItemProperty -LiteralPath "HKLM:\HARDWARE\DESCRIPTION\System\BIOS"
    if ($bios.SystemManufacturer -notmatch "ASUS|ASUSTeK") {
        Write-Host "Non-ASUS system detected; skipping ASUS power mode change."
        exit 0
    }

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Changing the ASUS power mode requires an elevated terminal. Run the build script as Administrator."
    }

    $asusWmi = Get-CimInstance -Namespace "root/wmi" -ClassName "AsusAtkWmi_WMNB" |
        Select-Object -First 1
    if ($null -eq $asusWmi) {
        throw "ASUS WMI interface AsusAtkWmi_WMNB was not found."
    }

    $response = Invoke-CimMethod -InputObject $asusWmi -MethodName "DEVS" -Arguments @{
        Device_ID     = $deviceId
        Control_status = $modeValues[$Mode]
    }

    if ([uint32]$response.result -ne 1) {
        throw "ASUS WMI rejected the $Mode request (result $($response.result))."
    }

    Write-Host "ASUS power mode: $Mode"
}
catch {
    [Console]::Error.WriteLine("ASUS power mode change failed: $($_.Exception.Message)")
    exit 1
}
