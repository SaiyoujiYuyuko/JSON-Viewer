@echo off
echo Run normally as the intended user. Do not run as administrator.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -File "%~dp0..\tools\ContextMenu.ps1" -Action Install
if errorlevel 1 echo Menu cleanup failed. Read the message above.
pause
