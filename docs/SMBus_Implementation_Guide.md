# SMBus Implementation Guide for OneClickRGB

## Overview

This guide describes how to implement SMBus/I2C communication for controlling DDR5 RAM RGB lighting on Windows systems.

## Architecture

```
+-----------------------+
| OneClickRGB           |
+-----------------------+
         |
+-----------------------+
| SMBusInterface        |  Abstract interface
+-----------------------+
         |
    +----+----+
    |         |
+-------+ +--------+
|WinRing| |InpOut  |  Concrete implementations
|0SMBus | |SMBus   |
+-------+ +--------+
    |         |
+-------------------------+
| Port I/O (kernel mode)  |
+-------------------------+
         |
+-------------------------+
| Intel PCH / AMD FCH     |  SMBus Controller
+-------------------------+
         |
+-------------------------+
| DDR5 SPD5 Hub           |  0x50-0x57
+-------------------------+
         |
+-------------------------+
| RGB Controller          |  Vendor-specific pages
+-------------------------+
```

## Files Overview

| File | Description |
|------|-------------|
| `src/smbus/SMBusInterface.h` | Abstract SMBus interface and constants |
| `src/smbus/SMBusWindows.h` | Windows implementation headers |
| `src/smbus/SMBusWindows.cpp` | Windows implementations (WinRing0, InpOut) |
| `src/controllers/GSkillTridentZ5Controller.h` | G.Skill DDR5 RGB controller |
| `src/controllers/GSkillTridentZ5Controller.cpp` | Implementation |
| `src/debug_smbus_scan.cpp` | Test/debug utility |
| `docs/GSkill_Trident_Z5_RGB_DDR5_Protocol.md` | Protocol documentation |

## Prerequisites

### WinRing0 Driver

1. Download WinRing0 from: https://github.com/GermanAizek/WinRing0
2. Copy to build directory:
   - `WinRing0x64.dll`
   - `WinRing0x64.sys`
3. The driver installs automatically on first use

### Alternative: inpoutx64

1. Download from: http://www.highrez.co.uk/downloads/inpout32/
2. Copy to build directory:
   - `inpoutx64.dll`
   - `inpoutx64.sys`

### Administrator Privileges

SMBus access requires administrator privileges due to direct hardware I/O.

## Implementation Steps

### Step 1: Initialize SMBus

```cpp
#include "smbus/SMBusInterface.h"
#include "smbus/SMBusWindows.h"

// Create SMBus interface (tries WinRing0, then InpOut)
auto smbus = SMBusInterface::Create();

if (!smbus || !smbus->IsInitialized()) {
    std::cerr << "SMBus initialization failed\n";
    return;
}

auto info = smbus->GetControllerInfo();
std::cout << "SMBus: " << info.description << " at 0x"
          << std::hex << info.base_address << "\n";
```

### Step 2: Scan for DDR5 Modules

```cpp
#include "controllers/GSkillTridentZ5Controller.h"

// Detect G.Skill modules
auto modules = GSkillTridentZ5Controller::DetectModules(smbus.get());

for (uint8_t addr : modules) {
    std::cout << "Found G.Skill RGB at DIMM"
              << (addr - 0x50) << "\n";
}
```

### Step 3: Control RGB

```cpp
// Create controller for specific DIMM
auto smbus_shared = std::shared_ptr<SMBusInterface>(
    smbus.release(),
    [](SMBusInterface* p) { delete p; }
);

GSkillTridentZ5Controller controller(smbus_shared, 0x50);

if (!controller.Initialize()) {
    std::cerr << "Controller init failed\n";
    return;
}

// Set static red color
controller.SetMode(1);  // Static mode
controller.SetAllLEDs(RGBColor{255, 0, 0});
controller.SetBrightness(100);
controller.Apply();

// Save to module memory (persistent)
controller.SaveToDevice();
```

## Intel PCH SMBus Detection

The SMBus controller is typically at PCI Bus 0, Device 31, Function 4:

```cpp
// Read PCI config to find SMBus base address
DWORD pci_addr = (0 << 8) | (31 << 3) | 4;  // Bus 0, Dev 31, Func 4

// Read Vendor/Device ID (offset 0x00)
DWORD vendor_device = ReadPciConfig(pci_addr, 0x00);
uint16_t vendor_id = vendor_device & 0xFFFF;  // Should be 0x8086 (Intel)

// Read BAR0 (offset 0x20) for I/O base address
DWORD bar0 = ReadPciConfig(pci_addr, 0x20);
uint16_t smbus_base = bar0 & 0xFFFE;  // Mask I/O bit
```

### Common Intel SMBus Device IDs

| Platform | Device ID |
|----------|-----------|
| Ice Lake | 0x02A3 |
| Tiger Lake | 0xA0A3 |
| Alder Lake-S | 0x7AA3 |
| Alder Lake-P | 0x51A3 |
| Raptor Lake-S | 0x7A23 |
| Comet Lake | 0x06A3 |
| Cannon Lake | 0xA323 |

## SMBus Transaction Protocol

### Write Byte Data

```cpp
bool WriteByte(uint8_t addr, uint8_t reg, uint8_t data) {
    // Wait for bus not busy
    while (InPort(base + HOST_STATUS) & STS_HOST_BUSY) {
        if (timeout) return false;
    }

    // Clear status
    OutPort(base + HOST_STATUS, 0xFF);

    // Set target address (write mode)
    OutPort(base + TRANSMIT_ADDR, (addr << 1));

    // Set command/register
    OutPort(base + HOST_COMMAND, reg);

    // Set data
    OutPort(base + HOST_DATA0, data);

    // Start transaction
    OutPort(base + HOST_CONTROL, CMD_BYTE_DATA | CTL_START);

    // Wait for completion
    while (!(InPort(base + HOST_STATUS) & STS_INTR)) {
        if (timeout || error) return false;
    }

    return true;
}
```

### Read Byte Data

```cpp
uint8_t ReadByte(uint8_t addr, uint8_t reg) {
    // Wait for bus not busy
    while (InPort(base + HOST_STATUS) & STS_HOST_BUSY);

    // Clear status
    OutPort(base + HOST_STATUS, 0xFF);

    // Set target address (read mode)
    OutPort(base + TRANSMIT_ADDR, (addr << 1) | 1);

    // Set command/register
    OutPort(base + HOST_COMMAND, reg);

    // Start transaction
    OutPort(base + HOST_CONTROL, CMD_BYTE_DATA | CTL_START);

    // Wait for completion
    while (!(InPort(base + HOST_STATUS) & STS_INTR));

    // Read result
    return InPort(base + HOST_DATA0);
}
```

## DDR5 SPD5 Hub Protocol

### Page Selection

DDR5 uses a paged memory model accessed through the SPD5 Hub:

```cpp
// Page selection register
#define PAGE_SELECT_REG  0x0B

// Available pages
#define PAGE_SPD         0x00  // SPD EEPROM data
#define PAGE_NVRAM       0x01  // Non-volatile settings
#define PAGE_TEMP        0x02  // Temperature sensor
#define PAGE_VENDOR_1    0x04  // Vendor RGB (G.Skill, etc.)
#define PAGE_VENDOR_2    0x05  // Alternate vendor page

// Select RGB page
smbus->WriteByte(0x50, PAGE_SELECT_REG, PAGE_VENDOR_1);
```

### RGB Controller Registers

```cpp
// G.Skill Trident Z5 RGB registers (on vendor page)
#define REG_MODE        0x20
#define REG_SPEED       0x21
#define REG_DIRECTION   0x22
#define REG_BRIGHTNESS  0x23
#define REG_COLOR_START 0x30
#define REG_APPLY       0x60
```

## Error Handling

### Common Errors

1. **"SMBus not initialized"**
   - WinRing0/InpOut driver not loaded
   - Not running as Administrator
   - Driver file missing

2. **"No device at address"**
   - DIMM slot empty
   - Module doesn't have RGB
   - Wrong address

3. **"Failed to select page"**
   - Not a DDR5 module
   - SPD5 Hub not responding

4. **Timeout**
   - SMBus busy (another process using it)
   - Hardware error

### Debug Tips

```cpp
// Enable verbose logging
#define SMBUS_DEBUG 1

// Scan all addresses to find devices
for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (smbus->ProbeAddress(addr)) {
        std::cout << "Device at 0x" << std::hex << (int)addr << "\n";
    }
}
```

## Safety Considerations

1. **Never write to SPD EEPROM pages** - This could corrupt memory timing data

2. **Only access vendor RGB pages** - Pages 4 and 5 are typically safe

3. **Implement timeouts** - Prevent infinite loops on bus errors

4. **Release bus properly** - Always return to SPD page when done

5. **Admin privileges** - Required for direct I/O, handle gracefully if missing

## Testing

### Build Test Utility

```bash
python compile_smbus_test.py
```

### Run Tests

```cmd
# Run as Administrator!
smbus_test.exe scan          # Scan all devices
smbus_test.exe dump 0x50     # Dump DIMM0 registers
smbus_test.exe test 0x50     # Interactive RGB test
```

## Integration with OneClickRGB

### HardwareScanner Integration

The `ScanI2C_Windows()` function in `HardwareScanner.cpp` should be updated to use the SMBus implementation:

```cpp
#include "../smbus/SMBusInterface.h"
#include "../controllers/GSkillTridentZ5Controller.h"

std::vector<DetectedHardware> HardwareScanner::ScanI2C_Windows() {
    std::vector<DetectedHardware> devices;

    auto smbus = SMBusInterface::Create();
    if (!smbus) return devices;

    auto modules = GSkillTridentZ5Controller::DetectModules(smbus.get());

    for (uint8_t addr : modules) {
        DetectedHardware hw;
        hw.vendor_id = 0x0D62;
        hw.product_id = 0x0001;
        hw.bus_type = DetectedHardware::BusType::I2C_SMBUS;
        hw.device_path = "SMBUS:" + std::to_string(addr);
        hw.product_name = "G.Skill Trident Z5 RGB DDR5";
        devices.push_back(hw);
    }

    return devices;
}
```

### Controller Factory

Register the controller in `ControllerFactory`:

```cpp
if (entry.controller_name == "GSkillTridentZ5") {
    auto smbus = SMBusInterface::Create();
    uint8_t addr = ParseSMBusAddress(hw.device_path);
    return std::make_unique<GSkillTridentZ5Controller>(smbus, addr);
}
```

## References

1. **OpenRGB Source Code**
   - https://gitlab.com/CalcProgrammer1/OpenRGB
   - `Controllers/GSkillDRAMController/`
   - `i2c_smbus/`

2. **JEDEC Standards**
   - JESD300-5: SPD for DDR5
   - SMBus 3.0 Specification

3. **Intel PCH Datasheets**
   - Platform Controller Hub documentation
   - SMBus Controller registers

4. **WinRing0 Documentation**
   - https://github.com/GermanAizek/WinRing0
