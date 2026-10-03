[CmdletBinding()]
param([string]$EditorPath)

$ErrorActionPreference = 'Stop'
if (-not [string]::IsNullOrWhiteSpace($EditorPath)) { return $EditorPath }

$settingsPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'settings.local.json'
if (Test-Path -LiteralPath $settingsPath -PathType Leaf) {
    $settings = Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($settings.EditorPath -isnot [string] -or [string]::IsNullOrWhiteSpace($settings.EditorPath)) {
        throw 'settings.local.json must contain a non-empty EditorPath string.'
    }
    return $settings.EditorPath
}

return 'C:\Tools\Notepad--\Notepad--.exe'
