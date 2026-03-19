@echo off
REM OneClickRGB Test Runner
REM Usage: run_tests.bat [all|unit|integration|stress]

echo ========================================
echo OneClickRGB Test Runner
echo ========================================
echo.

cd /d "%~dp0..\build"

REM Check if test executables exist
if not exist "test_suite.exe" (
    echo Building test_suite.exe...
    call compile_tests.bat
)

if not exist "test_integration.exe" (
    echo Building test_integration.exe...
    call compile_tests.bat
)

set TEST_TYPE=%1
if "%TEST_TYPE%"=="" set TEST_TYPE=all

if "%TEST_TYPE%"=="all" goto run_all
if "%TEST_TYPE%"=="unit" goto run_unit
if "%TEST_TYPE%"=="integration" goto run_integration
if "%TEST_TYPE%"=="stress" goto run_stress
if "%TEST_TYPE%"=="devices" goto run_devices
if "%TEST_TYPE%"=="colors" goto run_colors
if "%TEST_TYPE%"=="modes" goto run_modes

:run_all
echo Running ALL tests...
echo.
echo === Unit Tests ===
test_suite.exe --all --verbose
echo.
echo === Integration Tests ===
test_integration.exe
goto end

:run_unit
echo Running Unit Tests...
test_suite.exe --all --verbose
goto end

:run_integration
echo Running Integration Tests...
test_integration.exe
goto end

:run_stress
echo Running Stress Tests...
test_suite.exe --stress --verbose
goto end

:run_devices
echo Running Device Detection Tests...
test_suite.exe --devices --verbose
goto end

:run_colors
echo Running Color Tests...
test_suite.exe --colors --verbose
goto end

:run_modes
echo Running Mode Tests...
test_suite.exe --modes --verbose
goto end

:end
echo.
echo Tests completed.
pause
