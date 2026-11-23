# Packaging regression tests use inert generated fixtures, never executable application validation.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $repo 'scripts/Build-Names.ps1')
if ((Get-AudioHoleDisplayVersion '0.1.0') -ne '0.1' -or
    (Get-AudioHoleDisplayVersion '0.1.2') -ne '0.1.2' -or
    (Get-AudioHoleDownloadName -Kind Installer -Version '0.1.0' -Commit ('a' * 40)) -ne 'AudioHole Installer v0.1-aaaaaaa') {
    throw 'Versioned display names lost their version or nonzero patch number.'
}
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
        if ($relative -eq 'AudioHole License.rtf') {
            Copy-Item -LiteralPath (Join-Path $repo 'docs/legal/AudioHole License.rtf') -Destination $path
        } else {
            'Inert runtime fixture; not executable application content.' | Set-Content $path
        }
        $separator = $relative.LastIndexOf('/')
        $destination = if ($separator -lt 0) { '.' } else { $relative.Substring(0, $separator) }
        $installRules += 'install(FILES "' + $relative + '" DESTINATION "' + $destination + '" COMPONENT Runtime)'
    }
    $installRules | Set-Content "$fixture/source/CMakeLists.txt"
    & cmake -S "$fixture/source" -B "$fixture/build"
    if ($LASTEXITCODE -ne 0) { throw 'Fixture configure failed' }
    $project = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
    if ($project -notmatch 'project\(AudioHole VERSION (\d+\.\d+\.\d+) ') { throw 'Missing version' }
    $identity = @{version=$Matches[1]; commit=(& git -C $repo rev-parse HEAD); built_at_utc='2026-11-12T23:59:00Z'; build_date_utc='2026-11-12'; vintage='November 12, 2026'}
    $identity | ConvertTo-Json | Set-Content "$fixture/build/build-identity.json"

    & "$repo/scripts/Package.ps1" -Configuration Release -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/output"
    $zip = @(Get-ChildItem "$fixture/output/*.zip")
    if ($zip.Count -ne 1) { throw 'Expected exactly one ZIP' }
    Expand-Archive -LiteralPath $zip[0].FullName -DestinationPath "$fixture/extracted"
    $root = @(Get-ChildItem "$fixture/extracted" -Directory)[0]
    $names = @(Get-ChildItem $root.FullName -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($root.FullName.Length + 1).Replace('\', '/') } | Sort-Object)
    $expected = @(Get-Content (Join-Path $repo 'scripts/runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
    if (Compare-Object ($expected | Sort-Object) $names) { throw "Wrong contents: $names" }
    $readmeHash = (Get-FileHash -LiteralPath (Join-Path $repo 'docs/distribution/README.txt') -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $root.FullName 'README.txt') -Algorithm SHA256).Hash -ne $readmeHash) {
        throw 'Packaged README differs from the supplied document.'
    }
    if (Compare-Object @('docs/build-info.json') @($names | Where-Object { $_ -like 'docs/*' })) {
        throw 'Runtime docs must contain only installer/build metadata.'
    }
    $meta = Get-Content (Join-Path $root.FullName 'docs/build-info.json') -Raw | ConvertFrom-Json
    $expectedCommit = & git -C $repo rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected commit' }
    $sourceStatus = & git -C $repo status --porcelain
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read expected source status' }
    $expectedDirty = -not [string]::IsNullOrWhiteSpace(($sourceStatus -join "`n"))
    if ($meta.configuration -ne 'Release' -or $meta.source_dirty -ne $expectedDirty -or $meta.commit -ne $expectedCommit) { throw 'Wrong metadata' }
    $displayVersion = Get-AudioHoleDisplayVersion $meta.version
    $suffix = if ($expectedDirty) { '-dirty' } else { '' }
    $expectedName = "AudioHole Portable v$displayVersion-$($expectedCommit.Substring(0,7))$suffix"
    if ($zip[0].BaseName -ne $expectedName -or $root.Name -ne $expectedName -or
        $meta.source_archive -ne "AudioHole ZIP-source v$displayVersion-$($expectedCommit.Substring(0,7))$suffix.zip") {
        throw 'Archive/folder names must use the display version and seven-character commit.'
    }
    if ($meta.release_repository -ne 'RLCypher/audiohole' -or $meta.release_tag -ne ('v' + $meta.version) -or
        $meta.installer_file -ne "AudioHole Installer v$displayVersion-$($expectedCommit.Substring(0,7))$suffix.exe" -or
        $meta.portable_archive -ne ($expectedName + '.zip') -or
        $meta.debug_archive -ne "AudioHole ZIP-debug v$displayVersion-$($expectedCommit.Substring(0,7))$suffix.zip") { throw 'Release/update metadata differs from download names.' }
    if ($meta.format -ne 'audiohole-build-info' -or $meta.schema_version -ne 1 -or
        $meta.product -ne 'AudioHole' -or $meta.version -notmatch '^\d+\.\d+\.\d+$' -or
        $meta.build_number -lt 0 -or -not $meta.source_archive.EndsWith('.zip')) { throw 'Missing installer/source metadata' }
    if (Compare-Object ($meta.runtime_files | Sort-Object) $names) { throw 'Runtime manifest differs from package' }
    if ($meta.built_at_utc -ne $identity.built_at_utc -or $meta.build_date_utc -ne $identity.build_date_utc -or
        $meta.vintage -ne $identity.vintage -or $meta.project_codename -ne 'BLANCO') { throw 'Compiled Vintage was not preserved.' }
    $licenseHash = (Get-FileHash (Join-Path $repo 'docs/legal/AudioHole License.rtf')).Hash
    if ((Get-FileHash (Join-Path $root.FullName 'AudioHole License.rtf')).Hash -ne $licenseHash) { throw 'License differs from supplied RTF.' }

    Write-Host 'PASS: ZIP naming/content, installer metadata and explicit dirty-source metadata.'
    # The standalone installer consumes the runtime without copying itself or
    # its files into the portable folder. Its output directory contains one EXE.
    $compiler = Join-Path $fixture 'inert-compiler.ps1'
    @'
$payload = ($args | Where-Object { $_ -like '/DPayload=*' }).Substring(10)
$output = ($args | Where-Object { $_ -like '/DOutput=*' }).Substring(9)
$name = ($args | Where-Object { $_ -like '/DOutputName=*' }).Substring(13)
if (@(Get-ChildItem -LiteralPath $payload -Filter 'AudioHole Installer v*.exe').Count) { throw 'Installer recursively included in its own payload' }
if (-not (Test-Path -LiteralPath (Join-Path $payload 'AudioHole.exe'))) { throw 'Missing installer runtime payload' }
'Inert setup fixture; never execute.' | Set-Content -LiteralPath (Join-Path $output "$name.exe")
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $compiler
    $installerDirectory = Join-Path $fixture 'installer'
    & "$repo/scripts/Build-Installer.ps1" -Archive $zip[0].FullName -OutputDirectory $installerDirectory -Compiler $compiler
    $setup = @(Get-ChildItem -LiteralPath $installerDirectory -File -Recurse)
    if ($setup.Count -ne 1 -or $setup[0].Name -ne ((Get-AudioHoleDownloadName -Kind Installer -Version $meta.version -Commit $meta.commit -Dirty:$meta.source_dirty) + '.exe')) { throw 'Installer output must contain only the setup EXE' }
    & "$repo/scripts/Test-PortableBundle.ps1" -Directory $root.FullName
    $source = Join-Path $root.FullName $meta.source_archive
    'Inert source archive fixture' | Set-Content -LiteralPath $source
    try {
        & "$repo/scripts/Test-PortableBundle.ps1" -Directory $root.FullName
        throw 'Expected bundled source rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected portable bundle contents' -or
            -not $_.Exception.Message.Contains($meta.source_archive)) { throw }
    }
    Remove-Item -LiteralPath $source
    $misplaced = Join-Path $root.FullName $setup[0].Name
    Copy-Item -LiteralPath $setup[0].FullName -Destination $misplaced
    try {
        & "$repo/scripts/Test-PortableBundle.ps1" -Directory $root.FullName
        throw 'Expected bundled installer rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected portable bundle contents' -or
            -not $_.Exception.Message.Contains($setup[0].Name)) { throw }
    }
    Remove-Item -LiteralPath $misplaced
    try {
        & "$repo/scripts/Build-Installer.ps1" -Archive $zip[0].FullName -OutputDirectory $root.FullName -Compiler $compiler
        throw 'Expected portable/installer destination separation was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Installer output directory must be empty') { throw }
    }
    Write-Host 'PASS: standalone installer, portable installer/source exclusion and destination separation.'
    $licensePath = Join-Path $root.FullName 'AudioHole License.rtf'
    '{\rtf1 Wrong license fixture}' | Set-Content -LiteralPath $licensePath
    $wrongLicense = Join-Path $fixture 'wrong-license.zip'
    Push-Location $root.Parent.FullName
    try {
        & cmake -E tar cf $wrongLicense --format=zip $root.Name
        if ($LASTEXITCODE -ne 0) { throw 'Wrong-license fixture archive failed' }
    } finally { Pop-Location }
    Copy-Item -LiteralPath (Join-Path $repo 'docs/legal/AudioHole License.rtf') -Destination $licensePath -Force
    try {
        & "$repo/scripts/Build-Installer.ps1" -Archive $wrongLicense -OutputDirectory "$fixture/wrong-installer" -Compiler $compiler
        throw 'Expected mismatching license rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Installer agreement differs') { throw }
    }
    foreach ($case in @('date', 'commit')) {
        $invalid = $identity.Clone()
        if ($case -eq 'date') { $invalid.vintage = 'Wrong date' } else { $invalid.commit = '0' * 40 }
        $invalid | ConvertTo-Json | Set-Content "$fixture/build/build-identity.json"
        try {
            & "$repo/scripts/Package.ps1" -Configuration Release -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/stale-$case"
            throw 'Expected inconsistent build identity rejection was missing'
        } catch {
            if ($_.Exception.Message -notmatch 'Invalid compiled build identity|Build identity belongs to another commit') { throw }
        }
    }
    $identity | ConvertTo-Json | Set-Content "$fixture/build/build-identity.json"
    Write-Host 'PASS: inconsistent Vintage, stale commit and mismatching installer agreement rejected.'
    try {
        & "$repo/scripts/Package.ps1" -Configuration Release -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/output"
        throw 'Expected package overwrite rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Package already exists') { throw }
        Write-Host 'PASS: existing ZIP preserved.'
    }
    # A required guide must be present: prove omissions are rejected
    # with a useful filename, rather than weakening the allowlist to pass.
    $installRules | Where-Object { $_ -notlike '*FILES "Guides and Manuals/AudioHole User Manual.txt"*' } |
        Set-Content "$fixture/source/CMakeLists.txt"
    & cmake -S "$fixture/source" -B "$fixture/build"
    if ($LASTEXITCODE -ne 0) { throw 'Missing-document fixture configure failed' }
    try {
        & "$repo/scripts/Package.ps1" -Configuration Debug -BuildDirectory "$fixture/build" -OutputDirectory "$fixture/missing"
        throw 'Expected missing-runtime rejection was missing'
    } catch {
        if ($_.Exception.Message -notmatch 'Unexpected Runtime component' -or
            $_.Exception.Message -notmatch 'Missing: Guides and Manuals/AudioHole User Manual.txt') { throw }
        if (@(Get-ChildItem "$fixture/missing" -Force).Count -ne 0) { throw 'Missing-file staging was not cleaned up' }
        Write-Host 'PASS: missing user manual reported; staging cleaned up.'
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
