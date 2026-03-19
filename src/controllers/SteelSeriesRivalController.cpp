/*---------------------------------------------------------*\
| SteelSeries Rival Controller Implementation               |
| Based on OpenRGB SteelSeriesRivalController.cpp           |
\*---------------------------------------------------------*/
#include "SteelSeriesRivalController.h"
#include <iostream>
#include <cstring>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Constructor                                               |
\*---------------------------------------------------------*/
SteelSeriesRivalController::SteelSeriesRivalController()
{
    m_name = "SteelSeries Rival 600";
    m_vendor = "SteelSeries";
    m_type = DeviceType::Mouse;
    m_packet_buffer.fill(0);

    // HID parameters for SteelSeries Rival
    m_hid_params.vendor_id = 0x1038;
    m_hid_params.product_id = 0x1724;  // Rival 600
    m_hid_params.interface_number = 0;
    m_hid_params.usage_page = 0xFFC0;
    m_hid_params.packet_size = 65;
    m_hid_params.use_feature_report = true;  // Uses feature reports!
    m_hid_params.report_id = 0x00;

    // Define supported modes
    m_modes = {
        {"Direct",   SteelSeriesRival::EFFECT_DIRECT,  true,  false, true, false, 0},
        {"Pulsate",  SteelSeriesRival::EFFECT_PULSATE, true,  true,  true, false, 1}
    };

    // Initialize LED colors (8 zones)
    m_colors.resize(SteelSeriesRival::RIVAL600_LED_COUNT);
}

/*---------------------------------------------------------*\
| Initialize                                                |
\*---------------------------------------------------------*/
bool SteelSeriesRivalController::Initialize()
{
    if (!m_is_open) {
        std::cerr << "  [SteelSeriesRival] Device not open\n";
        return false;
    }

    std::cout << "  [SteelSeriesRival] Initializing...\n";

    // Define zones
    m_zones.clear();
    m_zones.push_back({"Logo",    1, 0});
    m_zones.push_back({"Wheel",   1, 1});
    m_zones.push_back({"Strip 1", 1, 2});
    m_zones.push_back({"Strip 2", 1, 3});
    m_zones.push_back({"Strip 3", 1, 4});
    m_zones.push_back({"Strip 4", 1, 5});
    m_zones.push_back({"Strip 5", 1, 6});
    m_zones.push_back({"Strip 6", 1, 7});

    std::cout << "  [SteelSeriesRival] Ready with " << m_zones.size() << " zones\n";
    return true;
}

/*---------------------------------------------------------*\
| Apply - Send current colors to device                     |
\*---------------------------------------------------------*/
bool SteelSeriesRivalController::Apply()
{
    if (!m_is_open) {
        return false;
    }

    // Send colors for each zone
    // Rival 600 can set all zones at once with zone mask 0xFF
    // or individual zones with (1 << zone_id)

    // Option 1: Set all zones to same color (fastest)
    if (m_colors.size() > 0) {
        // Check if all colors are the same
        bool all_same = true;
        for (size_t i = 1; i < m_colors.size(); i++) {
            if (!(m_colors[i] == m_colors[0])) {
                all_same = false;
                break;
            }
        }

        if (all_same) {
            // All same color - send single packet
            SendColorPacket(SteelSeriesRival::ZONE_ALL, m_colors[0]);
        } else {
            // Different colors - send per zone
            for (size_t i = 0; i < m_colors.size(); i++) {
                SendColorPacket(static_cast<uint8_t>(1 << i), m_colors[i]);
            }
        }
    }

    std::cout << "  [SteelSeriesRival] Applied RGB("
              << (int)m_colors[0].r << "," << (int)m_colors[0].g << "," << (int)m_colors[0].b
              << ") to " << m_colors.size() << " zones\n";

    return true;
}

/*---------------------------------------------------------*\
| Send Color Packet                                         |
| Rival 600 feature report format:                          |
| [0] = Report ID (0x00)                                    |
| [1] = Command (0x05)                                      |
| [2] = 0x00                                                |
| [3] = Zone mask (0xFF = all, or 1<<zone)                  |
| [4] = Red                                                 |
| [5] = Green                                               |
| [6] = Blue                                                |
\*---------------------------------------------------------*/
void SteelSeriesRivalController::SendColorPacket(uint8_t zone_mask, const RGBColor& color)
{
    m_packet_buffer.fill(0);

    m_packet_buffer[0] = SteelSeriesRival::REPORT_ID_SETTINGS;
    m_packet_buffer[1] = SteelSeriesRival::CMD_LED_EFFECT;
    m_packet_buffer[2] = 0x00;
    m_packet_buffer[3] = zone_mask;
    m_packet_buffer[4] = color.r;
    m_packet_buffer[5] = color.g;
    m_packet_buffer[6] = color.b;

    // SteelSeries uses feature reports
    SendFeatureReport(m_packet_buffer.data(), 65);
}

/*---------------------------------------------------------*\
| Send Effect Packet (for pulsate mode)                     |
\*---------------------------------------------------------*/
void SteelSeriesRivalController::SendEffectPacket(uint8_t zone, uint8_t effect,
                                                   uint16_t duration_ms,
                                                   const std::vector<RGBColor>& colors)
{
    m_packet_buffer.fill(0);

    m_packet_buffer[0] = SteelSeriesRival::REPORT_ID_SETTINGS;
    m_packet_buffer[1] = SteelSeriesRival::CMD_LED_EFFECT;

    // Effect header (28 bytes)
    m_packet_buffer[2] = zone;
    m_packet_buffer[3] = 0x00;
    m_packet_buffer[4] = effect;
    m_packet_buffer[5] = 0x00;

    // Duration (little-endian 16-bit at offset 6)
    m_packet_buffer[6] = static_cast<uint8_t>(duration_ms & 0xFF);
    m_packet_buffer[7] = static_cast<uint8_t>((duration_ms >> 8) & 0xFF);

    // Repeat flag at offset 22
    m_packet_buffer[22] = 0x01;  // Repeat

    // Color count at offset 27
    m_packet_buffer[27] = static_cast<uint8_t>(colors.size());

    // Colors start at offset 28
    size_t offset = 28;
    for (const auto& c : colors) {
        if (offset + 3 > 64) break;
        m_packet_buffer[offset++] = c.r;
        m_packet_buffer[offset++] = c.g;
        m_packet_buffer[offset++] = c.b;
    }

    SendFeatureReport(m_packet_buffer.data(), 65);
}

/*---------------------------------------------------------*\
| Set Zone Direct                                           |
\*---------------------------------------------------------*/
bool SteelSeriesRivalController::SetZoneDirect(int zone_index, const RGBColor& color)
{
    if (zone_index >= 0 && zone_index < static_cast<int>(m_colors.size())) {
        m_colors[zone_index] = color;
        uint8_t zone_mask = static_cast<uint8_t>(1 << zone_index);
        SendColorPacket(zone_mask, color);
        return true;
    }
    return false;
}

/*---------------------------------------------------------*\
| Save to Device                                            |
\*---------------------------------------------------------*/
bool SteelSeriesRivalController::SaveToDevice()
{
    m_packet_buffer.fill(0);
    m_packet_buffer[0] = SteelSeriesRival::REPORT_ID_SETTINGS;
    m_packet_buffer[1] = SteelSeriesRival::CMD_SAVE;

    return SendFeatureReport(m_packet_buffer.data(), 65);
}

} // namespace OneClickRGB
