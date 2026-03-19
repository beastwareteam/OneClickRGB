/*---------------------------------------------------------*\
| SteelSeries Rival Controller                              |
| Based on OpenRGB SteelSeriesRivalController               |
| Supports: Rival 600, Rival 650, Rival 3, etc.             |
\*---------------------------------------------------------*/
#pragma once

#include "../core/IRGBController.h"
#include <array>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| SteelSeries Rival Protocol Constants                      |
\*---------------------------------------------------------*/
namespace SteelSeriesRival {
    // Protocol uses feature reports
    constexpr uint8_t REPORT_ID_SETTINGS    = 0x00;

    // Commands (first byte of feature report)
    constexpr uint8_t CMD_LED_EFFECT        = 0x05;  // LED effect/color command
    constexpr uint8_t CMD_SAVE              = 0x09;  // Save to device

    // Rival 600 specific
    constexpr uint8_t RIVAL600_CMD_COLOR    = 0x05;  // Set LED color
    constexpr uint8_t RIVAL600_LED_COUNT    = 8;     // 8 zones

    // Effect modes
    constexpr uint8_t EFFECT_DIRECT         = 0x00;
    constexpr uint8_t EFFECT_PULSATE        = 0x01;

    // Zone IDs (for zone mask)
    constexpr uint8_t ZONE_LOGO             = 0x00;
    constexpr uint8_t ZONE_WHEEL            = 0x01;
    constexpr uint8_t ZONE_STRIP_1          = 0x02;
    constexpr uint8_t ZONE_STRIP_2          = 0x03;
    constexpr uint8_t ZONE_STRIP_3          = 0x04;
    constexpr uint8_t ZONE_STRIP_4          = 0x05;
    constexpr uint8_t ZONE_STRIP_5          = 0x06;
    constexpr uint8_t ZONE_STRIP_6          = 0x07;
    constexpr uint8_t ZONE_ALL              = 0xFF;
}

/*---------------------------------------------------------*\
| SteelSeriesRivalController Class                          |
\*---------------------------------------------------------*/
class SteelSeriesRivalController : public RGBControllerBase {
public:
    SteelSeriesRivalController();
    ~SteelSeriesRivalController() override = default;

    // IRGBController implementation
    bool Initialize() override;
    bool Apply() override;
    bool SaveToDevice() override;

    // Zone-specific control
    bool SetZoneDirect(int zone_index, const RGBColor& color);

private:
    // Protocol helpers
    void SendColorPacket(uint8_t zone_mask, const RGBColor& color);
    void SendEffectPacket(uint8_t zone, uint8_t effect, uint16_t duration_ms,
                          const std::vector<RGBColor>& colors);

    // Packet buffer
    std::array<uint8_t, 65> m_packet_buffer;
};

} // namespace OneClickRGB
