# Bundle the exact local source and the FetchContent sources used to build it.
[CmdletBinding()]
param(
    [string]$BuildDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/windows-msvc'),
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/source')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'Build-Names.ps1')
$projectText = Get-Content -LiteralPath (Join-Path $repo 'CMakeLists.txt') -Raw
if ($projectText -notmatch 'project\(AudioHole VERSION (\d+\.\d+\.\d+) ') { throw 'Cannot read the application version.' }
$version = $Matches[1]
$commit = & git -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Cannot identify source.' }
$status = & git -C $repo status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source status.' }
$dirty = -not [string]::IsNullOrWhiteSpace(($status -join "`n"))
$identity = Get-Content -LiteralPath (Join-Path $BuildDirectory 'build-identity.json') -Raw | ConvertFrom-Json
if ($identity.commit -ne $commit) { throw 'Source build identity belongs to another commit.' }
$name = Get-AudioHoleDownloadName -Kind Source -Version $version -Commit $commit -Dirty:$dirty
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$archive = Join-Path $OutputDirectory "$name.zip"
if (Test-Path -LiteralPath $archive) { throw "Source archive already exists: $archive" }
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('AudioHole-source-' + [guid]::NewGuid())
try {
    $stage = Join-Path $temporary $name
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    $paths = & git -C $repo -c core.quotepath=false ls-files --cached --others --exclude-standard
    if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate source.' }
    foreach ($relative in $paths) {
        $source = Join-Path $repo $relative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $target = Join-Path $stage $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    foreach ($dependency in @('wxwidgets', 'nlohmann_json', 'miniz', 'miniaudio', 'googletest')) {
        $source = Join-Path $BuildDirectory "_deps/$dependency-src"
        if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing corresponding dependency source: $dependency" }
        $target = Join-Path $stage "dependencies/$dependency"
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
    }
    [ordered]@{ product='AudioHole'; commit=$commit; source_dirty=$dirty; version=$identity.version;
        built_at_utc=$identity.built_at_utc; build_date_utc=$identity.build_date_utc; vintage=$identity.vintage } |
        ConvertTo-Json | Set-Content (Join-Path $stage 'source-build-info.json') -Encoding UTF8
    $archive = [IO.Path]::GetFullPath($archive)
    Push-Location $temporary
    try {
        & cmake -E tar cf $archive --format=zip $name
        if ($LASTEXITCODE -ne 0) { throw 'Corresponding source archive failed.' }
    } finally { Pop-Location }
    Write-Host "Created corresponding source: $archive"
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
