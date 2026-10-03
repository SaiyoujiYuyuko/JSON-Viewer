[CmdletBinding()]
param(
    [string]$EditorPath,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$EditorPath = & (Join-Path $PSScriptRoot 'Get-EditorPath.ps1') -EditorPath $EditorPath
if (-not $OutputDirectory) { $OutputDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'manual' }
if (-not [IO.Path]::IsPathRooted($EditorPath) -or (Split-Path $EditorPath -Leaf) -ine 'Notepad--.exe') {
    throw 'Specify the absolute path to Notepad--.exe.'
}
if ($EditorPath.Contains('"') -or $EditorPath.Contains("`n") -or $EditorPath.Contains("`r")) { throw 'Invalid executable path.' }
$extensions = @('.txt','.log','.ini','.cfg','.conf','.config','.json','.jsonc','.xml','.yaml','.yml','.toml','.md','.markdown','.csv','.tsv','.properties','.lst','.nfo','.sql','.srt','.ass')
function RegString([string]$Value) { return '"' + $Value.Replace('\','\\').Replace('"','\"') + '"' }
$appName = 'Notepad-- Portable'
$progId = 'NotepadMinusMinusPortable.Text'
$capability = 'Software\NotepadMinusMinusPortable\Capabilities'
$command = RegString ('"' + $EditorPath + '" "%1"')
$icon = RegString ('"' + $EditorPath + '",0')
$register = [Collections.Generic.List[string]]::new()
$unregister = [Collections.Generic.List[string]]::new()
$register.Add('Windows Registry Editor Version 5.00')
$register.Add('')
$register.Add('; Register a candidate editor only. Use the batch launcher or Windows Settings to select defaults.')
$register.Add('; Existing UserChoice and extension default values are not modified.')
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\Classes\' + $progId + ']')
$register.Add('@="Notepad-- Text Document"')
$register.Add('"FriendlyTypeName"="Notepad-- Text Document"')
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\Classes\' + $progId + '\Application]')
$register.Add('"ApplicationName"="Notepad-- Portable"')
$register.Add('"ApplicationDescription"="Portable text and JSON editor"')
$register.Add('"ApplicationIcon"=' + $icon)
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\Classes\' + $progId + '\DefaultIcon]')
$register.Add('@=' + $icon)
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\Classes\' + $progId + '\shell\open\command]')
$register.Add('@=' + $command)
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\' + $capability + ']')
$register.Add('"ApplicationName"="Notepad-- Portable"')
$register.Add('"ApplicationDescription"="Portable text and JSON editor"')
$register.Add('"ApplicationIcon"=' + $icon)
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\' + $capability + '\FileAssociations]')
foreach ($ext in $extensions) { $register.Add((RegString $ext) + '=' + (RegString $progId)) }
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\RegisteredApplications]')
$register.Add((RegString $appName) + '=' + (RegString $capability))
$register.Add('')
$unregister.Add('Windows Registry Editor Version 5.00')
$unregister.Add('')
$unregister.Add('; First restore defaults with the batch launcher or choose another editor in Windows Settings.')
$unregister.Add('; Removes only this package''s registrations, not UserChoice.')
$unregister.Add('')
$unregister.Add('[HKEY_CURRENT_USER\Software\RegisteredApplications]')
$unregister.Add((RegString $appName) + '=-')
$unregister.Add('')
foreach ($ext in $extensions) {
    $section = '[HKEY_CURRENT_USER\Software\Classes\' + $ext + '\OpenWithProgids]'
    $register.Add($section)
    $register.Add((RegString $progId) + '=hex(0):')
    $register.Add('')
    $unregister.Add($section)
    $unregister.Add((RegString $progId) + '=-')
    $unregister.Add('')
}
$register.Add('[HKEY_CURRENT_USER\Software\Classes\*\shell\NotepadMinusMinusPortable.Edit]')
$register.Add('"MUIVerb"="Edit with Notepad--"')
$register.Add('"Icon"=' + $icon)
$register.Add('"MultiSelectModel"="Single"')
$register.Add('')
$register.Add('[HKEY_CURRENT_USER\Software\Classes\*\shell\NotepadMinusMinusPortable.Edit\command]')
$register.Add('@=' + $command)
$register.Add('')
foreach ($key in @(('Software\Classes\' + $progId), 'Software\NotepadMinusMinusPortable', 'Software\Classes\*\shell\NotepadMinusMinusPortable.Edit')) {
    $unregister.Add('[-HKEY_CURRENT_USER\' + $key + ']')
    $unregister.Add('')
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'Register-Notepad--.reg'), ($register -join "`r`n"), [Text.Encoding]::Unicode)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'Unregister-Notepad--.reg'), ($unregister -join "`r`n"), [Text.Encoding]::Unicode)
Write-Output "Registry files generated for: $EditorPath"
Write-Output ('Extensions: ' + ($extensions -join ', '))
Write-Output 'No registry settings have been applied.'
