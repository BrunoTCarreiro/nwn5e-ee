@echo off
rem The Floating Disk generator's mesh reader (tools\data\floatdisk\build.js, #39): mdlmesh.exe from mdlmesh.cpp and this
rem repo's own model reader (src\data\vfx_model.cpp), into build\floatdisk\ (git-ignored). VS 2019 Build Tools, as
rem build.bat and test.bat. build.js runs this itself when the exe is missing or older than its sources.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0..\..\.."
if not exist build\obj\floatdisk mkdir build\obj\floatdisk
if not exist build\floatdisk mkdir build\floatdisk
cl /nologo /O2 /MT /W3 /EHsc /std:c++17 /Isrc /Fobuild\obj\floatdisk\ ^
  tools\data\floatdisk\mdlmesh.cpp src\data\vfx_model.cpp ^
  /Fe:build\floatdisk\mdlmesh.exe >build\obj\floatdisk\compile.log || goto :compile_failed
echo built build\floatdisk\mdlmesh.exe
exit /b 0

:compile_failed
type build\obj\floatdisk\compile.log
exit /b 1

:no_vcvars
echo vcvars64.bat not found: "%VCVARS%"
exit /b 1
