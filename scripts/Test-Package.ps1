# Native smoke of the audited runtime ZIP, before the optional installer is added
# beside AudioHole.exe in the final portable bundle. Runs outside the checkout.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Archive)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Package execution validation requires native Windows.' }
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("AudioHole-package-check-" + [guid]::NewGuid())
$process = $null
try {
    Expand-Archive -LiteralPath $archivePath -DestinationPath $temporary
    $roots = @(Get-ChildItem -LiteralPath $temporary -Directory)
    if ($roots.Count -ne 1) { throw 'Expected one package root.' }
    $root = $roots[0].FullName
    $files = @(Get-ChildItem -LiteralPath $root -File -Recurse -Force)
    $expected = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
    $relative = @($files | ForEach-Object { $_.FullName.Substring($root.TrimEnd([IO.Path]::DirectorySeparatorChar).Length + 1).Replace('\', '/') })
    if (Compare-Object ($expected | Sort-Object) ($relative | Sort-Object)) {
        throw 'Package contains missing or unexpected files.'
    }
    $approved = Get-Content (Join-Path $PSScriptRoot '../assets/personalities/provenance/deployed-assets.json') -Raw | ConvertFrom-Json
    foreach ($property in $approved.files.PSObject.Properties) {
        $actualHash = (Get-FileHash -LiteralPath (Join-Path $root $property.Name) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $property.Value) { throw "Avatar asset differs from approved input: $($property.Name)" }
    }
    $metadata = Get-Content (Join-Path $root 'docs/build-info.json') -Raw | ConvertFrom-Json
    if ($metadata.commit -notmatch '^[0-9a-f]{40}$' -or $metadata.source_dirty) { throw 'Invalid or dirty package metadata.' }
    # The embedded logo/icon must work even without the reference artwork copies.
    # This disposable extraction is distinct from the ZIP uploaded as the artifact.
    New-Item -ItemType Directory -Path (Join-Path $temporary 'reference-assets') | Out-Null
    foreach ($folder in @('branding', 'presentation')) {
        Move-Item -LiteralPath (Join-Path $root "assets/$folder") -Destination (Join-Path $temporary "reference-assets/$folder")
    }
    $exe = Join-Path $root 'AudioHole.exe'
    # The runner has VS installed: inspect imports so that cannot mask a missing
    # redistributable/debug CRT dependency during the isolated launch test.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Cannot locate dumpbin for the runtime dependency audit.' }
    $version = (Get-Content (Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
    $dumpbin = Join-Path $installation "VC/Tools/MSVC/$version/bin/Hostx64/x64/dumpbin.exe"
    foreach ($binary in @('AudioHole.exe', 'bin/audiohole-probe.exe')) {
        $imports = & $dumpbin /nologo /dependents (Join-Path $root $binary)
        if ($LASTEXITCODE -ne 0) { throw "Runtime import audit failed: $binary" }
        if (($imports -join "`n") -match '(?i)\b(?:msvcp\d+\w*|vcruntime\w*|ucrtbased|libstdc\+\+[^\s]*|libgcc[^\s]*)\.dll') {
            throw "Package unexpectedly imports a non-system compiler runtime DLL: $binary"
        }
    }
    # The helper accepts only the private protocol; never discover hardware here.
    $helper = Start-Process -FilePath (Join-Path $root 'bin/audiohole-probe.exe') -WorkingDirectory $root -PassThru
    if (-not $helper.WaitForExit(10000)) { $helper.Kill(); throw 'Private probe helper did not reject missing arguments.' }
    $helper.WaitForExit()
    if ($helper.ExitCode -ne 2) { throw 'Private probe helper accepted a public invocation.' }
    # Run the GUI's internal smoke mode from the extracted tree. It dispatches
    # native widget events and writes evidence; stdout is not a GUI contract.
    foreach ($mode in @('smoke-test', 'test-mode-smoke', 'tutorial-smoke', 'engine-smoke')) {
        $evidence = Join-Path $temporary "gui-evidence/$mode"
        $process = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru -ArgumentList @("--$mode", '--screenshot-dir', "`"$evidence`"")
        if (-not $process.WaitForExit(45000)) { throw "Extracted GUI $mode did not finish within 45 seconds." }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Extracted GUI $mode failed with exit code $($process.ExitCode)." }
        $result = Join-Path $evidence 'smoke-result.txt'
        if (-not (Test-Path $result) -or (Get-Content $result -Raw) -notmatch '^PASS:') {
            throw "Extracted GUI $mode did not produce successful smoke evidence."
        }
    }
    if ((Test-Path (Join-Path $root 'bin/AudioHole.has-run')) -or (Test-Path (Join-Path $root 'AudioHole.has-run'))) {
        throw 'Smoke execution modified the installed first-run marker.'
    }
    Write-Host "PASS: isolated package launch and runtime import audit ($($metadata.configuration), $($metadata.commit))."
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
