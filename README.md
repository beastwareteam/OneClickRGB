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
- **No Dependencies** - Single executable, no runtime installation needed
- **Modern UI** - Dark theme with rounded buttons and hover effects

---

## Supported Devices

| Device | Status | Protocol |
|--------|--------|----------|
| **ASUS Aura Mainboard** | ✅ Working | USB HID (0x0B05:0x19AF) |
| **ASUS Aura Addressable** | ✅ Working | 8 channels, 60 LEDs each |
| **SteelSeries Rival 600** | ✅ Working | USB HID |
| **EVision Keyboard** | ✅ Working | Effects: Static, Breathing, Wave, Rainbow |
| **G.Skill Trident Z5 RGB** | ✅ Working | SMBus via PawnIO |
| **G.Skill Trident Z5 Neo** | ✅ Working | SMBus via PawnIO |

### Compatibility

- **OS**: Windows 10 (1809+), Windows 11
- **Architecture**: x64 only
- **Privileges**: Administrator recommended (required for some devices)

---

## Quick Start

### Option 1: Download Release

1. Download latest release from [Releases](https://github.com/beastwareteam/OneClickRGB/releases)
2. Extract to any folder
3. Run `OneClickRGB.exe`

### Option 2: Build from Source

```batch
git clone https://github.com/beastwareteam/OneClickRGB.git
cd OneClickRGB
build_native.bat
```

**Requirements**: Visual Studio 2019/2022 Build Tools (free)

See [BUILD.md](BUILD.md) for detailed instructions.

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

### System Tray

Right-click the tray icon for quick access to:
- Color presets
- Profiles
- Power controls (Standby, Shutdown, Restart)
- Settings

---

## Installation Files

For distribution, include these files:

```
OneClickRGB/
├── OneClickRGB.exe      # Main application (required)
├── hidapi.dll           # USB HID library (required)
├── PawnIOLib.dll        # RAM control (optional)
├── SmbusI801.bin        # SMBus module (optional, for RAM)
└── config/
    └── devices.json     # Device database (optional)
```

**Minimum**: Just `OneClickRGB.exe` + `hidapi.dll` (~400 KB total)

---

## Configuration

Settings are stored in `%APPDATA%\OneClickRGB\`:

| File | Description |
|------|-------------|
| `app_settings.cfg` | Window position, language, theme |
| `profiles/*.json` | Saved color profiles |

---

## Version History

### v3.4 (Current)
- Production-ready build system
- Fixed RAM control (relative paths)
- Repository cleanup
- One-click build script

### v3.3
- Global hotkeys
- Window position memory
- System tray improvements

### v3.2
- Standby/resume detection
- HID reset on wake
- Power menu in tray

### v3.1
- Modern dark UI
- Custom titlebar
- Channel configuration

See [ROADMAP.md](ROADMAP.md) for planned features.

---

## Troubleshooting

### "Device not found"
- Run as Administrator
- Check if device is connected
- Some devices need specific USB ports

### "RAM not detected"
- PawnIO driver required for G.Skill RAM
- Run as Administrator
- Check `PawnIOLib.dll` and `SmbusI801.bin` are present

### "Colors don't persist after sleep"
- Enable "Standby Recovery" in settings
- App must be running (system tray)

---

## Contributing

Contributions welcome! See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

To add a new device:
1. Add VID/PID to `config/devices.json`
2. Implement controller in `src/controllers/`
3. Test and submit PR

---

## License

MIT License - see [LICENSE](LICENSE)

---

## Credits

- **HIDAPI** - Cross-platform HID library
- **PawnIO** - SMBus access for RAM control
- **OpenRGB** - Protocol documentation
