@echo off
REM ============================================================
REM  OneClickRGB - Manuelle Installation
REM  Interaktive Installation mit Optionen
REM ============================================================

setlocal EnableDelayedExpansion

REM Admin-Check
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [FEHLER] Bitte als Administrator ausfuehren!
    echo Rechtsklick auf install_manual.bat -^> "Als Administrator ausfuehren"
    pause
    exit /b 1
)

echo ============================================================
echo   OneClickRGB - Manuelle Installation
echo ============================================================
echo.

REM Installationsverzeichnis waehlen
set "DEFAULT_DIR=%ProgramFiles%\OneClickRGB"
echo Standard-Installationsverzeichnis: %DEFAULT_DIR%
echo.
echo Eigenen Pfad eingeben oder Enter fuer Standard:
set /p INSTALL_DIR="> "
if "%INSTALL_DIR%"=="" set "INSTALL_DIR=%DEFAULT_DIR%"

echo.
echo Installationsverzeichnis: %INSTALL_DIR%
echo.

REM Verzeichnis erstellen
echo [1/7] Erstelle Verzeichnis...
if not exist "%INSTALL_DIR%" (
    mkdir "%INSTALL_DIR%"
    echo     Verzeichnis erstellt.
) else (
    echo     Verzeichnis existiert bereits.
)

REM Dateien kopieren
echo.
echo [2/7] Kopiere Programmdateien...
echo     - OneClickRGB.exe
copy /Y "%~dp0OneClickRGB.exe" "%INSTALL_DIR%\" >nul
echo     - hidapi.dll
copy /Y "%~dp0hidapi.dll" "%INSTALL_DIR%\" >nul
echo     - PawnIOLib.dll
copy /Y "%~dp0PawnIOLib.dll" "%INSTALL_DIR%\" >nul
echo     - SmbusI801.bin
copy /Y "%~dp0SmbusI801.bin" "%INSTALL_DIR%\" >nul
echo     - icon.png
copy /Y "%~dp0icon.png" "%INSTALL_DIR%\" >nul
echo     Alle Dateien kopiert.

REM PawnIO Treiber
echo.
echo [3/7] PawnIO Treiber Installation
echo     Der PawnIO Treiber wird fuer G.Skill RAM benoetigt.
echo     Treiber jetzt installieren? (J/N)
set /p INSTALL_PAWNIO="> "
if /i "%INSTALL_PAWNIO%"=="J" (
    if exist "%~dp0PawnIO_setup.exe" (
        echo     Starte PawnIO Setup...
        start /wait "" "%~dp0PawnIO_setup.exe"
        echo     Treiber-Installation abgeschlossen.
    ) else (
        echo     [WARNUNG] PawnIO_setup.exe nicht gefunden!
    )
) else (
    echo     Treiber-Installation uebersprungen.
)

REM Desktop-Verknuepfung
echo.
echo [4/7] Desktop-Verknuepfung erstellen? (J/N)
set /p CREATE_DESKTOP="> "
if /i "%CREATE_DESKTOP%"=="J" (
    set "SHORTCUT=%USERPROFILE%\Desktop\OneClickRGB.lnk"
    powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%SHORTCUT%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.Save()"
    echo     Desktop-Verknuepfung erstellt.
) else (
    echo     Uebersprungen.
)

REM Startmenue
echo.
echo [5/7] Startmenue-Eintrag erstellen? (J/N)
set /p CREATE_STARTMENU="> "
if /i "%CREATE_STARTMENU%"=="J" (
    set "STARTMENU=%ProgramData%\Microsoft\Windows\Start Menu\Programs\OneClickRGB.lnk"
    powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%STARTMENU%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.Save()"
    echo     Startmenue-Eintrag erstellt.
) else (
    echo     Uebersprungen.
)

REM Autostart
echo.
echo [6/7] Autostart einrichten? (J/N)
echo     (Startet minimiert im System-Tray)
set /p CREATE_AUTOSTART="> "
if /i "%CREATE_AUTOSTART%"=="J" (
    set "AUTOSTART=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\OneClickRGB.lnk"
    powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%AUTOSTART%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.Arguments = '--minimized'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.Save()"
    echo     Autostart eingerichtet.
) else (
    echo     Uebersprungen.
)

REM Firewall-Regel (optional, nicht noetig aber schadet nicht)
echo.
echo [7/7] Installation abgeschlossen!
echo.
echo ============================================================
echo   Zusammenfassung
echo ============================================================
echo   Installiert in: %INSTALL_DIR%
echo.
echo   Enthaltene Dateien:
dir /b "%INSTALL_DIR%"
echo.
echo ============================================================
echo.
echo Programm jetzt starten? (J/N)
set /p START_NOW="> "
if /i "%START_NOW%"=="J" (
    start "" "%INSTALL_DIR%\OneClickRGB.exe"
)

echo.
pause
