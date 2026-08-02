@echo off
setlocal
cd /d "%~dp0"

echo ================================================
echo OneClickRGB Mouse Switch Test
echo ================================================

echo [1/3] Stopping running instance...
powershell -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" 2>nul

echo [2/3] Building...
call "%~dp0build_native.bat"
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo [3/3] Starting mouse switch test...
echo Command: OneClickRGB.exe --switch-test=mouse
"%~dp0build\OneClickRGB.exe" --switch-test=mouse

echo.
echo Check sequence (13s): OFF(2s) -> BLUE(4s) -> RED(4s) -> OFF(3s)
echo Target: mouse only.
endlocal
