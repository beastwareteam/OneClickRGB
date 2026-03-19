# G.Skill Trident Z5 RGB DDR5 RAM - SMBus/I2C Protocol Documentation

## Overview

G.Skill Trident Z5 RGB DDR5 RAM modules use an SMBus (System Management Bus) interface for RGB control. Unlike USB HID devices, DDR5 RAM RGB controllers communicate through the motherboard's SMBus controller, requiring low-level hardware access.

## SMBus Architecture

### Hardware Stack

```
+------------------+
|  Application     |  (OneClickRGB)
+------------------+
        |
+------------------+
|  SMBus Driver    |  (WinRing0, inpoutx64, or direct IO)
+------------------+
        |
+------------------+
|  SMBus Controller|  (Intel PCH, AMD FCH)
+------------------+
        |
+------------------+
|  DDR5 RGB SPD Hub|  (Address 0x50-0x57, Page Select: 0x0B)
+------------------+
        |
+------------------+
|  RGB Controller  |  (ENE, Holtek, or similar)
+------------------+
```

## DDR5 SPD5 Hub Architecture

DDR5 uses SPD5 (Serial Presence Detect version 5) with a Hub device that provides:
- Temperature sensor access
- SPD EEPROM access (device configuration)
- RGB controller access (vendor-specific pages)

### SPD5 Hub Addressing

| Component | I2C Address Range | Description |
|-----------|-------------------|-------------|
| SPD Hub | 0x50-0x57 | One per DIMM slot (DIMM0-7) |
| Temperature Sensor | 0x18-0x1F | Corresponding to each hub |
| RGB Page Select | 0x0B | Page selection register |

### Address Calculation

```cpp
// DIMM slot to SPD5 Hub address
uint8_t GetSPD5HubAddress(int dimm_slot) {
    return 0x50 + (dimm_slot & 0x07);
}

// DIMM slot to temperature sensor address
uint8_t GetTempSensorAddress(int dimm_slot) {
    return 0x18 + (dimm_slot & 0x07);
}
```

## G.Skill Trident Z5 RGB Controller

### RGB Controller Detection

G.Skill Trident Z5 DDR5 uses an integrated RGB controller accessible through the SPD5 Hub's vendor-specific pages.

**Detection Steps:**
1. Read SPD5 Hub device ID
2. Switch to RGB page (typically Page 4 or 5)
3. Read RGB controller signature
4. Verify G.Skill vendor ID

### Page Selection Protocol

```cpp
// SPD5 Hub Page Selection
// Register 0x0B controls the active page

#define SPD5_HUB_PAGE_REG    0x0B
#define SPD5_PAGE_SPD        0x00    // SPD EEPROM data
#define SPD5_PAGE_NVRAM      0x01    // Non-volatile settings
#define SPD5_PAGE_TEMP       0x02    // Temperature sensor
#define SPD5_PAGE_VENDOR_1   0x04    // Vendor-specific (often RGB)
#define SPD5_PAGE_VENDOR_2   0x05    // Vendor-specific (alternate)

void SelectSPD5Page(uint8_t hub_addr, uint8_t page) {
    SMBusWriteByte(hub_addr, SPD5_HUB_PAGE_REG, page);
}
```

## RGB Protocol (OpenRGB-Based Analysis)

### Register Map

Based on OpenRGB's GSkillDRAMController implementation:

| Register | Description | Access |
|----------|-------------|--------|
| 0x20 | Mode/Effect Register | R/W |
| 0x21 | Speed Register | R/W |
| 0x22 | Direction Register | R/W |
| 0x23 | Brightness Register | R/W |
| 0x24-0x2F | Reserved | - |
| 0x30-0x5F | LED Color Data (RGB) | R/W |
| 0x60 | Apply/Commit Register | W |

### Mode Definitions

```cpp
enum GSkillDRAMMode {
    GSKILL_MODE_OFF          = 0x00,
    GSKILL_MODE_STATIC       = 0x01,
    GSKILL_MODE_BREATHING    = 0x02,
    GSKILL_MODE_COLOR_CYCLE  = 0x03,
    GSKILL_MODE_WAVE         = 0x04,
    GSKILL_MODE_COMET        = 0x05,
    GSKILL_MODE_FLASH_DASH   = 0x06,
    GSKILL_MODE_RAINBOW      = 0x07,
    GSKILL_MODE_DUAL_RAINBOW = 0x08,
};
```

### LED Configuration

Trident Z5 RGB DDR5 typically has **10 addressable LEDs** per module.

```cpp
#define GSKILL_TRIDENT_Z5_LED_COUNT  10

struct GSkillLEDData {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

// LED color buffer (30 bytes for 10 LEDs)
#define GSKILL_COLOR_START_REG  0x30
#define GSKILL_COLOR_END_REG    0x4D  // 0x30 + (10 * 3) - 1
```

### Color Setting Protocol

```cpp
void SetLEDColors(uint8_t hub_addr, const GSkillLEDData* colors, int led_count) {
    // Switch to RGB page
    SelectSPD5Page(hub_addr, SPD5_PAGE_VENDOR_1);

    // Write color data
    for (int i = 0; i < led_count; i++) {
        uint8_t reg = GSKILL_COLOR_START_REG + (i * 3);
        SMBusWriteByte(hub_addr, reg + 0, colors[i].red);
        SMBusWriteByte(hub_addr, reg + 1, colors[i].green);
        SMBusWriteByte(hub_addr, reg + 2, colors[i].blue);
    }

    // Apply changes
    SMBusWriteByte(hub_addr, 0x60, 0x01);
}
```

## Windows SMBus Access Methods

### Option 1: WinRing0 (Recommended)

WinRing0 is an open-source library that provides kernel-level I/O access on Windows.

**Files Required:**
- WinRing0.dll / WinRing0x64.dll
- WinRing0.sys / WinRing0x64.sys

```cpp
#include "OlsApi.h"

class WinRing0SMBus {
public:
    bool Initialize() {
        // Load WinRing0 driver
        if (!InitializeOls()) {
            return false;
        }

        // Detect SMBus controller
        DetectSMBusController();
        return true;
    }

    void Shutdown() {
        DeinitializeOls();
    }

    bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) {
        // Intel PCH SMBus write sequence
        WaitForNotBusy();
        WriteIoPortByte(SMBUS_HOST_ADDR, (addr << 1));
        WriteIoPortByte(SMBUS_HOST_CMD, reg);
        WriteIoPortByte(SMBUS_HOST_DATA0, data);
        WriteIoPortByte(SMBUS_HOST_CTRL, SMBUS_BYTE_DATA | SMBUS_START);
        return WaitForComplete();
    }

    uint8_t ReadByte(uint8_t addr, uint8_t reg) {
        WaitForNotBusy();
        WriteIoPortByte(SMBUS_HOST_ADDR, (addr << 1) | 0x01);
        WriteIoPortByte(SMBUS_HOST_CMD, reg);
        WriteIoPortByte(SMBUS_HOST_CTRL, SMBUS_BYTE_DATA | SMBUS_START);
        WaitForComplete();
        return ReadIoPortByte(SMBUS_HOST_DATA0);
    }

private:
    uint16_t smbus_base_addr = 0;

    // Intel PCH SMBus registers (offset from base)
    static constexpr uint16_t SMBUS_HOST_STS   = 0x00;
    static constexpr uint16_t SMBUS_HOST_CTRL  = 0x02;
    static constexpr uint16_t SMBUS_HOST_CMD   = 0x03;
    static constexpr uint16_t SMBUS_HOST_ADDR  = 0x04;
    static constexpr uint16_t SMBUS_HOST_DATA0 = 0x05;
    static constexpr uint16_t SMBUS_HOST_DATA1 = 0x06;

    // Control bits
    static constexpr uint8_t SMBUS_START     = 0x40;
    static constexpr uint8_t SMBUS_BYTE_DATA = 0x08;
};
```

### Option 2: inpoutx64

Alternative low-level I/O library for Windows x64.

```cpp
#include "inpoutx64.h"

// Function pointers
typedef void (*lpOut32)(short, short);
typedef short (*lpInp32)(short);
typedef BOOL (*lpIsInpOutDriverOpen)(void);

lpOut32 Out32;
lpInp32 Inp32;
lpIsInpOutDriverOpen IsInpOutDriverOpen;

bool LoadInpOut() {
    HINSTANCE hLib = LoadLibrary("inpoutx64.dll");
    if (!hLib) return false;

    Out32 = (lpOut32)GetProcAddress(hLib, "Out32");
    Inp32 = (lpInp32)GetProcAddress(hLib, "Inp32");
    IsInpOutDriverOpen = (lpIsInpOutDriverOpen)
        GetProcAddress(hLib, "IsInpOutDriverOpen");

    return IsInpOutDriverOpen && IsInpOutDriverOpen();
}
```

### Option 3: Direct Driver Communication (WinIO Alternative)

For production use, consider creating a signed kernel driver or using the Windows I2C APIs (available on some systems).

## SMBus Controller Detection

### Intel PCH SMBus

```cpp
// PCI Device ID for Intel SMBus
// Location: Bus 0, Device 31, Function 4

#define INTEL_SMBUS_PCI_BUS      0
#define INTEL_SMBUS_PCI_DEV      31
#define INTEL_SMBUS_PCI_FUNC     4

// Common Intel SMBus PCI Device IDs
#define PCI_DEVICE_INTEL_SMBUS_ICL  0x02A3  // Ice Lake
#define PCI_DEVICE_INTEL_SMBUS_TGL  0xA0A3  // Tiger Lake
#define PCI_DEVICE_INTEL_SMBUS_ADL  0x7AA3  // Alder Lake
#define PCI_DEVICE_INTEL_SMBUS_RPL  0x7A23  // Raptor Lake

uint16_t GetIntelSMBusBaseAddress() {
    // Read PCI config space BAR0 (offset 0x20)
    uint32_t bar = ReadPciConfig(0, 31, 4, 0x20);
    return (uint16_t)(bar & 0xFFFE);  // Mask off I/O space bit
}
```

### AMD FCH SMBus

```cpp
// AMD SMBus is accessed via ACPI-defined I/O ports
// Typically at 0x0B00 or similar

#define AMD_SMBUS_BASE_DEFAULT  0x0B00

// AMD SMBus registers
#define AMD_SMBUS_STS   0x00
#define AMD_SMBUS_CMD   0x01
#define AMD_SMBUS_ADDR  0x02
#define AMD_SMBUS_DATA0 0x03
#define AMD_SMBUS_DATA1 0x04
#define AMD_SMBUS_BLOCK 0x05
```

## Implementation for OneClickRGB

### Proposed Class Structure

```cpp
/*---------------------------------------------------------*\
| GSkillTridentZ5Controller.h                               |
| Controller for G.Skill Trident Z5 RGB DDR5 RAM           |
\*---------------------------------------------------------*/

#pragma once
#include "IRGBController.h"
#include "SMBusInterface.h"

namespace OneClickRGB {

class GSkillTridentZ5Controller : public RGBControllerBase {
public:
    GSkillTridentZ5Controller(uint8_t spd_address);
    ~GSkillTridentZ5Controller() override;

    // IRGBController implementation
    bool Open(const std::string& device_path) override;
    void Close() override;
    bool Initialize() override;
    bool Apply() override;
    bool SetAllLEDs(const RGBColor& color) override;
    bool SetLEDColor(int led_index, const RGBColor& color) override;

    // DDR5 specific
    bool DetectRGBController();
    std::string GetModuleType() const;
    float GetModuleTemperature() const;

private:
    std::unique_ptr<SMBusInterface> m_smbus;
    uint8_t m_spd_address;      // 0x50-0x57
    uint8_t m_rgb_page;         // Vendor page for RGB
    bool m_direct_access;       // Direct register vs command mode

    static constexpr int LED_COUNT = 10;

    bool SelectRGBPage();
    bool WriteColorRegister(uint8_t reg, uint8_t value);
    uint8_t ReadColorRegister(uint8_t reg);
};

} // namespace OneClickRGB
```

### SMBus Interface Abstraction

```cpp
/*---------------------------------------------------------*\
| SMBusInterface.h                                          |
| Abstract interface for SMBus communication                |
\*---------------------------------------------------------*/

#pragma once
#include <cstdint>
#include <memory>

namespace OneClickRGB {

class SMBusInterface {
public:
    virtual ~SMBusInterface() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    // Basic SMBus operations
    virtual bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) = 0;
    virtual uint8_t ReadByte(uint8_t addr, uint8_t reg) = 0;

    // Block operations
    virtual bool WriteBlock(uint8_t addr, uint8_t reg,
                            const uint8_t* data, uint8_t len) = 0;
    virtual int ReadBlock(uint8_t addr, uint8_t reg,
                          uint8_t* buffer, uint8_t len) = 0;

    // Quick command (address-only)
    virtual bool QuickCommand(uint8_t addr, bool read) = 0;

    // Probe for device presence
    virtual bool ProbeAddress(uint8_t addr) = 0;

    // Factory methods
    static std::unique_ptr<SMBusInterface> CreateForPlatform();
};

// Windows implementations
class WinRing0SMBus : public SMBusInterface {
    // Implementation using WinRing0
};

class InpOutSMBus : public SMBusInterface {
    // Implementation using inpoutx64
};

} // namespace OneClickRGB
```

## Detection Flow

```cpp
std::vector<DetectedHardware> ScanDDR5RGB() {
    std::vector<DetectedHardware> devices;

    auto smbus = SMBusInterface::CreateForPlatform();
    if (!smbus->Initialize()) {
        std::cerr << "Failed to initialize SMBus\n";
        return devices;
    }

    // Scan all possible DDR5 DIMM slots (0x50-0x57)
    for (uint8_t addr = 0x50; addr <= 0x57; addr++) {
        if (!smbus->ProbeAddress(addr)) {
            continue;  // No device at this address
        }

        // Try to detect RGB controller
        // Switch to vendor page
        smbus->WriteByte(addr, 0x0B, 0x04);  // Page 4

        // Read vendor signature
        uint8_t sig0 = smbus->ReadByte(addr, 0x00);
        uint8_t sig1 = smbus->ReadByte(addr, 0x01);

        // G.Skill signature check
        if (IsGSkillSignature(sig0, sig1)) {
            DetectedHardware hw;
            hw.vendor_id = VendorID::GSKILL;
            hw.product_id = 0x0001;  // Trident Z5 RGB
            hw.bus_type = DetectedHardware::BusType::I2C_SMBUS;
            hw.device_path = std::to_string(addr);
            hw.product_name = "G.Skill Trident Z5 RGB";
            hw.manufacturer = "G.Skill";

            devices.push_back(hw);
        }
    }

    smbus->Shutdown();
    return devices;
}
```

## Dependencies for Implementation

### Required Libraries

1. **WinRing0** (MIT License)
   - GitHub: https://github.com/GermanAizek/WinRing0
   - Files: WinRing0x64.dll, WinRing0x64.sys, OlsApi.h

2. **Alternative: inpoutx64**
   - Website: http://www.highrez.co.uk/downloads/inpout32/
   - Files: inpoutx64.dll, inpoutx64.sys

### Driver Signing Considerations

- WinRing0 and inpoutx64 require either:
  - Test Signing Mode enabled
  - A signed kernel driver
  - Windows Developer Mode

For production deployment, consider:
- Using a properly signed driver
- Requesting an EV code signing certificate
- Using Windows Hardware Lab Kit (HLK) for certification

## References

1. **OpenRGB Project**
   - GitLab: https://gitlab.com/CalcProgrammer1/OpenRGB
   - Relevant files:
     - `Controllers/GSkillDRAMController/`
     - `i2c_smbus/`
     - `Controllers/DDR5Controller/`

2. **JEDEC SPD5 Specification**
   - Document: JESD300-5 (Serial Presence Detect for DDR5)

3. **Intel PCH Datasheets**
   - SMBus Controller specifications

4. **G.Skill Trident Z5 RGB Product Page**
   - https://www.gskill.com/

## Notes for OneClickRGB Integration

1. **Privilege Requirements**: SMBus access requires administrator privileges

2. **Safety Considerations**:
   - DDR5 SPD data is critical for memory operation
   - Only access RGB pages, never modify SPD EEPROM content
   - Implement proper error handling

3. **Hot-Plug**: DDR5 RAM is not hot-pluggable, but detection should handle:
   - System wake from sleep
   - Driver reinitialization

4. **Multi-Module Sync**: When multiple DIMMs are present, consider:
   - Synchronized color updates
   - Consistent addressing across all modules

5. **Compatibility Matrix**: Test with:
   - Intel Alder Lake / Raptor Lake / Meteor Lake platforms
   - AMD AM5 platforms (Ryzen 7000 series)
