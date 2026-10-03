@echo off
setlocal
pushd "%~dp0"
cl /nologo /std:c++17 /EHsc /W4 /MT /utf-8 AssociationBridge.cpp ucl\HashCodec.cpp ucl\HashTables.cpp /Fe:"..\tools\AssociationBridge.exe" /link advapi32.lib crypt32.lib ole32.lib shell32.lib shlwapi.lib uuid.lib
set "BUILD_RESULT=%errorlevel%"
popd
exit /b %BUILD_RESULT%
