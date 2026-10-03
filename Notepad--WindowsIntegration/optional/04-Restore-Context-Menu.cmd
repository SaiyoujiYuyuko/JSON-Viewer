@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -File "%~dp0..\tools\ContextMenu.ps1" -Action Restore
if errorlevel 1 echo Menu restore failed. Read the message above.
pause
