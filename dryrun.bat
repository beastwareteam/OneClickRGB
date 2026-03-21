@echo off
setlocal
REM OneClickRGB Dry Run - Build, Start (without hardware), Screenshot
REM Usage: dryrun.bat [--no-screenshot]
REM
REM This starts the app with --dry-run flag, which skips all hardware
REM communication. Use this for UI development and testing.

cd /d "%~dp0"

echo ================================================
echo OneClickRGB Dry Run
echo ================================================

REM Kill existing instance
echo [1/4] Stopping existing instance...
powershell -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" 2>nul
ping -n 2 127.0.0.1 >nul

REM Build
echo [2/4] Building...
call "%~dp0build_native.bat" >nul 2>&1
if errorlevel 1 (
    echo BUILD FAILED - Running full build for details:
    call "%~dp0build_native.bat"
    exit /b 1
)
echo Build successful.

REM Start with --dry-run flag (no hardware communication)
echo [3/4] Starting application (DRY RUN mode)...
start "" "%~dp0build\OneClickRGB.exe" --dry-run

REM Screenshot (unless --no-screenshot)
if "%1"=="--no-screenshot" (
    echo [4/4] Screenshot skipped.
) else (
    echo [4/4] Waiting for window and capturing screenshot...
    REM Wait for window to appear (up to 5 seconds)
    powershell -Command "for ($i=0; $i -lt 10; $i++) { $p = Get-Process -Name 'OneClickRGB' -ErrorAction SilentlyContinue; if ($p -and $p.MainWindowHandle -ne 0) { break }; Start-Sleep -Milliseconds 500 }"
    powershell -ExecutionPolicy Bypass -File "%~dp0docs\screenshots\capture_window.ps1"
)

echo ================================================
echo Dry Run Complete
echo ================================================
echo.
echo App running: build\OneClickRGB.exe
if not "%1"=="--no-screenshot" (
    echo Screenshot: docs\screenshots\02_main_window.png
)
endlocal
