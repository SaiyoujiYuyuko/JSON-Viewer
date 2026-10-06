# Runs the real orchestration against simulated associations and stub helpers.
# No native association helper, registry import, or real Set-FTA is executed.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $PSScriptRoot ('.build\mode-tests-' + [guid]::NewGuid().ToString('N'))
$source = [IO.File]::ReadAllText((Join-Path $packageRoot 'tools\BatchDefaults.ps1')).Replace("`r`n", "`n")
$import = '& "$env:SystemRoot\System32\reg.exe" import (Join-Path $runDir ''registration\Register-Notepad--.reg'') | Out-Host'
if (($source.Split(@($import), [StringSplitOptions]::None)).Count -ne 2) { throw 'Cannot isolate the registry import for testing.' }
$source = $source.Replace($import, 'Invoke-TestRegistration')
$tokens=$null; $parseErrors=$null
$ast = [Management.Automation.Language.Parser]::ParseInput($source, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Source parse failed.' }
$main = @($ast.EndBlock.Statements | Where-Object { $_ -is [Management.Automation.Language.TryStatementAst] })
if ($main.Count -ne 1) { throw 'Cannot locate the production main flow.' }
$bootstrap = @'
$case = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'case.json') -Raw | ConvertFrom-Json
$stateRoot = Join-Path $PSScriptRoot 'state'
$lastBackupFile = Join-Path $stateRoot 'last-backup.txt'
$testEditor = Join-Path (Split-Path -Parent $PSScriptRoot) 'Notepad--.exe'
$script:Choices = @{}
$script:Originals = @{}
$global:TextAssociationTest = [pscustomobject]@{LegacyWrites=0;LatestWrites=0;Preflights=0;Restores=0;Registrations=0;Menus=0}
foreach ($ext in $extensions) {
    $previous=[pscustomobject]@{Extension=$ext;EffectiveProgId='Previous.Editor';Executable='previous.exe';LegacyExists=$true;LegacyProgId='Previous.Editor';LatestExists=[bool]$case.Modern;LatestProgId=$(if($case.Modern){'Previous.Editor'}else{''})}
    $script:Originals[$ext]=$previous
    $script:Choices[$ext]=$previous.PSObject.Copy()
    if($Action -eq 'Restore' -or $case.AlreadySet) {
        $script:Choices[$ext].EffectiveProgId=$target
        $script:Choices[$ext].Executable=$testEditor
        $script:Choices[$ext].LegacyProgId=$target
        if($case.Modern){$script:Choices[$ext].LatestProgId=$target}
    }
}
function Get-ItemProperty { param($LiteralPath) return [pscustomobject]@{CurrentBuild='26200'} }
function Read-Choice([string]$Extension) { return $script:Choices[$Extension].PSObject.Copy() }
function Invoke-TestRegistration { $global:TextAssociationTest.Registrations++; $global:LASTEXITCODE=0 }
function Invoke-Bridge([string[]]$Arguments) {
    switch($Arguments[0]) {
        'preflight' { $global:TextAssociationTest.Preflights++; if($case.FailPreflight){throw 'Simulated incompatible hash'}; return 'MATCH' }
        'set-latest' { $global:TextAssociationTest.LatestWrites++; $choice=$script:Choices[$Arguments[1]]; $choice.LatestExists=$true; $choice.LatestProgId=$Arguments[2]; return 'VERIFIED' }
        default { throw ('Unexpected helper command in test: ' + $Arguments[0]) }
    }
}
if($Action -eq 'Restore') {
    New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
    $BackupPath=Join-Path $stateRoot 'before.json'
    [ordered]@{Version=1;Computer=$env:COMPUTERNAME;Sid=$sid;Modern=[bool]$case.Modern;Choices=@($extensions|ForEach-Object{$script:Originals[$_]})} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $BackupPath -Encoding UTF8
}
'@
$runner = $source.Insert($main[0].Extent.StartOffset, $bootstrap + "`n")
$engine = @'
function Set-FTA { param($ProgId,$Extension)
    $choice=$script:Choices[$Extension]
    if($ProgId -eq $target) {
        $global:TextAssociationTest.LegacyWrites++
        if($case.RejectWrites){return}
        $choice.Executable=$testEditor
        if($case.UnexpectedLatest){$choice.LatestExists=$true; $choice.LatestProgId=$ProgId}
    } else {
        $global:TextAssociationTest.Restores++
        $choice.Executable='previous.exe'
    }
    $choice.LegacyProgId=$ProgId; $choice.EffectiveProgId=$ProgId
}
'@
$cases = @(
    @{Name='build26200-legacy-apply';Modern=$false;Action='Apply';Writes=22;Latest=0;Preflights=0;Restores=0;Registrations=1;Menus=1;Error='';Result='OK'},
    @{Name='build26200-modern-apply';Modern=$true;Action='Apply';Writes=22;Latest=22;Preflights=1;Restores=0;Registrations=1;Menus=1;Error='';Result='OK'},
    @{Name='modern-incompatible';Modern=$true;Action='Apply';FailPreflight=$true;Writes=0;Latest=0;Preflights=1;Restores=0;Registrations=0;Menus=0;Error='Simulated incompatible hash'},
    @{Name='legacy-write-rejected';Modern=$false;Action='Apply';RejectWrites=$true;Writes=22;Latest=0;Preflights=0;Restores=22;Registrations=1;Menus=0;Error='22 association(s) failed';Result='FAILED'},
    @{Name='legacy-latest-appears';Modern=$false;Action='Apply';UnexpectedLatest=$true;Writes=22;Latest=0;Preflights=0;Restores=22;Registrations=1;Menus=0;Error='22 association(s) failed';Result='FAILED'},
    @{Name='build26200-legacy-restore';Modern=$false;Action='Restore';Writes=0;Latest=0;Preflights=0;Restores=22;Registrations=0;Menus=0;Error='';Result='RESTORED'},
    @{Name='legacy-already-set';Modern=$false;Action='Apply';AlreadySet=$true;Writes=0;Latest=0;Preflights=0;Restores=0;Registrations=0;Menus=1;Error=''},
    @{Name='legacy-status';Modern=$false;Action='Status';Writes=0;Latest=0;Preflights=0;Restores=0;Registrations=0;Menus=0;Error=''}
)
$utf8=[Text.UTF8Encoding]::new($true)
foreach($fixture in $cases) {
    $root=Join-Path $testRoot $fixture.Name
    $tools=Join-Path $root 'tools'
    New-Item -ItemType Directory -Force -Path $tools,(Join-Path $root 'source\sfta') | Out-Null
    [IO.File]::WriteAllText((Join-Path $tools 'case.json'),($fixture|ConvertTo-Json),$utf8)
    [IO.File]::WriteAllText((Join-Path $tools 'BatchDefaults.ps1'),$runner,$utf8)
    [IO.File]::WriteAllText((Join-Path $root 'source\sfta\SFTA.ps1'),$engine,$utf8)
    [IO.File]::WriteAllText((Join-Path $tools 'AssociationBridge.exe'),'test fixture only')
    [IO.File]::WriteAllText((Join-Path $root 'Notepad--.exe'),'test fixture only')
    [IO.File]::WriteAllText((Join-Path $tools 'Get-EditorPath.ps1'),'param($EditorPath) Join-Path (Split-Path -Parent $PSScriptRoot) ''Notepad--.exe''',$utf8)
    [IO.File]::WriteAllText((Join-Path $tools 'Generate-RegistryFiles.ps1'),'param($EditorPath,$OutputDirectory) # Registration is mocked by Invoke-TestRegistration.',$utf8)
    [IO.File]::WriteAllText((Join-Path $tools 'ContextMenu.ps1'),'param($Action,$EditorPath) $global:TextAssociationTest.Menus++',$utf8)
    $errorText=''
    try { & (Join-Path $tools 'BatchDefaults.ps1') -Action $fixture.Action *> (Join-Path $root 'test.log') }
    catch { $errorText=$_.Exception.Message }
    if(($fixture.Error -and -not $errorText.Contains($fixture.Error)) -or (-not $fixture.Error -and $errorText)) { throw ($fixture.Name + ': unexpected error: ' + $errorText) }
    $actual=$global:TextAssociationTest
    if($actual.LegacyWrites -ne $fixture.Writes -or $actual.LatestWrites -ne $fixture.Latest -or $actual.Preflights -ne $fixture.Preflights -or $actual.Restores -ne $fixture.Restores -or $actual.Registrations -ne $fixture.Registrations -or $actual.Menus -ne $fixture.Menus) { throw ($fixture.Name + ': incorrect operation routing') }
    if($fixture.Result) {
        $resultFile=Get-ChildItem -LiteralPath (Join-Path $tools 'state') -Filter '*-result.json' -Recurse
        if(@($resultFile).Count -ne 1){throw 'Expected one result file.'}
        $results=Get-Content -LiteralPath $resultFile.FullName -Raw | ConvertFrom-Json
        if($results.Count -ne 22 -or @($results|Where-Object Result -ne $fixture.Result).Count){throw ($fixture.Name + ': incorrect verified results')}
        if($fixture.UnexpectedLatest -and @($results|Where-Object {$_.Detail -notmatch 'ROLLBACK FAILED'}).Count){throw 'Unexpected latest records were silently treated as restored.'}
        if($fixture.RejectWrites -and @($results|Where-Object {$_.Detail -notmatch 'Previous selection restored'}).Count){throw 'Failed legacy writes did not restore their original selection.'}
    } elseif(Test-Path -LiteralPath (Join-Path $tools 'state')) { throw 'Pre-write rejection or unchanged operation unexpectedly created backup state.' }
    Write-Output ('PASS ' + $fixture.Name)
}
Remove-Variable -Name TextAssociationTest -Scope Global
Write-Output 'Eight mode regressions passed with simulated associations; no system settings changed.'
