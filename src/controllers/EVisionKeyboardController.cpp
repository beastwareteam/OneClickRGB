/*---------------------------------------------------------*\
| EVision V2 Keyboard Controller Implementation             |
| Based on OpenRGB EVisionV2KeyboardController              |
\*---------------------------------------------------------*/
#include "EVisionKeyboardController.h"
#include <iostream>
#include <cstring>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Constructor                                               |
\*---------------------------------------------------------*/
EVisionKeyboardController::EVisionKeyboardController()
{
    m_name = "GK650 Gaming Keyboard";
    m_vendor = "EVision/SONiX";
    m_type = DeviceType::Keyboard;
    m_packet_buffer.fill(0);

    // HID parameters for EVision keyboards
    m_hid_params.vendor_id = 0x3299;
    m_hid_params.product_id = 0x4E9F;
    m_hid_params.interface_number = 1;
    m_hid_params.usage_page = 0xFF1C;
    m_hid_params.packet_size = 64;
    m_hid_params.use_feature_report = false;
    m_hid_params.report_id = EVision::HEADER_BYTE;

    // Define supported modes - EVision V2 Protocol
    m_modes = {
        {"Static",          EVision::MODE_STATIC,         true,  false, true,  false, 1},
        {"Breathing",       EVision::MODE_BREATHING,      true,  true,  true,  false, 1},
        {"Spectrum Cycle",  EVision::MODE_SPECTRUM,       false, true,  true,  false, 0},
        {"Wave",            EVision::MODE_WAVE,           false, true,  true,  true,  0},
        {"Reactive",        EVision::MODE_REACTIVE,       true,  true,  true,  false, 1},
        {"Custom",          EVision::MODE_CUSTOM,         true,  false, true,  false, 0}
    };

    m_colors.resize(EVision::TOTAL_LEDS);
}

/*---------------------------------------------------------*\
| Initialize                                                |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::Initialize()
{
    if (!m_is_open) {
        std::cerr << "  [EVision] Device not open\n";
        return false;
    }

    std::cout << "  [EVision V2] Initializing...\n";

    m_zones.clear();
    m_zones.push_back({"Full Keyboard", static_cast<uint16_t>(m_colors.size()), 0});

    std::cout << "  [EVision V2] Ready\n";
    return true;
}

/*---------------------------------------------------------*\
| Compute Checksum (V2 Protocol)                            |
| Sum of bytes 3 to 63, stored in bytes 1-2 (little-endian) |
\*---------------------------------------------------------*/
void EVisionKeyboardController::ComputeChecksum(uint8_t* packet)
{
    uint16_t checksum = 0;
    for (int i = 3; i < EVision::PACKET_SIZE; i++) {
        checksum += packet[i];
    }
    packet[1] = checksum & 0xFF;
    packet[2] = (checksum >> 8) & 0xFF;
}

/*---------------------------------------------------------*\
| Query - Send command and read response (V2 Protocol)      |
\*---------------------------------------------------------*/
int EVisionKeyboardController::Query(uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata)
{
    m_packet_buffer.fill(0);

    m_packet_buffer[0] = EVision::HEADER_BYTE;  // 0x04
    m_packet_buffer[3] = cmd;
    m_packet_buffer[4] = size;
    m_packet_buffer[5] = offset & 0xFF;
    m_packet_buffer[6] = (offset >> 8) & 0xFF;

    if (idata && size > 0) {
        size_t copy_size = (size > 56) ? 56 : size;
        memcpy(&m_packet_buffer[8], idata, copy_size);
    }

    ComputeChecksum(m_packet_buffer.data());

    // Send
    if (!SendPacket(m_packet_buffer.data(), EVision::PACKET_SIZE)) {
        return -1;
    }

    // Read response
    uint8_t response[EVision::PACKET_SIZE];
    int bytes_read = ReceivePacket(response, EVision::PACKET_SIZE, 1000);

    if (bytes_read > 0 && odata && response[4] > 0) {
        memcpy(odata, &response[8], response[4]);
        return response[4];
    }

    return bytes_read > 0 ? 0 : -1;
}

/*---------------------------------------------------------*\
| Read Response (legacy wrapper)                            |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::ReadResponse(uint8_t* buffer, int timeout_ms)
{
    return ReceivePacket(buffer, EVision::PACKET_SIZE, timeout_ms) > 0;
}

/*---------------------------------------------------------*\
| Send Begin Command                                        |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::SendBegin()
{
    return Query(EVision::CMD_BEGIN, 0, nullptr, 0, nullptr) >= 0;
}

/*---------------------------------------------------------*\
| Send End Command                                          |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::SendEnd()
{
    return Query(EVision::CMD_END, 0, nullptr, 0, nullptr) >= 0;
}

/*---------------------------------------------------------*\
| Send Parameter (legacy - not used in V2)                  |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::SendParameter(uint8_t param_id, const uint8_t* data, uint8_t data_size)
{
    return Query(EVision::CMD_WRITE_CONFIG, param_id, data, data_size, nullptr) >= 0;
}

/*---------------------------------------------------------*\
| Send Color Data (for custom mode)                         |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::SendColorData(uint16_t offset, const uint8_t* data, uint8_t data_size)
{
    return Query(EVision::CMD_WRITE_CUSTOM_COLOR, offset, data, data_size, nullptr) >= 0;
}

/*---------------------------------------------------------*\
| Apply - Send current settings to device (V2 Protocol)     |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::Apply()
{
    if (!m_is_open) {
        return false;
    }

    // Get mode value
    uint8_t mode_value = m_current_mode_value;
    if (m_active_mode >= 0 && m_active_mode < static_cast<int>(m_modes.size())) {
        mode_value = m_modes[m_active_mode].value;
    }

    // Step 1: Begin configure
    SendBegin();

    // Step 2: Read and fix current profile
    uint8_t current_profile = 0;
    Query(EVision::CMD_READ_CONFIG, EVision::OFFSET_CURRENT_PROFILE, nullptr, 1, &current_profile);

    if (current_profile > 2) {
        current_profile = 0;
        Query(EVision::CMD_WRITE_CONFIG, EVision::OFFSET_CURRENT_PROFILE, &current_profile, 1, nullptr);
    }

    // Step 3: Build config buffer (18 bytes)
    uint8_t config[18];
    memset(config, 0, sizeof(config));

    config[EVision::PARAM_MODE]        = mode_value;
    config[EVision::PARAM_BRIGHTNESS]  = static_cast<uint8_t>(m_brightness * 4 / 100);  // 0-4
    config[EVision::PARAM_SPEED]       = 0x03;  // Normal speed
    config[EVision::PARAM_DIRECTION]   = 0x00;
    config[EVision::PARAM_RANDOM_COLOR]= 0x00;

    // Mode color
    if (!m_colors.empty()) {
        config[EVision::PARAM_MODE_COLOR + 0] = m_colors[0].r;
        config[EVision::PARAM_MODE_COLOR + 1] = m_colors[0].g;
        config[EVision::PARAM_MODE_COLOR + 2] = m_colors[0].b;
    } else {
        config[EVision::PARAM_MODE_COLOR + 0] = 255;
        config[EVision::PARAM_MODE_COLOR + 1] = 0;
        config[EVision::PARAM_MODE_COLOR + 2] = 0;
    }

    // Step 4: Write config to profile
    uint16_t offset = current_profile * 0x40 + EVision::OFFSET_FIRST_PROFILE;
    Query(EVision::CMD_WRITE_CONFIG, offset, config, sizeof(config), nullptr);

    // Step 5: End configure
    SendEnd();

    std::cout << "  [EVision V2] Applied mode " << (int)mode_value
              << " brightness " << (int)config[EVision::PARAM_BRIGHTNESS]
              << " RGB(" << (int)config[EVision::PARAM_MODE_COLOR]
              << "," << (int)config[EVision::PARAM_MODE_COLOR + 1]
              << "," << (int)config[EVision::PARAM_MODE_COLOR + 2] << ")\n";

    return true;
}

/*---------------------------------------------------------*\
| Save to Device                                            |
\*---------------------------------------------------------*/
bool EVisionKeyboardController::SaveToDevice()
{
    return true;  // V2 auto-saves on EndConfigure
}

} // namespace OneClickRGB
