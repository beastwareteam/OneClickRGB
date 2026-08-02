@echo off
setlocal
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

set LOG_FILE=%SCRIPT_DIR%test_edge_only.log
echo [%DATE% %TIME%] test_edge_only.bat started > "%LOG_FILE%"

set DURATION=%~1
if "%DURATION%"=="" set DURATION=13

where cmd >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmd.exe not found.
    echo ERROR: cmd.exe not found. >> "%LOG_FILE%"
    pause
    exit /b 1
)

if not exist "%SCRIPT_DIR%build_native.bat" (
    echo ERROR: build_native.bat not found in %SCRIPT_DIR%
    echo ERROR: build_native.bat missing >> "%LOG_FILE%"
    pause
    exit /b 1
)

echo ================================================
echo OneClickRGB Edge-Only Test
echo ================================================

echo [1/3] Stopping running instance...
powershell -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" 2>nul
echo Stop running instance done >> "%LOG_FILE%"

echo [2/3] Building...
call "%SCRIPT_DIR%build_native.bat" >> "%LOG_FILE%" 2>&1
if errorlevel 1 (
    echo Build failed.
    echo Build failed. See log: %LOG_FILE%
    echo Build failed >> "%LOG_FILE%"
    pause
    exit /b 1
)

if not exist "%SCRIPT_DIR%build\OneClickRGB.exe" (
    echo ERROR: build\OneClickRGB.exe not found after build.
    echo ERROR: build\OneClickRGB.exe not found >> "%LOG_FILE%"
    pause
    exit /b 1
)

echo [3/3] Starting Edge-only mode...
echo Command: OneClickRGB.exe --switch-test=edge
"%SCRIPT_DIR%build\OneClickRGB.exe" --switch-test=edge >> "%LOG_FILE%" 2>&1
set TEST_EXIT=%ERRORLEVEL%

echo.
echo Check sequence (13s): OFF(2s) -> BLUE(4s) -> RED(4s) -> OFF(3s)
echo Target: keyboard edge LEDs only.
echo Script exit code: %TEST_EXIT%
echo [%DATE% %TIME%] Script exit code: %TEST_EXIT% >> "%LOG_FILE%"
echo Log file: %LOG_FILE%

pause
exit /b %TEST_EXIT%
endlocal
