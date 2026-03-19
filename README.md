# OneClickRGB

**Simple One-Click RGB Control** - A lightweight alternative to complex RGB software.

## Philosophy

Unlike traditional RGB software that loads hundreds of drivers at startup, OneClickRGB uses a **Hardware-First** approach:

1. **Scan first** - Only enumerate connected USB/HID devices
2. **Match against database** - Check if VID/PID is in our controller database
3. **Lazy load** - Only initialize controllers for devices that are actually connected

This results in:
- **Faster startup** - No unnecessary driver loading
- **Lower memory usage** - Only active devices are initialized
- **Simpler codebase** - Focused on what matters

## Features

- **One-Click Color** - Set all RGB devices to one color instantly
- **Profile System** - Save and load device configurations
- **CLI & GUI** - Command-line for power users, GUI for everyone else
- **Lazy Loading** - Controllers only loaded when needed
- **JSON Profiles** - Human-readable configuration files

## Quick Start

### Command Line
```bash
# Scan for devices
oneclickrgb scan

# Set all devices to red
oneclickrgb red

# Set custom color (RGB)
oneclickrgb color 255 128 0

# Turn off all devices
oneclickrgb off

# Save current state
oneclickrgb save MyProfile

# Load a profile
oneclickrgb profile MyProfile
```

### C++ Integration
```cpp
#include "core/OneClickRGB.h"

int main() {
    auto& app = OneClickRGB::Application::Get();
    app.Initialize();

    // One-click color
    app.OneClickRed();

    // Or with custom color
    app.OneClickColor(255, 128, 0);

    // Load profile
    app.Profiles().LoadProfile("Gaming");

    return 0;
}
```

### Using Macros
```cpp
#include "core/OneClickRGB.h"

int main() {
    RGB_APP.Initialize();

    RGB_RED();                  // All red
    RGB_SET_COLOR(255, 0, 255); // Purple
    RGB_PROFILE("Work");        // Load profile
    RGB_OFF();                  // Turn off

    return 0;
}
```

## Building

### Requirements
- CMake 3.16+
- C++17 compiler
- HIDAPI (optional, bundled fallback)
- nlohmann/json (optional, bundled fallback)
- Qt5/Qt6 (optional, for GUI)

### Build Steps
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Build Options
```bash
# CLI only
cmake -DBUILD_GUI=OFF ..

# With OpenRGB controller support
cmake -DUSE_OPENRGB_CONTROLLERS=ON -DOPENRGB_PATH=/path/to/OpenRGB ..
```

## Architecture

```
OneClickRGB/
├── src/
│   ├── core/
│   │   ├── OneClickRGB.h/cpp     # Main application
│   │   ├── DeviceManager.h/cpp   # Device lifecycle management
│   │   └── ProfileManager.h/cpp  # Profile storage/loading
│   │
│   ├── scanner/
│   │   └── HardwareScanner.h/cpp # Hardware-first detection
│   │
│   ├── devices/
│   │   └── RGBDevice.h/cpp       # Unified device interface
│   │
│   └── main.cpp                  # CLI entry point
│
├── config/
│   ├── controller_database.json  # VID/PID -> Controller mapping
│   └── profiles/                 # User profiles (JSON)
│
└── CMakeLists.txt
```

## Controller Database

The controller database (`config/controller_database.json`) maps hardware VID/PID to controller implementations:

```json
{
    "vendor_id": "0x1B1C",
    "product_id": "0x1B2D",
    "controller": "CorsairPeripheral",
    "name": "Corsair K95 RGB Platinum",
    "type": "keyboard"
}
```

Only devices listed in this database will be detected and managed.

## Supported Devices

Current built-in support includes:
- **Corsair** - Keyboards, Mice, Lighting Node, Commander Core
- **Razer** - Huntsman, DeathAdder, Basilisk
- **Logitech** - G-Series keyboards and mice
- **SteelSeries** - Apex, Rival
- **ASUS** - Aura USB controllers
- **MSI** - Mystic Light
- **NZXT** - Hue 2, Smart Device
- **Cooler Master** - ARGB Controller
- **Gigabyte** - RGB Fusion

## License

GPL-2.0-or-later (compatible with OpenRGB)

## Credits

- Inspired by [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB)
- Controller protocols based on OpenRGB research
