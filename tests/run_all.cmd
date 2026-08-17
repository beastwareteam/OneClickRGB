@echo off
REM ================================================
REM OneClickRGB - alle automatisierbaren Pruefungen
REM
REM Keine dieser Pruefungen schreibt auf Hardware. Sie belegen, dass die App nur
REM dort schreibt, wo sie darf, nur meldet, was sie zurueckgelesen hat, und unter
REM --dry-run gar nicht schreibt.
REM
REM Was hier NICHT geprueft werden kann: ob ein Effekt sichtbar laeuft. Dafuer
REM gibt es tools\edge_probe_session.ps1 - die schreibt auf Hardware und braucht
REM einen Menschen davor.
REM
REM Aufruf: tests\run_all.cmd
REM ================================================
setlocal enabledelayedexpansion

cd /d "%~dp0.."
set FAIL=0

echo.
echo ################################################
echo # 1/4  Unit-Tests (cli_args.h, effect_limits.h, channel_config.h)
echo ################################################
call "%~dp0run_tests.cmd"
if errorlevel 1 set /a FAIL+=1

echo.
echo ################################################
echo # 2/4  Selbsttest kbdump_diff.ps1
echo ################################################
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test_kbdump_diff.ps1"
if errorlevel 1 set /a FAIL+=1

echo.
echo ################################################
echo # 3/4  Build
echo ################################################
REM Eine laufende OneClickRGB.exe sperrt die Ausgabedatei - erst beenden.
powershell -NoProfile -Command "Stop-Process -Name OneClickRGB -Force -ErrorAction SilentlyContinue" >nul 2>&1
REM Zusaetzlich das alte .obj wegwerfen. Selbst ohne laufende Instanz scheitert
REM cl.exe sonst gelegentlich mit "Permission denied" auf
REM build\oneclick_rgb_complete.obj; loeschen laesst sich die Datei aber immer,
REM und ein Neuaufbau kostet nichts, weil es hier nur eine Uebersetzungseinheit
REM gibt. Ein Build, der an einer gesperrten Zwischen-Datei haengt, wuerde sonst
REM als "Testfehler" durchgehen.
del /f /q "%~dp0..\build\oneclick_rgb_complete.obj" >nul 2>&1
call "%~dp0..\build_native.bat" >"%~dp0build\build.log" 2>&1
if errorlevel 1 (
    echo [FAIL] Build fehlgeschlagen - siehe tests\build\build.log
    type "%~dp0build\build.log"
    set /a FAIL+=1
) else (
    echo [OK] Build erfolgreich
)

echo.
echo ################################################
echo # 4/4  Dry-Run-Wache am fertigen Binary
echo ################################################
if not exist "%~dp0..\build\OneClickRGB.exe" (
    echo [SKIP] kein Binary vorhanden
    set /a FAIL+=1
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\tools\check_dryrun_flags.ps1"
    if errorlevel 1 set /a FAIL+=1
)

echo.
echo ################################################
if %FAIL%==0 (
    echo # ALLES OK
    echo ################################################
    exit /b 0
)
echo # %FAIL% Testblock^(e^) fehlgeschlagen
echo ################################################
exit /b 1
