[CmdletBinding()]
param(
    [ValidateSet('Install','Restore','Status')][string]$Action = 'Status',
    [string]$EditorPath
)
$ErrorActionPreference = 'Stop'
$ifeoPath = 'SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe'
$backupDir = Join-Path $env:ProgramData 'NotepadMinusMinusPortable\Replacement'
$backupPath = Join-Path $backupDir 'backup.json'
$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
try {
    $key = $base.OpenSubKey($ifeoPath)
    if ($Action -eq 'Status') {
        $debugger = if ($null -ne $key) { $key.GetValue('Debugger', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null }
        Write-Output "Current notepad.exe Debugger: $debugger"
        Write-Output "Backup: $backupPath (exists: $(Test-Path -LiteralPath $backupPath))"
        if ($key) { $key.Dispose() }
        return
    }
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run the install/restore CMD file as administrator.'
    }
    if ($Action -eq 'Install') {
        $EditorPath = & (Join-Path $PSScriptRoot 'Get-EditorPath.ps1') -EditorPath $EditorPath
        $EditorPath = (Resolve-Path -LiteralPath $EditorPath).ProviderPath
        if ((Split-Path $EditorPath -Leaf) -ine 'Notepad--.exe') { throw 'Expected Notepad--.exe.' }
        if ((-not [Environment]::Is64BitOperatingSystem) -or $env:PROCESSOR_ARCHITECTURE -eq 'ARM64') {
            throw 'This launcher package is tested for Windows x64.'
        }
        if ($key -and $key.GetValue('UseFilter', 0) -ne 0) {
            throw 'An existing IFEO path filter is enabled. This installer will not replace that configuration.'
        }
        $launcher = Join-Path (Split-Path $EditorPath) 'NotepadRedirect.exe'
        $expected = '"' + $launcher + '" --ifeo'
        $source = Join-Path $PSScriptRoot 'NotepadRedirect.exe'
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing launcher: $source" }
        if (Test-Path -LiteralPath $backupPath) {
            $saved = Get-Content -LiteralPath $backupPath -Raw | ConvertFrom-Json
            if ($saved.Computer -ne $env:COMPUTERNAME -or $saved.InstalledDebugger -ne $expected) {
                throw 'A backup for another installation exists. Restore it first.'
            }
            $current = if ($key) { $key.GetValue('Debugger', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null }
            if ($current -ne $expected) { throw 'A pending backup or changed replacement exists. Run Restore before installing again.' }
            Write-Output 'Already installed; the original backup is preserved.'
            return
        }
        $hadDebugger = $key -and ($key.GetValueNames() -contains 'Debugger')
        $previous = if ($hadDebugger) { $key.GetValue('Debugger', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null }
        $kind = if ($hadDebugger) { $key.GetValueKind('Debugger').ToString() } else { 'String' }
        if ($hadDebugger -and $kind -notin @('String','ExpandString')) { throw 'Unexpected existing Debugger value type.' }
        if ($hadDebugger -and $previous -eq $expected) { throw 'Replacement exists without a backup. Refusing to invent a previous setting.' }
        if (([IO.Path]::GetFullPath($source)) -ine ([IO.Path]::GetFullPath($launcher))) {
            if (Test-Path -LiteralPath $launcher) {
                if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $launcher).Hash) {
                    throw "A different launcher already exists: $launcher"
                }
            } else { Copy-Item -LiteralPath $source -Destination $launcher }
        }
        New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
        [ordered]@{
            Version = 1; Computer = $env:COMPUTERNAME; Created = (Get-Date -Format o)
            KeyExisted = ($null -ne $key); HadDebugger = [bool]$hadDebugger
            PreviousDebugger = $previous; PreviousKind = $kind; InstalledDebugger = $expected
        } | ConvertTo-Json | Set-Content -LiteralPath $backupPath -Encoding UTF8
        if ($key) {
            # Additional human-readable snapshot; Restore changes only our Debugger value.
            & "$env:SystemRoot\System32\reg.exe" export ('HKLM\' + $ifeoPath) (Join-Path $backupDir 'before.reg') /y | Out-Null
            if ($LASTEXITCODE -ne 0) { throw 'Registry backup export failed; nothing was installed.' }
            $key.Dispose(); $key = $null
        }
        $writeKey = $base.CreateSubKey($ifeoPath)
        try { $writeKey.SetValue('Debugger', $expected, [Microsoft.Win32.RegistryValueKind]::String) }
        finally { $writeKey.Dispose() }
        Write-Output "Installed: $expected"
        Write-Output "Previous replacement backed up to: $backupPath"
    } else {
        if (-not (Test-Path -LiteralPath $backupPath)) { throw 'No local backup exists. No registry changes were made.' }
        $saved = Get-Content -LiteralPath $backupPath -Raw | ConvertFrom-Json
        if ($saved.Version -ne 1 -or $saved.Computer -ne $env:COMPUTERNAME) { throw 'The backup does not belong to this computer.' }
        $current = if ($key) { $key.GetValue('Debugger', $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null }
        $alreadyRestored = if ($saved.HadDebugger) { $current -eq $saved.PreviousDebugger } else { $null -eq $current }
        if ($current -ne $saved.InstalledDebugger -and -not $alreadyRestored) {
            throw 'Another tool has changed the replacement since installation. No settings were overwritten.'
        }
        if ($key) { $key.Dispose(); $key = $null }
        if (-not $alreadyRestored) {
            $writeKey = $base.CreateSubKey($ifeoPath)
            try {
                if ($saved.HadDebugger) {
                    $kind = [Microsoft.Win32.RegistryValueKind]([Enum]::Parse([Microsoft.Win32.RegistryValueKind], $saved.PreviousKind))
                    $writeKey.SetValue('Debugger', [string]$saved.PreviousDebugger, $kind)
                } else { $writeKey.DeleteValue('Debugger', $false) }
            } finally { $writeKey.Dispose() }
        }
        # Archive the backup; do not copy a machine's previous settings to another PC.
        $archive = Join-Path $backupDir ('restored-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.json')
        Move-Item -LiteralPath $backupPath -Destination $archive
        Write-Output 'Restored the replacement configuration that existed before installation.'
        Write-Output 'File defaults are separate; choose them in Windows Settings if needed.'
    }
} finally {
    if ($key) { $key.Dispose() }
    $base.Dispose()
}
