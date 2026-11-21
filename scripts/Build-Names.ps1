# Shared display names; full version/commit values remain authoritative metadata.
function Get-AudioHoleDisplayVersion([string]$Version) {
    if ($Version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}$') { throw 'Invalid AudioHole version.' }
    $parts = $Version.Split('.')
    if ([int]$parts[2] -eq 0) { return "$($parts[0]).$($parts[1])" }
    return $Version
}
function Get-AudioHoleInstallerName([string]$Version) {
    return 'Install AudioHole v' + (Get-AudioHoleDisplayVersion $Version)
}
