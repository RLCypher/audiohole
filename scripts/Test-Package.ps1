# Native Windows smoke test of the exact ZIP uploaded by CI, outside the checkout.
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
    $files = @(Get-ChildItem -LiteralPath $root -File -Recurse)
    if ((($files.Name | Sort-Object) -join ',') -ne 'audiohole_dev.exe,build-info.json,README.txt') {
        throw 'Package contains missing or unexpected files.'
    }
    $metadata = Get-Content (Join-Path $root 'build-info.json') -Raw | ConvertFrom-Json
    if ($metadata.commit -notmatch '^[0-9a-f]{40}$' -or $metadata.source_dirty) { throw 'Invalid or dirty package metadata.' }
    $exe = Join-Path $root 'audiohole_dev.exe'
    # The runner has VS installed: inspect imports so that cannot mask a missing
    # redistributable/debug CRT dependency during the isolated launch test.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $installation = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Cannot locate dumpbin for the runtime dependency audit.' }
    $version = (Get-Content (Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
    $dumpbin = Join-Path $installation "VC/Tools/MSVC/$version/bin/Hostx64/x64/dumpbin.exe"
    $imports = & $dumpbin /nologo /dependents $exe
    if ($LASTEXITCODE -ne 0) { throw 'Runtime import audit failed.' }
    if (($imports -join "`n") -match '(?i)\b(?:msvcp\d+\w*|vcruntime\w*|ucrtbased|libstdc\+\+[^\s]*|libgcc[^\s]*)\.dll') {
        throw 'Package unexpectedly imports a non-system compiler runtime DLL.'
    }
    $stdout = Join-Path $temporary 'stdout.txt'
    $stderr = Join-Path $temporary 'stderr.txt'
    $process = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit(15000)) { throw 'Extracted application did not finish within 15 seconds.' }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Extracted application failed: $(Get-Content $stderr -Raw)" }
    if ((Get-Content $stdout -Raw) -notmatch 'COM initialized\. Audio routing is not implemented\.') {
        throw 'Extracted application did not report expected initialization.'
    }
    Write-Host "PASS: isolated package launch and runtime import audit ($($metadata.configuration), $($metadata.commit))."
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
