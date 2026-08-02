@echo off
REM ============================================================================
REM Quality gate. Run this before pushing - it is what CI runs.
REM
REM   1. the core and the tests build with warnings as errors
REM   2. every unit test passes
REM   3. the application builds
REM   4. the built application starts, behaves, and writes its files where it
REM      says it does (tools\smoke_test.ps1)
REM
REM Any failing stage stops the run and returns a non-zero exit code.
REM ============================================================================
setlocal
cd /d "%~dp0"

set "CMAKE=cmake.exe"
if exist "%~dp0tools\cmake\bin\cmake.exe" set "CMAKE=%~dp0tools\cmake\bin\cmake.exe"

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

echo.
echo === 1/4  Tests: configure and build =========================================
"%CMAKE%" -S . -B build_tests -G "NMake Makefiles" -DONECLICKRGB_BUILD_APP=OFF
if errorlevel 1 goto :failed
"%CMAKE%" --build build_tests
if errorlevel 1 goto :failed

echo.
echo === 2/4  Tests: run ========================================================
build_tests\oneclickrgb_tests.exe
if errorlevel 1 goto :failed

echo.
echo === 3/4  Application: build ================================================
"%CMAKE%" -S . -B build_app -G "NMake Makefiles" -DONECLICKRGB_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :failed
"%CMAKE%" --build build_app
if errorlevel 1 goto :failed

echo.
echo === 4/4  Application: smoke test ===========================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\smoke_test.ps1"
if errorlevel 1 goto :failed

echo.
echo ============================================================================
echo QUALITY GATE PASSED
echo ============================================================================
exit /b 0

:failed
echo.
echo ============================================================================
echo QUALITY GATE FAILED
echo ============================================================================
exit /b 1
