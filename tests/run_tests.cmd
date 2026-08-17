@echo off
REM ================================================
REM OneClickRGB - unit tests (no hardware required)
REM
REM Builds and runs tests\test_cli_args.cpp, which covers src\cli_args.h,
REM src\effect_limits.h and src\channel_config.h - the pure logic that decides
REM which bytes a command line sends to the keyboard and which colour an ASUS
REM channel gets. Everything that needs a device is covered by the probe flags
REM and tools\check_dryrun_flags.ps1 instead.
REM
REM Exit code 0 = all checks passed.
REM ================================================
setlocal enabledelayedexpansion

cd /d "%~dp0.."

echo.
echo ================================================
echo OneClickRGB Unit Tests
echo ================================================
echo.

REM --- locate a compiler (same search order as build_native.bat) -------------
where cl.exe >nul 2>&1
if not errorlevel 1 goto :have_cl

set VS_FOUND=0
for %%E in (Community Professional BuildTools) do (
    if !VS_FOUND!==0 if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        set VS_FOUND=1
        echo Found: Visual Studio 2022 %%E
    )
)
for %%E in (Community Professional BuildTools) do (
    if !VS_FOUND!==0 if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        set VS_FOUND=1
        echo Found: Visual Studio 2019 %%E
    )
)
if !VS_FOUND!==0 (
    echo ERROR: Visual Studio not found - cannot build the tests.
    exit /b 1
)

where cl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe still not on PATH after vcvars setup.
    exit /b 1
)

:have_cl
if not exist "%~dp0build" mkdir "%~dp0build"

REM A stale, locked .obj is the usual reason a rebuild fails here (same trap as
REM build_native.bat: an old test binary still running keeps its files open).
del /f /q "%~dp0build\test_cli_args.obj" >nul 2>&1
del /f /q "%~dp0build\test_cli_args.exe" >nul 2>&1

echo Compiling tests...
cl.exe /nologo /EHsc /std:c++17 /W4 /permissive- /I"%~dp0..\src" ^
    "%~dp0test_cli_args.cpp" ^
    /Fo"%~dp0build\\" /Fe"%~dp0build\test_cli_args.exe" >"%~dp0build\compile.log" 2>&1
if errorlevel 1 (
    echo.
    echo ================================================
    echo COMPILE FAILED - see tests\build\compile.log
    echo ================================================
    type "%~dp0build\compile.log"
    exit /b 1
)

echo.
"%~dp0build\test_cli_args.exe"
set RC=%ERRORLEVEL%

echo.
if %RC%==0 (
    echo ================================================
    echo UNIT TESTS PASSED
    echo ================================================
) else (
    echo ================================================
    echo UNIT TESTS FAILED  (exit %RC%^)
    echo ================================================
)
exit /b %RC%
