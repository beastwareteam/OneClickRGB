@echo off
REM ============================================================
REM  OneClickRGB - Standard Installation
REM  Installiert nach %ProgramFiles%\OneClickRGB
REM  Erstellt Desktop-Verknuepfung und Autostart
REM ============================================================

setlocal EnableDelayedExpansion

REM Admin-Check
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [FEHLER] Bitte als Administrator ausfuehren!
    echo Rechtsklick auf install.bat -^> "Als Administrator ausfuehren"
    pause
    exit /b 1
)

echo ============================================================
echo   OneClickRGB Installation
echo ============================================================
echo.

REM Installationsverzeichnis
set "INSTALL_DIR=%ProgramFiles%\OneClickRGB"

echo [1/6] Erstelle Installationsverzeichnis...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

echo [2/6] Kopiere Dateien...
copy /Y "%~dp0OneClickRGB.exe" "%INSTALL_DIR%\" >nul
copy /Y "%~dp0hidapi.dll" "%INSTALL_DIR%\" >nul
copy /Y "%~dp0PawnIOLib.dll" "%INSTALL_DIR%\" >nul
copy /Y "%~dp0SmbusI801.bin" "%INSTALL_DIR%\" >nul
copy /Y "%~dp0icon.png" "%INSTALL_DIR%\" >nul

echo [3/6] Installiere PawnIO Treiber (fuer G.Skill RAM)...
if exist "%~dp0PawnIO_setup.exe" (
    echo     Starte PawnIO Setup...
    start /wait "" "%~dp0PawnIO_setup.exe" /S
    echo     PawnIO Treiber installiert.
) else (
    echo     [WARNUNG] PawnIO_setup.exe nicht gefunden - G.Skill RAM evtl. nicht steuerbar
)

echo [4/6] Erstelle Desktop-Verknuepfung...
set "SHORTCUT=%USERPROFILE%\Desktop\OneClickRGB.lnk"
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%SHORTCUT%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.IconLocation = '%INSTALL_DIR%\icon.png'; $sc.Save()"

echo [5/6] Erstelle Startmenue-Eintrag...
set "STARTMENU=%ProgramData%\Microsoft\Windows\Start Menu\Programs\OneClickRGB.lnk"
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%STARTMENU%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.Save()"

echo [6/6] Richte Autostart ein...
set "AUTOSTART=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\OneClickRGB.lnk"
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $sc = $ws.CreateShortcut('%AUTOSTART%'); $sc.TargetPath = '%INSTALL_DIR%\OneClickRGB.exe'; $sc.Arguments = '--minimized'; $sc.WorkingDirectory = '%INSTALL_DIR%'; $sc.Save()"

echo.
echo ============================================================
echo   Installation abgeschlossen!
echo ============================================================
echo.
echo   Installiert in: %INSTALL_DIR%
echo   Desktop-Verknuepfung: Erstellt
echo   Autostart: Aktiviert (minimiert)
echo.
echo   Starte OneClickRGB jetzt? (J/N)
set /p START_NOW="> "
if /i "%START_NOW%"=="J" (
    start "" "%INSTALL_DIR%\OneClickRGB.exe"
)

echo.
echo Fertig!
pause
