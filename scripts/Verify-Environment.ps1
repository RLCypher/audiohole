# Read-only prerequisite check. No installers, PATH changes, or registry writes.
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$problems = [System.Collections.Generic.List[string]]::new()

if ($env:OS -ne 'Windows_NT') {
    $problems.Add('Use Windows x64 for native MSVC development; this host is not Windows.')
} elseif (-not [Environment]::Is64BitProcess) {
    $problems.Add('Open a 64-bit PowerShell session.')
}

foreach ($tool in @('cmake', 'ctest', 'git')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        $problems.Add("Install $tool and open a terminal where it is on PATH. See BUILDING.md.")
        continue
    }
    $versionOutput = & $tool --version
    if ($LASTEXITCODE -ne 0) {
        $problems.Add("$tool --version failed; check that installation.")
    } else {
        Write-Host ($versionOutput | Select-Object -First 1)
        if ($tool -in @('cmake', 'ctest')) {
            $match = [regex]::Match(($versionOutput -join "`n"), 'version (\d+\.\d+\.\d+)')
            if (-not $match.Success -or [version]$match.Groups[1].Value -lt [version]'3.25.0') {
                $problems.Add("$tool 3.25 or newer is required.")
            }
        }
    }
}

if ($env:OS -eq 'Windows_NT') {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        $problems.Add('Install Visual Studio/Build Tools 2022 with Desktop development with C++; vswhere.exe was not found.')
    } else {
        $installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or -not $installation) {
            $problems.Add('Use Visual Studio Installer to add the VS 2022 MSVC v143 x64/x86 build tools.')
        } else {
            $versionFile = Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt'
            if (-not (Test-Path -LiteralPath $versionFile)) {
                $problems.Add('Repair the VS 2022 C++ tools: their default toolchain version file is missing.')
            } else {
                $toolsVersion = (Get-Content -LiteralPath $versionFile -Raw).Trim()
                $compiler = Join-Path $installation "VC/Tools/MSVC/$toolsVersion/bin/Hostx64/x64/cl.exe"
                if (-not (Test-Path -LiteralPath $compiler)) {
                    $problems.Add('Add the MSVC x64 compiler using Visual Studio Installer.')
                } else {
                    Write-Host "MSVC $toolsVersion (x64): $installation"
                }
            }
        }
    }

    $kits = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -ErrorAction SilentlyContinue
    $sdkVersions = @()
    if ($kits -and $kits.PSObject.Properties['KitsRoot10']) {
        $sdkRoot = $kits.KitsRoot10
        $sdkVersions = @(Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory -ErrorAction SilentlyContinue | Where-Object {
            (Test-Path (Join-Path $_.FullName 'um/Windows.h')) -and
            (Test-Path (Join-Path $_.FullName 'ucrt/stdio.h')) -and
            (Test-Path (Join-Path $sdkRoot "Lib/$($_.Name)/um/x64/ole32.lib")) -and
            (Test-Path (Join-Path $sdkRoot "Lib/$($_.Name)/ucrt/x64/ucrt.lib"))
        })
    }
    if ($sdkVersions.Count -eq 0) {
        $problems.Add('Add a Windows 10 or Windows 11 SDK (headers and x64 libraries) using Visual Studio Installer.')
    } else {
        Write-Host "Windows SDK: $(($sdkVersions.Name | Sort-Object -Descending) -join ', ')"
    }
}

if ($problems.Count -ne 0) {
    foreach ($problem in $problems) { Write-Host "MISSING: $problem" -ForegroundColor Red }
    exit 1
}
Write-Host 'Environment checks passed. First test configure needs HTTPS access to codeload.github.com; GoogleTest is fetched with a pinned checksum.'
