# OneClickRGB - Build Instructions

## Prerequisites

### Required
- **CMake** 3.16 or higher ([Download](https://cmake.org/download/))
- **Visual Studio 2019/2022** with C++ Desktop Development workload
  - Or **Visual Studio Build Tools** with MSVC compiler

### Optional (for GUI)
- **Qt 5.15+** or **Qt 6.x** ([Download](https://www.qt.io/download-qt-installer))
  - Components needed: `Qt Core`, `Qt Widgets`

### Bundled Dependencies (no installation required)
- HIDAPI (in `dependencies/hidapi/`)
- nlohmann/json (in `dependencies/nlohmann/`)
- PawnIO (in `dependencies/PawnIO/`)

## Quick Build

### Using Build Script (Recommended)
```batch
cd OneClickRGB
scripts\build.bat
```

### Manual Build
```batch
cd OneClickRGB
mkdir build
cd build

# Configure (adjust Qt path as needed)
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\msvc2019_64" ^
    -DBUILD_GUI=ON

# Build
cmake --build . --config Release

# Copy dependencies
copy ..\dependencies\hidapi\hidapi.dll Release\
copy ..\dependencies\PawnIO\PawnIOLib.dll Release\
mkdir Release\modules
copy ..\dependencies\PawnIO\modules\*.bin Release\modules\
mkdir Release\config
copy ..\config\*.json Release\config\
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_GUI` | ON | Build Qt GUI application |
| `BUILD_CLI` | ON | Build command-line interface |
| `USE_OPENRGB_CONTROLLERS` | OFF | Use OpenRGB controller implementations |

## Output Files

After building, the `build/Release/` folder contains:
```
Release/
├── OneClickRGB.exe      # Main GUI application
├── oneclickrgb.exe      # CLI tool
├── hidapi.dll           # HID device communication
├── PawnIOLib.dll        # SMBus/RAM control
├── Qt6Core.dll          # Qt runtime (after windeployqt)
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── platforms/           # Qt platform plugins
├── modules/
│   └── SmbusI801.bin    # SMBus driver module
└── config/
    └── devices.json     # Device database
```

## Creating an Installer

### Using Inno Setup
1. Install [Inno Setup](https://jrsoftware.org/isinfo.php)
2. Open `installer/OneClickRGB.iss`
3. Adjust paths if needed
4. Compile (Ctrl+F9)
5. Find installer in `dist/OneClickRGB_Setup_x.x.x.exe`

### Manual Packaging
```batch
# Run windeployqt to gather Qt dependencies
windeployqt --release --no-translations build\Release\OneClickRGB.exe

# Create a zip of the Release folder
```

## Troubleshooting

### "Qt not found"
Set the `CMAKE_PREFIX_PATH` environment variable:
```batch
set CMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
```

### "HIDAPI not found"
The bundled HIDAPI should work automatically. If not:
1. Check that `dependencies/hidapi/hidapi.dll` exists
2. Copy it to your `build/Release/` folder

### "PawnIO driver not found" (at runtime)
PawnIO is only needed for G.Skill RAM control. The app will:
1. First try to load `PawnIOLib.dll` from the app directory
2. Offer to download and install PawnIO driver if needed

### Build fails with "cannot find -lQt6..."
Ensure Qt is properly installed and `CMAKE_PREFIX_PATH` is set correctly.

## Development

### Adding New Device Support
1. Add device to `config/devices.json`
2. Create controller in `src/controllers/`
3. Register in `src/core/ControllerFactory.cpp`

### Project Structure
```
OneClickRGB/
├── src/
│   ├── core/           # Core library (DeviceManager, Profiles)
│   ├── controllers/    # Device-specific controllers
│   ├── scanner/        # Hardware detection
│   ├── ui/             # Qt GUI components
│   └── modules/        # Plugin modules
├── config/             # Device database, profiles
├── dependencies/       # Bundled third-party libs
├── resources/          # Icons, Qt resources
├── installer/          # Inno Setup script
└── scripts/            # Build scripts
```
