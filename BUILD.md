# Building OneClickRGB

## Quick Build (Recommended)

```batch
git clone https://github.com/beastwareteam/OneClickRGB.git
cd OneClickRGB
build_native.bat
```

Output: `build\OneClickRGB.exe`

---

## Requirements

### Minimum
- **Windows 10/11** (x64)
- **Visual Studio Build Tools 2019 or 2022**
  - Download: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
  - Select: "Desktop development with C++"

### All Dependencies Bundled
No manual installation needed:
- HIDAPI (`dependencies/hidapi/`)
- PawnIO (`dependencies/PawnIO/`)
- nlohmann/json (`dependencies/nlohmann/`)

---

## Build Options

### Option 1: Automated Script (Recommended)

```batch
build_native.bat
```

This script:
1. Finds Visual Studio automatically (2019 or 2022)
2. Compiles `oneclick_rgb_complete.cpp`
3. Copies all dependencies to `build/`

### Option 2: Manual Build

Open **x64 Native Tools Command Prompt** (from Start Menu → Visual Studio):

```batch
cd OneClickRGB

cl /nologo /EHsc /MD /O2 /std:c++17 /DUNICODE /D_UNICODE ^
   /I"src" /I"dependencies/hidapi" /I"dependencies" ^
   src/oneclick_rgb_complete.cpp ^
   /Fe"build/OneClickRGB.exe" ^
   /link /LIBPATH:"dependencies/hidapi" ^
   hidapi.lib shell32.lib comctl32.lib user32.lib gdi32.lib ^
   comdlg32.lib advapi32.lib setupapi.lib gdiplus.lib ^
   dwmapi.lib uxtheme.lib wtsapi32.lib powrprof.lib

copy dependencies\hidapi\hidapi.dll build\
copy dependencies\PawnIO\PawnIOLib.dll build\
copy dependencies\PawnIO\modules\SmbusI801.bin build\
```

### Option 3: CMake (for IDE integration)

```batch
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## Build Output

After successful build:

```
build/
├── OneClickRGB.exe      # Main application (~240 KB)
├── hidapi.dll           # USB HID library (~160 KB)
├── PawnIOLib.dll        # RAM control (~4 KB)
├── SmbusI801.bin        # SMBus module (~40 KB)
├── config/
│   └── devices.json     # Device database
└── modules/
    ├── SmbusI801.bin
    ├── SmbusNCT6793.bin
    └── SmbusPIIX4.bin
```

---

## Project Structure

```
OneClickRGB/
├── src/
│   ├── oneclick_rgb_complete.cpp   # Main GUI (all features)
│   ├── oneclick_gui.cpp            # Simplified version
│   ├── oneclick_rgb.cpp            # CLI version
│   ├── core/                       # Core library
│   ├── controllers/                # Device controllers
│   ├── scanner/                    # Hardware detection
│   └── ui/                         # Qt UI components
├── dependencies/
│   ├── hidapi/                     # USB HID (bundled)
│   ├── PawnIO/                     # SMBus access (bundled)
│   └── nlohmann/                   # JSON library (bundled)
├── config/
│   ├── devices.json                # Device database
│   └── controller_database.json    # Controller mappings
├── installer/
│   └── OneClickRGB.iss             # Inno Setup script
└── build_native.bat                # Build script
```

---

## Creating an Installer

### Using Inno Setup

1. Install [Inno Setup 6](https://jrsoftware.org/isinfo.php)
2. Open `installer/OneClickRGB.iss`
3. Press Ctrl+F9 to compile
4. Output: `dist/OneClickRGB_Setup_x.x.x.exe`

### Manual ZIP Distribution

```batch
mkdir OneClickRGB_v3.4
copy build\OneClickRGB.exe OneClickRGB_v3.4\
copy build\hidapi.dll OneClickRGB_v3.4\
copy build\PawnIOLib.dll OneClickRGB_v3.4\
copy build\SmbusI801.bin OneClickRGB_v3.4\
```

---

## Troubleshooting

### "cl is not recognized"
→ Run from **x64 Native Tools Command Prompt**, not regular cmd

### "Cannot find hidapi.lib"
→ Check `dependencies/hidapi/hidapi.lib` exists

### "LNK2019: unresolved external"
→ Make sure all libraries are linked (see manual build command)

### Build works but app crashes
→ Ensure `hidapi.dll` is next to the exe

---

## Compiler Warnings

The following warnings are expected and harmless:

- `C4005: UNICODE macro redefinition` - Intentional, ensures Unicode build
- `C4996: sprintf unsafe` - Legacy code, works correctly
- `C4244: conversion from LRESULT to int` - Windows API quirk

---

## Development

### Adding a New Device

1. Add VID/PID to `config/devices.json`:
```json
{
  "vendorId": "0x1234",
  "productId": "0x5678",
  "name": "My Device",
  "controller": "MyDeviceController"
}
```

2. Create controller in `src/controllers/MyDeviceController.cpp`

3. Register in device detection code

### Building Debug Version

```batch
cl /nologo /EHsc /MDd /Od /Zi /std:c++17 /DUNICODE /D_UNICODE /DDEBUG ^
   ... (same as release)
```
