@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -File "%~dp0tools\BatchDefaults.ps1" -Action Restore
if errorlevel 1 echo Some selections were not restored. Read the results above.
pause
