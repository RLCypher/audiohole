# Build a portable folder with the optional installer beside AudioHole.exe.
# The compiler consumes a separate audited runtime staging tree, never itself.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Archive,
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/portable'),
    [string]$Compiler = "$env:ProgramFiles/Inno Setup 7/ISCC.exe"
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw 'Install Inno Setup 7.1.1, or pass -Compiler.' }
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('AudioHole-installer-' + [guid]::NewGuid())
try {
    Expand-Archive -LiteralPath $Archive -DestinationPath $temporary
    $roots = @(Get-ChildItem -LiteralPath $temporary -Directory)
    if ($roots.Count -ne 1) { throw 'Expected one runtime package root.' }
    $payload = $roots[0].FullName
    $meta = Get-Content -LiteralPath (Join-Path $payload 'docs/build-info.json') -Raw | ConvertFrom-Json
    if ($meta.format -ne 'audiohole-build-info' -or $meta.schema_version -ne 1 -or
        $meta.product -ne 'AudioHole' -or $meta.version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}$' -or
        $meta.commit -notmatch '^[0-9a-f]{40}$' -or $meta.build_number -lt 0) { throw 'Invalid installer build metadata.' }
    $files = @(Get-ChildItem -LiteralPath $payload -File -Recurse -Force | ForEach-Object { $_.FullName.Substring($payload.Length + 1).Replace('\', '/') } | Sort-Object)
    $allowed = @(Get-Content -LiteralPath (Join-Path $repo 'scripts/runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
    if ((Compare-Object ($allowed | Sort-Object) $files) -or
        (Compare-Object ($meta.runtime_files | Sort-Object) $files)) { throw 'Installer payload differs from audited runtime manifest.' }
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $output = [IO.Path]::GetFullPath($OutputDirectory)
    $name = "AudioHole-Setup-$($meta.version)-build$($meta.build_number)-$($meta.configuration)-$($meta.commit.Substring(0,12))"
    if (Test-Path -LiteralPath (Join-Path $output "$name.exe")) { throw 'Installer already exists.' }
    $existing = @(Get-ChildItem -LiteralPath $output -File -Recurse -Force)
    if ($existing.Count -eq 0) {
        foreach ($entry in Get-ChildItem -LiteralPath $payload -Force) {
            Copy-Item -LiteralPath $entry.FullName -Destination $output -Recurse -Force
        }
    } else {
        # CI has already extracted and tested the runtime in this folder. Refuse
        # to mix a different build or unrelated files into the portable bundle.
        $prefixLength = $output.TrimEnd([IO.Path]::DirectorySeparatorChar).Length + 1
        $existingNames = @($existing | ForEach-Object { $_.FullName.Substring($prefixLength).Replace('\', '/') } | Sort-Object)
        if (Compare-Object $files $existingNames) { throw 'Portable destination differs from the runtime package.' }
        foreach ($relative in $files) {
            $sourceHash = (Get-FileHash -LiteralPath (Join-Path $payload $relative) -Algorithm SHA256).Hash
            $targetHash = (Get-FileHash -LiteralPath (Join-Path $output $relative) -Algorithm SHA256).Hash
            if ($sourceHash -ne $targetHash) { throw "Portable destination file differs from the runtime package: $relative" }
        }
    }
    & $Compiler "/DPayload=$payload" "/DOutput=$output" "/DOutputName=$name" "/DAppVersion=$($meta.version)" (Join-Path $repo 'installer/AudioHole.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
    & (Join-Path $PSScriptRoot 'Test-PortableBundle.ps1') -Directory $output
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
