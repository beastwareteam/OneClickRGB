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
build_app.bat
```

**Requirements**: Visual Studio 2019/2022 Build Tools

`build_native.bat` still works and builds without CMake.
See [BUILD.md](BUILD.md) for detailed instructions.

### Running the tests

```batch
run_tests.bat
```

The tests use fake HID and SMBus backends, so no RGB hardware and no
administrator rights are required.

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
| `config.json` | Colors, devices, window position, language, theme, per-zone correction |
| `profiles/*.rgb` | Saved color profiles |
| `asus_hw_config.bin` | Cached ASUS channel layout (re-scanned when the board changes) |

Settings from older versions (`app_settings.cfg`, `channels.cfg`) are migrated
into `config.json` automatically on first start.

---

## Version History

### v3.6 (Current)
- **Fix: Autostart now runs as administrator** via a scheduled task, so the RAM
  color is applied at logon. A `Run` key entry can never start an elevated app.
- **Fix: Windows key is no longer left locked** after applying a profile, and
  the result is verified by reading the register back
- Fix: `--dry-run` no longer touches hardware on startup
- Fix: RAM retries briefly while the PawnIO service is still starting
- Device protocols split into testable modules behind a hardware abstraction
- Unit test suite (88 tests), CMake build, working CI

### v3.5
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
- The status log names the exact cause (driver not running, DLL missing, ...)

### "RAM color is not applied after logging in"
- Enable **Autostart** in the application. This registers a scheduled task that
  runs with administrator rights - required for the SMBus driver.
- A shortcut in the Startup folder will *not* work: Windows refuses to
  auto-elevate programs started that way.
- Verify with: `schtasks /Query /TN "OneClickRGB Autostart"`

### "Windows key does not work"
- Apply any color once; the keyboard's key lock is cleared on every apply.
- The status log reports whether the keyboard confirmed the unlock.

### "Colors don't persist after sleep"
- Enable "Autostart" in settings
- App must be running (system tray)

---

## Source Structure

```
src/
├── oneclick_rgb_complete.cpp   Win32 front end (window, tray, hotkeys)
├── hal/                        Hardware abstraction
│   ├── hid_backend.h             USB HID interface
│   ├── hid_backend_hidapi.*      Real backend (hidapi)
│   ├── hid_backend_dryrun.h      Logging backend for --dry-run
│   ├── smbus_backend.h           SMBus interface
│   └── smbus_backend_pawnio.*    Real backend (PawnIO)
├── devices/                    Device protocols
│   ├── asus_aura.*               ASUS Aura mainboard
│   ├── evision.*                 EVision keyboard + edge zone
│   ├── gskill_ram.*              G.Skill Trident Z5 over SMBus
│   └── steelseries.*             SteelSeries Rival 600
├── autostart.*                 Scheduled task at logon (elevated)
├── profile.*                   Colour profile storage
├── app_config.h                Unified settings (config.json)
├── channel_config.h            Per-zone colour correction
├── themes.h                    Theme definitions
├── modern_ui.h                 UI components
└── OneClickRGB.ico/rc          Resources

tests/                          Unit tests (fake backends, no hardware)
```

The device protocols talk to the interfaces in `src/hal` rather than to hidapi
or PawnIO directly. That is what lets `tests/` assert the exact bytes each
protocol puts on the wire without any hardware attached.

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
