[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable, [Parameter(Mandatory)][string]$Evidence)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$run = Join-Path $Evidence ([guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $run -Force | Out-Null
$parent = $null
$child = $null
try {
    $parent = Start-Process -FilePath $Executable -PassThru -ArgumentList @('--restart-smoke', '--screenshot-dir', "`"$run`"")
    if (-not $parent.WaitForExit(30000)) { throw 'Restart smoke parent did not close cleanly.' }
    $parent.WaitForExit()
    if ($parent.ExitCode -ne 0) { throw "Restart smoke parent failed: $($parent.ExitCode)" }
    $childIdFile = Join-Path $run 'restart-child.pid'
    if (-not (Test-Path $childIdFile)) { throw 'Restart did not launch a child process.' }
    $childPid = [int]((Get-Content -LiteralPath $childIdFile -Raw).Trim())
    $child = Get-Process -Id $childPid -ErrorAction SilentlyContinue
    if ($child -and -not $child.WaitForExit(45000)) { throw 'Restarted GUI did not finish its smoke checks.' }
    $result = Join-Path $run 'smoke-result.txt'
    if (-not (Test-Path $result) -or (Get-Content $result -Raw) -notmatch '^PASS:') {
        throw 'Restarted GUI did not produce successful smoke evidence.'
    }
    Write-Host 'PASS: inherited parent-process handshake and restarted GUI smoke.'
} finally {
    foreach ($process in @($parent, $child)) {
        if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    }
}
