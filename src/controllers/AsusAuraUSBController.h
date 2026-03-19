/*---------------------------------------------------------*\
| ASUS Aura USB Controller                                  |
| Based on OpenRGB AuraUSBController implementation         |
| Protocol: 0xEC magic byte + command structure             |
\*---------------------------------------------------------*/
#pragma once

#include "../core/IRGBController.h"
#include <array>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| ASUS Aura USB Protocol Constants                          |
\*---------------------------------------------------------*/
namespace AuraUSB {
    // All packets start with this magic byte (also acts as report ID)
    constexpr uint8_t MAGIC_BYTE                = 0xEC;

    // Commands (second byte after magic)
    constexpr uint8_t CMD_FIRMWARE_VERSION      = 0x82;
    constexpr uint8_t CMD_CONFIG_TABLE          = 0xB0;
    constexpr uint8_t CMD_DIRECT_CONTROL        = 0x40;  // Direct LED control
    constexpr uint8_t CMD_EFFECT_CONTROL        = 0x35;  // Effect mode control
    constexpr uint8_t CMD_EFFECT_CHANNEL        = 0x36;  // Effect channel selection
    constexpr uint8_t CMD_EFFECT_COMMIT         = 0x3F;  // Commit effect to device
    constexpr uint8_t CMD_RESET_1               = 0x38;  // Reset sequence part 1
    constexpr uint8_t CMD_SET_MODE              = 0x35;  // Set lighting mode

    // Effect modes (for CMD_SET_MODE)
    constexpr uint8_t MODE_OFF                  = 0x00;
    constexpr uint8_t MODE_STATIC               = 0x01;
    constexpr uint8_t MODE_BREATHING            = 0x02;
    constexpr uint8_t MODE_COLOR_CYCLE          = 0x03;
    constexpr uint8_t MODE_SPECTRUM_CYCLE       = 0x04;
    constexpr uint8_t MODE_RAINBOW              = 0x05;
    constexpr uint8_t MODE_SPECTRUM_CYCLE_BREATH= 0x06;
    constexpr uint8_t MODE_CHASE                = 0x07;
    constexpr uint8_t MODE_SPECTRUM_CYCLE_CHASE = 0x08;
    constexpr uint8_t MODE_SPECTRUM_CYCLE_WAVE  = 0x09;
    constexpr uint8_t MODE_CHASE_FADE           = 0x0A;
    constexpr uint8_t MODE_SPECTRUM_CYCLE_CHASE_FADE = 0x0B;
    constexpr uint8_t MODE_CHASE_RAINBOW_PULSE  = 0x0C;
    constexpr uint8_t MODE_RAINBOW_FLICKER      = 0x0D;
    constexpr uint8_t MODE_DIRECT               = 0xFF;  // Direct control mode

    // Direct mode apply flags (byte 2 of direct packet)
    constexpr uint8_t DIRECT_APPLY              = 0x80;  // Apply changes immediately
    constexpr uint8_t DIRECT_FLUSH              = 0x00;  // Flush without applying

    // Channel IDs for addressable headers (byte after device ID in effect mode)
    constexpr uint8_t CHANNEL_MAINBOARD         = 0x00;
    constexpr uint8_t CHANNEL_ARGB_1            = 0x01;  // ARGB header 1
    constexpr uint8_t CHANNEL_ARGB_2            = 0x02;  // ARGB header 2
    constexpr uint8_t CHANNEL_ARGB_3            = 0x03;  // ARGB header 3

    // Max LEDs per packet in direct mode
    constexpr int MAX_LEDS_PER_PACKET           = 20;

    // Default LED count if config cannot be read
    constexpr int DEFAULT_LED_COUNT             = 12;
}

/*---------------------------------------------------------*\
| AsusAuraUSBController Class                               |
\*---------------------------------------------------------*/
class AsusAuraUSBController : public RGBControllerBase {
public:
    AsusAuraUSBController();
    ~AsusAuraUSBController() override = default;

    // IRGBController implementation
    bool Initialize() override;
    bool Apply() override;
    bool SaveToDevice() override;

private:
    // Protocol helpers
    void SendDirectPacket(uint8_t channel, uint8_t start_led, const uint8_t* colors, uint8_t led_count, bool apply);
    void SendModePacket(uint8_t mode, uint8_t speed, uint8_t direction, const RGBColor& color);
    void SendEndSequence();

    // Read device info
    bool ReadFirmwareVersion();
    bool ReadConfigTable();

    // Internal state
    std::array<uint8_t, 65> m_packet_buffer;
    char m_firmware_version[17] = {0};
    uint8_t m_config_table[60] = {0};
    uint8_t m_current_effect_mode = AuraUSB::MODE_DIRECT;
    uint8_t m_effect_speed = 0x02;  // Medium speed
    uint8_t m_effect_direction = 0x00;
};

} // namespace OneClickRGB
