# OneClickRGB Roadmap

## Current Version: 3.1

### What's Working
- [x] Custom frameless window with dark titlebar
- [x] ASUS Aura Mainboard control (OpenRGB protocol)
- [x] SteelSeries mouse support
- [x] EVision keyboard with effects (Static, Breathing, Wave, Rainbow, etc.)
- [x] G.Skill RAM RGB control
- [x] Edge LED modes
- [x] Profile save/load
- [x] System tray with quick color presets
- [x] HID reset on startup and after standby/resume
- [x] Power menu (Standby, Shutdown, Restart)
- [x] Bilingual UI (EN/DE)
- [x] Dark/Light theme toggle

---

## Phase 1: Bug Fixes (Priority: High)

### UI Issues
- [ ] **Titlebar button hover effects** - Implement WM_MOUSEMOVE tracking for proper hover state
- [ ] **Window resize edge detection** - Fix WM_NCHITTEST for all edges
- [ ] **Control responsiveness** - Verify all buttons and sliders respond to clicks

### Stability
- [ ] **Error handling for missing devices** - Graceful degradation when hardware not found
- [ ] **HID device reconnection** - Auto-reconnect after USB replug

---

## Phase 2: Quick Wins (1-2 hours each)

### Hotkey Support
- [x] Global hotkeys for preset colors (Ctrl+Alt+1-4, 0)
- [x] Hotkey to toggle all LEDs on/off (Ctrl+Alt+Space)
- [ ] Custom hotkey configuration UI

### Profile System
- [ ] Export profiles to JSON file
- [ ] Import profiles from file
- [ ] Profile sharing format specification

### UX Improvements
- [x] Save/restore window position
- [x] Remember last used profile on startup
- [ ] Tray icon tooltip showing current color
- [ ] Minimize on startup option

---

## Phase 3: New Features (Medium Effort)

### Hardware Support
- [ ] **Corsair devices** - iCUE protocol research
- [ ] **Razer devices** - Chroma SDK integration
- [ ] **NZXT devices** - CAM protocol
- [ ] **Gigabyte RGB Fusion** - USB HID protocol
- [ ] **MSI Mystic Light** - USB HID protocol

### Advanced Effects
- [ ] Color breathing with custom speed
- [ ] Rainbow wave direction control
- [ ] Audio-reactive mode (beat detection)
- [ ] Screen color sync (ambient light)
- [ ] Temperature-based colors (CPU/GPU temp)

### Sync & Automation
- [ ] Sync all devices to same color
- [ ] Time-based color schedules
- [ ] Application-specific profiles (auto-switch when game starts)
- [ ] Multi-monitor ambient sync

---

## Phase 4: Architecture (Larger Effort)

### Code Quality
- [ ] Modularize device drivers into separate DLLs
- [ ] Implement proper logging system
- [ ] Add unit tests for protocol handlers
- [ ] Configuration file instead of registry

### Platform
- [ ] Linux support (libusb)
- [ ] macOS support (IOKit)
- [ ] CLI-only mode for headless systems

### Community
- [ ] Plugin API for third-party device support
- [ ] Web-based remote control
- [ ] Mobile app companion

---

## Known Hardware Compatibility

| Device | Status | Notes |
|--------|--------|-------|
| ASUS Aura Mainboard | ✅ Working | OpenRGB protocol, Direct Mode |
| ASUS Aura GPU | ⚠️ Untested | Should work with same protocol |
| SteelSeries Rival 3 | ✅ Working | Full color control |
| EVision Keyboard | ✅ Working | All effects supported |
| G.Skill Trident Z RGB | ✅ Working | Per-module color |
| Corsair RAM | ❌ Not yet | Needs iCUE protocol |
| Razer devices | ❌ Not yet | Needs Chroma SDK |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to help with:
- Testing on your hardware
- Reverse-engineering new device protocols
- Code contributions
- Documentation

---

## Version History

### v3.1 (Current)
- Custom titlebar with dark theme
- HID reset on startup/resume
- Power menu in tray
- Slider debouncing
- ASUS Aura OpenRGB protocol fix

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
