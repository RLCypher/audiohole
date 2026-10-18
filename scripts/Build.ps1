[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$Launch,
    [switch]$Package
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
Push-Location $repoRoot
try {
    & "$PSScriptRoot/Verify-Environment.ps1"
    if ($LASTEXITCODE -ne 0) { throw 'Prerequisites are missing. Follow the guidance above.' }
    & cmake --preset windows-msvc
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $preset = "windows-$($Configuration.ToLowerInvariant())"
    & cmake --build --preset $preset --parallel 2
    if ($LASTEXITCODE -ne 0) { throw "$Configuration build failed." }
    & ctest --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "$Configuration tests failed." }
    if ($Package) {
        & "$PSScriptRoot/Package.ps1" -Configuration $Configuration
    }
    if ($Launch) {
        & (Join-Path $repoRoot "build/windows-msvc/bin/$Configuration/audiohole.exe")
        if ($LASTEXITCODE -ne 0) { throw 'AudioHole GUI failed.' }
    }
} finally {
    Pop-Location
}
