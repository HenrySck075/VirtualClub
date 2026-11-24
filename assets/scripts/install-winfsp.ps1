$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent()
)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$($MyInvocation.MyCommand.Path)`""
    $process = Start-Process -FilePath "powershell.exe" -ArgumentList $arguments -Verb RunAs -Wait -PassThru
    exit $process.ExitCode
}

function Test-WinFsp {
    $service = Get-Service -Name "WinFsp.Launcher" -ErrorAction SilentlyContinue
    if ($service) {
        return $true
    }

    $dllNames = @("winfsp-x64.dll", "winfsp-x86.dll")
    $roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramW6432) |
        Where-Object { $_ } |
        Select-Object -Unique
    foreach ($root in $roots) {
        foreach ($dllName in $dllNames) {
            if (Test-Path (Join-Path $root "WinFsp\bin\$dllName")) {
                return $true
            }
        }
    }
    return $false
}

if (Test-WinFsp) {
    Write-Host "WinFsp is already installed."
    exit 0
}

$installed = $false

$winget = Get-Command "winget.exe" -ErrorAction SilentlyContinue
if ($winget) {
    Write-Host "Installing WinFsp with winget..."
    & $winget.Source install --id "WinFsp.WinFsp" --exact --scope machine --accept-package-agreements --accept-source-agreements --disable-interactivity
    if (Test-WinFsp) {
        $installed = $true
    }
}

if (-not $installed) {
    $choco = Get-Command "choco.exe" -ErrorAction SilentlyContinue
    if ($choco) {
        Write-Host "Installing WinFsp with Chocolatey..."
        & $choco.Source install winfsp -y --no-progress
        if (Test-WinFsp) {
            $installed = $true
        }
    }
}

if (-not $installed) {
    Write-Host "Downloading the latest WinFsp installer..."
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/winfsp/winfsp/releases/latest" -Headers @{ "User-Agent" = "VirtualClub" }
    $asset = $release.assets | Where-Object {
        $_.name -match "(?i)^winfsp.*\.(msi)$"
    } | Select-Object -First 1
    if (-not $asset) {
        throw "Could not find a WinFsp MSI in the latest GitHub release."
    }

    $installer = Join-Path $env:TEMP $asset.name
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $installer
    $process = Start-Process -FilePath "msiexec.exe" `
        -ArgumentList "/i `"$installer`" /qn /norestart" `
        -Wait -PassThru
    if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) {
        throw "WinFsp MSI installation failed with exit code $($process.ExitCode)."
    }
    Remove-Item -Path $installer -Force -ErrorAction SilentlyContinue
}

if (-not (Test-WinFsp)) {
    throw "WinFsp installation completed but the WinFsp service or files were not detected."
}

Write-Host "WinFsp was installed successfully."
