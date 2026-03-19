/*---------------------------------------------------------*\
| AsusAuraMainboardController.h                             |
|                                                           |
| ASUS Aura Mainboard RGB Controller for OneClickRGB        |
| Based on OpenRGB implementation                           |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <vector>
#include <hidapi.h>

namespace OneClickRGB {

// ASUS Aura USB Commands
#define AURA_REQUEST_FIRMWARE_VERSION   0x82
#define AURA_REQUEST_CONFIG_TABLE       0xB0
#define AURA_CONTROL_MODE_DIRECT        0x40
#define AURA_MAINBOARD_CONTROL_MODE_EFFECT       0x35
#define AURA_MAINBOARD_CONTROL_MODE_EFFECT_COLOR 0x36
#define AURA_MAINBOARD_CONTROL_MODE_COMMIT       0x35

// ASUS Aura Modes
#define AURA_MODE_OFF           0x00
#define AURA_MODE_STATIC        0x01
#define AURA_MODE_BREATHING     0x02
#define AURA_MODE_COLOR_CYCLE   0x03
#define AURA_MODE_RAINBOW       0x04
#define AURA_MODE_DIRECT        0xFF

#define LEDS_PER_PACKET         20

struct AuraDeviceInfo
{
    unsigned char effect_channel;
    unsigned char direct_channel;
    unsigned char num_leds;
    unsigned char num_rgb_headers;
};

class AsusAuraMainboardController
{
public:
    AsusAuraMainboardController(hid_device* dev_handle, const char* path);
    ~AsusAuraMainboardController();

    bool Initialize();

    std::string GetDeviceLocation();
    std::string GetFirmwareVersion();

    // Get device configuration
    unsigned int GetLEDCount();
    unsigned int GetChannelCount();

    // Set colors
    void SetDirectColor(unsigned char red, unsigned char green, unsigned char blue);
    void SetAllLEDs(unsigned char red, unsigned char green, unsigned char blue);
    void SetLED(unsigned int led_idx, unsigned char red, unsigned char green, unsigned char blue);

    // Set colors per channel (for ARGB headers)
    void SetChannelColor(unsigned char channel, unsigned char led_count,
                         unsigned char red, unsigned char green, unsigned char blue);
    void SetAllChannelsColor(unsigned char red, unsigned char green, unsigned char blue,
                             unsigned char leds_per_channel = 60);

    // Set mode
    void SetMode(unsigned char mode, unsigned char red, unsigned char green, unsigned char blue);

    // Apply changes
    void Apply();

private:
    hid_device* dev;
    std::string location;
    char version[17];
    unsigned char config_table[60];

    unsigned char num_leds;
    unsigned char num_rgb_headers;
    unsigned char num_addressable_headers;
    unsigned char num_channels;  // Number of ARGB channels detected

    std::vector<unsigned char> led_colors;  // RGB per LED

    void GetFirmwareVersionFromDevice();
    void GetConfigTable();
    void SendDirect(unsigned char device, unsigned char led_count, unsigned char* colors);
    void SendEffect(unsigned char channel, unsigned char mode);
    void SendColor(unsigned char start_led, unsigned char led_count, unsigned char* led_data);
};

} // namespace OneClickRGB
