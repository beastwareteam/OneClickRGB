@echo off
setlocal
cd /d "%~dp0"

set PROFILE=%~1
set DURATION=%~2
if "%PROFILE%"=="" set PROFILE=
if "%DURATION%"=="" set DURATION=13

echo ================================================
echo OneClickRGB Aura-Only Test
echo ================================================
echo.
echo Optional usage:
echo   test_aura_only.bat PROFILE_NAME DURATION_SECONDS
echo Example:
echo   test_aura_only.bat MainboardTest 13
echo.

echo [1/3] Stopping running instance...
powershell -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" 2>nul

echo [2/3] Building...
call "%~dp0build_native.bat"
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo [3/3] Starting Aura-only mode...
if "%PROFILE%"=="" (
    echo Command: OneClickRGB.exe --only-aura
    start "" "%~dp0build\OneClickRGB.exe" --only-aura
) else (
    echo Command: OneClickRGB.exe --only-aura --profile=%PROFILE%
    start "" "%~dp0build\OneClickRGB.exe" --only-aura --profile=%PROFILE%
)

echo.
echo Waiting %DURATION% seconds for visual validation...
timeout /t %DURATION% /nobreak >nul

echo Stopping test instance...
powershell -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" 2>nul

echo.
echo Check: only mainboard/onboard Aura channels should react.
echo All other devices are disabled for this run.
echo Test window duration: %DURATION% seconds.
endlocal
