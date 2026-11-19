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
$projectText = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($projectText -notmatch 'project\(AudioHole VERSION (\d+\.\d+\.\d+) ') { throw 'Cannot read the application version.' }
$version = $Matches[1]
$buildNumber = 0L
if ($env:GITHUB_RUN_NUMBER) {
    if (-not [long]::TryParse($env:GITHUB_RUN_NUMBER, [ref]$buildNumber) -or $buildNumber -lt 1) {
        throw 'Invalid CI build number.'
    }
}
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
    $expected = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runtime-files.txt'))
    $relative = @($files | ForEach-Object { $_.FullName.Substring($stage.TrimEnd([IO.Path]::DirectorySeparatorChar).Length + 1).Replace('\', '/') })
    $difference = @(Compare-Object ($expected | Sort-Object) ($relative | Sort-Object))
    if ($difference.Count -ne 0) {
        $missing = @($difference | Where-Object SideIndicator -eq '<=' | ForEach-Object InputObject)
        $unexpected = @($difference | Where-Object SideIndicator -eq '=>' | ForEach-Object InputObject)
        throw ("Unexpected Runtime component contents; audit the package allowlist before distributing.`n" +
            "Missing: $($missing -join ', ')`nUnexpected: $($unexpected -join ', ')")
    }
    $runUrl = if ($env:GITHUB_ACTIONS -eq 'true') {
        "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID"
    } else { 'Local build (no GitHub Actions run)' }
    $metadata = [ordered]@{
        format = 'audiohole-build-info'
        schema_version = 1
        product = 'AudioHole'
        version = $version
        build_number = $buildNumber
        commit = $commit
        configuration = $Configuration
        platform = 'Windows x64'
        source_dirty = $dirty
        purpose = 'AudioHole GUI, engine 0.6, Plug Board 0.2.6 and fictional tutorial evaluation'
        engine_module = '0.6'
        plug_board_module = '0.2.6'
        native_latency_preset = 'Low'
        executable = 'AudioHole.exe'
        actions_run = $runUrl
        packaged_at_utc = [DateTime]::UtcNow.ToString('o')
        source_archive = "AudioHole-Source-$($commit.Substring(0,12))$suffix.zip"
        runtime_files = @($expected) + @('docs/build-info.json', 'README.txt')
    }
    $metadata | ConvertTo-Json | Set-Content (Join-Path $stage 'docs/build-info.json') -Encoding UTF8
    @"
AudioHole - Windows x64 - $Configuration
Version: $version (build $buildNumber)
Commit: $commit
Source has uncommitted changes: $dirty
CI: $runUrl

Purpose: GUI, guided tutorial and isolated working-configuration evaluation, not a formal release.
Code: GPL-3.0-only; see LICENSE and docs/LICENSING.md. Matching source: $($metadata.source_archive).
Launch AudioHole.exe. First-time onboarding uses Norm's fictional in-memory tutorial. The working environment offers real Windows device scanning and isolated persistent configuration.
Engine 0.6 provides real routing through native WASAPI, Miniaudio and Experimental adapters. Automatic initially selects an available native adapter. Explicit device checks use temporary shared-mode streams while routing is stopped. Device/profile bundle transfer and the engine handoff are described in docs/CONFIGURATION_BUNDLES.md.
Use docs/MANUAL_GUI_CHECKLIST.md for evaluation workflows.
The Release package is preferred for ordinary manual testing.

Requires Windows 10 22H2 or a supported Windows 11 x64 release.
No Visual Studio, SDK, CMake, Git, test framework, administrator rights,
driver, service, or separately installed VC runtime is needed to launch this build.
The reference MSVC build uses the static CRT. Normal Windows system DLLs remain required.

Known limitations: Tutorial devices, scans, routing, activity and Test Beep are simulated. Working mode has real configuration I/O, Windows device scanning, and explicit temporary capture-level and playback-beep checks. Deep testing requires consent. The engine is a validation prototype: Stop before profile changes or device recovery; automatic reconnection, live switching and destination mixing are unavailable. See docs/ENGINE_INTEGRATION.md.
Tutorial Test Beep emits NO SOUND. Working-mode device checks open only the selected available endpoint, never record audio, and cancel on navigation or hiding the window. Working configuration and preferences persist. Tutorial data stays in memory; only explicit completion/skip writes onboarding state in the shared user data folder. See docs/TUTORIAL.md.
Synchronization algorithms, production schemas, and storage roots remain unresolved.
wxWidgets 3.2.6 is statically linked; third-party notices are in licenses/.
The header-only nlohmann/json 3.11.3 MIT notice and Miniaudio 0.11.25 license are included.
Personality folders in personalities/ contain editable manifests and dialogue. Copy a valid folder to install it.
Missing selections fall back to an entire available package; no valid packages disables personality presentation. Light/Dark/System choices are in Settings.
Follow System is the default. Windows-owned dialogs retain system styling.
The welcome appears when onboarding state is absent from the shared user data folder. Replay Tutorial resets this marker and restarts.
Interface and commentary wording preview the intended production experience.
This package is for private evaluation. AudioHole's project license remains undecided.
The test-only GoogleTest dependency is not part of this package.
"@ | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
    $rootFiles = @((Get-ChildItem -LiteralPath $stage -File).Name)
    if (Compare-Object @('AudioHole.exe', 'LICENSE', 'README.txt') ($rootFiles | Sort-Object)) {
        throw 'Distribution root must contain only AudioHole.exe, LICENSE and README.txt beside its folders.'
    }
    Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
    Write-Host "Created $archive"
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
