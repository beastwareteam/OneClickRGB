@echo off
REM ============================================================
REM  OneClickRGB - Build and run the unit test suite
REM
REM  Tests link against src/hal's interfaces and fake backends,
REM  so no RGB hardware and no administrator rights are needed.
REM ============================================================
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

if not defined VCVARS (
    echo ERROR: Visual Studio Build Tools not found.
    exit /b 1
)

call %VCVARS% >nul 2>&1

set "CMAKE=cmake.exe"
if exist "%~dp0tools\cmake\bin\cmake.exe" set "CMAKE=%~dp0tools\cmake\bin\cmake.exe"

echo [1/3] Configuring...
"%CMAKE%" -S . -B build_tests -G "NMake Makefiles" -DONECLICKRGB_BUILD_APP=OFF -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b 1

echo [2/3] Building...
"%CMAKE%" --build build_tests
if errorlevel 1 exit /b 1

echo [3/3] Running tests...
"%~dp0build_tests\oneclickrgb_tests.exe"
exit /b %errorlevel%
