# Packaging regression tests use inert generated fixtures, never executable application validation.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('AudioHole-distribution-tests-' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path "$fixture/source" -Force | Out-Null
    # Build the inert installer from the authoritative list, including root-level
    # documents. Actual application install rules are audited by Package.ps1 in
    # the later native build step; this fixture tests the packager itself.
    $installRules = @(
        'cmake_minimum_required(VERSION 3.25)'
        'project(PackageFixture NONE)'
    )
    foreach ($relative in @(Get-Content (Join-Path $repo 'scripts/runtime-files.txt'))) {
        $path = Join-Path "$fixture/source" $relative
        New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
        'Inert runtime fixture; not executable application content.' | Set-Content $path
        $separator = $relative.LastIndexOf('/')
        $destination = if ($separator -lt 0) { '.' } else { $relative.Substring(0, $separator) }
        $installRules += 'install(FILES "' + $relative + '" DESTINATION "' + $destination + '" COMPONENT Runtime)'
    }
    $installRules | Set-Content "$fixture/source/CMakeLists.txt"
    & cmake -S "$fixture/source" -B "$fixture/build"
    if ($LASTEXITCODE -ne 0) { throw 'Fixture configure failed' }
    & "$repo/scripts/Package.ps1" -Configuration Release -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/output"
    $zip = @(Get-ChildItem "$fixture/output/*.zip")
    if ($zip.Count -ne 1) { throw 'Expected exactly one ZIP' }
    Expand-Archive -LiteralPath $zip[0].FullName -DestinationPath "$fixture/extracted"
    $root = @(Get-ChildItem "$fixture/extracted" -Directory)[0]
    $names = @(Get-ChildItem $root.FullName -Recurse -File | ForEach-Object { $_.FullName.Substring($root.FullName.Length + 1).Replace('\', '/') } | Sort-Object)
    $expected = @(Get-Content (Join-Path $repo 'scripts/runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
    if (Compare-Object ($expected | Sort-Object) $names) { throw "Wrong contents: $names" }
    $meta = Get-Content (Join-Path $root.FullName 'docs/build-info.json') -Raw | ConvertFrom-Json
    $expectedCommit = & git -C $repo rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected commit' }
    $sourceStatus = & git -C $repo status --porcelain
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected source status' }
    $expectedDirty = -not [string]::IsNullOrWhiteSpace(($sourceStatus -join "`n"))
    if ($meta.configuration -ne 'Release' -or $meta.source_dirty -ne $expectedDirty -or $meta.commit -ne $expectedCommit) { throw 'Wrong metadata' }
    Write-Host 'PASS: ZIP naming/content and explicit dirty-source metadata.'
    try {
        & "$repo/scripts/Package.ps1" -Configuration Release -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/output"
        throw 'Expected package overwrite rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Package already exists') { throw }
        Write-Host 'PASS: existing ZIP preserved.'
    }
    # A root-level document caused the regression: prove omissions are rejected
    # with a useful filename, rather than weakening the allowlist to pass.
    $installRules | Where-Object { $_ -notlike '*FILES "docs/CONFIGURATION_BUNDLES.md"*' } |
        Set-Content "$fixture/source/CMakeLists.txt"
    & cmake -S "$fixture/source" -B "$fixture/build"
    if ($LASTEXITCODE -ne 0) { throw 'Missing-document fixture configure failed' }
    try {
        & "$repo/scripts/Package.ps1" -Configuration Debug -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/missing"
        throw 'Expected missing-runtime rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected Runtime component' -or
            $_.Exception.Message -notmatch 'Missing: docs/CONFIGURATION_BUNDLES.md') { throw }
        if (@(Get-ChildItem "$fixture/missing" -Force).Count -ne 0) { throw 'Missing-file staging was not cleaned up' }
        Write-Host 'PASS: missing handoff document reported; staging cleaned up.'
    }
    $installRules | Set-Content "$fixture/source/CMakeLists.txt"
    'Unwanted test fixture' | Set-Content "$fixture/source/audiohole_tests.exe"
    'install(FILES audiohole_tests.exe DESTINATION . COMPONENT Runtime)' | Add-Content "$fixture/source/CMakeLists.txt"
    & cmake -S "$fixture/source" -B "$fixture/build"
    if ($LASTEXITCODE -ne 0) { throw 'Second fixture configure failed' }
    try {
        & "$repo/scripts/Package.ps1" -Configuration Debug -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/rejected"
        throw 'Expected unexpected-runtime rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected Runtime component') { throw }
        if ($_.Exception.Message -notmatch 'Unexpected: audiohole_tests.exe') { throw }
        if (@(Get-ChildItem "$fixture/rejected" -Force).Count -ne 0) { throw 'Rejected staging files were not cleaned up' }
        Write-Host 'PASS: unexpected runtime/test executable rejected; staging cleaned up.'
    }
    if ($env:OS -ne 'Windows_NT') {
    try {
        & "$repo/scripts/Test-Package.ps1" -Archive $zip[0].FullName
        throw 'Expected non-Windows execution rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'requires native Windows') { throw }
        Write-Host 'PASS: native package smoke check refuses an unsupported host.'
    }
    }
} finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
