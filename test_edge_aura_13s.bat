@echo off
setlocal
cd /d "%~dp0"

set PROFILE=%~1
set DURATION=%~2
if "%DURATION%"=="" set DURATION=13

echo ================================================
echo OneClickRGB Isolated Test: EDGE then AURA
echo ================================================
echo Duration per step: %DURATION% seconds
echo.

echo [1/4] Edge-only test...
call "%~dp0test_edge_only.bat" %DURATION%
if errorlevel 1 exit /b 1

echo.
echo [2/4] Cooldown (2s)...
timeout /t 2 /nobreak >nul

echo [3/4] Aura-only test...
if "%PROFILE%"=="" (
    call "%~dp0test_aura_only.bat" "" %DURATION%
) else (
    call "%~dp0test_aura_only.bat" %PROFILE% %DURATION%
)
if errorlevel 1 exit /b 1

echo.
echo [4/4] Done.
echo Please confirm:
echo - Edge test: only edge reacted?
echo - Aura test: only onboard/mainboard reacted?
endlocal
