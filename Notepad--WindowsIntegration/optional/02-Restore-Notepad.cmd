@echo off
echo Run this optional system-wide action as administrator.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -File "%~dp0..\tools\NotepadReplacement.ps1" -Action Restore
if errorlevel 1 echo Restore failed. Read the message above.
pause
