@echo off
echo Set all 22 text extensions to Notepad-- for the current user.
echo Run normally. Do not choose Run as administrator.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -File "%~dp0tools\BatchDefaults.ps1" -Action Apply
if errorlevel 1 echo FAILED or partly applied. Read the per-extension results above.
pause
