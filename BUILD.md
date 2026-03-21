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

Open **x64 Native Tools Command Prompt** (from Start Menu -> Visual Studio):

```batch
cd OneClickRGB

cl /nologo /EHsc /MD /O2 /std:c++17 /DUNICODE /D_UNICODE ^
   /I"src" /I"dependencies/hidapi" ^
   src/oneclick_rgb_complete.cpp ^
   /Fe"build/OneClickRGB.exe" ^
   /link /LIBPATH:"dependencies/hidapi" ^
   hidapi.lib shell32.lib comctl32.lib user32.lib gdi32.lib ^
   comdlg32.lib advapi32.lib gdiplus.lib wtsapi32.lib

copy dependencies\hidapi\hidapi.dll build\
copy dependencies\PawnIO\PawnIOLib.dll build\
copy dependencies\PawnIO\modules\SmbusI801.bin build\
copy src\icon.png build\
```

---

## Build Output

After successful build:

```
build/
├── OneClickRGB.exe      Main application (~170 KB)
├── hidapi.dll           USB HID library (~160 KB)
├── PawnIOLib.dll        RAM control (~4 KB)
├── SmbusI801.bin        SMBus module (~40 KB)
└── icon.png             Application icon
```

---

## Project Structure

```
OneClickRGB/
├── src/
│   ├── oneclick_rgb_complete.cpp   Main application
│   ├── themes.h                    Theme definitions
│   ├── channel_config.h            Channel config
│   ├── modern_ui.h                 UI components
│   ├── OneClickRGB.ico             Icon resource
│   └── icon.png                    PNG icon for tray
├── dependencies/
│   ├── hidapi/                     USB HID (bundled)
│   └── PawnIO/                     SMBus access (bundled)
├── portable/                       Distribution package
├── docs/                           Documentation
├── installer/
│   └── OneClickRGB.iss             Inno Setup script
└── build_native.bat                Build script
```

---

## Creating Distribution Package

### Using install scripts

The `portable/` folder contains ready-to-distribute files:

```
portable/
├── OneClickRGB.exe
├── hidapi.dll
├── PawnIOLib.dll
├── SmbusI801.bin
├── icon.png
├── PawnIO_setup.exe
├── install.bat           Auto-install
├── install_manual.bat    Interactive install
├── uninstall.bat         Clean removal
└── README.txt
```

### Creating ZIP

```batch
powershell Compress-Archive -Path portable\* -DestinationPath OneClickRGB_v1.0_Portable.zip
```

---

## Troubleshooting

### "cl is not recognized"
Run from **x64 Native Tools Command Prompt**, not regular cmd

### "Cannot find hidapi.lib"
Check `dependencies/hidapi/hidapi.lib` exists

### "LNK2019: unresolved external"
Make sure all libraries are linked (see manual build command)

### Build works but app crashes
Ensure `hidapi.dll` is next to the exe

---

## Compiler Warnings

The following warnings are expected and harmless:

- `C4005: UNICODE macro redefinition` - Intentional, ensures Unicode build
- `C4996: sprintf unsafe` - Legacy code, works correctly
- `C4244: conversion from LRESULT to int` - Windows API quirk

---

## Development

### Source Files

| File | Description |
|------|-------------|
| `oneclick_rgb_complete.cpp` | Main application, all features |
| `themes.h` | Dark/Light/Colorblind themes |
| `channel_config.h` | Per-channel color correction |
| `modern_ui.h` | Custom UI components |

### Building Debug Version

```batch
cl /nologo /EHsc /MDd /Od /Zi /std:c++17 /DUNICODE /D_UNICODE ^
   ... (same as release, add /Zi for debug symbols)
```
