@echo off
setlocal
pushd "%~dp0"
cl /nologo /std:c++17 /EHsc /W4 /MT /utf-8 NotepadRedirect.cpp /Fe:"..\tools\NotepadRedirect.exe" /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib
set "BUILD_RESULT=%errorlevel%"
popd
exit /b %BUILD_RESULT%
