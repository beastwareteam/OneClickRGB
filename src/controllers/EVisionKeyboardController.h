/*---------------------------------------------------------*\
| EVision Keyboard Controller                               |
| Based on OpenRGB EVisionKeyboardController                |
| Supports: GK650, Endorfy Omnis, and other SONiX keyboards |
\*---------------------------------------------------------*/
#pragma once

#include "../core/IRGBController.h"
#include <array>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| EVision Keyboard Protocol Constants                       |
\*---------------------------------------------------------*/
namespace EVision {
    // Packet structure
    constexpr uint8_t HEADER_BYTE               = 0x04;
    constexpr size_t  PACKET_SIZE               = 64;   // 64 bytes, NOT 65!
    constexpr size_t  MAX_DATA_SIZE             = 0x36; // 54 bytes of color data per packet

    // Commands (byte 3 of packet) - V2 Protocol
    constexpr uint8_t CMD_BEGIN                 = 0x01;  // Begin configure
    constexpr uint8_t CMD_END                   = 0x02;  // End configure
    constexpr uint8_t CMD_READ_CONFIG           = 0x05;  // Read config
    constexpr uint8_t CMD_WRITE_CONFIG          = 0x06;  // Write config
    constexpr uint8_t CMD_READ_CUSTOM_COLOR     = 0x0A;
    constexpr uint8_t CMD_WRITE_CUSTOM_COLOR    = 0x0B;

    // Profile offsets
    constexpr uint8_t OFFSET_CURRENT_PROFILE    = 0x00;
    constexpr uint8_t OFFSET_FIRST_PROFILE      = 0x01;

    // Parameter offsets within config
    constexpr uint8_t PARAM_MODE                = 0x00;
    constexpr uint8_t PARAM_BRIGHTNESS          = 0x01;
    constexpr uint8_t PARAM_SPEED               = 0x02;
    constexpr uint8_t PARAM_DIRECTION           = 0x03;
    constexpr uint8_t PARAM_RANDOM_COLOR        = 0x04;
    constexpr uint8_t PARAM_MODE_COLOR          = 0x05;  // RGB at 0x05, 0x06, 0x07

    // Lighting Modes - EVision V2 Protocol (GK650)
    // Mode 0x06 = "Normal" = Static single color
    constexpr uint8_t MODE_STATIC               = 0x06;  // Static/Normal - single color
    constexpr uint8_t MODE_BREATHING            = 0x05;  // Breathing
    constexpr uint8_t MODE_SPECTRUM             = 0x04;  // Spectrum cycle
    constexpr uint8_t MODE_WAVE                 = 0x03;  // Wave
    constexpr uint8_t MODE_REACTIVE             = 0x02;  // Reactive
    constexpr uint8_t MODE_CUSTOM               = 0x14;  // Per-key custom RGB

    // Keyboard matrix
    constexpr int ROWS                          = 6;
    constexpr int COLS                          = 23;
    constexpr int TOTAL_LEDS                    = 126;
}

/*---------------------------------------------------------*\
| EVisionKeyboardController Class                           |
\*---------------------------------------------------------*/
class EVisionKeyboardController : public RGBControllerBase {
public:
    EVisionKeyboardController();
    ~EVisionKeyboardController() override = default;

    // IRGBController implementation
    bool Initialize() override;
    bool Apply() override;
    bool SaveToDevice() override;

private:
    // Protocol helpers (V2)
    int  Query(uint8_t cmd, uint16_t offset = 0, const uint8_t* idata = nullptr, uint8_t size = 0, uint8_t* odata = nullptr);
    bool SendBegin();
    bool SendEnd();
    bool SendParameter(uint8_t param_id, const uint8_t* data, uint8_t data_size);
    bool SendColorData(uint16_t offset, const uint8_t* data, uint8_t data_size);
    void ComputeChecksum(uint8_t* packet);

    // Read response
    bool ReadResponse(uint8_t* buffer, int timeout_ms = 100);

    // Packet buffer
    std::array<uint8_t, EVision::PACKET_SIZE> m_packet_buffer;

    // Current settings
    uint8_t m_current_mode_value = EVision::MODE_STATIC;
};

} // namespace OneClickRGB
