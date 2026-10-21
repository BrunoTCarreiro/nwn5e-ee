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

set CFLAGS=/nologo /O2 /MT /W3 /EHsc /std:c++17 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /Fo"%OUT%\obj\\"

rem The private layer (docs/PRIVATE-LAYER.md): personal-use rules kept out of
rem this public repo, compiled in when private\ exists. "build.bat public"
rem leaves it out - build any DLL that leaves this machine that way.
set PRIVATE_SRC=
set PRIVATE_DEF=
if /i not "%~1"=="public" if exist private\src\*.cpp set PRIVATE_SRC=private\src\*.cpp
if /i not "%~1"=="public" if exist private\rules\*.cpp set PRIVATE_SRC=%PRIVATE_SRC% private\rules\*.cpp
if defined PRIVATE_SRC set PRIVATE_DEF=/DNWN5E_PRIVATE /Iprivate

cl %CFLAGS% /LD /Ithird_party\minhook\include /Isrc %PRIVATE_DEF% ^
  src\core\dllmain.cpp src\core\log.cpp src\core\config.cpp src\core\hooks.cpp ^
  src\core\dice.cpp src\core\attack_roll.cpp src\core\ruleset.cpp src\core\sheet.cpp src\core\creature.cpp ^
  src\core\rules_crit.cpp src\core\rules_attack.cpp src\core\rules_extra_attack.cpp src\core\rules_sheet.cpp ^
  src\core\rules_debug.cpp src\core\engine.cpp src\core\rules_racial.cpp src\core\rules_two_weapon.cpp ^
  src\core\rules_advantage.cpp src\core\rules_retarget.cpp src\core\feats.cpp src\core\rules_opportunity.cpp ^
  src\core\rules_potion.cpp src\core\effects.cpp src\core\rules_conditions.cpp src\core\chat.cpp src\core\rest.cpp ^
  src\core\rules_mastery.cpp src\core\rules_push.cpp src\core\creature_update.cpp ^
  src\core\skill_rank.cpp src\core\rules_skills.cpp src\core\rules_nwn_skills.cpp src\core\rules_casting.cpp ^
  src\core\rules_initiative.cpp src\core\rules_saves.cpp src\core\rules_save_roll.cpp src\core\rules_labels.cpp ^
  src\core\rules_spell_attack.cpp src\core\rules_shove.cpp src\core\rules_skill_dcs.cpp src\core\rules_perception.cpp ^
  src\core\rules_loaded.cpp src\core\data_serving.cpp src\core\rules_progression.cpp src\core\rules_alignment.cpp ^
  src\core\rules_hit_points.cpp src\core\rules_feat_cadence.cpp src\core\rules_abilities.cpp ^
  src\core\rules_features.cpp src\core\rules_fighter.cpp src\core\nui.cpp src\core\nui_test.cpp ^
  src\core\client_ui.cpp src\core\chargen_ui.cpp src\core\rules_feats.cpp src\core\rules_equipment.cpp src\core\rules_weapon_size.cpp src\core\rules_finesse.cpp ^
  src\data\twoda.cpp src\data\ubjson.cpp ^
  src\rules\*.cpp ^
  %PRIVATE_SRC% third_party\minhook\src\buffer.c third_party\minhook\src\hook.c ^
  third_party\minhook\src\trampoline.c third_party\minhook\src\hde\hde64.c ^
  /Fe:"%OUT%\nwn5e.dll" /link /DLL user32.lib || exit /b 1

cl %CFLAGS% src\loader\main.cpp /Fe:"%OUT%\nwn5e.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib || exit /b 1

rem nwn5e's data (data\README.md), fresh next to the DLL; the private layer's
rem data only in a private build. data\files holds compiled scripts served as
rem they are (nwn5e_secwind.ncs: compiled by 1.69's clcompile from scripts\,
rem the interim of EP6).
if exist "%OUT%\data" rmdir /s /q "%OUT%\data"
xcopy /e /i /q /y data "%OUT%\data" >nul || exit /b 1
if defined PRIVATE_SRC if exist private\data xcopy /e /i /q /y private\data "%OUT%\data\private" >nul

rem build\nwn5e.ini holds the player's settings: only create it, never overwrite it.
if not exist "%OUT%\nwn5e.ini" copy nwn5e.ini "%OUT%\nwn5e.ini" >nul
if defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll WITH the private layer (personal use only)
if not defined PRIVATE_SRC echo built %OUT%\nwn5e.exe %OUT%\nwn5e.dll
exit /b 0

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
