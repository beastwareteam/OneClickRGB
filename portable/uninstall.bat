@echo off
REM ============================================================
REM  OneClickRGB - Deinstallation
REM ============================================================

setlocal EnableDelayedExpansion

REM Admin-Check
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [FEHLER] Bitte als Administrator ausfuehren!
    pause
    exit /b 1
)

echo ============================================================
echo   OneClickRGB Deinstallation
echo ============================================================
echo.
echo WARNUNG: Dies entfernt OneClickRGB vollstaendig!
echo.
echo Fortfahren? (J/N)
set /p CONFIRM="> "
if /i not "%CONFIRM%"=="J" (
    echo Abgebrochen.
    pause
    exit /b 0
)

echo.
echo [1/5] Beende laufende Instanzen...
taskkill /F /IM OneClickRGB.exe 2>nul
timeout /t 2 /nobreak >nul

echo [2/5] Entferne Autostart...
del "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\OneClickRGB.lnk" 2>nul

echo [3/5] Entferne Startmenue-Eintrag...
del "%ProgramData%\Microsoft\Windows\Start Menu\Programs\OneClickRGB.lnk" 2>nul

echo [4/5] Entferne Desktop-Verknuepfung...
del "%USERPROFILE%\Desktop\OneClickRGB.lnk" 2>nul

echo [5/5] Entferne Programmdateien...
set "INSTALL_DIR=%ProgramFiles%\OneClickRGB"
if exist "%INSTALL_DIR%" (
    rmdir /S /Q "%INSTALL_DIR%"
    echo     Programmverzeichnis entfernt.
) else (
    echo     Programmverzeichnis nicht gefunden.
)

echo.
echo Benutzereinstellungen entfernen? (J/N)
echo (Profile und Einstellungen in %APPDATA%\OneClickRGB)
set /p REMOVE_SETTINGS="> "
if /i "%REMOVE_SETTINGS%"=="J" (
    if exist "%APPDATA%\OneClickRGB" (
        rmdir /S /Q "%APPDATA%\OneClickRGB"
        echo     Einstellungen entfernt.
    )
)

echo.
echo ============================================================
echo   Deinstallation abgeschlossen!
echo ============================================================
echo.
echo Hinweis: PawnIO Treiber wurde NICHT entfernt.
echo          (Wird evtl. von anderer Software benoetigt)
echo.
pause
