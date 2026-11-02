@echo off
rem Builds build\nwn5e.exe and build\nwn5e.dll (64-bit, static CRT) for NWN:
rem Enhanced Edition with VS 2019 Build Tools.
rem NWN5E_OUT=<folder> builds there instead (e.g. to check a build while the
rem game runs and build\nwn5e.dll is in use).
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
set OUT=build
if defined NWN5E_OUT set OUT=%NWN5E_OUT%
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\obj" mkdir "%OUT%\obj"

rem The recipe core's stamp (core_stamp.bat): compiled in, and written beside the DLL as
rem core_stamp.txt for the tools (nwn5e-tools' helper carries the same when built from these sources).
set CORE_STAMP=
for /f %%s in ('call "%~dp0core_stamp.bat"') do set CORE_STAMP=%%s
if not defined CORE_STAMP echo core_stamp.bat gave no stamp & exit /b 1

set CFLAGS=/nologo /O2 /MT /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /Fo"%OUT%\obj\\"

rem The private layer (docs/PRIVATE-LAYER.md): personal-use rules kept out of
rem this public repo, compiled in when private\ exists. "build.bat public"
rem leaves it out - build any DLL that leaves this machine that way.
rem NWN5E_PRIVATE=<folder> takes it from elsewhere (the animation session's worktree uses the main checkout's,
rem CLAUDE.md "Two sessions, two working copies").
set PRIV=private
if defined NWN5E_PRIVATE set PRIV=%NWN5E_PRIVATE%
set PRIVATE_SRC=
set PRIVATE_DEF=
if /i not "%~1"=="public" if exist %PRIV%\src\*.cpp set PRIVATE_SRC=%PRIV%\src\*.cpp
if /i not "%~1"=="public" if exist %PRIV%\rules\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% %PRIV%\rules\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /I%PRIV%

rem The dev layer (nwn5e-tools\dev, docs/BENCH-REDESIGN.md): the tools' in-game parts - !animtrace,
rem the !anim browser, the VFX editor, !vfx, !light - compiled in when that checkout sits beside this one
rem (NWN5E_TOOLS=<folder> to point elsewhere). "build.bat public" leaves it out, like the private layer.
set DEV_SRC=
set DEV_DEF=
set TOOLS=%NWN5E_TOOLS%
if not defined TOOLS set TOOLS=%~dp0..\nwn5e-tools
if /i not "%~1"=="public" if exist "%TOOLS%\dev\dev_layer.cpp" set DEV_SRC=%TOOLS%\dev\*.cpp
if defined DEV_SRC set DEV_DEF=/DNWN5E_DEV /Isrc\core
if defined DEV_SRC echo dev layer: on (%TOOLS%\dev)

rem Every source in src\core, src\data and src\rules goes in (a new file needs no line here). They share one obj
rem folder, so a name used in two of them would overwrite the other's object: stop on one.
for %%f in (src\data\*.cpp src\rules\*.cpp) do if exist src\core\%%~nxf (echo duplicate source name: %%~nxf& exit /b 1)
for %%f in (src\rules\*.cpp) do if exist src\data\%%~nxf (echo duplicate source name: %%~nxf& exit /b 1)

cl %CFLAGS% /LD /Ithird_party\minhook\include /Isrc %PRIVATE_DEF% %DEV_DEF% /DNWN5E_CORE_STAMP=\"%CORE_STAMP%\" ^
  src\core\*.cpp src\data\*.cpp ^
  src\rules\*.cpp ^
  %PRIVATE_SRC% %DEV_SRC% third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c ^
  /Fe:"%OUT%\nwn5e.dll" /link /DLL user32.lib || exit /b 1

cl %CFLAGS% src\loader\main.cpp /Fe:"%OUT%\nwn5e.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib || exit /b 1
>"%OUT%\core_stamp.txt" echo %CORE_STAMP%

rem nwn5e's data (data\README.md), fresh next to the DLL; the private layer's
rem data only in a private build. data\files holds compiled scripts served as
rem they are (nwn5e_secwind.ncs: compiled by 1.69's clcompile from scripts\,
rem the interim of EP6).
if exist "%OUT%\data" rmdir /s /q "%OUT%\data"
xcopy /e /i /q /y data "%OUT%\data" >nul || exit /b 1
if defined PRIVATE_SRC if exist %PRIV%\data xcopy /e /i /q /y %PRIV%\data "%OUT%\data\private" >nul

rem build\nwn5e.ini holds the player's settings: only create it, never overwrite it.
if not exist "%OUT%\nwn5e.ini" copy nwn5e.ini "%OUT%\nwn5e.ini" >nul
if defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll WITH the private layer (personal use only)
if not defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
