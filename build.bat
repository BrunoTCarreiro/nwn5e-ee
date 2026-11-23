@echo off
rem Builds build\nwn5e.exe and build\nwn5e.dll (64-bit, static CRT) for NWN:
rem Enhanced Edition with VS 2019 Build Tools.
rem NWN5E_OUT=<folder> builds there instead (e.g. to check a build while the
rem game runs and build\nwn5e.dll is in use).
rem Arguments, in any order: "public" (below), "clean" - every source compiled and every game-derived model built
rem again (docs/TOOLS-PLAN.md, "Faster checks": by default only what changed is, below).
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
set OUT=build
if defined NWN5E_OUT set OUT=%NWN5E_OUT%
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\obj" mkdir "%OUT%\obj"
set PUBLIC=
set CLEAN=
set FRESH=
for %%a in (%*) do (
  if /i "%%~a"=="public" set PUBLIC=1
  if /i "%%~a"=="clean" set CLEAN=--clean& set FRESH=--fresh
)

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
if not defined PUBLIC if exist %PRIV%\src\*.cpp set PRIVATE_SRC=%PRIV%\src\*.cpp
if not defined PUBLIC if exist %PRIV%\rules\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% %PRIV%\rules\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /I%PRIV%

rem The dev layer (nwn5e-tools\dev, docs/BENCH-REDESIGN.md): the tools' in-game parts - !animtrace,
rem the !anim browser, the VFX editor, !vfx, !light - compiled in when that checkout sits beside this one
rem (NWN5E_TOOLS=<folder> to point elsewhere). "build.bat public" leaves it out, like the private layer.
set DEV_SRC=
set DEV_DEF=
set TOOLS=%NWN5E_TOOLS%
if not defined TOOLS set TOOLS=%~dp0..\nwn5e-tools
if not defined PUBLIC if exist "%TOOLS%\dev\dev_layer.cpp" set DEV_SRC=%TOOLS%\dev\*.cpp
if defined DEV_SRC set DEV_DEF=/DNWN5E_DEV /Isrc\core
if defined DEV_SRC echo dev layer: on (%TOOLS%\dev)

rem Every source in src\core, src\data and src\rules goes in (a new file needs no line here). They share one obj
rem folder, so a name used in two of them would overwrite the other's object: stop on one.
for %%f in (src\data\*.cpp src\rules\*.cpp) do if exist src\core\%%~nxf (echo duplicate source name: %%~nxf& exit /b 1)
for %%f in (src\rules\*.cpp) do if exist src\data\%%~nxf (echo duplicate source name: %%~nxf& exit /b 1)

set DLLARGS=%CFLAGS% /LD /Ithird_party\minhook\include /Isrc %PRIVATE_DEF% %DEV_DEF% /DNWN5E_CORE_STAMP=\"%CORE_STAMP%\" ^
  src\core\*.cpp src\data\*.cpp ^
  src\rules\*.cpp ^
  %PRIVATE_SRC% %DEV_SRC% third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c ^
  /Fe:"%OUT%\nwn5e.dll" /link /DLL user32.lib
set EXEARGS=%CFLAGS% src\loader\main.cpp /Fe:"%OUT%\nwn5e.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib

rem Only what changed is compiled again (docs/TOOLS-PLAN.md, "Faster checks"; scripts\cl_incr.py): each source's
rem object is kept in %OUT%\obj and compiled again when the source, a file it includes or the flags change, the changed
rem ones at once on every core; the core stamp only where it's named (anim_recipe.cpp). Without Python, or with
rem NWN5E_INCREMENTAL=off, cl compiles every source in one run, as before.
if /i "%NWN5E_INCREMENTAL%"=="off" goto :full
python -I -c "" >nul 2>nul || goto :no_python
python -I scripts\cl_incr.py %CLEAN% --volatile NWN5E_CORE_STAMP -- %DLLARGS% || exit /b 1
python -I scripts\cl_incr.py %CLEAN% -- %EXEARGS% || exit /b 1
goto :compiled
:no_python
echo python isn't on PATH: compiling every source
:full
cl %DLLARGS% || exit /b 1
cl %EXEARGS% || exit /b 1
:compiled
>"%OUT%\core_stamp.txt" echo %CORE_STAMP%

rem nwn5e's data (data\README.md), fresh next to the DLL; the private layer's
rem data only in a private build. data\files holds compiled scripts served as
rem they are (nwn5e_secwind.ncs: compiled by 1.69's clcompile from scripts\,
rem the interim of EP6).
rem robocopy /MIR makes %OUT%\data the same tree as data\ - removing what data\ hasn't (the last build's private and
rem game-derived files, made again below), copying only files that differ ("Faster checks"; "clean" starts empty).
if defined CLEAN if exist "%OUT%\data" rmdir /s /q "%OUT%\data"
robocopy data "%OUT%\data" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (echo copying data\ into the build failed - robocopy's exit code is 8 or more& exit /b 1)
if defined PRIVATE_SRC if exist %PRIV%\data xcopy /e /i /q /y %PRIV%\data "%OUT%\data\private" >nul

rem The models made of the game's own art (tools\data\README.md) are never committed (the owner, 2026-11-17:
rem "go with 1, remove the six files from main"): the recipes and the player's own game make them here, into
rem %OUT%\data\files. Anything missing - Node, the game, nwn5e-tools, the mesh reader - or any other failure stops
rem the build: an install must never lack the Floating Disk's models. NWN5E_DERIVED=skip builds without them, for a
rem build not meant to be installed. "%TOOLS%\.": a trailing backslash in NWN5E_TOOLS would escape the closing quote.
if /i "%NWN5E_DERIVED%"=="skip" echo warning: NWN5E_DERIVED=skip - this build has no game-derived models, the Floating Disk's - don't install it
if /i not "%NWN5E_DERIVED%"=="skip" where node >nul 2>nul || (echo the game-derived models need Node.js: node isn't on PATH - install it, or set NWN5E_DERIVED=skip for a build not meant to be installed& exit /b 1)
rem Each generator's output is kept in build\derived_cache and reused while everything it read is as it was ("clean":
rem every one runs again; tools\data\README.md, "The cache").
if /i not "%NWN5E_DERIVED%"=="skip" node tools\data\build_derived.js --out "%OUT%\data\files" --tools "%TOOLS%\." %FRESH% || (echo the game-derived models weren't built: the line above says why - fix it, or set NWN5E_DERIVED=skip for a build not meant to be installed& exit /b 1)

rem build\nwn5e.ini holds the player's settings: only create it, never overwrite it.
if not exist "%OUT%\nwn5e.ini" copy nwn5e.ini "%OUT%\nwn5e.ini" >nul
if defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll WITH the private layer (personal use only)
if not defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
