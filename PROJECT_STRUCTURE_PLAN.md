# OneClickRGB - Vollständiger Projektstruktur- und Implementierungsplan

**Version:** 1.0.0  
**Status:** Produktionsvorbereitung  
**Erstellt:** 19. März 2026

---

## PHASE 0: ANALYSE DES AKTUELLEN ZUSTANDS

### ✅ Bereits vorhanden:
- CLI-Version (funktionstüchtig)
- Core Library (Devices, Scanner, ProfileManager)
- 3 Controller: ASUS Aura, SteelSeries Rival 600, E-Vision Keyboard
- CMakeLists.txt (Qt-vorbereitet)
- Dependency Management (HIDAPI, nlohmann_json)

### ⚠️ Verbesserungsbedarf:
- GUI-Version noch nicht komplett
- Keine Installer/Deployment-Lösung
- Keine automatisierten Tests
- Dokumentation unvollständig
- Build-Prozess manuell
- Keine Versionskontrolle/Releases

---

## PHASE 1: PROJEKTSTRUKTUR REORGANISIEREN

### 1.1 Zieldateistruktur
```
OneClickRGB/
├── .github/                          # GitHub-spezifische Dateien
│   ├── workflows/
│   │   ├── build.yml                 # CI/CD Build-Pipeline
│   │   ├── release.yml               # Automatische Release-Erstellung
│   │   └── tests.yml                 # Automatisierte Tests
│   └── ISSUE_TEMPLATE/               # Issue-Templates
│
├── docs/
│   ├── README.md                     # Hauptdokumentation
│   ├── INSTALLATION.md               # Installations-Guide
│   ├── USER_GUIDE.md                 # Benutzerhandbuch
│   ├── DEVELOPER_GUIDE.md            # Entwickler-Guide
│   ├── API_REFERENCE.md              # API-Dokumentation
│   ├── SUPPORTED_DEVICES.md          # Unterstützte Geräte
│   ├── CHANGELOG.md                  # Versions-Historie
│   ├── CONTRIBUTING.md               # Beitrag-Richtlinien
│   ├── architecture/
│   │   ├── system_design.md
│   │   ├── device_detection.md
│   │   └── profile_system.md
│   └── screenshots/                  # UI-Screenshots
│
├── src/
│   ├── core/                         # Kern-Funktionalität
│   │   ├── OneClickRGB.cpp/h         # Hauptapplika­tion
│   │   ├── DeviceManager.cpp/h       # Geräte-Verwaltung
│   │   ├── ProfileManager.cpp/h      # Profil-Verwaltung
│   │   ├── ConfigManager.cpp/h       # Konfigurationsmanager
│   │   └── AutoStart.cpp/h           # Autostart-Funktionalität
│   │
│   ├── scanner/                      # Hardware-Scanner
│   │   ├── HardwareScanner.cpp/h
│   │   ├── DeviceDatabase.cpp/h      # Geräte-Datenbank
│   │   └── DetectionEngine.cpp/h     # Erkennungsmodul
│   │
│   ├── devices/                      # Geräte-Abstraktion
│   │   ├── RGBDevice.cpp/h
│   │   ├── HIDController.cpp/h
│   │   └── DeviceFactory.cpp/h
│   │
│   ├── controllers/                  # Gerätespezif. Controller
│   │   ├── IController.h             # Interface
│   │   ├── AsusAuraController.cpp/h
│   │   ├── SteelSeriesController.cpp/h
│   │   ├── EVisionController.cpp/h
│   │   ├── CorsairController.cpp/h
│   │   ├── LogitechController.cpp/h
│   │   └── ControllerRegistry.cpp/h  # Registrierung
│   │
│   ├── profiles/                     # Profil-System
│   │   ├── ProfileFormat.h
│   │   ├── ProfileLoader.cpp/h
│   │   └── ProfileValidator.cpp/h
│   │
│   ├── cli/                          # CLI-Interface
│   │   ├── main.cpp                  # CLI-Einstiegspunkt
│   │   ├── CommandParser.cpp/h
│   │   ├── CLI.cpp/h
│   │   └── HelpText.cpp/h
│   │
│   ├── gui/                          # GUI-Interface (Qt)
│   │   ├── main_gui.cpp              # Gui-Einstiegspunkt
│   │   ├── MainWindow.cpp/h
│   │   ├── DeviceCard.cpp/h
│   │   ├── QuickActions.cpp/h
│   │   ├── SettingsDialog.cpp/h
│   │   ├── ProfileDialog.cpp/h
│   │   ├── resources.qrc             # Qt-Ressourcen
│   │   └── styles/
│   │       └── modern.qss            # Qt-Stylesheets
│   │
│   ├── utils/                        # Utility-Funktionen
│   │   ├── Logger.cpp/h
│   │   ├── StringUtils.cpp/h
│   │   ├── FileUtils.cpp/h
│   │   ├── ColorUtils.cpp/h
│   │   └── PlatformUtils.cpp/h
│   │
│   └── common/                       # Gemeinsame Definitionen
│       ├── RGBColor.h
│       ├── Enums.h
│       ├── Types.h
│       └── Constants.h
│
├── includes/                         # Public Header-Dateien
│   └── OneClickRGB/
│       └── OneClickRGB.h             # Public API
│
├── dependencies/                     # Externe Abhängigkeiten
│   ├── hidapi/
│   │   ├── hidapi.h
│   │   ├── hidapi.dll
│   │   └── hidapi.lib
│   ├── nlohmann/
│   │   └── json.hpp
│   ├── PawnIO/                       # RAM-RGB-Support
│   │   └── ...
│   └── README.md                     # Abhängigkeits-Beschreibung
│
├── tests/
│   ├── unit/
│   │   ├── test_ColorUtils.cpp
│   │   ├── test_DeviceManager.cpp
│   │   ├── test_ProfileManager.cpp
│   │   └── test_*.cpp
│   ├── integration/
│   │   ├── test_HardwareScanning.cpp
│   │   ├── test_DeviceControl.cpp
│   │   └── test_*.cpp
│   ├── CMakeLists.txt
│   └── README.md
│
├── build/                            # Build-Output (generiert)
│   ├── bin/                          # Ausführbare Dateien
│   │   ├── Release/
│   │   │   ├── oneclickrgb.exe
│   │   │   ├── oneclickrgb_gui.exe
│   │   │   └── hidapi.dll
│   │   └── Debug/
│   └── lib/                          # Bibliotheken
│
├── installer/                        # Installer-Skripte
│   ├── windows/
│   │   ├── OneClickRGB_Installer.nsi # NSIS Installer
│   │   ├── create_installer.bat
│   │   └── resources/
│   │       ├── icon.ico
│   │       ├── banner.bmp
│   │       └── license.txt
│   ├── portable/
│   │   ├── create_portable.bat       # Portable Version
│   │   └── launcher.bat
│   └── README.md
│
├── scripts/
│   ├── build.bat                     # Windows Build
│   ├── build.sh                      # Linux Build
│   ├── clean.bat
│   ├── test.bat
│   ├── package.bat
│   ├── version.py                    # Versionsverwaltung
│   └── README.md
│
├── config/
│   ├── default_config.json           # Standard-Konfiguration
│   ├── device_database.json          # Geräte-Datenbank
│   └── profiles_example/             # Beispiel-Profile
│       ├── Gaming.json
│       ├── Office.json
│       └── Calm.json
│
├── tools/                            # Entwickler-Tools
│   ├── device_tester.cpp/h           # Geräte-Tester
│   ├── profile_validator.cpp/h       # Profil-Validator
│   └── device_scanner_debug.cpp      # Debug-Scanner
│
├── legacy/                           # Alte Implementierungen
│   └── README.md                     # Erklärung
│
├── production/                       # Produktions-Dateien
│   ├── build.bat                     # Produktions-Build
│   ├── compile.py                    # Python Build-Skript
│   └── build/                        # Build-Output
│
├── .gitignore
├── .editorconfig
├── CMakeLists.txt                    # CMake Konfiguration
├── CMakePresets.json                 # CMake Presets
├── VERSION.txt                       # Version: 1.0.0
├── LICENSE                           # Lizenz (z.B. MIT/GPL)
├── README.md                         # Projekt-README
├── CONTRIBUTING.md                   # Beitrag-Richtlinien
├── CODE_OF_CONDUCT.md                # Code-of-Conduct
├── CHANGELOG.md                      # Versions-Historia
├── vcpkg.json                        # Dependency Management (optional)
└── .clang-format                     # Code-Formatierung

```

---

## PHASE 2: BUILD-SYSTEM PROFESSIONALISIEREN

### 2.1 CMakeLists.txt Hierarchie

**1. Haupt-CMakeLists.txt** - root-Level
```cmake
cmake_minimum_required(VERSION 3.20)
project(OneClickRGB VERSION 1.0.0 LANGUAGES CXX)

# Build-Optionen
option(BUILD_CLI "Build CLI application" ON)
option(BUILD_GUI "Build GUI application" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_TOOLS "Build developer tools" OFF)
option(USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sub-Verzeichnisse
add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()
```

### 2.2 Build-Skripte

**Windows build.bat**
```batch
@echo off
:: Windows Build Script mit Profile und Optionen
```

**Linux build.sh**
```bash
#!/bin/bash
# Linux Build Script mit automake/cmake
```

### 2.3 Version Management (version.py)
```python
# Automatisierte Versionsverwaltung
# - Git-Tag auslesen
# - Version in Dateien aktualisieren
# - Build-Info generieren
```

---

## PHASE 3: ABHÄNGIGKEITEN PROFESSIONELL MANAGEN

### 3.1 Dependency Management Optionen

**Option A: vcpkg (empfohlen)**
```
vcpkg install hidapi:x64-windows nlohmann-json:x64-windows
```

**Option B: Conan** (C++ Package Manager)
```
conan install . --build=missing
```

**Option C: Bundled (aktuell)**
- HIDAPI lokal gelagert
- nlohmann_json als Header-Only

### 3.2 Dependencies einrichten:

1. **HIDAPI**
   - Windows: Pre-built DLLs + LIBs
   - Linux: Über apt-get installieren
   - macOS: Über brew installieren

2. **nlohmann_json**
   - Header-only library
   - Keine Runtime-Abhängigkeit

3. **Qt5/Qt6** (optional für GUI)
   - Windows: Über Qt-Online-Installer
   - Linux: apt-get install qt5-default
   - macOS: brew install qt5

4. **Google Test** (für UI-Tests)
   ```
   vcpkg install gtest:x64-windows
   ```

---

## PHASE 4: GERÄTE-SUPPORT ERWEITERN

### 4.1 Neue Controller hinzufügen

Template für neuen Controller:

```cpp
// src/controllers/NewDeviceController.h
class NewDeviceController : public IController {
    // Interface implementieren
    // Hardware-spezifische Logik
};

// Controller registrieren in ControllerRegistry.cpp
REGISTER_CONTROLLER(NewDeviceController, VENDOR_ID, PRODUCT_ID);
```

### 4.2 Unterstützte Geräte:

**Phase 1 (aktuell):**
- ✅ ASUS Aura (Mainboard RGB)
- ✅ ASUS Aura (Laptop RGB)
- ✅ SteelSeries Rival 600 (Maus)
- ✅ E-Vision Keyboards

**Phase 2 (geplant):**
- Corsair iCUE Geräte
- Logitech G-Serie
- Razer RGB
- HyperX RGB

**Phase 3 (später):**
- NZXT Hue
- Lian-Li Bädeker-Beleuchtung
- Custom USB RGB

---

## PHASE 5: QUALITÄTSSICHERUNG

### 5.1 Testing-Strategie

**Unit Tests** (src/logic testen)
```
tests/unit/
├── test_ColorUtils.cpp      → RGB-Verarbeitung
├── test_DeviceManager.cpp   → Geräte-API
├── test_ProfileManager.cpp  → Profil-System
└── test_*.cpp
```

**Integration Tests** (Hardware-Interaktion)
```
tests/integration/
├── test_HardwareScanner.cpp
├── test_DeviceControl.cpp
└── test_ProfileLoading.cpp
```

**Build mit Tests:**
```bash
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build
ctest --build-config Release --output-on-failure
```

### 5.2 Code-Qualität

- **Clang-Format** für einheitliche Formatierung
- **Static Analysis** (cppcheck, clang-tidy)
- **Memory Leak Detection** (valgrind auf Linux)
- **Code Coverage** (gcov/lcov)

### 5.3 Automatisiertes Testing (CI/CD)

GitHub Actions Workflow:
```yaml
name: Build & Test
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      - uses: ilammy/msvc-dev-cmd@v1
      - run: cmake -B build -S .
      - run: cmake --build build
      - run: ctest --build-config Release
```

---

## PHASE 6: INSTALLER & DEPLOYMENT

### 6.1 Windows Installer (NSIS)

**Optionen:**
- Exe-Installer (NSIS)
- MSI (Windows Installer)
- Portable ZIP
- Microsoft Store

**Zu inkludieren:**
- oneclickrgb.exe (CLI)
- oneclickrgb_gui.exe (GUI)
- hidapi.dll, PawnIOLib.dll
- Beispiel-Profile
- Hilfedatei
- Deinstallations-Support

### 6.2 Portable Version

```
OneClickRGB_Portable_1.0.0/
├── oneclickrgb.exe
├── oneclickrgb_gui.exe
├── hidapi.dll
├── PawnIOLib.dll
├── README.txt
├── profiles/
└── config/default.json
```

### 6.3 Update-Mechanismus

- GitHub Releases als Download-Quelle
- Auto-Update-Check (optional)
- Version in Datei speichern

---

## PHASE 7: DOKUMENTATION

### 7.1 Benutzer-Dokumentation

**README.md**
- Kurzbeschreibung
- Quick Start
- Systeme anforderungen
- Screenshots

**INSTALLATION.md**
- Schritt-für-Schritt Installation
- Fehlerbehandlung
- Deinstallation

**USER_GUIDE.md**
- CLI-Befehle
- GUI-Walkthrough
- Profil-Erstellung
- Tipps & Tricks

### 7.2 Entwickler-Dokumentation

**DEVELOPER_GUIDE.md**
- Projekt-Setup
- Build-Anleitung
- Code-Struktur
- Debugging-Tipps

**API_REFERENCE.md**
- Namespace OneClickRGB::
- Öffentliche Klassen
- Methoden & Parameter
- Beispiele

**CONTRIBUTING.md**
- Branch-Strategie
- Pull Request Prozess
- Code-Standards
- Testing-Anforderungen

### 7.3 Technische Dokumentation

**SUPPORTED_DEVICES.md**
- Alle unterstützten Geräte
- VID/PID Mappings
- Bekannte Einschränkungen

**architecture/**
- System Design
- Geräte-Erkennung
- Profil-System
- Kommunikationsprotokoll

---

## PHASE 8: VERSIONIERUNG & RELEASES

### 8.1 Semantic Versioning
- Format: MAJOR.MINOR.PATCH-PRERELEASE+BUILD
- Beispiel: 1.0.0, 1.1.0-beta, 1.2.0-rc1

### 8.2 Release Process

1. **Feature Development**
   ```
   git checkout -b feature/new-controller
   ```

2. **Version Update**
   ```
   # Bearbeite VERSION.txt: 1.0.0 → 1.1.0
   ```

3. **CHANGELOG Update**
   ```
   # Dokumentiere Änderungen in CHANGELOG.md
   ```

4. **Build & Test**
   ```
   cmake -B build -DBUILD_TESTS=ON
   cmake --build build --config Release
   ctest --build-config Release
   ```

5. **Git Release**
   ```
   git tag -a v1.1.0 -m "Release 1.1.0"
   git push origin v1.1.0
   ```

6. **GitHub Release**
   - Erstelle Release auf GitHub
   - Upload: oneclickrgb_1.1.0_installer.exe
   - Upload: OneClickRGB_1.1.0_portable.zip
   - Upload: oneclickrgb_1.1.0_cli.exe

### 8.3 Versionshistorie (CHANGELOG.md)

```markdown
## [1.1.0] - 2026-03-20

### Added
- New Corsair Controller support
- Profile import/export
- GUI Theme selector

### Fixed
- ASUS detection on Windows 11
- Memory leak in device scanning

### Changed
- Improved error messages
```

---

## PHASE 9: BENUTZER-KONFIGURATION

### 9.1 Konfiguration (config/default_config.json)

```json
{
  "app_name": "OneClickRGB",
  "version": "1.0.0",
  "cli": {
    "show_logo": true,
    "colorize_output": true
  },
  "gui": {
    "remember_window_size": true,
    "start_minimized": false,
    "enable_tray_icon": true
  },
  "hardware": {
    "auto_scan_on_startup": true,
    "scan_interval_ms": 5000,
    "max_devices": 20
  },
  "profiles": {
    "auto_load_startup": true,
    "startup_profile": "Default"
  },
  "autostart": {
    "enabled": false,
    "profile": "Default"
  },
  "logging": {
    "level": "INFO",
    "file": "oneclickrgb.log"
  }
}
```

### 9.2 Geräte-Datenbank (config/device_database.json)

```json
{
  "devices": [
    {
      "name": "ASUS Aura LED Controller",
      "vendor_id": "0xb05",
      "product_id": "0x19af",
      "controller": "AsusAuraController",
      "features": ["color", "brightness", "effects"],
      "zones": ["Primary"]
    },
    // ... weitere Geräte
  ]
}
```

### 9.3 Beispiel-Profile

**Gaming.json**
```json
{
  "name": "Gaming",
  "description": "Aggressive RGB für Gaming",
  "devices": {
    "ASUS Aura": {
      "color": [255, 0, 0],
      "brightness": 100,
      "effect": "breathing"
    }
  }
}
```

---

## PHASE 10: PRODUKTIONSVORBEREITUNG - CHECKLISTE

### ✅ Code-Qualität
- [ ] Alle Unit Tests grün
- [ ] Alle Integration Tests grün
- [ ] Code Review durchgeführt
- [ ] Static Analysis bestanden
- [ ] No Memory Leaks (valgrind/AddressSanitizer)

### ✅ Dokumentation
- [ ] README vollständig
- [ ] Installation Guide komplett
- [ ] API-Dokumentation aktuell
- [ ] CHANGELOG erstellt
- [ ] Code-Kommentare überprüft

### ✅ Build & Deployment
- [ ] Windows EXE testet
- [ ] Windows GUI testet
- [ ] Installer testet
- [ ] Portable Version testet
- [ ] DLL-Abhängigkeiten korrekt

### ✅ Hardware-Kompatibilität
- [ ] Mit echten Geräten getestet
- [ ] Mit mehreren Geräten gleichzeitig getestet
- [ ] Edge-Cases getestet
- [ ] Fehlerbehandlung robust

### ✅ Performance
- [ ] Startup-Zeit < 2 Sekunden
- [ ] Speichernutzung < 50 MB
- [ ] CPU-Auslastung minimal
- [ ] Keine Freeze bei Hardware-Scan

### ✅ Sicherheit
- [ ] Input Validation
- [ ] Keine Hardcoded Secrets
- [ ] Safe File I/O
- [ ] Error Handling robust

### ✅ User Experience
- [ ] Intuitive CLI-Befehle
- [ ] Hilfreiche Fehlermeldungen
- [ ] Logo/Branding konsistent
- [ ] Performance wahrnehmbar schnell

---

## IMPLEMENTIERUNGS-ROADMAP

### **Sprint 1 (Woche 1-2):** Projektstruktur
- [ ] Ordner reorganisieren
- [ ] CMakeLists.txt modularisieren
- [ ] Dependencies in vcpkg/Conan migrieren
- [ ] Build-Skripte aktualisieren

### **Sprint 2 (Woche 2-3):** Feature vollends
- [ ] GUI (Qt) vollständig implementieren
- [ ] Neue Controller hinzufügen
- [ ] Profile-System erweitern

### **Sprint 3 (Woche 4-5):** Testing & QA
- [ ] Unit Tests schreiben
- [ ] Integration Tests schreiben
- [ ] CI/CD Setup GitHub Actions
- [ ] Code Coverage > 80%

### **Sprint 4 (Woche 5-6):** Installer & Docs
- [ ] Windows Installer (NSIS)
- [ ] Portable Version
- [ ] Dokumentation vollständig
- [ ] Video-Tutorials (optional)

### **Sprint 5 (Woche 6-7):** Release Prep
- [ ] Final Testing
- [ ] Performance Tuning
- [ ] Security Audit
- [ ] v1.0.0 Release

---

## TECHNISCHE ANFORDERUNGEN

### Build-Tools:
- Microsoft Visual Studio 2019+ oder Visual Studio Build Tools
- CMake 3.20+
- Git
- Python 3.8+ (für Skripte)

### Laufzeit-Anforderungen:
- Windows 10/11 (x64)
- .NET Framework 4.5+ (optional)
- VC++ Redistributable 2019

### Entwicklungs-Anforderungen:
- C++17 Compiler
- Qt5/Qt6 SDK (für GUI)
- Google Test (für Unit Tests)

---

## GESCHÄTZTER AUFWAND

| Phase | Aufwand | Dauer |
|-------|---------|-------|
| Projektstruktur | 4h | 1 Tag |
| Build-System | 6h | 1.5 Tage |
|GUI-Implementierung | 20h | 4 Tage |
| Neue Controller | 12h | 2-3 Tage |
| Testing-Framework | 16h | 2-3 Tage |
| Installer | 8h | 1.5 Tage |
| Dokumentation | 12h | 2 Tage |
| QA & Polish | 16h | 2-3 Tage |
| **TOTAL** | **94h** | **2-3 Wochen** |

---

## ERFOLGS-KRITERIEN

1. **Funktionalität**
   - [x] CLI funktioniert
   - [ ] GUI funktioniert
   - [ ] 6+ Geräte-Controller
   - [ ] Profile-System stabil

2. **Zuverlässigkeit**
   - [x] No crashes unter normaler Nutzung
   - [x] Fehlerbehandlung robust
   - [ ] Automatisierte Tests

3. **Performance**
   - [x] Startup < 2 Sekunden
   - [x] RAM < 50 MB
   - [x] CPU-friendly

4. **Usability**
   - [x] CLI intuitiva
   - [ ] GUI user-friendly
   - [x] Dokumentation klar

5. **Wartbarkeit**
   - [ ] Clean Code (Code Review bestanden)
   - [ ] Dokumentation vollständig
   - [ ] Build reproduzierbar
   - [ ] Tests > 80% Coverage

---

## NEXT STEPS

### Sofort (Diese Woche):
1. Diesen Plan mit dem Team durchgehen
2. Ordnerstruktur reorganisieren
3. CMakeLists.txt modularisieren
4. Git-Repository mit .gitignore aufsetzen

### Kurz (Nächste 2 Wochen):
1. GUI-Version abschließen
2. Build-System professionalisieren
3. Testing-Framework aufsetzen
4. Neue Controller hinzufügen

### Mittelfristig (1 Monat):
1. Installer erstellen
2. Komplette Dokumentation
3. Release vorbereiten
4. v1.0.0 publizieren

---

**Anhänge:**
- A: Datei-Templates
- B: Build-Konfigurationen
- C: Testing-Strategie
- D: Deployment-Checkliste
- E: Fehler-Codes & Meldungen

