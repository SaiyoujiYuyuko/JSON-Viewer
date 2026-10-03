[CmdletBinding()]
param(
    [ValidateSet('Install','Restore','Status')][string]$Action = 'Status',
    [string]$EditorPath
)
$ErrorActionPreference = 'Stop'
$backupDir = Join-Path $env:LOCALAPPDATA 'NotepadMinusMinusPortable\ContextMenu'
$backupPath = Join-Path $backupDir 'backup.json'
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$user = [Microsoft.Win32.Registry]::CurrentUser
$duplicateKeys = @('Software\Classes\*\shell\Notepad--', 'Software\Classes\*\shell\Notepad4')
$blockedKey = 'Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked'

function Read-Value([string]$Path, [string]$Name) {
    $key = $user.OpenSubKey($Path)
    try {
        $exists = $key -and $key.GetValueNames() -contains $Name
        if ($exists -and $key.GetValueKind($Name).ToString() -notin @('String','ExpandString')) {
            throw "Unexpected registry value type: $Path / $Name"
        }
        [pscustomobject]@{
            Path=$Path; Name=$Name; KeyExisted=($null -ne $key); Existed=[bool]$exists
            Value=$(if ($exists) { $key.GetValue($Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null })
            Kind=$(if ($exists) { $key.GetValueKind($Name).ToString() } else { 'String' })
        }
    } finally { if ($key) { $key.Dispose() } }
}
function Is-Installed($Value) { return $Value.Existed -and $Value.Kind -eq 'String' -and $Value.Value -ceq '' }
function Is-Original($Current, $Old) {
    return $Current.Existed -eq $Old.Existed -and (-not $Old.Existed -or ($Current.Kind -eq $Old.Kind -and $Current.Value -ceq $Old.Value))
}
function Read-Backup {
    $saved = Get-Content -LiteralPath $backupPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($saved.Version -ne 1 -or $saved.Computer -ne $env:COMPUTERNAME -or $saved.Sid -ne $sid) {
        throw 'Context menu backup does not belong to this computer/user.'
    }
    foreach ($entry in $saved.Changes) {
        if (($entry.Path -notin $duplicateKeys -or $entry.Name -ne 'LegacyDisable') -and
            ($entry.Path -ne $blockedKey -or $entry.Name -notmatch '^\{[0-9A-Fa-f-]{36}\}$')) {
            throw 'Context menu backup contains an unsupported registry location.'
        }
    }
    return $saved
}
function Notify-Shell {
    if (-not ('NddContextMenu.Shell' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace NddContextMenu {
    public static class Shell {
        [DllImport("shell32.dll")]
        public static extern void SHChangeNotify(uint eventId, uint flags, IntPtr item1, IntPtr item2);
    }
}
'@
    }
    [NddContextMenu.Shell]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
}

if ($Action -eq 'Restore') {
    if (-not (Test-Path -LiteralPath $backupPath)) { Write-Output 'No context menu backup; nothing to restore.'; return }
    $saved = Read-Backup
    # Validate every value before restoring any of them; retain later changes.
    foreach ($old in $saved.Changes) {
        $now = Read-Value $old.Path $old.Name
        if (-not (Is-Installed $now) -and -not (Is-Original $now $old)) {
            throw 'A context menu setting was changed later. No settings overwritten.'
        }
    }
    foreach ($old in $saved.Changes) {
        $key = $user.OpenSubKey($old.Path, $true)
        try {
            if ($old.Existed) {
                if (-not $key) { $key = $user.CreateSubKey($old.Path) }
                $key.SetValue($old.Name, [string]$old.Value, [Microsoft.Win32.RegistryValueKind]$old.Kind)
            } elseif ($key) { $key.DeleteValue($old.Name, $false) }
        } finally { if ($key) { $key.Dispose() } }
        if (-not (Is-Original (Read-Value $old.Path $old.Name) $old)) { throw 'Context menu restore verification failed.' }
        # Remove only empty keys created by this tool; preserve other values/subkeys.
        if (-not $old.KeyExisted) {
            $key = $user.OpenSubKey($old.Path)
            $empty = $key -and $key.ValueCount -eq 0 -and $key.SubKeyCount -eq 0
            if ($key) { $key.Dispose() }
            if ($empty) { $user.DeleteSubKey($old.Path, $false) }
        }
    }
    Move-Item -LiteralPath $backupPath -Destination (Join-Path $backupDir ('restored-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.json'))
    Notify-Shell
    Write-Output 'Previous context menu visibility restored. File associations are unchanged.'
    return
}

# Detect the installed Notepad package rather than hard-code one release's CLSID.
$clsids = @()
foreach ($package in @(Get-AppxPackage -Name Microsoft.WindowsNotepad)) {
    $manifest = Get-AppxPackageManifest -Package $package.PackageFullName
    $verbs = $manifest.SelectNodes("//*[local-name()='Extension' and @Category='windows.fileExplorerContextMenus']//*[local-name()='Verb' and @Id='OpenInNotepad']")
    foreach ($verb in $verbs) { $clsids += ([guid]$verb.Clsid).ToString('B').ToUpperInvariant() }
}
$clsids = @($clsids | Sort-Object -Unique)
$targets = @()
foreach ($duplicateKey in $duplicateKeys) {
    $name = Split-Path -Leaf $duplicateKey
    $duplicate = [Microsoft.Win32.Registry]::ClassesRoot.OpenSubKey('*\shell\' + $name + '\command')
    if ($duplicate) {
        try {
            $pattern = '(?i)' + [regex]::Escape($name) + '\.exe(?:"|\s|$)'
            if ([string]$duplicate.GetValue('') -notmatch $pattern) { throw "The $name menu points to an unexpected command." }
            $targets += Read-Value $duplicateKey 'LegacyDisable'
        } finally { $duplicate.Dispose() }
    }
}
foreach ($clsid in $clsids) { $targets += Read-Value $blockedKey $clsid }

if ($Action -eq 'Status') {
    $targets | Select-Object Path,Name,@{Name='HiddenByCurrentUser';Expression={$_.Existed}} | Format-Table -AutoSize
    Write-Output "Context menu backup exists: $(Test-Path -LiteralPath $backupPath)"
    return
}

$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run menu cleanup normally as the intended user, not as administrator.' }
$EditorPath = & (Join-Path $PSScriptRoot 'Get-EditorPath.ps1') -EditorPath $EditorPath
$EditorPath = (Resolve-Path -LiteralPath $EditorPath).ProviderPath
$keep = $user.OpenSubKey('Software\Classes\*\shell\NotepadMinusMinusPortable.Edit')
$command = if ($keep) { $keep.OpenSubKey('command') } else { $null }
try {
    if (-not $keep -or -not $command -or $keep.GetValue('MUIVerb') -cne 'Edit with Notepad--' -or
        $command.GetValue('') -ine ('"' + $EditorPath + '" "%1"') -or
        $keep.GetValueNames() -contains 'LegacyDisable' -or $keep.GetValueNames() -contains 'ProgrammaticAccessOnly') {
        throw 'Register the Edit with Notepad-- menu first using the generated Register-Notepad--.reg file.'
    }
} finally { if ($command) { $command.Dispose() }; if ($keep) { $keep.Dispose() } }

if (Test-Path -LiteralPath $backupPath) {
    $saved = Read-Backup
    foreach ($old in $saved.Changes) {
        $now = Read-Value $old.Path $old.Name
        if (-not (Is-Installed $now) -and -not (Is-Original $now $old)) { throw 'A menu setting changed later. Restore or review it before applying again.' }
    }
    $known = @($saved.Changes | ForEach-Object { $_.Path + '|' + $_.Name })
    $changes = @($saved.Changes) + @($targets | Where-Object { ($_.Path + '|' + $_.Name) -notin $known })
} else { $changes = @($targets) }
if (-not $changes.Count) { Write-Output 'No duplicate menu entries found.'; return }
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
[ordered]@{Version=1;Computer=$env:COMPUTERNAME;Sid=$sid;Changes=$changes} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $backupPath -Encoding UTF8
foreach ($old in $changes) {
    $key = $user.CreateSubKey($old.Path)
    try { $key.SetValue($old.Name, '', [Microsoft.Win32.RegistryValueKind]::String) }
    finally { $key.Dispose() }
    if (-not (Is-Installed (Read-Value $old.Path $old.Name))) { throw 'Context menu setting verification failed.' }
}
Notify-Shell
Write-Output 'Kept Edit with Notepad--; hid legacy Notepad--/Notepad4 and Windows Notepad menus.'
Write-Output "Previous visibility backed up to: $backupPath"
Write-Output 'Reopen the context menu. If Explorer still caches an old entry, sign out and back in.'
