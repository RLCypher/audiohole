# Packages an already-built CMake Runtime component; never walks/copies the build
# tree. Unique staging is always removed, including rejected/failed installations.
# Source dirtiness is explicit metadata, not hidden behind an apparently clean SHA.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/windows-msvc'),
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/packages'),
    [switch]$Unverified
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
if ($Unverified) { $suffix += '-UNVERIFIED' }
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
    $expected = @('audiohole.exe', 'MANUAL_GUI_CHECKLIST.md', 'wxWidgets-LGPL.txt', 'wxWidgets-LICENSE.txt', 'nlohmann-json-LICENSE.txt', 'personalities/default.json', 'personalities/uwu.json', 'personalities/README.md', 'assets/branding/audiohole-logo.png', 'assets/branding/audiohole.ico', 'assets/presentation/placeholder-avatar.png')
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
        purpose = 'Milestone 0.2 simulated GUI evaluation'
        executable = 'audiohole.exe'
        actions_run = $runUrl
        packaged_at_utc = [DateTime]::UtcNow.ToString('o')
    }
    if ($Unverified) { $metadata['validation_status'] = 'unverified' }
    $metadata | ConvertTo-Json | Set-Content (Join-Path $stage 'build-info.json') -Encoding UTF8
    $readme = @"
AudioHole - Windows x64 - $Configuration
Commit: $commit
Source has uncommitted changes: $dirty
CI: $runUrl

Purpose: Milestone 0.2 simulated GUI evaluation, not a formal release.
Launch audiohole.exe to evaluate the native GUI with authored simulated devices.
SIMULATION ONLY: no audio capture/render, real endpoint enumeration, or persistent edits.
Use MANUAL_GUI_CHECKLIST.md for evaluation workflows.
The Release package is preferred for ordinary manual testing.

Requires Windows 10 22H2 or a supported Windows 11 x64 release.
No Visual Studio, SDK, CMake, Git, test framework, administrator rights,
driver, service, or separately installed VC runtime is needed to launch this build.
The reference MSVC build uses the static CRT. Normal Windows system DLLs remain required.

Known limitations: all devices, scans, activity, failures, and Test Beep are simulated.
Test Beep emits NO SOUND. Profiles and settings last only for this session.
Synchronization algorithms, production schemas, and storage roots remain unresolved.
wxWidgets 3.2.6 is statically linked; license and LGPL text are included.
The header-only nlohmann/json 3.11.3 MIT notice is included.
Norm and UwU commentary are editable JSON files in personalities/.
Vanilla is the built-in fallback. Light/Dark/System choices are in Settings.
Follow System is the default. Windows-owned dialogs retain system styling.
The startup notice discloses all simulated functions, including silent Test Beep.
Interface and commentary wording preview the intended production experience.
This package is for private evaluation. AudioHole's project license remains undecided.
The test-only GoogleTest dependency is not part of this package.
"@
    if ($Unverified) {
        $readme = "UNVERIFIED TESTER BUILD`nAutomated validation has not completed. Use for manual GUI testing only.`nCheck the matching Actions run for failures before treating this build as verified.`n`n$readme"
    }
    $readme | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
    Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
    Write-Host "Created $archive"
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
