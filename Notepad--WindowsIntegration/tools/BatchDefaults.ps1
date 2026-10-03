[CmdletBinding()]
param(
    [ValidateSet('Apply','Restore','Status')][string]$Action = 'Status',
    [string]$EditorPath,
    [string]$BackupPath
)
$ErrorActionPreference = 'Stop'
$extensions = @('.txt','.log','.ini','.cfg','.conf','.config','.json','.jsonc','.xml','.yaml','.yml','.toml','.md','.markdown','.csv','.tsv','.properties','.lst','.nfo','.sql','.srt','.ass')
$target = 'NotepadMinusMinusPortable.Text'
$bridge = Join-Path $PSScriptRoot 'AssociationBridge.exe'
$stateRoot = Join-Path $env:LOCALAPPDATA 'NotepadMinusMinusPortable\Associations'
$lastBackupFile = Join-Path $stateRoot 'last-backup.txt'
$sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$savedEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

function Invoke-Bridge([string[]]$Arguments) {
    $result = @(& $bridge @Arguments)
    if ($LASTEXITCODE -ne 0) { throw ($result -join ' ') }
    return $result
}
function Read-Choice([string]$Extension) {
    $effective = @{}
    foreach ($line in (Invoke-Bridge @('query',$Extension))) {
        $parts = $line -split "`t", 2
        if ($parts.Count -eq 2) { $effective[$parts[0]] = $parts[1] }
    }
    $basePath = 'Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\' + $Extension
    $legacy = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($basePath + '\UserChoice')
    $latest = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($basePath + '\UserChoiceLatest')
    $child = if ($latest) { $latest.OpenSubKey('ProgId') } else { $null }
    try {
        [pscustomobject]@{
            Extension = $Extension; EffectiveProgId = [string]$effective.ProgId; Executable = [string]$effective.Executable
            LegacyExists = ($null -ne $legacy)
            LegacyProgId = $(if ($legacy) { [string]$legacy.GetValue('ProgId') } else { '' })
            LatestExists = ($null -ne $latest)
            LatestProgId = $(if ($child) { [string]$child.GetValue('ProgId') } elseif ($latest) { [string]$latest.GetValue('ProgId') } else { '' })
        }
    } finally {
        if ($child) { $child.Dispose() }; if ($latest) { $latest.Dispose() }; if ($legacy) { $legacy.Dispose() }
    }
}
function Restore-Choice($Old, [bool]$Modern) {
    # Restore selections with freshly computed hashes, not stale registry exports.
    if ($Old.LegacyExists) { Set-FTA -ProgId $Old.LegacyProgId -Extension $Old.Extension }
    else { Invoke-Bridge @('clear-legacy',$Old.Extension,$target) | Out-Null }
    if ($Modern) {
        if ($Old.LatestExists) { Invoke-Bridge @('set-latest',$Old.Extension,$Old.LatestProgId) | Out-Null }
        else { Invoke-Bridge @('clear-latest',$Old.Extension,$target) | Out-Null }
    }
    $after = Read-Choice $Old.Extension
    # With no previous default Windows may choose a registered fallback. Restore
    # absence of explicit choices, rather than fabricate a default that never existed.
    if (($Old.EffectiveProgId -and $after.EffectiveProgId -ne $Old.EffectiveProgId) -or $after.LatestProgId -ne $Old.LatestProgId -or $after.LegacyProgId -ne $Old.LegacyProgId) {
        throw 'The restored selection does not match the backup.'
    }
}

try {
    if (-not (Test-Path -LiteralPath $bridge -PathType Leaf)) { throw "Missing helper: $bridge" }
    $before = @($extensions | ForEach-Object { Read-Choice $_ })
    $modern = @($before | Where-Object LatestExists).Count -gt 0
    $build = [int](Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuild
    Write-Host "Windows build: $build; UserChoiceLatest present: $modern"
    if ($modern) { Invoke-Bridge @('preflight') | ForEach-Object { Write-Host "Native hash check: $_" } }
    if ($Action -eq 'Status') {
        $before | Select-Object Extension,EffectiveProgId,LegacyProgId,LatestProgId | Format-Table -AutoSize
        Write-Host 'Read-only check finished. No associations changed.'
        return
    }
    $principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run this script normally as the intended user, not with Run as administrator.'
    }
    # On new Windows, require an existing native hash as a compatibility anchor.
    if ($build -ge 26100 -and -not $modern) {
        throw 'No native UserChoiceLatest anchor found on this new Windows build. Set .txt once in Windows Settings, then retry.'
    }
    $engine = Join-Path (Split-Path -Parent $PSScriptRoot) 'source\sfta\SFTA.ps1'
    . $engine
    if ($Action -eq 'Apply') {
        $EditorPath = & (Join-Path $PSScriptRoot 'Get-EditorPath.ps1') -EditorPath $EditorPath
        $EditorPath = (Resolve-Path -LiteralPath $EditorPath).ProviderPath
        if ((Split-Path $EditorPath -Leaf) -ine 'Notepad--.exe') { throw 'Expected Notepad--.exe.' }
        $pending = @($before | Where-Object {
            $_.EffectiveProgId -ne $target -or $_.LegacyProgId -ne $target -or
            ($modern -and $_.LatestProgId -ne $target) -or $_.Executable -ine $EditorPath
        })
        if ($pending.Count -eq 0) {
            & (Join-Path $PSScriptRoot 'ContextMenu.ps1') -Action Install -EditorPath $EditorPath
            Write-Host 'All 22 associations already match; the previous backup is preserved.'
            return
        }
        foreach ($old in $before) {
            if (($old.LegacyExists -and -not $old.LegacyProgId) -or ($old.LatestExists -and -not $old.LatestProgId)) {
                throw "Incomplete existing association: $($old.Extension). No changes made."
            }
        }
        New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
        $runName = (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
        $runDir = Join-Path $stateRoot $runName
        New-Item -ItemType Directory -Path $runDir | Out-Null
        $BackupPath = Join-Path $runDir 'before.json'
        [ordered]@{Version=1;Computer=$env:COMPUTERNAME;Sid=$sid;Build=$build;Modern=$modern;EditorPath=$EditorPath;Choices=$before} |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $BackupPath -Encoding UTF8
        $BackupPath | Set-Content -LiteralPath $lastBackupFile -Encoding UTF8
        & (Join-Path $PSScriptRoot 'Generate-RegistryFiles.ps1') -EditorPath $EditorPath -OutputDirectory (Join-Path $runDir 'registration')
        & "$env:SystemRoot\System32\reg.exe" import (Join-Path $runDir 'registration\Register-Notepad--.reg') | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Application registration failed. Associations were not changed.' }
        $results = @()
        foreach ($old in $before) {
            try {
                Set-FTA -ProgId $target -Extension $old.Extension
                if ($modern) { Invoke-Bridge @('set-latest',$old.Extension,$target) | Out-Null }
                $after = Read-Choice $old.Extension
                if ($after.EffectiveProgId -ne $target -or $after.LegacyProgId -ne $target -or
                    ($modern -and $after.LatestProgId -ne $target) -or $after.Executable -ine $EditorPath) {
                    throw 'Windows effective association, command, or stored choices do not match the requested editor.'
                }
                $results += [pscustomobject]@{Extension=$old.Extension;Result='OK';Detail=$after.Executable}
            } catch {
                $failure = $_.Exception.Message
                try { Restore-Choice $old $modern; $failure += ' Previous selection restored.' }
                catch { $failure += ' ROLLBACK FAILED: ' + $_.Exception.Message }
                $results += [pscustomobject]@{Extension=$old.Extension;Result='FAILED';Detail=$failure}
            }
        }
    } else {
        if (-not $BackupPath) {
            if (-not (Test-Path -LiteralPath $lastBackupFile)) { throw 'No batch association backup exists.' }
            $BackupPath = (Get-Content -LiteralPath $lastBackupFile -Raw).Trim()
        }
        $backup = Get-Content -LiteralPath $BackupPath -Raw | ConvertFrom-Json
        if ($backup.Version -ne 1 -or $backup.Computer -ne $env:COMPUTERNAME -or $backup.Sid -ne $sid) {
            throw 'The backup does not belong to this computer/user.'
        }
        if ([bool]$backup.Modern -ne $modern) { throw 'Windows association mode changed since this backup; restore manually.' }
        $runDir = Split-Path -Parent $BackupPath
        $results = @()
        foreach ($old in $backup.Choices) {
            if ($old.Extension -notin $extensions) { throw 'Backup contains an unsupported extension.' }
            try {
                $now = Read-Choice $old.Extension
                if ($now.EffectiveProgId -notin @($target,$old.EffectiveProgId) -or
                    $now.LegacyProgId -notin @($target,$old.LegacyProgId) -or
                    $now.LatestProgId -notin @($target,$old.LatestProgId)) {
                    throw 'This association was changed after the batch; left unchanged.'
                }
                Restore-Choice $old $modern
                $detail = if ($old.EffectiveProgId) { $old.EffectiveProgId } else { 'No previous explicit default; Windows may select a fallback.' }
                $results += [pscustomobject]@{Extension=$old.Extension;Result='RESTORED';Detail=$detail}
            } catch { $results += [pscustomobject]@{Extension=$old.Extension;Result='FAILED';Detail=$_.Exception.Message} }
        }
    }
    $results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDir ($Action.ToLower() + '-result.json')) -Encoding UTF8
    $results | Format-Table -AutoSize -Wrap
    Write-Host "Backup / results: $runDir"
    $failed = @($results | Where-Object Result -eq 'FAILED').Count
    if ($failed) { throw "$failed association(s) failed. See the per-extension results; this was not a complete success." }
    if ($Action -eq 'Apply') { & (Join-Path $PSScriptRoot 'ContextMenu.ps1') -Action Install -EditorPath $EditorPath }
    Write-Host "$($results.Count) associations verified."
} finally { [Console]::OutputEncoding = $savedEncoding }
