# OneClickRGB Documentation Index

## User Documentation

| Document | Description |
|----------|-------------|
| [README.md](../README.md) | Quick start and feature overview |
| [BUILD.md](../BUILD.md) | Building from source |
| [GUIDE.md](GUIDE.md) | User guide and tips |
| [CHANGELOG.md](../CHANGELOG.md) | Release history and fixed defects |

## Technical Documentation

| Document | Description |
|----------|-------------|
| [GSkill_Trident_Z5_RGB_DDR5_Protocol.md](GSkill_Trident_Z5_RGB_DDR5_Protocol.md) | G.Skill RAM SMBus protocol |
| [SMBus_Implementation_Guide.md](SMBus_Implementation_Guide.md) | SMBus driver implementation |
| [TESTING.md](TESTING.md) | Test suite: layout, how to run, what is covered |

## Source Code Reference

### Main Application

| File | Description |
|------|-------------|
| `src/oneclick_rgb_complete.cpp` | Win32 front end: window, tray, hotkeys, dialogs |
| `src/hal/` | Hardware abstraction (HID, SMBus) - real, dry-run and fake backends |
| `src/devices/` | Device protocols, testable without hardware |
| `src/autostart.cpp` | Elevated scheduled task at logon |
| `src/profile.cpp` | Color profile serialization and storage |
| `src/app_config.h` | Unified settings (`config.json`) |
| `src/themes.h` | Theme system (Dark/Light/Colorblind) |
| `src/channel_config.h` | Per-channel color correction |
| `src/modern_ui.h` | Custom UI components |
| `tests/` | Unit tests + fake backends |

### Dependencies

| Component | Location | Purpose |
|-----------|----------|---------|
| HIDAPI | `dependencies/hidapi/` | USB HID communication |
| PawnIO | `dependencies/PawnIO/` | SMBus access for RAM |

## Application Structure

```
OneClickRGB Window
├── Group 1: Color
│   ├── Color Preview (live RGB display)
│   ├── Hex Input (#RRGGBB)
│   ├── Pick Button (color dialog)
│   ├── Theme/Language buttons
│   ├── 7 Preset Buttons (Blue, Red, Green, etc.)
│   └── RGB Sliders (0-255 with live values)
│
├── Group 2: Effects
│   ├── Keyboard Mode (Static, Breathing, Wave, etc.)
│   ├── Edge Mode (for laptop keyboards)
│   ├── Brightness Slider (0-100%)
│   └── Speed Slider (animation timing)
│
├── Group 3: Devices
│   ├── Device Checkboxes (ASUS, Keyboard, RAM, etc.)
│   ├── Channel Settings Button
│   └── ASUS Test Button
│
├── Group 4: Profiles
│   ├── Profile Dropdown
│   ├── Save/Load Buttons
│   └── Settings Checkboxes (Autostart, Tray, Live)
│
├── Apply Button (sends colors to hardware)
│
└── Status Log (scrollable, Tab-accessible)
```

## Theme System

### Available Themes

| Theme | ID | Background | Accent |
|-------|------|------------|--------|
| Dark | 0 | Dark blue-gray gradient | Blue |
| Light | 1 | Light gray-white | Blue |
| Colorblind | 2 | Warm cream/beige | Orange |

### Theme Properties

Each theme defines 30+ colors for:
- Window background (gradient)
- Control backgrounds (button, hover, pressed)
- Accent colors (apply button, checked items)
- Text colors (primary, secondary, on-accent)
- Borders (normal, hover, focus)
- Status log colors
- Group box colors
- Checkbox colors
- Slider channel colors (R/G/B or colorblind alternatives)

## Keyboard Accessibility

| Control | Tab Stop | Keys |
|---------|----------|------|
| RGB Sliders | Yes | Arrow Left/Right |
| Combos | Yes | Arrow Up/Down |
| Buttons | Yes | Enter, Space |
| Checkboxes | Yes | Space to toggle |
| Status Log | Yes | Arrow Up/Down to scroll |

## Tooltips

All controls show descriptive tooltips on hover:
- Balloon style with 300px max width
- Multi-line support (use \n in strings)
- 400ms initial delay
- 15 second display time
- Localized (EN/DE)

## Settings Storage

Location: `%APPDATA%\OneClickRGB\`

| File | Format | Contents |
|------|--------|----------|
| `config.json` | JSON | Colors, devices, theme, language, window position, per-zone correction |
| `profiles/*.rgb` | Key=Value | Saved color configurations |
| `asus_hw_config.bin` | Binary | Cached ASUS channel layout |

`app_settings.cfg` and `channels.cfg` from earlier versions are migrated into
`config.json` on first start (see `RGBConfig::MigrateLegacy`).

## Hardware Communication

### USB HID (ASUS, Keyboard)
- Uses HIDAPI library
- Direct USB communication
- No driver installation needed

### SMBus (G.Skill RAM)
- Requires PawnIO driver **and administrator rights**
- Intel SMBus controller (I801)
- Probed addresses: 0x70-0x77
- The driver is opened with a short retry, because at logon the PawnIO service
  is often still starting

## Screenshots

See `docs/screenshots/` for application screenshots.
