# Packages an already-built CMake Runtime component; never walks/copies the build
# tree. Unique staging is always removed, including rejected/failed installations.
# Source dirtiness is explicit metadata, not hidden behind an apparently clean SHA.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/windows-msvc'),
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/packages')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$commit = & git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify the source commit.' }
$status = & git -C $repoRoot status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source status.' }
$dirty = -not [string]::IsNullOrWhiteSpace(($status -join "`n"))
$suffix = if ($dirty) { '-dirty' } else { '' }
$name = "AudioHole-Windows-x64-$Configuration-$($commit.Substring(0,12))$suffix"
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$archive = Join-Path $outputRoot "$name.zip"
if (Test-Path -LiteralPath $archive) { throw "Package already exists; choose another output directory: $archive" }
$temporary = Join-Path $outputRoot ([guid]::NewGuid().ToString())
$stage = Join-Path $temporary $name
try {
    & cmake --install $BuildDirectory --config $Configuration --prefix $stage --component Runtime
    if ($LASTEXITCODE -ne 0) { throw 'Runtime installation failed; build and test this configuration first.' }
    $files = @(Get-ChildItem -LiteralPath $stage -File -Recurse)
    # Explicit runtime allowlist: never copy a build tree, tests, or sources.
    $expected = @('audiohole.exe', 'MANUAL_GUI_CHECKLIST.md', 'wxWidgets-LGPL.txt', 'wxWidgets-LICENSE.txt', 'nlohmann-json-LICENSE.txt', 'personalities/default/personality.json', 'personalities/default/dialogue/responses.json', 'personalities/default/preview/placeholder.png', 'personalities/uwu/personality.json', 'personalities/uwu/dialogue/responses.json', 'personalities/uwu/preview/placeholder.png', 'personalities/vanilla/personality.json', 'personalities/vanilla/preview/logo.png', 'personalities/README.md', 'assets/branding/audiohole-logo.png', 'assets/branding/audiohole.ico', 'assets/presentation/placeholder-avatar.png')
    $relative = @($files | ForEach-Object { $_.FullName.Substring($stage.TrimEnd([IO.Path]::DirectorySeparatorChar).Length + 1).Replace('\', '/') })
    if (Compare-Object ($expected | Sort-Object) ($relative | Sort-Object)) {
        throw 'Unexpected Runtime component contents; audit the package allowlist before distributing.'
    }
    $runUrl = if ($env:GITHUB_ACTIONS -eq 'true') {
        "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID"
    } else { 'Local build (no GitHub Actions run)' }
    $metadata = [ordered]@{
        product = 'AudioHole'
        commit = $commit
        configuration = $Configuration
        platform = 'Windows x64'
        source_dirty = $dirty
        purpose = 'Preview Build GUI and isolated configuration evaluation'
        executable = 'audiohole.exe'
        actions_run = $runUrl
        packaged_at_utc = [DateTime]::UtcNow.ToString('o')
    }
    $metadata | ConvertTo-Json | Set-Content (Join-Path $stage 'build-info.json') -Encoding UTF8
    @"
AudioHole - Windows x64 - $Configuration
Commit: $commit
Source has uncommitted changes: $dirty
CI: $runUrl

Purpose: Preview Build GUI and isolated configuration evaluation, not a formal release.
Launch audiohole.exe to evaluate the native GUI with authored simulated devices.
No real audio capture/render or endpoint enumeration. Preview uses simulated records; Test persists isolated configuration.
Use MANUAL_GUI_CHECKLIST.md for evaluation workflows.
The Release package is preferred for ordinary manual testing.

Requires Windows 10 22H2 or a supported Windows 11 x64 release.
No Visual Studio, SDK, CMake, Git, test framework, administrator rights,
driver, service, or separately installed VC runtime is needed to launch this build.
The reference MSVC build uses the static CRT. Normal Windows system DLLs remain required.

Known limitations: Preview devices, scans, activity, failures and Test Beep are simulated. Test has real configuration I/O but no audio/scanner backend.
Test Beep emits NO SOUND. Test configuration and Preview Build preferences persist; Preview fixtures remain session-only.
Synchronization algorithms, production schemas, and storage roots remain unresolved.
wxWidgets 3.2.6 is statically linked; license and LGPL text are included.
The header-only nlohmann/json 3.11.3 MIT notice is included.
Personality folders in personalities/ contain editable manifests and dialogue. Copy a valid folder to install it.
Missing selections fall back to an entire available package; no valid packages disables personality presentation. Light/Dark/System choices are in Settings.
Follow System is the default. Windows-owned dialogs retain system styling.
The optional startup dialog explains both modes, including silent Preview Test Beep.
Interface and commentary wording preview the intended production experience.
This package is for private evaluation. AudioHole's project license remains undecided.
The test-only GoogleTest dependency is not part of this package.
"@ | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
    Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
    Write-Host "Created $archive"
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
