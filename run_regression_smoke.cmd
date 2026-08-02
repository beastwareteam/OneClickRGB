@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set FAIL=0
set EDGE_RUNS=5
set MOUSE_RUNS=5

echo ================================================
echo OneClickRGB Regression Smoke
echo ================================================
echo.

call "%~dp0build_native.bat"
if errorlevel 1 (
  echo [FAIL] Build failed
  exit /b 1
)

if not exist "%~dp0build\OneClickRGB.exe" (
  echo [FAIL] Executable missing: build\OneClickRGB.exe
  exit /b 1
)

echo [1/2] Edge switch-test loop (%EDGE_RUNS%x)
for /L %%I in (1,1,%EDGE_RUNS%) do (
  "%~dp0build\OneClickRGB.exe" --switch-test=edge >nul 2>&1
  if errorlevel 1 (
    echo [FAIL] edge run %%I
    set /a FAIL+=1
  )
)

echo [2/2] Mouse switch-test loop (%MOUSE_RUNS%x)
for /L %%I in (1,1,%MOUSE_RUNS%) do (
  "%~dp0build\OneClickRGB.exe" --switch-test=mouse >nul 2>&1
  if errorlevel 1 (
    echo [FAIL] mouse run %%I
    set /a FAIL+=1
  )
)

echo.
if %FAIL% EQU 0 (
  echo [OK] Regression smoke passed
  echo edge=%EDGE_RUNS% mouse=%MOUSE_RUNS% fails=0
  exit /b 0
)

echo [FAIL] Regression smoke failed
echo edge=%EDGE_RUNS% mouse=%MOUSE_RUNS% fails=%FAIL%
exit /b 1
