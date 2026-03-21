# OneClickRGB

**Lightweight RGB controller for Windows** - Control all your RGB devices with one unified interface.

[![Build Status](https://github.com/beastwareteam/OneClickRGB/workflows/Build/badge.svg)](https://github.com/beastwareteam/OneClickRGB/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Windows](https://img.shields.io/badge/Platform-Windows%2010%2F11-blue.svg)]()

---

## Features

- **Multi-Device Support** - ASUS Aura, SteelSeries, EVision Keyboard, G.Skill RAM
- **One-Click Colors** - Set all devices to one color instantly
- **Profile System** - Save and load color configurations
- **System Tray** - Quick access to presets and power controls
- **Global Hotkeys** - Change colors without switching windows
- **Standby Recovery** - Automatic RGB restore after Windows sleep/resume
- **Themes** - Dark, Light, and Colorblind-friendly modes
- **Tooltips** - Hover over any control for help
- **Accessibility** - Full keyboard navigation (Tab, Enter, Arrow keys)
- **No Dependencies** - Single executable, no runtime installation needed
- **Modern UI** - GDI+ rendered with shadows and rounded corners

---

## Supported Devices

| Device | Status | Protocol |
|--------|--------|----------|
| **ASUS Aura Mainboard** | Working | USB HID (0x0B05:0x19AF) |
| **ASUS Aura Addressable** | Working | 8 channels, 60 LEDs each |
| **SteelSeries Rival 600** | Working | USB HID |
| **EVision Keyboard** | Working | Effects: Static, Breathing, Wave, Rainbow |
| **G.Skill Trident Z5 RGB** | Working | SMBus via PawnIO |
| **G.Skill Trident Z5 Neo** | Working | SMBus via PawnIO |

### Compatibility

- **OS**: Windows 10 (1809+), Windows 11
- **Architecture**: x64 only
- **Privileges**: Administrator recommended

---

## Installation

### Option 1: Portable Package

1. Download `OneClickRGB_v1.0_Portable.zip` from [Releases](https://github.com/beastwareteam/OneClickRGB/releases)
2. Extract to any folder
3. Run `install.bat` as Administrator (or just run `OneClickRGB.exe` directly)

### Option 2: Build from Source

```batch
git clone https://github.com/beastwareteam/OneClickRGB.git
cd OneClickRGB
build_native.bat
```

**Requirements**: Visual Studio 2019/2022 Build Tools

See [BUILD.md](BUILD.md) for detailed instructions.

---

## Portable Package Contents

```
OneClickRGB/
├── OneClickRGB.exe     165 KB   Main application
├── hidapi.dll          159 KB   USB HID library
├── PawnIOLib.dll         4 KB   SMBus interface (G.Skill RAM)
├── SmbusI801.bin        40 KB   Intel SMBus module
├── icon.png            193 KB   Application icon
├── PawnIO_setup.exe    3.1 MB   Driver installer (run once)
├── install.bat                  Automatic installation
├── install_manual.bat           Interactive installation
├── uninstall.bat                Clean removal
└── README.txt                   Quick reference
```

---

## Usage

### Global Hotkeys

| Hotkey | Action |
|--------|--------|
| `Ctrl+Alt+1` | Blue |
| `Ctrl+Alt+2` | Red |
| `Ctrl+Alt+3` | Green |
| `Ctrl+Alt+4` | White |
| `Ctrl+Alt+0` | Off (Black) |
| `Ctrl+Alt+Space` | Toggle On/Off |

### Keyboard Navigation

| Key | Action |
|-----|--------|
| `Tab` | Move between controls |
| `Enter` / `Space` | Activate button/checkbox |
| `Arrow Keys` | Adjust sliders |

### System Tray

Right-click the tray icon for quick access to:
- Color presets
- Profiles
- Power controls (Standby, Shutdown, Restart)
- Settings

---

## Themes

Switch between themes using the Theme button:

| Theme | Description |
|-------|-------------|
| **Dark** | Default dark mode with blue accent |
| **Light** | Bright mode with clean appearance |
| **Colorblind** | Warm cream tones, Orange/Blue palette |

---

## Configuration

Settings are stored in `%APPDATA%\OneClickRGB\`:

| File | Description |
|------|-------------|
| `app_settings.cfg` | Window position, language, theme |
| `profiles/*.json` | Saved color profiles |

---

## Version History

### v3.5 (Current)
- Info tooltips on all controls
- Theme system (Dark/Light/Colorblind)
- Keyboard accessibility
- Live RGB value display
- Portable package with installers
- Repository cleanup

### v3.4
- Production-ready build system
- Fixed RAM control (relative paths)
- One-click build script

### v3.3
- Global hotkeys
- Window position memory
- System tray improvements

### v3.2
- Standby/resume detection
- HID reset on wake
- Power menu in tray

See [ROADMAP.md](ROADMAP.md) for planned features.

---

## Troubleshooting

### "Device not found"
- Run as Administrator
- Check if device is connected
- Some devices need specific USB ports

### "RAM not detected"
- Run `PawnIO_setup.exe` once as Administrator
- Restart PC after driver installation
- Check `PawnIOLib.dll` and `SmbusI801.bin` are present

### "Colors don't persist after sleep"
- Enable "Autostart" in settings
- App must be running (system tray)

---

## Source Structure

```
src/
├── oneclick_rgb_complete.cpp   Main application (all-in-one)
├── themes.h                    Theme definitions
├── channel_config.h            Channel configuration
├── modern_ui.h                 UI components
└── OneClickRGB.ico/rc/res      Resources
```

---

## Contributing

Contributions welcome! See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

---

## License

MIT License - see [LICENSE](LICENSE)

---

## Credits

- **HIDAPI** - Cross-platform HID library
- **PawnIO** - SMBus access for RAM control
- **OpenRGB** - Protocol documentation
