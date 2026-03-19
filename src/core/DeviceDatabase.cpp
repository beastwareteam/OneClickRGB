/*---------------------------------------------------------*\
| OneClickRGB - Device Database Implementation              |
\*---------------------------------------------------------*/
#include "DeviceDatabase.h"
#include <algorithm>
#include <iostream>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Singleton Instance                                        |
\*---------------------------------------------------------*/
DeviceDatabase& DeviceDatabase::Instance()
{
    static DeviceDatabase instance;
    return instance;
}

/*---------------------------------------------------------*\
| Register Device Definition                                |
\*---------------------------------------------------------*/
void DeviceDatabase::RegisterDevice(const DeviceDefinition& def)
{
    m_devices.push_back(def);
}

/*---------------------------------------------------------*\
| Find Device by VID/PID/Interface/UsagePage                |
\*---------------------------------------------------------*/
const DeviceDefinition* DeviceDatabase::FindDevice(uint16_t vid, uint16_t pid,
                                                   int interface_num,
                                                   uint16_t usage_page) const
{
    // First try exact match with interface and usage page
    for (const auto& def : m_devices) {
        if (def.vendor_id == vid && def.product_id == pid) {
            // Check interface if specified
            if (def.interface_number >= 0 && interface_num >= 0) {
                if (def.interface_number != interface_num) {
                    continue;
                }
            }
            // Check usage page if specified
            if (def.usage_page != 0 && usage_page != 0) {
                if (def.usage_page != usage_page) {
                    continue;
                }
            }
            return &def;
        }
    }

    // Fallback: try VID/PID only match
    for (const auto& def : m_devices) {
        if (def.vendor_id == vid && def.product_id == pid) {
            return &def;
        }
    }

    return nullptr;
}

/*---------------------------------------------------------*\
| Initialize Known Devices                                  |
| This is where all supported devices are registered        |
\*---------------------------------------------------------*/
void DeviceDatabase::InitializeKnownDevices()
{
    // Clear existing
    m_devices.clear();

    //-------------------------------------------------------
    // ASUS AURA Controllers
    //-------------------------------------------------------
    RegisterDevice({
        .vendor_id          = VendorID::ASUS,
        .product_id         = AsusAuraPID::AURA_MAINBOARD_1,  // 0x19AF
        .interface_number   = 2,                               // Interface 2 (from your hardware scan)
        .usage_page         = UsagePage::VENDOR_ASUS_AURA,    // 0xFF72
        .usage              = 0,                               // Any usage
        .name               = "ASUS Aura LED Controller",
        .vendor             = "ASUS",
        .type               = DeviceType::Motherboard,
        .controller_class   = "AsusAuraUSBController",
        .packet_size        = 65,
        .use_feature_report = false,  // Uses hid_write, not feature report
        .report_id          = 0xEC,   // Magic byte / Report ID
        .create_controller  = nullptr // Will be set by controller registration
    });

    RegisterDevice({
        .vendor_id          = VendorID::ASUS,
        .product_id         = AsusAuraPID::AURA_MAINBOARD_2,
        .interface_number   = 0,
        .usage_page         = UsagePage::VENDOR_ASUS_AURA,
        .usage              = 0x00A1,
        .name               = "ASUS Aura LED Controller",
        .vendor             = "ASUS",
        .type               = DeviceType::Motherboard,
        .controller_class   = "AsusAuraUSBController",
        .packet_size        = 65,
        .use_feature_report = false,
        .report_id          = 0xEC,
        .create_controller  = nullptr
    });

    //-------------------------------------------------------
    // SteelSeries Mice
    //-------------------------------------------------------
    RegisterDevice({
        .vendor_id          = VendorID::STEELSERIES,
        .product_id         = SteelSeriesPID::RIVAL_600,      // 0x1724
        .interface_number   = 0,                               // Interface 0
        .usage_page         = UsagePage::VENDOR_STEELSERIES,  // 0xFFC0
        .usage              = 0x0001,
        .name               = "SteelSeries Rival 600",
        .vendor             = "SteelSeries",
        .type               = DeviceType::Mouse,
        .controller_class   = "SteelSeriesRivalController",
        .packet_size        = 65,
        .use_feature_report = true,   // Uses feature reports
        .report_id          = 0x00,
        .create_controller  = nullptr
    });

    RegisterDevice({
        .vendor_id          = VendorID::STEELSERIES,
        .product_id         = SteelSeriesPID::RIVAL_600_DOTA,
        .interface_number   = 0,
        .usage_page         = UsagePage::VENDOR_STEELSERIES,
        .usage              = 0x0001,
        .name               = "SteelSeries Rival 600 Dota 2 Edition",
        .vendor             = "SteelSeries",
        .type               = DeviceType::Mouse,
        .controller_class   = "SteelSeriesRivalController",
        .packet_size        = 65,
        .use_feature_report = true,
        .report_id          = 0x00,
        .create_controller  = nullptr
    });

    //-------------------------------------------------------
    // EVision / SONiX Keyboards
    //-------------------------------------------------------
    RegisterDevice({
        .vendor_id          = VendorID::EVISION_SONIX,
        .product_id         = EVisionPID::GK650_KEYBOARD,     // 0x4E9F
        .interface_number   = 1,                               // Interface 1 for RGB control
        .usage_page         = UsagePage::VENDOR_EVISION,      // 0xFF1C
        .usage              = 0x0001,
        .name               = "GK650 Gaming Keyboard",
        .vendor             = "EVision/SONiX",
        .type               = DeviceType::Keyboard,
        .controller_class   = "EVisionKeyboardController",
        .packet_size        = 64,                              // 64 bytes, not 65
        .use_feature_report = false,  // Uses hid_write
        .report_id          = 0x04,   // EVision header byte
        .create_controller  = nullptr
    });

    std::cout << "[DATABASE] Registered " << m_devices.size() << " device definitions\n";
}

} // namespace OneClickRGB
