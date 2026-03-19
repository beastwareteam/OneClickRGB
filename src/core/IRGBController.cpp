/*---------------------------------------------------------*\
| OneClickRGB - RGBControllerBase Implementation            |
\*---------------------------------------------------------*/
#include "IRGBController.h"
#include <hidapi.h>
#include <iostream>
#include <cstring>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| RGBControllerBase Destructor                              |
\*---------------------------------------------------------*/
RGBControllerBase::~RGBControllerBase()
{
    Close();
}

/*---------------------------------------------------------*\
| Open HID Device                                           |
\*---------------------------------------------------------*/
bool RGBControllerBase::Open(const std::string& device_path)
{
    if (m_is_open) {
        Close();
    }

    hid_device* dev = hid_open_path(device_path.c_str());
    if (!dev) {
        std::cerr << "  [HID] Failed to open: " << device_path << "\n";
        return false;
    }

    m_hid_device = dev;
    m_is_open = true;
    m_location = device_path;

    std::cout << "  [HID] Opened: " << m_name << "\n";
    return true;
}

/*---------------------------------------------------------*\
| Close HID Device                                          |
\*---------------------------------------------------------*/
void RGBControllerBase::Close()
{
    if (m_hid_device) {
        hid_close(static_cast<hid_device*>(m_hid_device));
        m_hid_device = nullptr;
    }
    m_is_open = false;
}

/*---------------------------------------------------------*\
| Send Packet (Write or Feature Report based on HIDParams)  |
\*---------------------------------------------------------*/
bool RGBControllerBase::SendPacket(const uint8_t* data, size_t length)
{
    if (!m_is_open || !m_hid_device) {
        return false;
    }

    hid_device* dev = static_cast<hid_device*>(m_hid_device);
    int result;

    if (m_hid_params.use_feature_report) {
        result = hid_send_feature_report(dev, data, length);
    } else {
        result = hid_write(dev, data, length);
    }

    return result > 0;
}

/*---------------------------------------------------------*\
| Send Feature Report                                       |
\*---------------------------------------------------------*/
bool RGBControllerBase::SendFeatureReport(const uint8_t* data, size_t length)
{
    if (!m_is_open || !m_hid_device) {
        return false;
    }

    hid_device* dev = static_cast<hid_device*>(m_hid_device);
    int result = hid_send_feature_report(dev, data, length);
    return result > 0;
}

/*---------------------------------------------------------*\
| Get Feature Report                                        |
\*---------------------------------------------------------*/
int RGBControllerBase::GetFeatureReport(uint8_t report_id, uint8_t* buffer, size_t length)
{
    if (!m_is_open || !m_hid_device) {
        return -1;
    }

    buffer[0] = report_id;
    hid_device* dev = static_cast<hid_device*>(m_hid_device);
    return hid_get_feature_report(dev, buffer, length);
}

/*---------------------------------------------------------*\
| Receive Packet                                            |
\*---------------------------------------------------------*/
int RGBControllerBase::ReceivePacket(uint8_t* buffer, size_t length, int timeout_ms)
{
    if (!m_is_open || !m_hid_device) {
        return -1;
    }

    hid_device* dev = static_cast<hid_device*>(m_hid_device);
    return hid_read_timeout(dev, buffer, length, timeout_ms);
}

/*---------------------------------------------------------*\
| Set Mode                                                  |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetMode(int mode_index)
{
    if (mode_index >= 0 && mode_index < static_cast<int>(m_modes.size())) {
        m_active_mode = mode_index;
        return true;
    }
    return false;
}

/*---------------------------------------------------------*\
| Get LED Color                                             |
\*---------------------------------------------------------*/
RGBColor RGBControllerBase::GetLEDColor(int led_index) const
{
    if (led_index >= 0 && led_index < static_cast<int>(m_colors.size())) {
        return m_colors[led_index];
    }
    return RGBColor();
}

/*---------------------------------------------------------*\
| Set LED Color                                             |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetLEDColor(int led_index, const RGBColor& color)
{
    if (led_index >= 0 && led_index < static_cast<int>(m_colors.size())) {
        m_colors[led_index] = color;
        return true;
    }
    return false;
}

/*---------------------------------------------------------*\
| Set All LEDs                                              |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetAllLEDs(const RGBColor& color)
{
    for (auto& c : m_colors) {
        c = color;
    }
    return true;
}

/*---------------------------------------------------------*\
| Set Zone Color                                            |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetZoneColor(int zone_index, const RGBColor& color)
{
    if (zone_index >= 0 && zone_index < static_cast<int>(m_zones.size())) {
        const auto& zone = m_zones[zone_index];
        for (uint16_t i = 0; i < zone.led_count; i++) {
            int led_idx = zone.start_index + i;
            if (led_idx < static_cast<int>(m_colors.size())) {
                m_colors[led_idx] = color;
            }
        }
        return true;
    }
    return false;
}

/*---------------------------------------------------------*\
| Set Brightness                                            |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetBrightness(uint8_t brightness)
{
    m_brightness = brightness > 100 ? 100 : brightness;
    return true;
}

/*---------------------------------------------------------*\
| Set Speed                                                 |
\*---------------------------------------------------------*/
bool RGBControllerBase::SetSpeed(uint8_t speed)
{
    m_speed = speed > 100 ? 100 : speed;
    return true;
}

} // namespace OneClickRGB
