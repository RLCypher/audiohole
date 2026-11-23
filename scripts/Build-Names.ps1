# Shared display names; full version/commit values remain authoritative metadata.
function Get-AudioHoleDisplayVersion([string]$Version) {
    if ($Version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}$') { throw 'Invalid AudioHole version.' }
    $parts = $Version.Split('.')
    if ([int]$parts[2] -eq 0) { return "$($parts[0]).$($parts[1])" }
    return $Version
}
function Get-AudioHoleDownloadName {
    param(
        [Parameter(Mandatory)][ValidateSet('Installer', 'Portable', 'Debug', 'Source')][string]$Kind,
        [Parameter(Mandatory)][string]$Version,
        [Parameter(Mandatory)][string]$Commit,
        [switch]$Dirty
    )
    if ($Commit -notmatch '^[0-9a-f]{40}$') { throw 'Invalid AudioHole source commit.' }
    $label = switch ($Kind) {
        'Debug' { 'ZIP-debug' }
        'Source' { 'ZIP-source' }
        default { $Kind }
    }
    $suffix = if ($Dirty) { '-dirty' } else { '' }
    return "AudioHole $label v$(Get-AudioHoleDisplayVersion $Version)-$($Commit.Substring(0,7))$suffix"
}
