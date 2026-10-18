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
    if ($files.Count -ne 1 -or $files[0].Name -ne 'audiohole_dev.exe') {
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
        purpose = 'Milestone 0.1 development foundation evaluation'
        executable = 'audiohole_dev.exe'
        actions_run = $runUrl
        packaged_at_utc = [DateTime]::UtcNow.ToString('o')
    }
    $metadata | ConvertTo-Json | Set-Content (Join-Path $stage 'build-info.json') -Encoding UTF8
    @"
AudioHole - Windows x64 - $Configuration
Commit: $commit
Source has uncommitted changes: $dirty
CI: $runUrl

Purpose: Milestone 0.1 development foundation evaluation, not a formal release.
Launch audiohole_dev.exe from a terminal to see its startup diagnostic.
It initializes COM, prints a message, and exits; there is no GUI or audio routing yet.
The Release package is preferred for ordinary manual testing.

Requires Windows 10 22H2 or a supported Windows 11 x64 release.
No Visual Studio, SDK, CMake, Git, test framework, administrator rights,
driver, service, or separately installed VC runtime is needed to launch this build.
The reference MSVC build uses the static CRT. Normal Windows system DLLs remain required.

Known limitations: no GUI, real endpoints, audio capture/render, profiles, or persistence.
This package is for private evaluation. AudioHole's project license remains undecided.
The test-only GoogleTest dependency is not part of this package.
"@ | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
    Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
    Write-Host "Created $archive"
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
