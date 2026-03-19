# OneClickRGB - Schritt-für-Schritt Implementierungsanleitung

**Dieser Plan zeigt konkrete, ausführbare Schritte zur professionellen Umstrukturierung.**

---

## SCHRITT 1: GIT-REPOSITORY SETUP (30 Minuten)

### 1.1 .gitignore erstellen

```plaintext
# Build-Verzeichnisse
build/
bin/
obj/
*.o
*.a
*.so
*.dylib
*.dll

# IDE
.vscode/
.idea/
*.sln.user
*.vcxproj.user
CMakeUserPresets.json

# CMake
CMakeCache.txt
CMakeFiles/
Makefile
cmake_install.cmake

# Logging
*.log
output.txt

# Betriebssystem
.DS_Store
Thumbs.db
$RECYCLE.BIN/

# Test-Outputs
test_results.xml
coverage.html

# Temporäre Dateien
*.tmp
*.bak
*~
```

### 1.2 .editorconfig erstellen

```plaintext
root = true

[*]
indent_style = space
indent_size = 4
end_of_line = crlf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true

[*.md]
indent_size = 2

[*.json]
indent_size = 2
```

---

## SCHRITT 2: ORDNERSTRUKTUR REORGANISIEREN (2-3 Stunden)

### 2.1 Neue Ordner erstellen

```powershell
# Führe folgende Befehle aus:
cd D:\xampp\htdocs\RGB\OneClickRGB

# Neue Ordner
mkdir docs
mkdir includes
mkdir installer
mkdir scripts
mkdir tools
mkdir config

# Subdirectories in src/
mkdir src\cli
mkdir src\gui
mkdir src\utils
mkdir src\common
mkdir src\profiles
mkdir src\controllers

# Test-Struktur
mkdir tests\unit
mkdir tests\integration

# Installer-Struktur
mkdir installer\windows
mkdir installer\portable
mkdir installer\windows\resources
```

### 2.2 Existing Files verschieben

```powershell
# Dateien organisieren
Move-Item src\main.cpp src\cli\

# GUI-Skripte aus production nach src\gui\
# (Diese werden später implementiert)

# Legacy-Zeug bleibt wo es ist, aber ist jetzt isoliert
```

---

## SCHRITT 3: CMAKE MODULARISIEREN (1-2 Stunden)

### 3.1 Haupt-CMakeLists.txt (Root)

Erstelle neue Datei: `D:\xampp\htdocs\RGB\OneClickRGB\CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(OneClickRGB 
    VERSION 1.0.0 
    DESCRIPTION "Simple One-Click RGB Control"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build-Optionen
option(BUILD_CLI "Build CLI application" ON)
option(BUILD_GUI "Build GUI application" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_TOOLS "Build developer tools" OFF)

# Output-Verzeichnisse
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler-Warnungen
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sub-Verzeichnisse
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Print Build-Info
message(STATUS "OneClickRGB ${PROJECT_VERSION}")
message(STATUS "  Build CLI: ${BUILD_CLI}")
message(STATUS "  Build GUI: ${BUILD_GUI}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
```

### 3.2 src/CMakeLists.txt

```cmake
# Core Library Sources
set(CORE_SOURCES
    core/OneClickRGB.cpp
    core/DeviceManager.cpp
    core/ProfileManager.cpp
    core/ConfigManager.cpp
    core/AutoStart.cpp
    scanner/HardwareScanner.cpp
    devices/RGBDevice.cpp
    devices/HIDController.cpp
    utils/Logger.cpp
    utils/StringUtils.cpp
    utils/ColorUtils.cpp
)

# Create Core Library
add_library(oneclickrgb_core STATIC ${CORE_SOURCES})

target_include_directories(oneclickrgb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../includes
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Dependencies
target_include_directories(oneclickrgb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/hidapi
    ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/nlohmann
)

# Platform-specific
if(WIN32)
    target_link_libraries(oneclickrgb_core PUBLIC 
        setupapi.lib
        shell32.lib
    )
endif()

# CLI Application
if(BUILD_CLI)
    add_executable(oneclickrgb cli/main.cpp)
    target_link_libraries(oneclickrgb PRIVATE oneclickrgb_core)
    
    if(WIN32)
        target_link_libraries(oneclickrgb PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../dependencies/hidapi/hidapi.lib
        )
    endif()
endif()

# GUI Application (Qt)
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets QUIET)
    endif()
    
    if(Qt6_FOUND OR Qt5_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTORCC ON)
        
        add_executable(oneclickrgb_gui
            gui/main_gui.cpp
            gui/MainWindow.cpp
            gui/DeviceCard.cpp
            gui/QuickActions.cpp
        )
        
        target_link_libraries(oneclickrgb_gui PRIVATE
            oneclickrgb_core
            $<IF:$<TARGET_EXISTS:Qt6::Widgets>,Qt6::Widgets,Qt5::Widgets>
        )
    endif()
endif()
```

### 3.3 tests/CMakeLists.txt

```cmake
# Google Test einbinden
find_package(GTest REQUIRED)

# Unit Tests
add_executable(unit_tests
    unit/test_ColorUtils.cpp
    unit/test_StringUtils.cpp
)

target_link_libraries(unit_tests PRIVATE
    oneclickrgb_core
    GTest::gtest_main
)

# Integration Tests
add_executable(integration_tests
    integration/test_HardwareScanning.cpp
)

target_link_libraries(integration_tests PRIVATE
    oneclickrgb_core
    GTest::gtest_main
)

# Tests registrieren
add_test(NAME UnitTests COMMAND unit_tests)
add_test(NAME IntegrationTests COMMAND integration_tests)
```

---

## SCHRITT 4: BUILD-SKRIPTE MODERNISIEREN (1 Stunde)

### 4.1 scripts/build.bat (Windows)

```batch
@echo off
setlocal enabledelayedexpansion

set BUILD_TYPE=Release
set CMAKE_GENERATOR=Visual Studio 16 2019
set BUILD_CLI=ON
set BUILD_GUI=OFF
set BUILD_TESTS=OFF

if "%1%"=="debug" (
    set BUILD_TYPE=Debug
)
if "%1%"=="gui" (
    set BUILD_GUI=ON
)
if "%1%"=="test" (
    set BUILD_TESTS=ON
)

echo.
echo ================================================
echo OneClickRGB Build Script
echo ================================================
echo Build Type: %BUILD_TYPE%
echo Build CLI: %BUILD_CLI%
echo Build GUI: %BUILD_GUI%
echo Build Tests: %BUILD_TESTS%

REM Visual Studio Setup
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

REM CMake Configure
echo.
echo [1/3] Configuring CMake...
cmake -B build -G "%CMAKE_GENERATOR%" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DBUILD_CLI=%BUILD_CLI% ^
    -DBUILD_GUI=%BUILD_GUI% ^
    -DBUILD_TESTS=%BUILD_TESTS%

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

REM CMake Build
echo.
echo [2/3] Building...
cmake --build build --config %BUILD_TYPE% --parallel 4

if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

REM Copy DLLs
echo.
echo [3/3] Copying dependencies...
copy dependencies\hidapi\hidapi.dll build\bin\%BUILD_TYPE%\ >nul 2>&1

echo.
echo ================================================
echo BUILD SUCCESSFUL!
echo ================================================
echo Output: build\bin\%BUILD_TYPE%\oneclickrgb.exe
```

### 4.2 scripts/build.sh (Linux)

```bash
#!/bin/bash

BUILD_TYPE="Release"
BUILD_CLI=ON
BUILD_GUI=OFF
BUILD_TESTS=OFF

# Parse arguments
case "${1:-}" in
    debug)
        BUILD_TYPE="Debug"
        ;;
    gui)
        BUILD_GUI=ON
        ;;
    test)
        BUILD_TESTS=ON
        ;;
esac

echo "===========================================" 
echo "OneClickRGB Build Script"
echo "==========================================="
echo "Build Type: $BUILD_TYPE"
echo "Build CLI: $BUILD_CLI"
echo "Build GUI: $BUILD_GUI"
echo ""

# Create build directory
cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DBUILD_CLI=$BUILD_CLI \
    -DBUILD_GUI=$BUILD_GUI \
    -DBUILD_TESTS=$BUILD_TESTS

# Build
cmake --build build --parallel $(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "==========================================="
    echo "BUILD SUCCESSFUL!"
    echo "==========================================="
else
    echo ""
    echo "ERROR: Build failed"
    exit 1
fi
```

### 4.3 scripts/clean.bat

```batch
@echo off
echo Cleaning build artifacts...
rmdir /s /q build
rmdir /s /q bin
del /q *.log
echo Done!
```

---

## SCHRITT 5: DOKUMENTATION SETUP (2 Stunden)

### 5.1 docs/INSTALLATION.md

```markdown
# Installation Guide

## Systemanforderungen

- Windows 10/11 (x64)
- 100 MB Festplatte
- Administrator-Rechte (für RGB-Zugriff)

## Installation

### Option 1: Installer (empfohlen)

1. Download: `OneClickRGB_1.0.0_installer.exe`
2. Doppelklick zum Ausführen
3. Folge dem Setup-Wizard
4. Starte OneClickRGB aus dem Startmenü

### Option 2: Portable Version

1. Download: `OneClickRGB_1.0.0_portable.zip`
2. Entpacke das ZIP
3. Führe `oneclickrgb.exe` aus

### Option 3: CLI nur

1. Download: `oneclickrgb_cli.exe`
2. Kopiere die Datei in ein Verzeichnis deiner Wahl
3. Öffne eine Kommandozeile und führe aus:
   ```
   oneclickrgb --help
   ```

## Abhängigkeiten

- Microsoft VC++ Redistributable 2019 (wird vom Installer installiert)
- Für GUI: Qt5/Qt6 Laufzeitbibliotheken

## Fehlerbehandlung

### "hidapi.dll nicht gefunden"
Stelle sicher, dass die DLL im gleichen Verzeichnis wie die exe ist.

### "Keine RGB-Geräte gefunden"
- Verbinde deine Geräte über USB
- Starte die Anwendung neu
- Überprüfe Gerätetreiber

## Update auf neue Version

1. Download der neuen Version
2. Führe den Installer aus (überschreibt alte Version)
3. Deine Profile bleiben erhalten
```

### 5.2 docs/USER_GUIDE.md

```markdown
# Benutzerhandbuch

## CLI Commands

### Geräte scannen
```bash
oneclickrgb scan
```

### Alle Geräte auf eine Farbe setzen
```bash
oneclickrgb color 255 0 0      # Rot
oneclickrgb color 0 255 0      # Grün
oneclickrgb color 0 0 255      # Blau
```

### Vordefinierte Farben
```bash
oneclickrgb red
oneclickrgb green
oneclickrgb blue
oneclickrgb white
oneclickrgb off
```

### Profile verwalten
```bash
oneclickrgb save MyProfile     # Aktuellen Zustand speichern
oneclickrgb profile MyProfile  # Profil laden
oneclickrgb profiles           # Alle Profile zeigen
```

### Autostart
```bash
oneclickrgb autostart on
oneclickrgb startup Gaming     # Gaming-Profil beim Boot laden
```

## GUI Bedienung

1. Starte `oneclickrgb_gui.exe`
2. Geräte werden automatisch erkannt
3. Wähle ein Gerät
4. Wähle eine Farbe
5. Klick "Apply"

## Profile erstellen

### Methode 1: Via CLI
```bash
oneclickrgb color 255 100 0    # Setze gewünschte Farbe
oneclickrgb save Gaming        # Speichere als "Gaming"
```

### Methode 2: Datei-Editor
Erstelle `C:\Users\[USER]\AppData\Local\OneClickRGB\profiles\MyProfile.json`:
```json
{
  "name": "MyProfile",
  "devices": {
    "ASUS Aura": {"color": [255, 0, 0]},
    "SteelSeries": {"color": [0, 255, 0]}
  }
}
```
```

### 5.3 docs/DEVELOPER_GUIDE.md

```markdown
# Entwickler-Guide

## Setup

### Voraussetzungen
- Visual Studio 2019+ oder Build Tools
- CMake 3.20+
- Git
- Python 3.8+

### Projekt klonen
```bash
git clone https://github.com/yourusername/OneClickRGB.git
cd OneClickRGB
```

### Build
```bash
scripts/build.bat              # Windows
./scripts/build.sh             # Linux
```

### Mit Tests bauen
```bash
scripts/build.bat test         # Fügt Tests hinzu
```

## Projektstruktur

```
src/
├── core/              - Kernfunktionalität
├── scanner/           - Hardware-Erkennung
├── devices/           - Geräte-Abstraktion
├── controllers/       - Kontroller-Implementierungen
├── cli/               - CLI-Interface
├── gui/               - GUI-Interface
└── utils/             - Utility-Funktionen
```

## Neuen Controller hinzufügen

1. Erstelle `src/controllers/NewController.cpp/h`
2. Implementiere `IController` Interface
3. Registriere in `ControllerRegistry.cpp`

## Testing

```bash
# Unit Tests ausführen
ctest --build-config Release --output-on-failure

# Spezifischen Test ausführen
ctest -R test_ColorUtils --output-on-failure
```

## Code-Stil

- Verwende clang-format: `clang-format -i src/file.cpp`
- 4 Spaces Einschuss
- camelCase für Variablen
- PascalCase für Klassen

## Git Workflow

1. Feature-Branch: `git checkout -b feature/my-feature`
2. Commits: `git commit -m "Add: my feature"`
3. Push: `git push origin feature/my-feature`
4. Pull Request auf GitHub erstellen
5. Review + Merge
```

---

## SCHRITT 6: KONFIGURATIONSDATEIEN (1 Stunde)

### 6.1 VERSION.txt

```
1.0.0
```

### 6.2 config/default_config.json

```json
{
  "app": {
    "name": "OneClickRGB",
    "version": "1.0.0",
    "organization": "OneClickRGB"
  },
  "cli": {
    "show_logo": true,
    "colorize_output": true,
    "verbose": false
  },
  "hardware": {
    "auto_scan_on_startup": true,
    "scan_interval_ms": 5000,
    "max_devices": 20,
    "enable_debug": false
  },
  "profiles": {
    "auto_load_startup": false,
    "startup_profile": "Default",
    "save_location": "profiles"
  },
  "autostart": {
    "enabled": false,
    "profile": "Default"
  },
  "logging": {
    "level": "INFO",
    "file": "oneclickrgb.log",
    "max_size_mb": 10
  }
}
```

### 6.3 config/device_database.json

```json
{
  "version": "1.0.0",
  "last_updated": "2026-03-19",
  "devices": [
    {
      "id": "asus_aura_0b05_19af",
      "name": "ASUS Aura LED Controller",
      "vendor_id": "0x0b05",
      "product_id": "0x19af",
      "manufacturer": "ASUS",
      "controller_type": "AsusAuraController",
      "interface": 2,
      "usage_page": 0xff72,
      "features": ["color", "brightness", "effects"],
      "zones": ["Primary"],
      "notes": "Mainboard RGB Controller"
    },
    {
      "id": "steelseries_rival_1038_1724",
      "name": "SteelSeries Rival 600",
      "vendor_id": "0x1038",
      "product_id": "0x1724",
      "manufacturer": "SteelSeries",
      "controller_type": "SteelSeriesController",
      "interface": 0,
      "usage_page": 0xffc0,
      "features": ["color", "effects"],
      "zones": ["Main", "Logo", "Accents"],
      "notes": "Gaming Maus"
    }
  ]
}
```

---

## SCHRITT 7: BUILD-TESTING (30 Minuten)

### 7.1 Build testen

```bash
# Sauberer Build mit CMake
cd D:\xampp\htdocs\RGB\OneClickRGB
scripts\build.bat

# Output prüfen
dir build\bin\Release\

# Exe testen
build\bin\Release\oneclickrgb.exe --help
```

### 7.2 Dependencies prüfen

```bash
# DLLs im Verzeichnis?
dir build\bin\Release\*.dll

# Exe starten?
build\bin\Release\oneclickrgb.exe scan
```

---

## SCHRITT 8: GIT INITIALISIEREN (15 Minuten)

```bash
cd D:\xampp\htdocs\RGB\OneClickRGB

# Git initialisieren
git init

# .gitignore hinzufügen
git add .gitignore

# Erste Strukturdateien
git add CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt
git add docs/*.md
git add scripts/*.bat scripts/*.sh
git add config/*.json
git add VERSION.txt
git add README.md

# Commit
git commit -m "Initial: Project structure and build system"

# Branch erstellen
git branch development

# Remote hinzufügen (optional)
# git remote add origin https://github.com/username/OneClickRGB.git
# git push -u origin main
```

---

## SCHRITT 9: ERSTE TESTS (1 Stunde)

### 9.1 Manuelle Tests

```bash
# CLI testen
oneclickrgb scan
oneclickrgb red
oneclickrgb color 255 128 0
oneclickrgb off
oneclickrgb save TestProfile
oneclickrgb profile TestProfile
oneclickrgb profiles
```

### 9.2 Unit Tests einrichtete

Erstelle `tests/unit/test_ColorUtils.cpp`:

```cpp
#include <gtest/gtest.h>
#include "utils/ColorUtils.h"

using namespace OneClickRGB;

TEST(ColorUtils, RGBToHex) {
    RGBColor color(255, 128, 0);
    EXPECT_EQ(color.ToHex(), 0xFF8000);
}

TEST(ColorUtils, HexToRGB) {
    RGBColor color = RGBColor::FromHex(0xFF0000);
    EXPECT_EQ(color.r, 255);
    EXPECT_EQ(color.g, 0);
    EXPECT_EQ(color.b, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

---

## SCHRITT 10: CI/CD SETUP (1 Stunde)

### 10.1 GitHub Actions Workflow

Erstelle `.github/workflows/build.yml`:

```yaml
name: Build & Test

on:
  push:
    branches: [ main, development ]
  pull_request:
    branches: [ main, development ]

jobs:
  build:
    runs-on: windows-latest

    strategy:
      matrix:
        build-type: [Release, Debug]

    steps:
    - uses: actions/checkout@v3
    
    - name: Setup MSVC
      uses: ilammy/msvc-dev-cmd@v1
    
    - name: Install CMake
      uses: lukka/get-cmake@latest
    
    - name: Configure CMake
      run: |
        cmake -B build -G "Visual Studio 17 2022" `
          -DCMAKE_BUILD_TYPE=${{ matrix.build-type }} `
          -DBUILD_CLI=ON `
          -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build --config ${{ matrix.build-type }}
    
    - name: Run Tests
      run: ctest --build-config ${{ matrix.build-type }} --output-on-failure
    
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: oneclickrgb-${{ matrix.build-type }}
        path: build/bin/${{ matrix.build-type }}/
```

---

## SCHRITT 11: DOKUMENTATION FINALISIEREN (1 Stunde)

### 11.1 README.md aktualisieren

```markdown
# OneClickRGB

**Simple One-Click RGB Control für Windows**

Steuere alle deine RGB-Geräte mit einem einfachen Klick - kein komplexes Setup, keine unnötigen Treiber.

## Features

✨ **Einfach bedienen**: Ein Befehl für alle Geräte
🎨 **Mehrere Farben**: 16 Millionen Farben
⚡ **Quick Profiles**: Speichere deine liebsten Einstellungen
🎮 **Autostart**: Profile beim Boot laden
🖥️ **CLI & GUI**: Wahl zwischen Command-Line oder grafischer Oberfläche

## Schnellstart

```bash
# Installation
# Lade OneClickRGB_1.0.0_installer.exe herunter

# CLI Beispiele
oneclickrgb red                 # Alle Geräte rot
oneclickrgb color 255 128 0    # RGB-Farbe (Orange)
oneclickrgb save Gaming        # Profil speichern
oneclickrgb profile Gaming     # Profil laden
```

[Weitere Dokumentation](docs/)

## Unterstützte Geräte

- ✅ ASUS Aura (Mainboard)
- ✅ ASUS Aura (Laptop)
- ✅ SteelSeries Rival 600
- ✅ E-Vision Keyboards
- 🔜 Corsair iCUE
- 🔜 Logitech G-Serie
- 🔜 Razer RGB

## Installation

[Siehe Installationsguide](docs/INSTALLATION.md)

## Lizenz

MIT License - Siehe [LICENSE](LICENSE)

## Kontakt & Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/OneClickRGB/issues)
- **Diskussionen**: [GitHub Discussions](https://github.com/yourusername/OneClickRGB/discussions)
```

---

## ZUSAMMENFASSUNG NACHPRÜFUNG

Nach Abschluss aller Schritte sollte folgendes vorhanden sein:

- ✅ Saubere Ordnerstruktur
- ✅ Modulare CMake-Config
- ✅ Build-Skripte (.bat & .sh)
- ✅ Dokumentation (5+ Dateien)
- ✅ Git-Konfiguration (.gitignore, .editorconfig)
- ✅ Konfigurationsdateien (config/*.json)
- ✅ Test-Struktur
- ✅ CI/CD-Workflow
- ✅ Funktionierendes Build-System

**Geschätzte Gesamtdauer: 8-10 Stunden**
**Danach: Production-ready!**

