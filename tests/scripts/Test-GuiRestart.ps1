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
    $childExit = $null
    if ($child) {
        if (-not $child.WaitForExit(45000)) { throw 'Restarted GUI did not finish its smoke checks.' }
        $child.WaitForExit()
        $childExit = $child.ExitCode
    }
    $result = Join-Path $run 'smoke-result.txt'
    $report = if (Test-Path -LiteralPath $result) {
        Get-Content -LiteralPath $result -Raw
    } else { '(smoke-result.txt was not written)' }
    # A restarted GUI has its own native process handles, so its stderr is not
    # necessarily inherited by CTest. Keep the actual failed assertion in the
    # ordinary test log even when optional evidence uploads are disabled.
    if (($null -ne $childExit -and $childExit -ne 0) -or $report -notmatch '^PASS:') {
        throw "Restarted GUI failed (child exit: $childExit): $report"
    }
    Write-Host 'PASS: inherited parent-process handshake and restarted GUI smoke.'
} finally {
    foreach ($process in @($parent, $child)) {
        if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    }
}
