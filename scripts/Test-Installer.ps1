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
function Run-Setup([string]$Label) {
    $log = Join-Path $temporary "$Label.log"
    $process = Start-Process -FilePath $Installer -PassThru -ArgumentList @('/CURRENTUSER', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$target`"", "/LOG=`"$log`"")
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
try {
    New-Item -ItemType Directory -Path $temporary -Force | Out-Null
    $null = Run-Setup 'fresh'
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
    $uninstaller = Get-InstalledUninstaller
    $uninstallLog = Join-Path $temporary 'uninstall-keep.log'
    $process = Start-Process -FilePath $uninstaller -PassThru -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    if (-not $process.WaitForExit(60000)) { $process.Kill(); throw 'Uninstall timed out' }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0 -or (Test-Path (Join-Path $target 'AudioHole.exe'))) { throw 'Application uninstall failed' }
    if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Default uninstall lost user data' }
    $null = Run-Setup 'reinstall-after-uninstall'
    $uninstaller = Get-InstalledUninstaller
    $log = Run-Setup 'verify-reinstalled-registration'
    if (-not $log.Contains("AudioHole registered uninstaller: $uninstaller")) {
        throw 'Installer did not resolve the current registered uninstaller.'
    }
    $uninstaller = Get-InstalledUninstaller
    $uninstallLog = Join-Path $temporary 'uninstall-final.log'
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$uninstallLog`"")
    if (-not $existingData) { $arguments += '/PURGEDATA=1' }
    $process = Start-Process -FilePath $uninstaller -PassThru -ArgumentList $arguments
    if (-not $process.WaitForExit(60000)) { $process.Kill(); throw 'Final uninstall timed out' }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw 'Final uninstall failed' }
    if (-not $existingData -and (Test-Path -LiteralPath $dataRoot)) { throw 'Explicit purge retained the AudioHole data directory' }
    Write-Host 'PASS: fresh install, version decisions, owned-file retirement, data preservation and uninstall lifecycle.'
} finally {
    New-Item -ItemType Directory -Path $Evidence -Force | Out-Null
    if (Test-Path -LiteralPath $temporary) {
        Get-ChildItem -LiteralPath $temporary -Filter '*.log' | Copy-Item -Destination $Evidence -Force
    }
    if (Test-Path -LiteralPath $sentinel) { Remove-Item -LiteralPath $sentinel -Force }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
