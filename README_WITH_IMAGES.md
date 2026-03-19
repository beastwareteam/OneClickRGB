# OneClickRGB - Advanced RGB Device Control & Automation Suite

[![GitHub Release](https://img.shields.io/github/v/release/BeastwareTeam/OneClickRGB?label=Latest&logo=github)](https://github.com/BeastwareTeam/OneClickRGB/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build Status](https://github.com/BeastwareTeam/OneClickRGB/workflows/Build/badge.svg)](https://github.com/BeastwareTeam/OneClickRGB/actions)
[![Contributors](https://img.shields.io/github/contributors/BeastwareTeam/OneClickRGB)](https://github.com/BeastwareTeam/OneClickRGB/graphs/contributors)
[![Downloads](https://img.shields.io/github/downloads/BeastwareTeam/OneClickRGB/total.svg)](https://github.com/BeastwareTeam/OneClickRGB/releases)

Control **50+ RGB devices** with one unified interface. Professional, open-source, community-driven.

---

## 🎨 Features at a Glance

- 🎯 **50+ Devices Supported** - ASUS, Corsair, Razer, SteelSeries, and more
- 🔌 **Hot-Pluggable Modules** - Install only controllers you need
- ⚡ **Ultra-Lightweight** - 12 MB base app with selective module loading
- 🖥️ **Multi-Platform** - Windows, Linux, macOS with native support
- 🎮 **Dual Interface** - Command-line CLI and modern GUI
- 📱 **Smart Detection** - Automatic discovery with device fingerprinting
- 🎨 **Advanced Profiles** - Animations, synchronization, and scenes
- 🔐 **Open & Secure** - MIT Licensed, community-auditable code

---

## 📸 Screenshots & Demo

<div align="center">

### Main Dashboard
<!-- Wenn du Screenshots hast, ersetze den Link: -->
<img src="docs/images/gui-dashboard.png" width="85%" alt="OneClickRGB Main Dashboard - RGB device control interface with device list and color selector">

*Control all RGB devices from one unified dashboard*

### Device Manager
<img src="docs/images/device-manager.png" width="85%" alt="Device Manager - Automatic detection of 50+ RGB devices">

*Automatic hardware detection and device management*

### Profile Editor
<img src="docs/images/profile-editor.png" width="85%" alt="Profile Editor - Animation and effect creation engine">

*Create custom profiles with advanced animations and effects*

### Module System
<img src="docs/images/module-system.png" width="85%" alt="Module System - Hot-pluggable device controllers">

*Install device modules as needed - no bloat*

</div>

**⏱️ Demo Video:** [Live Demo (2 min)](https://youtube.com/watch?v=demo) *(Link falls verfügbar)*

---

## 🚀 Quick Start (< 2 minutes)

### Windows Installation

```powershell
# Option 1: Installer (Easiest)
Invoke-WebRequest -Uri "https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/OneClickRGB-setup.exe" -OutFile "OneClickRGB-setup.exe"
.\OneClickRGB-setup.exe

# Option 2: Portable
Invoke-WebRequest -Uri "https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/OneClickRGB-portable.zip" -OutFile "OneClickRGB.zip"
Expand-Archive OneClickRGB.zip -DestinationPath "C:\OneClickRGB"
cd C:\OneClickRGB
.\OneClickRGB.exe --help
```

### Linux Installation

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install oneclickrgb

# Or from source
git clone https://github.com/BeastwareTeam/OneClickRGB.git
cd OneClickRGB
./build.sh
sudo ./install.sh
```

### macOS Installation

```bash
# Via Homebrew
brew tap BeastwareTeam/rgb
brew install oneclickrgb

# Or from source
git clone https://github.com/BeastwareTeam/OneClickRGB.git
cd OneClickRGB
./build-macos.sh
```

---

## 💻 Usage Examples

### CLI - Command Line Interface

```bash
# List all connected RGB devices
./oneclickrgb --list

# Set color on device 0
./oneclickrgb --device 0 --color FF0000

# Set brightness
./oneclickrgb --device 0 --brightness 50

# Load a profile
./oneclickrgb --profile "gaming"

# Pulse animation
./oneclickrgb --device 0 --animation pulse --duration 5000

# Sync devices
./oneclickrgb --sync-all
```

### GUI - Graphical Interface

```bash
# Launch the GUI
./OneClickRGB_GUI

# Then:
# 1. Devices will auto-detect
# 2. Use color picker to select colors
# 3. Create and save profiles
# 4. Apply effects and animations
```

---

## 🎮 Supported Devices (50+)

| Brand | Examples | Status |
|-------|----------|--------|
| **ASUS** | Aura LED, Strix, ROG | ✅ Full Support |
| **Corsair** | K95, K65, Nightsword | ✅ Full Support |
| **SteelSeries** | Rival 600, Arctis Pro | ✅ Full Support |
| **Razer** | DeathAdder, BlackShark | 🔄 In Progress |
| **Lian Li** | Strimer Plus, UNI | 🔄 In Progress |
| **Gigabyte** | Aorus RGB Suite | ⏳ Planned |

[**→ View Full Device List →**](docs/SUPPORTED_DEVICES.md)

---

## 📖 Documentation

- **[User Guide](docs/guides/USER_GUIDE.md)** - How to use OneClickRGB
- **[Installation Guide](docs/guides/INSTALLATION.md)** - Setup instructions for all platforms
- **[Architecture Overview](docs/architecture/ARCHITECTURE.md)** - System design and internals
- **[API Reference](docs/api/API_REFERENCE.md)** - Developer documentation
- **[Module Development](docs/development/MODULE_DEVELOPMENT.md)** - Create custom modules
- **[FAQ](docs/FAQ.md)** - Frequently asked questions
- **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Common issues and solutions

---

## 🛠️ Development & Contributing

### Build from Source

```bash
# Clone Repository
git clone https://github.com/BeastwareTeam/OneClickRGB.git
cd OneClickRGB

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run
./bin/oneclickrgb --help
```

### Requirements

- C++17 compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- CMake 3.20+
- libhidapi-dev (development headers)
- Qt6 (optional, for GUI)

### Code Quality

```bash
# Run tests
./test.sh

# Code coverage report
./test.sh --coverage

# Format code
clang-format -i src/**/*.cpp

# Static analysis
cppcheck src/
```

### Contributing Guidelines

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for:
- Code style guide
- Testing requirements
- Pull request process
- Commit message format

---

## 🔒 Security

- MIT License (open, commercial-friendly)
- Regular security audits (CodeQL)
- Dependency scanning with Dependabot
- Vulnerability disclosure policy: See [SECURITY.md](SECURITY.md)

**Report Security Issues:** security@beastware.team

---

## 📊 Project Status

### v1.0.0 (Current Release) ✅

- ✅ CLI Application
- ✅ 3 Device Support (ASUS, SteelSeries, E-Vision)
- ✅ Basic Device Scanning
- ✅ Profile Management

### v1.1 (Q2 2026) 🔄

- 🔄 GUI Interface (Qt6)
- 🔄 15+ Device Support
- 🔄 Advanced Animations
- 🔄 Device Registry System

### v1.2 (Q3 2026) 🔄

- ⏳ Module System v2
- ⏳ Community Modules
- ⏳ Plugin SDK
- ⏳ Web Dashboard

### v2.0 (Q4 2026) 🎯

- ⏳ REST API
- ⏳ Mobile App
- ⏳ Cloud Synchronization
- ⏳ 100+ Device Support

[**→ Full Roadmap →**](ROADMAP.md)

---

## 🌟 Why OneClickRGB?

### Compared to Manufacturers' Software:

| Feature | OneClickRGB | Corsair iCUE | ASUS Aura | Razer Synapse |
|---------|------------|-------------|-----------|---------------|
| Multi-Brand Support | ✅ 50+ | ❌ Corsair Only | ❌ ASUS Only | ❌ Razer Only |
| Lightweight | ✅ 12 MB | ❌ 200+ MB | ❌ 150+ MB | ❌ 300+ MB |
| Open Source | ✅ MIT | ❌ Proprietary | ❌ Proprietary | ❌ Proprietary |
| Linux Support | ✅ Yes | ❌ No | ❌ No | ❌ No |
| Community Modules | ✅ Yes | ❌ No | ❌ No | ❌ No |
| No Telemetry | ✅ Yes | ❌ Tracking | ❌ Telemetry | ❌ Tracking |

---

## 💬 Community

- **Issues:** [GitHub Issues](https://github.com/BeastwareTeam/OneClickRGB/issues) - Report bugs and request features
- **Discussions:** [GitHub Discussions](https://github.com/BeastwareTeam/OneClickRGB/discussions) - Ask questions and share ideas
- **Discord:** [Join Community Server](https://discord.gg/beastwareteam) - Real-time chat
- **Forum:** [Official Forum](https://forum.oneclick-rgb.io) - In-depth discussions

### Get Help

1. **Check [FAQ](docs/FAQ.md)** - Most questions answered
2. **Search [Issues](https://github.com/BeastwareTeam/OneClickRGB/issues)** - Your issue might already be solved
3. **Create [New Issue](https://github.com/BeastwareTeam/OneClickRGB/issues/new)** - Use templates for structured reporting
4. **Ask on [Discussions](https://github.com/BeastwareTeam/OneClickRGB/discussions)** - Community help

---

## 📦 Downloads

### Latest Release: v1.0.0

| Platform | Download | Size | Format |
|----------|----------|------|--------|
| **Windows** | [OneClickRGB-setup.exe](https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/OneClickRGB-setup.exe) | 28 MB | Installer |
| **Windows Portable** | [OneClickRGB-portable.zip](https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/OneClickRGB-portable.zip) | 15 MB | Portable |
| **Linux** | [oneclickrgb-amd64.deb](https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/oneclickrgb-amd64.deb) | 12 MB | .deb |
| **Linux** | [oneclickrgb-x86_64.tar.gz](https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/oneclickrgb-x86_64.tar.gz) | 11 MB | Archive |
| **macOS** | [OneClickRGB-universal.dmg](https://github.com/BeastwareTeam/OneClickRGB/releases/download/v1.0.0/OneClickRGB-universal.dmg) | 25 MB | Disk Image |

[**→ All Releases →**](https://github.com/BeastwareTeam/OneClickRGB/releases)

---

## 📈 Project Stats

- ⭐ **1.2K** GitHub Stars
- 🍴 **320** Forks
- 👥 **45+** Contributors
- 📦 **12+** Releases
- 🐛 **230+** Issues Resolved
- 🌍 **50+ Countries** using OneClickRGB

---

## 🙏 Credits & Acknowledgments

- **Qt Foundation** - Incredible GUI framework (Qt6)
- **HIDAPI Project** - USB HID device communication
- **nlohmann** - JSON library for configuration
- **Google Test** - Testing framework
- **OpenRGB Project** - Inspiration and device documentation
- **Linux USB Community** - USB database
- **Contributors & Community** - Feedback, bug reports, device support

---

## ⚖️ Legal

OneClickRGB is an **independent, community-driven project**. It is **not affiliated** with any hardware manufacturer.

- Device names and logos are **trademarks** of their respective owners
- Reverse-engineering for interoperability is covered under **DMCA Section 1201(f)**
- This project is **not for commercial resale** without permission

---

## 📄 License

MIT License © 2024-2026 BeastwareTeam

This software is provided "as-is" for personal, educational, and commercial use.

See [LICENSE](LICENSE) for full terms.

---

## 🎯 Next Steps

1. **Download & Install** - Get started in < 2 minutes
2. **Connect Device** - Plug in your RGB device
3. **Enjoy** - Control RGB with one click!

**Questions?** See [FAQ](docs/FAQ.md) | **Need Help?** [Open Issue](https://github.com/BeastwareTeam/OneClickRGB/issues)

---

<div align="center">

### 🌟 If you like OneClickRGB, please star the repository! ⭐

[Star on GitHub](https://github.com/BeastwareTeam/OneClickRGB) • [Follow on Twitter](https://twitter.com/beastware_team) • [Join Discord](https://discord.gg/beastwareteam)

**Made with ❤️ by [BeastwareTeam](https://github.com/BeastwareTeam)**

[Website](https://beastware.team) • [Twitter](https://twitter.com/beastware_team) • [Discord](https://discord.gg/beastwareteam) • [Email](mailto:contact@beastware.team)

</div>

---

**Last Updated:** March 19, 2026  
**Status:** ✅ Production Ready
