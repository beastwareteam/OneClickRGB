/*---------------------------------------------------------*\
| OneClickRGB - Device Database                             |
| Contains VID/PID mappings and protocol specifications     |
| Based on OpenRGB device detection patterns                |
\*---------------------------------------------------------*/
#pragma once

#include "IRGBController.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Device Definition Structure                               |
| Complete specification for detecting and creating a       |
| controller for a specific device                          |
\*---------------------------------------------------------*/
struct DeviceDefinition {
    // USB Identification
    uint16_t    vendor_id;
    uint16_t    product_id;

    // HID Interface Selection (critical for correct communication)
    int         interface_number;   // -1 = any, specific number required for multi-interface devices
    uint16_t    usage_page;         // HID usage page (0 = any)
    uint16_t    usage;              // HID usage (0 = any)

    // Device Information
    std::string name;               // Device name
    std::string vendor;             // Vendor name
    DeviceType  type;               // Device category

    // Protocol Specification
    std::string controller_class;   // Controller class name for factory
    size_t      packet_size;        // HID packet size (typically 65)
    bool        use_feature_report; // true = hid_send_feature_report, false = hid_write
    uint8_t     report_id;          // Report ID (first byte), 0 = none

    // Factory function
    std::function<std::unique_ptr<IRGBController>()> create_controller;
};

/*---------------------------------------------------------*\
| DeviceDatabase Class                                      |
\*---------------------------------------------------------*/
class DeviceDatabase {
public:
    static DeviceDatabase& Instance();

    // Registration
    void RegisterDevice(const DeviceDefinition& def);

    // Lookup
    const DeviceDefinition* FindDevice(uint16_t vid, uint16_t pid,
                                       int interface_num = -1,
                                       uint16_t usage_page = 0) const;

    // Get all registered devices
    const std::vector<DeviceDefinition>& GetAllDevices() const { return m_devices; }

    // Initialize with known devices
    void InitializeKnownDevices();

private:
    DeviceDatabase() = default;
    std::vector<DeviceDefinition> m_devices;
};

/*---------------------------------------------------------*\
| Vendor IDs                                                |
\*---------------------------------------------------------*/
namespace VendorID {
    constexpr uint16_t ASUS           = 0x0B05;
    constexpr uint16_t STEELSERIES    = 0x1038;
    constexpr uint16_t EVISION_SONIX  = 0x3299;  // EVision/SONiX keyboards
    constexpr uint16_t CORSAIR        = 0x1B1C;
    constexpr uint16_t RAZER          = 0x1532;
    constexpr uint16_t LOGITECH       = 0x046D;
    constexpr uint16_t MSI            = 0x1462;
    constexpr uint16_t GIGABYTE       = 0x1044;
    constexpr uint16_t NZXT           = 0x1E71;
    constexpr uint16_t COOLERMASTER   = 0x2516;
    constexpr uint16_t HYPERX         = 0x0951;
    constexpr uint16_t GSKILL         = 0x0D62;  // G.Skill RAM
}

/*---------------------------------------------------------*\
| ASUS Aura Product IDs                                     |
\*---------------------------------------------------------*/
namespace AsusAuraPID {
    // Motherboard Controllers
    constexpr uint16_t AURA_TERMINAL           = 0x1889;
    constexpr uint16_t ROG_STRIX_LC            = 0x18F3;
    constexpr uint16_t AURA_MAINBOARD_1        = 0x19AF;  // Common AURA LED Controller
    constexpr uint16_t AURA_MAINBOARD_2        = 0x1939;
    constexpr uint16_t AURA_MAINBOARD_3        = 0x18F3;
    constexpr uint16_t AURA_ADDRESSABLE        = 0x1867;

    // GPU Controllers
    constexpr uint16_t ROG_STRIX_GPU           = 0x1868;
}

/*---------------------------------------------------------*\
| SteelSeries Product IDs                                   |
\*---------------------------------------------------------*/
namespace SteelSeriesPID {
    // Mice
    constexpr uint16_t RIVAL_600               = 0x1724;
    constexpr uint16_t RIVAL_600_DOTA          = 0x172E;
    constexpr uint16_t RIVAL_650               = 0x172B;
    constexpr uint16_t RIVAL_3                 = 0x1824;
    constexpr uint16_t SENSEI_TEN              = 0x1832;
    constexpr uint16_t AEROX_3                 = 0x1836;

    // Keyboards
    constexpr uint16_t APEX_PRO                = 0x1610;
    constexpr uint16_t APEX_7                  = 0x1612;

    // Headsets
    constexpr uint16_t ARCTIS_5                = 0x12AA;
}

/*---------------------------------------------------------*\
| EVision / SONiX Product IDs                               |
\*---------------------------------------------------------*/
namespace EVisionPID {
    constexpr uint16_t GK650_KEYBOARD          = 0x4E9F;  // Your keyboard
    constexpr uint16_t KEYBOARD_04D9           = 0x04D9;
    constexpr uint16_t ENDORFY_OMNIS           = 0x0012;
}

/*---------------------------------------------------------*\
| Usage Pages                                               |
\*---------------------------------------------------------*/
namespace UsagePage {
    constexpr uint16_t GENERIC_DESKTOP         = 0x0001;
    constexpr uint16_t CONSUMER                = 0x000C;
    constexpr uint16_t VENDOR_ASUS_AURA        = 0xFF72;
    constexpr uint16_t VENDOR_STEELSERIES      = 0xFFC0;
    constexpr uint16_t VENDOR_EVISION          = 0xFF1C;
    constexpr uint16_t VENDOR_GENERIC          = 0xFF00;
}

} // namespace OneClickRGB
