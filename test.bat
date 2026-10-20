@echo off
rem Builds the 2024 rules unit tests (64-bit) and runs them (no game needed).
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build\obj\rules mkdir build\obj\rules

rem The private layer's rules and tests (docs/PRIVATE-LAYER.md), when present.
set PRIVATE_SRC=
set PRIVATE_DEF=
if exist private\rules\*.cpp set PRIVATE_SRC=private\rules\*.cpp
if exist private\tests\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% private\tests\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /Iprivate

cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /Isrc /Itests %PRIVATE_DEF% /Fobuild\obj\rules\ ^
  tests\rules_main.cpp tests\rules\*.cpp tests\data\*.cpp src\rules\*.cpp src\data\*.cpp %PRIVATE_SRC% ^
  /Fe:build\rules_test.exe >build\obj\rules\compile.log || goto :compile_failed

build\rules_test.exe
exit /b %errorlevel%

:compile_failed
type build\obj\rules\compile.log
exit /b 1

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
