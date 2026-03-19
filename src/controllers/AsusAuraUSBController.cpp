/*---------------------------------------------------------*\
| ASUS Aura USB Controller Implementation                   |
| Based on OpenRGB AuraUSBController.cpp                    |
\*---------------------------------------------------------*/
#include "AsusAuraUSBController.h"
#include <iostream>
#include <cstring>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Constructor                                               |
\*---------------------------------------------------------*/
AsusAuraUSBController::AsusAuraUSBController()
{
    m_name = "ASUS Aura LED Controller";
    m_vendor = "ASUS";
    m_type = DeviceType::Motherboard;
    m_packet_buffer.fill(0);

    // HID parameters for ASUS Aura USB
    m_hid_params.vendor_id = 0x0B05;
    m_hid_params.product_id = 0x19AF;
    m_hid_params.interface_number = 0;
    m_hid_params.usage_page = 0xFF72;
    m_hid_params.packet_size = 65;
    m_hid_params.use_feature_report = false;  // hid_write
    m_hid_params.report_id = AuraUSB::MAGIC_BYTE;

    // Define supported modes
    m_modes = {
        {"Direct",         AuraUSB::MODE_DIRECT,        true,  false, true,  false, 0},
        {"Static",         AuraUSB::MODE_STATIC,        true,  false, true,  false, 1},
        {"Breathing",      AuraUSB::MODE_BREATHING,     true,  true,  true,  false, 1},
        {"Color Cycle",    AuraUSB::MODE_COLOR_CYCLE,   false, true,  true,  false, 0},
        {"Spectrum Cycle", AuraUSB::MODE_SPECTRUM_CYCLE,false, true,  true,  false, 0},
        {"Rainbow",        AuraUSB::MODE_RAINBOW,       false, true,  true,  true,  0},
        {"Chase",          AuraUSB::MODE_CHASE,         true,  true,  true,  true,  1},
        {"Off",            AuraUSB::MODE_OFF,           false, false, false, false, 0}
    };
}

/*---------------------------------------------------------*\
| Initialize                                                |
\*---------------------------------------------------------*/
bool AsusAuraUSBController::Initialize()
{
    if (!m_is_open) {
        std::cerr << "  [AuraUSB] Device not open\n";
        return false;
    }

    std::cout << "  [AuraUSB] Initializing...\n";

    // Try to read firmware version
    ReadFirmwareVersion();

    // Try to read config table to get LED count
    if (!ReadConfigTable()) {
        // Use defaults
        m_colors.resize(AuraUSB::DEFAULT_LED_COUNT);
    }

    // Define zones
    m_zones.clear();
    m_zones.push_back({"Mainboard RGB", static_cast<uint16_t>(m_colors.size()), 0});

    std::cout << "  [AuraUSB] Ready with " << m_colors.size() << " LEDs\n";
    return true;
}

/*---------------------------------------------------------*\
| Read Firmware Version                                     |
\*---------------------------------------------------------*/
bool AsusAuraUSBController::ReadFirmwareVersion()
{
    m_packet_buffer.fill(0);
    m_packet_buffer[0] = AuraUSB::MAGIC_BYTE;
    m_packet_buffer[1] = AuraUSB::CMD_FIRMWARE_VERSION;

    if (!SendPacket(m_packet_buffer.data(), 65)) {
        return false;
    }

    // Read response (may not work on all devices)
    uint8_t response[65] = {0};
    response[0] = AuraUSB::CMD_FIRMWARE_VERSION;
    int result = GetFeatureReport(AuraUSB::CMD_FIRMWARE_VERSION, response, 65);

    if (result > 0) {
        memcpy(m_firmware_version, &response[2], 16);
        m_version = m_firmware_version;
        std::cout << "  [AuraUSB] Firmware: " << m_version << "\n";
        return true;
    }

    return false;
}

/*---------------------------------------------------------*\
| Read Config Table                                         |
\*---------------------------------------------------------*/
bool AsusAuraUSBController::ReadConfigTable()
{
    m_packet_buffer.fill(0);
    m_packet_buffer[0] = AuraUSB::MAGIC_BYTE;
    m_packet_buffer[1] = AuraUSB::CMD_CONFIG_TABLE;

    if (!SendPacket(m_packet_buffer.data(), 65)) {
        m_colors.resize(AuraUSB::DEFAULT_LED_COUNT);
        return false;
    }

    // Read config response
    uint8_t response[65] = {0};
    response[0] = AuraUSB::CMD_CONFIG_TABLE;
    int result = GetFeatureReport(AuraUSB::CMD_CONFIG_TABLE, response, 65);

    if (result > 0) {
        memcpy(m_config_table, response, 60);
        // LED count is typically at offset 0x02
        int led_count = m_config_table[0x02];
        if (led_count > 0 && led_count <= 120) {
            m_colors.resize(led_count);
            std::cout << "  [AuraUSB] Config: " << led_count << " LEDs\n";
            return true;
        }
    }

    // Fallback to default
    m_colors.resize(AuraUSB::DEFAULT_LED_COUNT);
    return false;
}

/*---------------------------------------------------------*\
| Apply - Send current colors to device                     |
\*---------------------------------------------------------*/
bool AsusAuraUSBController::Apply()
{
    if (!m_is_open) {
        return false;
    }

    int led_count = static_cast<int>(m_colors.size());

    // Prepare color data buffer (RGB triplets)
    std::vector<uint8_t> color_data(led_count * 3);
    for (int i = 0; i < led_count; i++) {
        color_data[i * 3 + 0] = m_colors[i].r;
        color_data[i * 3 + 1] = m_colors[i].g;
        color_data[i * 3 + 2] = m_colors[i].b;
    }

    // Send color data in packets of MAX_LEDS_PER_PACKET
    int leds_sent = 0;
    while (leds_sent < led_count) {
        int leds_in_packet = std::min(AuraUSB::MAX_LEDS_PER_PACKET, led_count - leds_sent);
        bool is_last_packet = (leds_sent + leds_in_packet >= led_count);

        SendDirectPacket(
            AuraUSB::CHANNEL_MAINBOARD,
            static_cast<uint8_t>(leds_sent),
            &color_data[leds_sent * 3],
            static_cast<uint8_t>(leds_in_packet),
            is_last_packet
        );

        leds_sent += leds_in_packet;
    }

    std::cout << "  [AuraUSB] Applied RGB("
              << (int)m_colors[0].r << "," << (int)m_colors[0].g << "," << (int)m_colors[0].b
              << ") to " << led_count << " LEDs\n";

    return true;
}

/*---------------------------------------------------------*\
| Send Direct Packet                                        |
| Packet format:                                            |
| [0] = 0xEC (magic)                                        |
| [1] = 0x40 (direct control)                               |
| [2] = channel | apply_flag                                |
| [3] = start LED index                                     |
| [4] = LED count                                           |
| [5+] = RGB data (3 bytes per LED)                         |
\*---------------------------------------------------------*/
void AsusAuraUSBController::SendDirectPacket(uint8_t channel, uint8_t start_led,
                                              const uint8_t* colors, uint8_t led_count, bool apply)
{
    m_packet_buffer.fill(0);

    m_packet_buffer[0] = AuraUSB::MAGIC_BYTE;
    m_packet_buffer[1] = AuraUSB::CMD_DIRECT_CONTROL;
    m_packet_buffer[2] = channel | (apply ? AuraUSB::DIRECT_APPLY : AuraUSB::DIRECT_FLUSH);
    m_packet_buffer[3] = start_led;
    m_packet_buffer[4] = led_count;

    // Copy RGB data
    size_t data_size = led_count * 3;
    if (data_size > 60) data_size = 60;  // Max data that fits
    memcpy(&m_packet_buffer[5], colors, data_size);

    SendPacket(m_packet_buffer.data(), 65);
}

/*---------------------------------------------------------*\
| Send Mode Packet                                          |
| For effect modes (Static, Breathing, etc.)                |
\*---------------------------------------------------------*/
void AsusAuraUSBController::SendModePacket(uint8_t mode, uint8_t speed, uint8_t direction,
                                            const RGBColor& color)
{
    m_packet_buffer.fill(0);

    // Effect mode packet structure (from OpenRGB)
    m_packet_buffer[0] = AuraUSB::MAGIC_BYTE;
    m_packet_buffer[1] = AuraUSB::CMD_SET_MODE;
    m_packet_buffer[2] = 0x00;  // Channel
    m_packet_buffer[3] = 0x00;
    m_packet_buffer[4] = mode;
    m_packet_buffer[5] = speed;
    m_packet_buffer[6] = direction;
    m_packet_buffer[7] = 0x00;  // Color mode (0 = single color)
    m_packet_buffer[8] = color.r;
    m_packet_buffer[9] = color.g;
    m_packet_buffer[10] = color.b;

    SendPacket(m_packet_buffer.data(), 65);
}

/*---------------------------------------------------------*\
| Send End Sequence                                         |
\*---------------------------------------------------------*/
void AsusAuraUSBController::SendEndSequence()
{
    m_packet_buffer.fill(0);
    m_packet_buffer[0] = AuraUSB::MAGIC_BYTE;
    m_packet_buffer[1] = AuraUSB::CMD_EFFECT_COMMIT;
    m_packet_buffer[2] = 0x55;  // Commit magic
    m_packet_buffer[3] = 0x00;

    SendPacket(m_packet_buffer.data(), 65);
}

/*---------------------------------------------------------*\
| Save to Device                                            |
\*---------------------------------------------------------*/
bool AsusAuraUSBController::SaveToDevice()
{
    // Send commit sequence to save settings to device EEPROM
    SendEndSequence();
    return true;
}

} // namespace OneClickRGB
