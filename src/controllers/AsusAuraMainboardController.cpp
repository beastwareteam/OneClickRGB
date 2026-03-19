/*---------------------------------------------------------*\
| AsusAuraMainboardController.cpp                           |
|                                                           |
| ASUS Aura Mainboard RGB Controller for OneClickRGB        |
| Based on OpenRGB implementation                           |
|                                                           |
| IMPORTANT: This device uses 64-byte packets WITHOUT       |
| Report ID prefix. Do NOT use 65-byte packets!             |
\*---------------------------------------------------------*/

#include "AsusAuraMainboardController.h"
#include <cstring>
#include <iostream>

namespace OneClickRGB {

// Packet size for ASUS Aura USB controllers
// Uses 64-byte packets WITHOUT Report ID (not 65 bytes!)
constexpr size_t AURA_PACKET_SIZE = 64;

AsusAuraMainboardController::AsusAuraMainboardController(hid_device* dev_handle, const char* path)
{
    dev = dev_handle;
    location = path;
    memset(version, 0, sizeof(version));
    memset(config_table, 0, sizeof(config_table));
    num_leds = 0;
    num_rgb_headers = 0;
    num_addressable_headers = 0;
    num_channels = 0;
}

AsusAuraMainboardController::~AsusAuraMainboardController()
{
    if (dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

bool AsusAuraMainboardController::Initialize()
{
    if (!dev)
    {
        std::cerr << "[ASUS Aura] No device handle!\n";
        return false;
    }

    std::cout << "[ASUS Aura] Initializing mainboard controller...\n";

    // Get firmware version
    GetFirmwareVersionFromDevice();
    std::cout << "[ASUS Aura] Firmware: " << version << "\n";

    // Get config table
    GetConfigTable();

    // Parse config table based on actual response format
    // Response: ec 30 00 00 1e 9f 03 01 00 00 78 3c ...
    // Byte 6 (config_table[2]): Number of channels
    num_channels = config_table[0x02];

    // Traditional parsing (may return 0 on some devices)
    num_leds = config_table[0x1B];
    num_rgb_headers = config_table[0x1D];
    num_addressable_headers = config_table[0x02];

    // If traditional parsing fails, use channel count
    if (num_channels > 0 && num_channels <= 8)
    {
        // Each ARGB channel can have up to 60 LEDs (0x3C)
        // Use reasonable defaults for LED count per channel
        if (num_leds == 0)
        {
            num_leds = num_channels * 60;  // Max capacity
        }
        std::cout << "[ASUS Aura] Detected " << (int)num_channels << " ARGB channels\n";
    }

    if (num_leds < num_rgb_headers)
    {
        num_rgb_headers = 0;
    }

    std::cout << "[ASUS Aura] Total LEDs: " << (int)num_leds << "\n";
    std::cout << "[ASUS Aura] RGB Headers: " << (int)num_rgb_headers << "\n";
    std::cout << "[ASUS Aura] ARGB Headers/Channels: " << (int)num_addressable_headers << "\n";

    // Initialize LED color buffer per channel
    // Allocate for all channels with max LEDs
    led_colors.resize(num_channels * 60 * 3, 0);

    // Send Gen1 init command (64 bytes, not 65!)
    unsigned char usb_buf[AURA_PACKET_SIZE];
    memset(usb_buf, 0x00, sizeof(usb_buf));
    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = 0x52;
    usb_buf[0x02] = 0x53;
    usb_buf[0x03] = 0x00;
    usb_buf[0x04] = 0x01;
    hid_write(dev, usb_buf, AURA_PACKET_SIZE);

    return true;
}

std::string AsusAuraMainboardController::GetDeviceLocation()
{
    return "HID: " + location;
}

std::string AsusAuraMainboardController::GetFirmwareVersion()
{
    return std::string(version);
}

unsigned int AsusAuraMainboardController::GetLEDCount()
{
    return num_leds;
}

unsigned int AsusAuraMainboardController::GetChannelCount()
{
    return 1 + num_addressable_headers;
}

void AsusAuraMainboardController::GetFirmwareVersionFromDevice()
{
    unsigned char usb_buf[AURA_PACKET_SIZE];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = AURA_REQUEST_FIRMWARE_VERSION;

    // Use 64-byte packets (no Report ID prefix)
    hid_write(dev, usb_buf, AURA_PACKET_SIZE);
    hid_read(dev, usb_buf, AURA_PACKET_SIZE);

    // Response: ec 02 [firmware string 16 bytes]
    if (usb_buf[1] == 0x02)
    {
        memcpy(version, &usb_buf[2], 16);
    }
}

void AsusAuraMainboardController::GetConfigTable()
{
    unsigned char usb_buf[AURA_PACKET_SIZE];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = AURA_REQUEST_CONFIG_TABLE;

    // Use 64-byte packets (no Report ID prefix)
    hid_write(dev, usb_buf, AURA_PACKET_SIZE);
    hid_read(dev, usb_buf, AURA_PACKET_SIZE);

    // Response: ec 30 00 00 [config data 60 bytes]
    if (usb_buf[1] == 0x30)
    {
        memcpy(config_table, &usb_buf[4], 60);
    }
}

void AsusAuraMainboardController::SetDirectColor(unsigned char red, unsigned char green, unsigned char blue)
{
    // Set all LEDs to the same color
    for (unsigned int i = 0; i < num_leds; i++)
    {
        led_colors[i * 3 + 0] = red;
        led_colors[i * 3 + 1] = green;
        led_colors[i * 3 + 2] = blue;
    }

    // Send direct mode colors
    SendDirect(0x04, num_leds, led_colors.data());
}

void AsusAuraMainboardController::SetAllLEDs(unsigned char red, unsigned char green, unsigned char blue)
{
    SetDirectColor(red, green, blue);
}

void AsusAuraMainboardController::SetLED(unsigned int led_idx, unsigned char red, unsigned char green, unsigned char blue)
{
    if (led_idx < num_leds)
    {
        led_colors[led_idx * 3 + 0] = red;
        led_colors[led_idx * 3 + 1] = green;
        led_colors[led_idx * 3 + 2] = blue;
    }
}

void AsusAuraMainboardController::SetMode(unsigned char mode, unsigned char red, unsigned char green, unsigned char blue)
{
    SendEffect(0, mode);

    if (mode != AURA_MODE_DIRECT)
    {
        // For non-direct modes, set the color
        for (unsigned int i = 0; i < num_leds; i++)
        {
            led_colors[i * 3 + 0] = red;
            led_colors[i * 3 + 1] = green;
            led_colors[i * 3 + 2] = blue;
        }
        SendColor(0, num_leds, led_colors.data());
    }
}

void AsusAuraMainboardController::Apply()
{
    SendDirect(0x04, num_leds, led_colors.data());
}

void AsusAuraMainboardController::SendDirect(unsigned char device, unsigned char led_count, unsigned char* colors)
{
    unsigned char usb_buf[AURA_PACKET_SIZE];
    unsigned char offset = 0;
    unsigned char sent_led_count = LEDS_PER_PACKET;
    bool apply = false;

    while (!apply)
    {
        if (offset + sent_led_count > led_count)
        {
            sent_led_count = led_count - offset;
        }

        if (offset + sent_led_count == led_count)
        {
            apply = true;
        }

        memset(usb_buf, 0x00, sizeof(usb_buf));

        usb_buf[0x00] = 0xEC;
        usb_buf[0x01] = AURA_CONTROL_MODE_DIRECT;
        usb_buf[0x02] = (apply ? 0x80 : 0x00) | device;
        usb_buf[0x03] = offset;
        usb_buf[0x04] = sent_led_count;

        // Copy color data
        for (unsigned char led_idx = 0; led_idx < sent_led_count; led_idx++)
        {
            usb_buf[0x05 + (led_idx * 3) + 0] = colors[(offset + led_idx) * 3 + 0];
            usb_buf[0x05 + (led_idx * 3) + 1] = colors[(offset + led_idx) * 3 + 1];
            usb_buf[0x05 + (led_idx * 3) + 2] = colors[(offset + led_idx) * 3 + 2];
        }

        // IMPORTANT: Use 64-byte packets, NOT 65!
        hid_write(dev, usb_buf, AURA_PACKET_SIZE);

        offset += sent_led_count;
    }
}

void AsusAuraMainboardController::SetChannelColor(unsigned char channel, unsigned char led_count,
                                                   unsigned char red, unsigned char green, unsigned char blue)
{
    // Set color for all LEDs on a specific channel
    std::vector<unsigned char> colors(led_count * 3);
    for (unsigned int i = 0; i < led_count; i++)
    {
        colors[i * 3 + 0] = red;
        colors[i * 3 + 1] = green;
        colors[i * 3 + 2] = blue;
    }
    SendDirect(channel, led_count, colors.data());
}

void AsusAuraMainboardController::SetAllChannelsColor(unsigned char red, unsigned char green, unsigned char blue,
                                                       unsigned char leds_per_channel)
{
    // Set same color on all detected channels
    for (unsigned int ch = 0; ch < num_channels; ch++)
    {
        SetChannelColor(ch, leds_per_channel, red, green, blue);
    }
}

void AsusAuraMainboardController::SendEffect(unsigned char channel, unsigned char mode)
{
    unsigned char usb_buf[AURA_PACKET_SIZE];
    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = AURA_MAINBOARD_CONTROL_MODE_EFFECT;
    usb_buf[0x02] = channel;
    usb_buf[0x03] = 0x00;
    usb_buf[0x04] = 0x00;
    usb_buf[0x05] = mode;

    // IMPORTANT: Use 64-byte packets, NOT 65!
    hid_write(dev, usb_buf, AURA_PACKET_SIZE);
}

void AsusAuraMainboardController::SendColor(unsigned char start_led, unsigned char led_count, unsigned char* led_data)
{
    unsigned short mask = ((1 << led_count) - 1) << start_led;
    unsigned char usb_buf[AURA_PACKET_SIZE];

    memset(usb_buf, 0x00, sizeof(usb_buf));

    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = AURA_MAINBOARD_CONTROL_MODE_EFFECT_COLOR;
    usb_buf[0x02] = mask >> 8;
    usb_buf[0x03] = mask & 0xff;
    usb_buf[0x04] = 0x00;

    memcpy(&usb_buf[0x05 + 3 * start_led], led_data, led_count * 3);

    // IMPORTANT: Use 64-byte packets, NOT 65!
    hid_write(dev, usb_buf, AURA_PACKET_SIZE);
}

} // namespace OneClickRGB
