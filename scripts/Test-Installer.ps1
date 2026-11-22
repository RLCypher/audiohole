# Native installation lifecycle test. Never launches routing or touches preexisting data.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Installer,
    [string]$Evidence = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/installer-evidence')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Installer lifecycle validation requires native Windows.' }
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('AudioHole installer test ' + [guid]::NewGuid())
$target = Join-Path $temporary 'AudioHole'
$dataRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'AudioHole'
$existingData = Test-Path -LiteralPath $dataRoot
$sentinel = Join-Path $dataRoot ('installer-test-' + [guid]::NewGuid() + '.json')
function Get-InstalledUninstaller {
    # Never cache the numbered filename across install/uninstall cycles. Inno
    # may choose the next free number while a previous uninstaller cleans up.
    $registration = Get-ItemProperty -LiteralPath 'Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioHole_is1'
    $expectedDirectory = [IO.Path]::GetFullPath($target).TrimEnd('\')
    $registeredDirectory = [IO.Path]::GetFullPath([string]$registration.InstallLocation).TrimEnd('\')
    if (-not $expectedDirectory.Equals($registeredDirectory, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Uninstaller registration points outside the test installation.'
    }
    $command = ([string]$registration.UninstallString).Trim()
    $uninstallerPath = if ($command -match '^"([^"]+)"(?:\s.*)?$') { $Matches[1] } else { $command }
    if ([IO.Path]::GetFileName($uninstallerPath) -notmatch '^unins[0-9]{3}\.exe$' -or
        -not $expectedDirectory.Equals([IO.Path]::GetFullPath((Split-Path $uninstallerPath)), [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $uninstallerPath -PathType Leaf)) {
        throw "Registered uninstaller is missing or outside the test installation: $uninstallerPath"
    }
    return $uninstallerPath
}
function Run-Setup([string]$Label, [int]$ParentId = 0) {
    $log = Join-Path $temporary "$Label.log"
    $arguments = @('/CURRENTUSER', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$target`"", "/LOG=`"$log`"")
    if ($ParentId -gt 0) { $arguments += "/UPDATEPARENT=$ParentId" }
    $process = Start-Process -FilePath $Installer -PassThru -ArgumentList $arguments
    if (-not $process.WaitForExit(120000)) { $process.Kill(); throw "Installer timed out: $Label" }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 80 | Write-Host }
        throw "Installer failed ($Label, $($process.ExitCode)); see $log"
    }
    $uninstallerPath = Get-InstalledUninstaller
    Write-Host "Installed uninstaller ($Label): $uninstallerPath"
    return Get-Content -LiteralPath $log -Raw
}
function Run-Uninstall([string]$Label, [switch]$PurgeData) {
    $uninstallerPath = Get-InstalledUninstaller
    $log = Join-Path $temporary "$Label.log"
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$log`"")
    if ($PurgeData) { $arguments += '/PURGEDATA=1' }
    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    $process = Start-Process -FilePath $uninstallerPath -PassThru -ArgumentList $arguments
    if (-not $process.WaitForExit(60000)) { $process.Kill(); throw "Uninstall launcher timed out: $Label" }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Uninstall launcher failed: $Label ($($process.ExitCode))" }
    # Inno runs its actual uninstall in a temporary second process. Launcher
    # exit alone does not prove that post-uninstall data removal has finished.
    # Require the pinned engine's success/end records and an exclusive log open,
    # which proves its writer has closed before checking state or copying logs.
    while ([DateTime]::UtcNow -lt $deadline) {
        $contents = $null
        try {
            $stream = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
            try {
                $reader = [IO.StreamReader]::new($stream)
                try { $contents = $reader.ReadToEnd() } finally { $reader.Dispose() }
            } finally { $stream.Dispose() }
        } catch [IO.IOException] {
            # The worker may not have created the log yet, or may still own it.
        }
        if ($null -ne $contents -and $contents.Contains('Log closed.')) {
            if (-not $contents.Contains('Uninstallation process succeeded.')) {
                Write-Host $contents
                throw "Uninstallation did not succeed: $Label"
            }
            return
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Uninstall worker did not finish within 60 seconds: $Label; see $log"
}
$completed = $false
try {
    New-Item -ItemType Directory -Path $temporary -Force | Out-Null
    $null = Run-Setup 'fresh'
    # A harmless parent waits until setup announces its parent wait. Setup must
    # then observe its exit before normal installation; no AudioHole is launched.
    $parentScript = Join-Path $temporary 'parent-fixture.ps1'
    @'
param([string]$Log)
$deadline = [DateTime]::UtcNow.AddSeconds(30)
while ([DateTime]::UtcNow -lt $deadline) {
    $contents = if (Test-Path -LiteralPath $Log) { Get-Content -LiteralPath $Log -Raw -ErrorAction SilentlyContinue } else { '' }
    if ($contents -and $contents.Contains('AudioHole update parent wait:')) { exit 0 }
    Start-Sleep -Milliseconds 100
}
exit 1
'@ | Set-Content -LiteralPath $parentScript
    $handoffLog = Join-Path $temporary 'update-handoff.log'
    $parent = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -PassThru -ArgumentList "-NoLogo -NoProfile -File `"$parentScript`" -Log `"$handoffLog`""
    try {
        $handoff = Run-Setup 'update-handoff' $parent.Id
        $parent.WaitForExit()
        if ($parent.ExitCode -ne 0 -or -not $handoff.Contains('AudioHole update parent exited.')) {
            throw 'Installer did not complete the automatic-update parent-exit handoff.'
        }
    } finally {
        if (-not $parent.HasExited) { $parent.Kill() }
    }
    $metadataFile = Join-Path $target 'docs/build-info.json'
    $meta = Get-Content -LiteralPath $metadataFile -Raw | ConvertFrom-Json
    if ($meta.product -ne 'AudioHole') { throw 'Wrong installed metadata' }
    foreach ($relative in $meta.runtime_files) {
        if (-not (Test-Path -LiteralPath (Join-Path $target $relative) -PathType Leaf)) { throw "Missing installed file: $relative" }
    }
    New-Item -ItemType Directory -Path $dataRoot -Force | Out-Null
    '{"preserve":"profiles settings and onboarding"}' | Set-Content -LiteralPath $sentinel
    'user-added content' | Set-Content -LiteralPath (Join-Path $target 'custom.txt')
    $currentVersion = $meta.version
    foreach ($case in @(
        @{ old='0.0.0'; action='Update' },
        @{ old=$currentVersion; action='Reinstall' },
        @{ old='999.0.0'; action='Reinstall' },
        @{ old='invalid'; action='Reinstall' }
    )) {
        $previous = Get-Content -LiteralPath $metadataFile -Raw | ConvertFrom-Json
        $previous.version = $case.old
        $previous.runtime_files += 'docs/retired-installer-fixture.txt'
        'retired owned file' | Set-Content (Join-Path $target 'docs/retired-installer-fixture.txt')
        $previous | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $metadataFile -Encoding utf8BOM
        $log = Run-Setup ("version-" + $case.old)
        if ($log -notmatch ('action: ' + $case.action)) { throw "Wrong version decision: $($case.old)" }
        if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Reinstallation lost saved data' }
        if ((Get-Content (Join-Path $target 'custom.txt') -Raw).Trim() -ne 'user-added content') { throw 'Reinstallation lost unrelated files' }
        if ($case.old -ne 'invalid' -and (Test-Path (Join-Path $target 'docs/retired-installer-fixture.txt'))) { throw 'Retired owned file was not removed' }
    }
    Run-Uninstall 'uninstall-keep'
    if (Test-Path (Join-Path $target 'AudioHole.exe')) { throw 'Application uninstall failed' }
    if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Default uninstall lost user data' }
    $null = Run-Setup 'reinstall-after-uninstall'
    $uninstaller = Get-InstalledUninstaller
    $log = Run-Setup 'verify-reinstalled-registration'
    if (-not $log.Contains("AudioHole registered uninstaller: $uninstaller")) {
        throw 'Installer did not resolve the current registered uninstaller.'
    }
    Run-Uninstall 'uninstall-final' -PurgeData:(-not $existingData)
    if (Test-Path (Join-Path $target 'AudioHole.exe')) { throw 'Final application uninstall failed' }
    if (-not $existingData -and (Test-Path -LiteralPath $dataRoot)) { throw 'Explicit purge retained the AudioHole data directory' }
    Write-Host 'PASS: fresh install, version decisions, owned-file retirement, data preservation and uninstall lifecycle.'
    $completed = $true
} finally {
    if (-not $completed -and (Test-Path -LiteralPath $temporary)) {
        foreach ($log in Get-ChildItem -LiteralPath $temporary -Filter '*.log') {
            Write-Host ("Installer test log: " + $log.Name)
            Get-Content -LiteralPath $log.FullName -Tail 40 | Write-Host
        }
    }
    if (Test-Path -LiteralPath $temporary) {
        if ($env:AUDIOHOLE_CAPTURE_EVIDENCE -eq '1') {
            New-Item -ItemType Directory -Path $Evidence -Force | Out-Null
            Get-ChildItem -LiteralPath $temporary -Filter '*.log' | Copy-Item -Destination $Evidence -Force
        }
    }
    if (Test-Path -LiteralPath $sentinel) { Remove-Item -LiteralPath $sentinel -Force }
    # Cleanup must not replace a more useful uninstall/assertion failure. Logs
    # have already been copied to evidence; an unfinished worker may retain one.
    if (Test-Path -LiteralPath $temporary) {
        try { Remove-Item -LiteralPath $temporary -Recurse -Force }
        catch { Write-Warning "Installer test temporary files remain at ${temporary}: $($_.Exception.Message)" }
    }
}
