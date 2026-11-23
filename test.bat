@echo off
rem Builds the 2024 rules unit tests (64-bit) and runs them (no game needed).
rem Only what changed is compiled again (docs/TOOLS-PLAN.md, "Faster checks"; scripts\cl_incr.py): each source's object
rem is kept in build\obj\rules and compiled again when the source, a file it includes or the flags change, the changed
rem ones at once on every core. "test.bat clean" compiles every source. Without Python, or with NWN5E_INCREMENTAL=off,
rem cl compiles them all in one run, as before.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build\obj\rules mkdir build\obj\rules
set CLEAN=
if /i "%~1"=="clean" set CLEAN=--clean

rem The private layer's rules and tests (docs/PRIVATE-LAYER.md), when present: private\ here, or
rem NWN5E_PRIVATE=<folder> (the animation session's worktree uses the main checkout's).
set PRIV=private
if defined NWN5E_PRIVATE set PRIV=%NWN5E_PRIVATE%
set PRIVATE_SRC=
set PRIVATE_DEF=
if exist %PRIV%\rules\*.cpp set PRIVATE_SRC=%PRIV%\rules\*.cpp
if exist %PRIV%\tests\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% %PRIV%\tests\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /I%PRIV%

set CLARGS=/nologo /O2 /MT /W3 /EHsc /std:c++17 /Isrc /Itests %PRIVATE_DEF% /Fobuild\obj\rules\ ^
  tests\rules_main.cpp tests\rules\*.cpp tests\data\*.cpp src\rules\*.cpp src\data\*.cpp %PRIVATE_SRC% ^
  /Fe:build\rules_test.exe

if /i "%NWN5E_INCREMENTAL%"=="off" goto :full
python -I -c "" >nul 2>nul || goto :no_python
python -I scripts\cl_incr.py %CLEAN% --log build\obj\rules\compile.log -- %CLARGS% || goto :compile_failed
goto :run

:no_python
echo python isn't on PATH: compiling every source
:full
cl %CLARGS% >build\obj\rules\compile.log || goto :compile_failed

:run
build\rules_test.exe
exit /b %errorlevel%

:compile_failed
type build\obj\rules\compile.log
exit /b 1

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
