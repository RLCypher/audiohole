# Native Rich Edit checks in Windows light, dark and high-contrast modes.
# Temporary Windows appearance changes are restored even if validation fails.
[CmdletBinding(DefaultParameterSetName='Application')]
param(
    [Parameter(Mandatory, ParameterSetName='Application')][string]$Executable,
    [Parameter(Mandatory, ParameterSetName='Installer')][string]$Installer,
    [Parameter(Mandatory, ParameterSetName='Installer')][string]$Target
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'License theme validation requires native Windows.' }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AudioHoleLicenseAppearance {
    [StructLayout(LayoutKind.Sequential)]
    public struct HighContrast { public uint Size, Flags; public IntPtr Scheme; }
    [DllImport("user32.dll", EntryPoint="SystemParametersInfoW", SetLastError=true)]
    static extern bool Parameters(uint action, uint size, ref HighContrast value, uint flags);
    public static HighContrast Read() {
        var value = new HighContrast { Size = (uint)Marshal.SizeOf<HighContrast>() };
        if (!Parameters(0x42, value.Size, ref value, 0))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        value.Scheme = IntPtr.Zero;
        return value;
    }
    public static void Set(HighContrast value, bool enabled) {
        value.Flags = enabled ? value.Flags | 1U : value.Flags & ~1U;
        if (!Parameters(0x43, value.Size, ref value, 2))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }
}
'@
$personalize = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
$original = Get-ItemProperty -LiteralPath $personalize -ErrorAction SilentlyContinue
$hadLight = $null -ne $original -and $null -ne $original.PSObject.Properties['AppsUseLightTheme']
$originalLight = if ($hadLight) { $original.AppsUseLightTheme } else { 1 }
$contrast = [AudioHoleLicenseAppearance]::Read()
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('AudioHole license colors ' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path $temporary -Force | Out-Null
    if (-not (Test-Path -LiteralPath $personalize)) {
        New-Item -Path $personalize -Force | Out-Null
    }
    foreach ($mode in @('light', 'dark', 'high-contrast')) {
        [AudioHoleLicenseAppearance]::Set($contrast, ($mode -eq 'high-contrast'))
        New-ItemProperty -LiteralPath $personalize -Name AppsUseLightTheme -PropertyType DWord -Value ([int]($mode -ne 'dark')) -Force | Out-Null
        $output = Join-Path $temporary "$mode-output.txt"
        $errors = Join-Path $temporary "$mode-errors.txt"
        if ($PSCmdlet.ParameterSetName -eq 'Installer') {
            $log = Join-Path $temporary "$mode-setup.log"
            $arguments = @('/CURRENTUSER', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/LICENSECOLORS=1', "/DIR=`"$Target`"", "/LOG=`"$log`"")
            $program = $Installer
        } else {
            $arguments = @('--license-smoke')
            $program = $Executable
        }
        $process = Start-Process -FilePath $program -PassThru -ArgumentList $arguments -RedirectStandardOutput $output -RedirectStandardError $errors
        if (-not $process.WaitForExit(90000)) { $process.Kill(); throw "License $mode check timed out." }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            Get-ChildItem -LiteralPath $temporary -File | ForEach-Object { Get-Content -LiteralPath $_.FullName | Write-Host }
            throw "License $mode check failed ($($process.ExitCode))."
        }
        if ($PSCmdlet.ParameterSetName -eq 'Installer') {
            $text = Get-Content -LiteralPath $log -Raw
            $expectedDark = [int]($mode -eq 'dark')
            $expectedContrast = [int]($mode -eq 'high-contrast')
            if (-not $text.Contains("PASS: complete license ink and RTF formatting; dark=$expectedDark; high-contrast=$expectedContrast;")) {
                throw "Installer did not verify the complete agreement in $mode mode: $text"
            }
        } elseif ((Get-Content -LiteralPath $output -Raw) -notmatch 'PASS: complete portable license ink and RTF formatting') {
            throw "Application did not verify the complete agreement in $mode mode."
        }
        Write-Host "PASS: license agreement in $mode mode."
    }
} finally {
    [AudioHoleLicenseAppearance]::Set($contrast, (($contrast.Flags -band 1) -ne 0))
    if ($hadLight) {
        New-ItemProperty -LiteralPath $personalize -Name AppsUseLightTheme -PropertyType DWord -Value $originalLight -Force | Out-Null
    } else {
        Remove-ItemProperty -LiteralPath $personalize -Name AppsUseLightTheme -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
