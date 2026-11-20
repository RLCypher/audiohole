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
    $names = @(Get-ChildItem $root.FullName -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($root.FullName.Length + 1).Replace('\', '/') } | Sort-Object)
    $expected = @(Get-Content (Join-Path $repo 'scripts/runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
    if (Compare-Object ($expected | Sort-Object) $names) { throw "Wrong contents: $names" }
    $meta = Get-Content (Join-Path $root.FullName 'docs/build-info.json') -Raw | ConvertFrom-Json
    $expectedCommit = & git -C $repo rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected commit' }
    $sourceStatus = & git -C $repo status --porcelain
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected source status' }
    $expectedDirty = -not [string]::IsNullOrWhiteSpace(($sourceStatus -join "`n"))
    if ($meta.configuration -ne 'Release' -or $meta.source_dirty -ne $expectedDirty -or $meta.commit -ne $expectedCommit) { throw 'Wrong metadata' }
    if ($meta.format -ne 'audiohole-build-info' -or $meta.schema_version -ne 1 -or
        $meta.product -ne 'AudioHole' -or $meta.version -notmatch '^\d+\.\d+\.\d+$' -or
        $meta.build_number -lt 0 -or -not $meta.source_archive.EndsWith('.zip')) { throw 'Missing installer/source metadata' }
    if (Compare-Object ($meta.runtime_files | Sort-Object) $names) { throw 'Runtime manifest differs from package' }
    Write-Host 'PASS: ZIP naming/content, installer metadata and explicit dirty-source metadata.'
    # The final download must remain directly runnable, with its optional setup
    # beside the application. A compiler fixture verifies payload separation
    # without building or executing an installer on this host.
    $compiler = Join-Path $fixture 'inert-compiler.ps1'
    @'
$payload = ($args | Where-Object { $_ -like '/DPayload=*' }).Substring(10)
$output = ($args | Where-Object { $_ -like '/DOutput=*' }).Substring(9)
$name = ($args | Where-Object { $_ -like '/DOutputName=*' }).Substring(13)
if (@(Get-ChildItem -LiteralPath $payload -Filter '*Setup*.exe').Count) { throw 'Installer recursively included in its own payload' }
if (-not (Test-Path -LiteralPath (Join-Path $payload 'AudioHole.exe'))) { throw 'Missing installer runtime payload' }
'Inert setup fixture; never execute.' | Set-Content -LiteralPath (Join-Path $output "$name.exe")
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $compiler
    $bundle = Join-Path $fixture 'portable'
    & "$repo/scripts/Build-Installer.ps1" -Archive $zip[0].FullName -OutputDirectory $bundle -Compiler $compiler
    $setup = @(Get-ChildItem -LiteralPath $bundle -Filter 'AudioHole-Setup-*.exe')
    if ($setup.Count -ne 1 -or -not (Test-Path -LiteralPath (Join-Path $bundle 'AudioHole.exe'))) { throw 'Portable application and installer were not colocated' }
    & "$repo/scripts/Test-PortableBundle.ps1" -Directory $bundle
    & "$repo/scripts/Build-Installer.ps1" -Archive $zip[0].FullName -OutputDirectory $root.FullName -Compiler $compiler
    $source = Join-Path $bundle $meta.source_archive
    'Inert source archive fixture' | Set-Content -LiteralPath $source
    try {
        & "$repo/scripts/Test-PortableBundle.ps1" -Directory $bundle
        throw 'Expected bundled source rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected portable bundle contents' -or
            -not $_.Exception.Message.Contains($meta.source_archive)) { throw }
    }
    Remove-Item -LiteralPath $source
    Remove-Item -LiteralPath $setup[0].FullName
    try {
        & "$repo/scripts/Test-PortableBundle.ps1" -Directory $bundle
        throw 'Expected misplaced/missing installer rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected portable bundle contents.*Missing: AudioHole-Setup-') { throw }
    }
    'Different build fixture' | Set-Content -LiteralPath (Join-Path $bundle 'AudioHole.exe')
    try {
        & "$repo/scripts/Build-Installer.ps1" -Archive $zip[0].FullName -OutputDirectory $bundle -Compiler $compiler
        throw 'Expected mixed-build destination rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Portable destination file differs.*AudioHole.exe') { throw }
    }
    Write-Host 'PASS: installer beside portable application, separate payload, source exclusion, missing-installer and mixed-build rejection.'
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
