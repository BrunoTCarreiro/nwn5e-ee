@echo off
rem Prints the recipe core's stamp: the SHA-256 of its sources, concatenated in this order (a
rem missing one skipped). build.bat compiles it into the DLL and writes it beside it as
rem core_stamp.txt; nwn5e-tools' helper (anim_helper\build_helper.bat) compiles in the same, so the
rem tools can tell when they and the DLL run the same recipe core.
setlocal enabledelayedexpansion
cd /d "%~dp0"
set FILES=
for %%f in (src\data\anim_recipe.h src\data\anim_recipe.cpp src\data\anim_assemble.h src\data\anim_assemble.cpp) do (
  if exist %%f (
    if defined FILES (set FILES=!FILES!+%%f) else (set FILES=%%f)
  )
)
set JOINED=%TEMP%\nwn5e_core_stamp_%RANDOM%%RANDOM%.bin
copy /b !FILES! "!JOINED!" >nul || exit /b 1
set HASH=
for /f "skip=1 delims=" %%h in ('certutil -hashfile "!JOINED!" SHA256') do if not defined HASH set HASH=%%h
del "!JOINED!"
set HASH=!HASH: =!
echo !HASH!
