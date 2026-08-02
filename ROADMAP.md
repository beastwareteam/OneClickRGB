# OneClickRGB Roadmap

## Current Version: 3.6

### Completed Features ✅

- [x] Native Win32 GUI (no Qt dependency)
- [x] ASUS Aura Mainboard control (8 channels, 60 LEDs each)
- [x] SteelSeries mouse support
- [x] EVision keyboard with effects (Static, Breathing, Wave, Rainbow, etc.)
- [x] G.Skill RAM RGB control via SMBus
- [x] Profile save/load system
- [x] System tray with quick color presets
- [x] Global hotkeys (Ctrl+Alt+1-4, 0, Space)
- [x] Standby/resume detection with auto-reset
- [x] Power menu (Standby, Shutdown, Restart)
- [x] Bilingual UI (EN/DE)
- [x] Dark theme
- [x] One-click build system
- [x] Bundled dependencies (no installation needed)
- [x] Modern UI with rounded buttons (GDI+ rendered)
- [x] Button hover effects with mouse tracking
- [x] Accent styling for primary actions

- [x] Installer - Inno Setup based installer with uninstall support
- [x] Autostart toggle in settings (scheduled task, runs elevated)
- [x] Theme system (Dark/Light/Colorblind) and keyboard accessibility
- [x] Hardware abstraction layer + unit test suite
- [x] CMake build and working CI

---

## Next Release: v3.7

### Planned Features

- [ ] **Tray icon tooltip** - Show current color/profile
- [ ] **Custom hotkey configuration** - User-definable shortcuts
- [ ] **Split the Win32 front end** - `oneclick_rgb_complete.cpp` is still
      ~3000 lines; the window procedure and the dialogs should follow the
      device protocols out of it

### Bug Fixes

- [ ] Better error messages when devices not found
- [ ] USB reconnection handling (currently only handled on resume from standby)

---

## Future Versions

### Hardware Support (v4.0)
- [ ] Corsair devices (iCUE protocol)
- [ ] Razer devices (Chroma SDK)
- [ ] NZXT devices (HUE 2, Kraken)
- [ ] Gigabyte RGB Fusion 2.0
- [ ] MSI Mystic Light
- [ ] More RAM vendors (Corsair, Kingston)

### Advanced Effects (v4.1)
- [ ] Audio-reactive mode
- [ ] Screen color sync (ambient light)
- [ ] Temperature-based colors
- [ ] Custom effect editor

### Platform Support (v5.0)
- [ ] Linux support (libusb)
- [ ] macOS support (IOKit)
- [ ] CLI-only mode

---

## Device Compatibility

| Device | Status | VID:PID | Notes |
|--------|--------|---------|-------|
| **ASUS Aura Mainboard** | ✅ Working | 0B05:19AF | OpenRGB protocol |
| **ASUS Aura Addressable** | ✅ Working | 0B05:19AF | 8 channels |
| **SteelSeries Rival 600** | ✅ Working | 1038:1724 | Full RGB |
| **EVision Keyboard** | ✅ Working | 3299:4E9F | All effects |
| **G.Skill Trident Z5** | ✅ Working | SMBus | Via PawnIO |
| **G.Skill Trident Z5 Neo** | ✅ Working | SMBus | Via PawnIO |
| Corsair RAM | ❌ Planned | - | iCUE protocol |
| Razer devices | ❌ Planned | - | Chroma SDK |
| NZXT HUE 2 | ❌ Planned | 1E71:2006 | USB HID |

---

## System Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| OS | Windows 10 1809 | Windows 11 |
| Architecture | x64 | x64 |
| RAM | 50 MB | 100 MB |
| Disk | 5 MB | 10 MB |
| Privileges | User | Administrator |

---

## Contributing

Help wanted for:

1. **Hardware Testing** - Test on your RGB devices
2. **Protocol Research** - Reverse-engineer new devices
3. **Code** - Bug fixes and new features
4. **Documentation** - Improve guides and translations

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for guidelines.

---

## Version History

### v3.6 (Current)
- Fix: autostart runs elevated via scheduled task (RAM color at logon)
- Fix: Windows key no longer left locked, with read-back verification
- Fix: --dry-run no longer touches hardware
- Device protocols extracted behind a hardware abstraction layer
- Unit test suite (88 tests), CMake build, repaired CI

See [CHANGELOG.md](CHANGELOG.md) for details.

### v3.5
- Info tooltips, theme system, keyboard accessibility
- Portable package with installers

### v3.4
- Production-ready build system
- Fixed RAM control paths
- Repository cleanup
- One-click build script

### v3.3
- Global hotkeys
- Window position memory

### v3.2
- Standby/resume detection
- HID reset on wake

### v3.1
- Modern dark UI
- Custom titlebar

### v3.0
- GDI+ graphics rewrite
- Channel configuration

### v2.0
- Multi-device support
- Profile system

### v1.0
- Initial release
