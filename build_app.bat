@echo off
REM Configure and build the GUI application via CMake.
setlocal
cd /d "%~dp0"

set "VCVARS="
for %%P in (
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if not defined VCVARS if exist %%P set "VCVARS=%%P"

if not defined VCVARS ( echo ERROR: Visual Studio Build Tools not found. & exit /b 1 )
call %VCVARS% >nul 2>&1

set "CMAKE=cmake.exe"
if exist "%~dp0tools\cmake\bin\cmake.exe" set "CMAKE=%~dp0tools\cmake\bin\cmake.exe"

"%CMAKE%" -S . -B build_app -G "NMake Makefiles" -DONECLICKRGB_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
"%CMAKE%" --build build_app
exit /b %errorlevel%
