# OneClickRGB

Lightweight RGB controller for Windows. Control multiple RGB devices with one unified interface.

---

## Features

- **Multi-Device Support** - ASUS Aura, SteelSeries, EVision Keyboard, G.Skill RAM
- **Unified Control** - One app for all your RGB devices
- **Profile System** - Save and load color configurations
- **System Tray** - Quick access to presets and power controls
- **Global Hotkeys** - Change colors without switching windows
- **Standby Recovery** - Automatic HID reset after Windows resume
- **Bilingual UI** - English and German interface
- **Dark Theme** - Modern dark interface with light theme option

---

## Supported Devices

| Device | Status | Notes |
|--------|--------|-------|
| ASUS Aura Mainboard | Working | OpenRGB protocol, Direct Mode |
| ASUS Aura GPU | Untested | Should work with same protocol |
| SteelSeries Mouse | Working | Full color control |
| EVision Keyboard | Working | Static, Breathing, Wave, Rainbow effects |
| G.Skill Trident Z RGB | Working | Per-module color control |

---

## Quick Start

### Download

Get the latest release from the [Releases](https://github.com/anthropics/RGB/releases) page.

### Run

1. Extract `OneClickRGB.exe` and `hidapi.dll` to the same folder
2. Run `OneClickRGB.exe` as Administrator (required for HID access)
3. Select your devices and set colors

### Global Hotkeys

| Hotkey | Action |
|--------|--------|
| Ctrl+Alt+1 | Blue |
| Ctrl+Alt+2 | Red |
| Ctrl+Alt+3 | Green |
| Ctrl+Alt+4 | White |
| Ctrl+Alt+0 | Off (Black) |
| Ctrl+Alt+Space | Toggle LEDs On/Off |

---

## Building from Source

### Requirements

- **Visual Studio 2019 or 2022** with C++ Desktop Development workload
  - Or just [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- No other dependencies needed (HIDAPI is bundled)

### Quick Build (Recommended)

```batch
git clone https://github.com/anthropics/OneClickRGB.git
cd OneClickRGB
build_native.bat
```

The executable will be in `build\OneClickRGB.exe`.

### Manual Build

```batch
# Open "x64 Native Tools Command Prompt for VS 2022"
cd OneClickRGB

cl /nologo /EHsc /MD /O2 /std:c++17 /DUNICODE /D_UNICODE ^
   /I"src" /I"dependencies\hidapi" ^
   src\oneclick_gui.cpp ^
   /Fe"build\OneClickRGB.exe" ^
   /link /LIBPATH:"dependencies\hidapi" ^
   hidapi.lib shell32.lib comctl32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib setupapi.lib

copy dependencies\hidapi\hidapi.dll build\
```

### Project Structure

```
OneClickRGB/
├── src/
│   ├── oneclick_rgb_complete.cpp  # Full GUI (native Win32, all features)
│   ├── oneclick_gui.cpp           # Simplified GUI
│   └── oneclick_rgb.cpp           # CLI version
├── dependencies/
│   ├── hidapi/                    # USB HID library (bundled)
│   └── PawnIO/                    # SMBus driver for RAM (optional)
├── config/
│   └── devices.json               # Device database
└── build/                         # Output directory
```

---

## Configuration

Settings are stored in `%APPDATA%\OneClickRGB\`:

- `app_settings.cfg` - Window position, language, theme, last profile
- `profiles/` - Saved color profiles

---

## Version History

### v3.1 (Current)
- Custom titlebar with dark theme
- HID reset on startup/resume
- Power menu in tray (Standby, Shutdown, Restart)
- Global hotkeys for color presets
- Window position saving
- Slider debouncing for smooth control

### v3.0
- Complete rewrite with GDI+ graphics
- Modern dark UI
- Channel configuration system

### v2.0
- Multi-device support
- Profile system
- System tray integration

### v1.0
- Initial release
- Basic ASUS Aura control

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for planned features and known issues.

---

## License

MIT License

---

## Credits

- **HIDAPI** - USB HID device communication
- **OpenRGB** - Protocol documentation and inspiration

