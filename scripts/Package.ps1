# Packages the runtime input used to assemble the final portable distribution.
# Never walks/copies the build tree. Unique staging is always removed, including rejected/failed installations.
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
. (Join-Path $PSScriptRoot 'Build-Names.ps1')
$projectText = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($projectText -notmatch 'project\(AudioHole VERSION (\d+\.\d+\.\d+) ') { throw 'Cannot read the application version.' }
$version = $Matches[1]
$identity = Get-Content -LiteralPath (Join-Path $BuildDirectory 'build-identity.json') -Raw | ConvertFrom-Json
$builtAt = if ($identity.built_at_utc -is [DateTime]) {
    # PowerShell 7.5+ converts ISO JSON timestamps to DateTime automatically.
    [DateTimeOffset]::new($identity.built_at_utc.ToUniversalTime())
} else {
    [DateTimeOffset]::ParseExact($identity.built_at_utc, "yyyy-MM-dd'T'HH:mm:ss'Z'", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal)
}
if ($identity.version -ne $version -or $identity.build_date_utc -ne $builtAt.ToString('yyyy-MM-dd') -or
    $identity.vintage -ne $builtAt.ToString('MMMM d, yyyy', [Globalization.CultureInfo]::InvariantCulture)) {
    throw 'Invalid compiled build identity or Vintage.'
}
$buildNumber = 0L
if ($env:GITHUB_RUN_NUMBER) {
    if (-not [long]::TryParse($env:GITHUB_RUN_NUMBER, [ref]$buildNumber) -or $buildNumber -lt 1) {
        throw 'Invalid CI build number.'
    }
}
$commit = & git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify the source commit.' }
if ($identity.commit -ne $commit) { throw 'Build identity belongs to another commit; reconfigure and rebuild.' }
$status = & git -C $repoRoot status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source status.' }
$dirty = -not [string]::IsNullOrWhiteSpace(($status -join "`n"))
$kind = if ($Configuration -eq 'Release') { 'Portable' } else { 'Debug' }
$name = Get-AudioHoleDownloadName -Kind $kind -Version $version -Commit $commit -Dirty:$dirty
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$archive = Join-Path $outputRoot "$name.zip"
if (Test-Path -LiteralPath $archive) { throw "Package already exists; choose another output directory: $archive" }
$temporary = Join-Path $outputRoot ([guid]::NewGuid().ToString())
$stage = Join-Path $temporary $name
try {
    & cmake --install $BuildDirectory --config $Configuration --prefix $stage --component Runtime
    if ($LASTEXITCODE -ne 0) { throw 'Runtime installation failed; build and test this configuration first.' }
    $files = @(Get-ChildItem -LiteralPath $stage -File -Recurse -Force)
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
        built_at_utc = $builtAt.UtcDateTime.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'")
        build_date_utc = $identity.build_date_utc
        vintage = $identity.vintage
        project_codename = 'BLANCO'
        release_repository = 'RLCypher/audiohole'
        release_tag = "v$version"
        installer_file = (Get-AudioHoleDownloadName -Kind Installer -Version $version -Commit $commit -Dirty:$dirty) + '.exe'
        portable_archive = (Get-AudioHoleDownloadName -Kind Portable -Version $version -Commit $commit -Dirty:$dirty) + '.zip'
        debug_archive = (Get-AudioHoleDownloadName -Kind Debug -Version $version -Commit $commit -Dirty:$dirty) + '.zip'
        source_archive = (Get-AudioHoleDownloadName -Kind Source -Version $version -Commit $commit -Dirty:$dirty) + '.zip'
        runtime_files = @($expected) + @('docs/build-info.json', 'README.txt')
    }
    New-Item -ItemType Directory -Path (Join-Path $stage 'docs') -Force | Out-Null
    $metadata | ConvertTo-Json | Set-Content (Join-Path $stage 'docs/build-info.json') -Encoding UTF8
    Copy-Item -LiteralPath (Join-Path $repoRoot 'docs/distribution/README.txt') -Destination (Join-Path $stage 'README.txt')
    $rootFiles = @((Get-ChildItem -LiteralPath $stage -File).Name)
    if (Compare-Object @('AudioHole License.rtf', 'AudioHole.exe', 'README.txt') ($rootFiles | Sort-Object)) {
        throw 'Distribution root must contain only AudioHole.exe, AudioHole License.rtf and README.txt beside its folders.'
    }
    # Archive the audited payload with CMake's ZIP writer.
    Push-Location $temporary
    try {
        & cmake -E tar cf $archive --format=zip $name
        if ($LASTEXITCODE -ne 0) { throw 'Runtime archive creation failed.' }
    } finally { Pop-Location }
    Write-Host "Created $archive"
    if ($env:GITHUB_OUTPUT) {
        "artifact_name=$name" >> $env:GITHUB_OUTPUT
    }
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
