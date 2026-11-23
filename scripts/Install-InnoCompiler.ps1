# Install the pinned CI/developer compiler; this tool is never a runtime payload.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'Inno compiler setup requires native Windows.' }
$compiler = "$env:ProgramFiles/Inno Setup 7/ISCC.exe"
if (-not (Test-Path -LiteralPath $compiler) -or [version](Get-Item $compiler).VersionInfo.FileVersionRaw -lt [version]'7.1.1.0') {
    $download = Join-Path ([IO.Path]::GetTempPath()) ('innosetup-7.1.1-' + [guid]::NewGuid() + '.exe')
    try {
        Invoke-WebRequest https://github.com/jrsoftware/issrc/releases/download/is-7_1_1/innosetup-7.1.1-x64.exe -OutFile $download
        if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne '8A5A6711C3766A6E5FB74F280BA0E3551E719CE94538D901FDBD1F8F9920B3F0') { throw 'Installer compiler integrity check failed.' }
        $directory = Split-Path $compiler
        $process = Start-Process $download -ArgumentList '/ALLUSERS', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$directory`"" -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw 'Installer compiler setup failed.' }
    } finally {
        if (Test-Path -LiteralPath $download) { Remove-Item -LiteralPath $download -Force }
    }
}
return $compiler
