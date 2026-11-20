# Validate the final portable folder without executing its optional installer.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Directory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Directory).Path
$metadata = Get-Content -LiteralPath (Join-Path $root 'docs/build-info.json') -Raw | ConvertFrom-Json
if ($metadata.product -ne 'AudioHole' -or $metadata.commit -notmatch '^[0-9a-f]{40}$' -or
    $metadata.version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}$' -or
    $metadata.configuration -notin @('Debug', 'Release') -or $metadata.build_number -lt 0) {
    throw 'Invalid portable bundle metadata.'
}
$installer = "AudioHole-Setup-$($metadata.version)-build$($metadata.build_number)-$($metadata.configuration)-$($metadata.commit.Substring(0,12)).exe"
$runtime = @(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runtime-files.txt')) + @('docs/build-info.json', 'README.txt')
if (Compare-Object ($runtime | Sort-Object) ($metadata.runtime_files | Sort-Object)) {
    throw 'Portable runtime manifest differs from the approved runtime files.'
}
$expected = @($runtime) + @($installer)
$prefixLength = $root.TrimEnd([IO.Path]::DirectorySeparatorChar).Length + 1
$actual = @(Get-ChildItem -LiteralPath $root -File -Recurse -Force | ForEach-Object {
    $_.FullName.Substring($prefixLength).Replace('\', '/')
})
$difference = @(Compare-Object ($expected | Sort-Object) ($actual | Sort-Object))
if ($difference.Count) {
    $missing = @($difference | Where-Object SideIndicator -eq '<=' | ForEach-Object InputObject)
    $unexpected = @($difference | Where-Object SideIndicator -eq '=>' | ForEach-Object InputObject)
    throw "Unexpected portable bundle contents. Missing: $($missing -join ', '); Unexpected: $($unexpected -join ', ')"
}
Write-Host 'PASS: portable AudioHole.exe and optional installer share the main folder; no source archive or unexpected files are bundled.'
