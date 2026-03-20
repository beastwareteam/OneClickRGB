/*---------------------------------------------------------*\
| HIDController.cpp                                         |
|                                                           |
| Real HIDAPI implementation for HID communication          |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "HIDController.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <memory>
#include <hidapi.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Static HIDAPI initialization                              |
\*---------------------------------------------------------*/
static bool g_hidapi_initialized = false;

static bool EnsureHIDAPIInit()
{
    if (!g_hidapi_initialized)
    {
        if (hid_init() == 0)
        {
            g_hidapi_initialized = true;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/*---------------------------------------------------------*\
| HIDController Implementation                              |
\*---------------------------------------------------------*/
HIDController::HIDController()
{
    EnsureHIDAPIInit();
}

HIDController::~HIDController()
{
    Close();
}

bool HIDController::Open(uint16_t vendor_id, uint16_t product_id, int interface_number)
{
    if (is_open)
    {
        Close();
    }

    if (!EnsureHIDAPIInit())
    {
        last_error = "Failed to initialize HIDAPI";
        return false;
    }

    // Enumerate and find the device with correct interface
    hid_device_info* devices = hid_enumerate(vendor_id, product_id);
    hid_device_info* current = devices;

    while (current)
    {
        if (interface_number < 0 || current->interface_number == interface_number)
        {
            dev = hid_open_path(current->path);
            if (dev)
            {
                device_path = current->path;
                break;
            }
        }
        current = current->next;
    }

    hid_free_enumeration(devices);

    if (dev)
    {
        is_open = true;
        hid_set_nonblocking(dev, 1);
        std::cout << "  [HID] Opened device "
                  << std::hex << vendor_id << ":" << product_id << std::dec << "\n";
        return true;
    }

    last_error = "Failed to open HID device";
    return false;
}

bool HIDController::Open(const std::string& path)
{
    if (is_open)
    {
        Close();
    }

    if (!EnsureHIDAPIInit())
    {
        last_error = "Failed to initialize HIDAPI";
        return false;
    }

    dev = hid_open_path(path.c_str());

    if (dev)
    {
        device_path = path;
        is_open = true;
        hid_set_nonblocking(dev, 1);
        std::cout << "  [HID] Opened device at path\n";
        return true;
    }

    last_error = "Failed to open device at path";
    return false;
}

void HIDController::Close()
{
    if (dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
    is_open = false;
    device_path.clear();
}

bool HIDController::IsOpen() const
{
    return is_open && dev != nullptr;
}

bool HIDController::Write(const uint8_t* data, size_t length)
{
    if (!dev)
    {
        last_error = "Device not open";
        return false;
    }

    int result = hid_write(dev, data, length);

    if (result < 0)
    {
        const wchar_t* err = hid_error(dev);
        if (err)
        {
            last_error = WideToString(err);
        }
        return false;
    }

    return true;
}

bool HIDController::Write(const std::vector<uint8_t>& data)
{
    return Write(data.data(), data.size());
}

bool HIDController::SendFeatureReport(const uint8_t* data, size_t length)
{
    if (!dev)
    {
        last_error = "Device not open";
        return false;
    }

    int result = hid_send_feature_report(dev, data, length);

    if (result < 0)
    {
        const wchar_t* err = hid_error(dev);
        if (err)
        {
            last_error = WideToString(err);
        }
        return false;
    }

    return true;
}

bool HIDController::SendFeatureReport(const std::vector<uint8_t>& data)
{
    return SendFeatureReport(data.data(), data.size());
}

int HIDController::Read(uint8_t* data, size_t length, int timeout_ms)
{
    if (!dev)
    {
        return -1;
    }

    return hid_read_timeout(dev, data, length, timeout_ms);
}

std::vector<uint8_t> HIDController::Read(size_t length, int timeout_ms)
{
    std::vector<uint8_t> buffer(length);

    int result = Read(buffer.data(), length, timeout_ms);

    if (result > 0)
    {
        buffer.resize(result);
    }
    else
    {
        buffer.clear();
    }

    return buffer;
}

int HIDController::GetFeatureReport(uint8_t report_id, uint8_t* data, size_t length)
{
    if (!dev)
    {
        return -1;
    }

    data[0] = report_id;
    return hid_get_feature_report(dev, data, length);
}

std::string HIDController::GetManufacturer() const
{
    if (!dev) return "";

    wchar_t buffer[256];
    if (hid_get_manufacturer_string(dev, buffer, 256) == 0)
    {
        return WideToString(buffer);
    }
    return "";
}

std::string HIDController::GetProduct() const
{
    if (!dev) return "";

    wchar_t buffer[256];
    if (hid_get_product_string(dev, buffer, 256) == 0)
    {
        return WideToString(buffer);
    }
    return "";
}

std::string HIDController::GetSerialNumber() const
{
    if (!dev) return "";

    wchar_t buffer[256];
    if (hid_get_serial_number_string(dev, buffer, 256) == 0)
    {
        return WideToString(buffer);
    }
    return "";
}

std::string HIDController::WideToString(const wchar_t* wstr) const
{
    if (!wstr || !wstr[0]) return "";

#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    std::string result;
    while (*wstr)
    {
        if (*wstr < 128)
        {
            result += static_cast<char>(*wstr);
        }
        wstr++;
    }
    return result;
#endif
}

/*---------------------------------------------------------*\
| GenericRGBProtocol Implementation                         |
\*---------------------------------------------------------*/
bool GenericRGBProtocol::SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b)
{
    // Generic protocol - just log what we would do
    std::cout << "  [GenericProto] SetAllColors RGB("
              << (int)r << "," << (int)g << "," << (int)b << ")\n";
    return true;
}

/*---------------------------------------------------------*\
| AsusAuraMainboardProtocol Implementation                  |
| Based on OpenRGB AsusAuraMainboardController              |
\*---------------------------------------------------------*/
bool AsusAuraMainboardProtocol::Initialize(HIDController& hid)
{
    std::cout << "  [AuraMainboard] Initializing with 0xEC protocol...\n";

    // ASUS Aura USB uses 0xEC as magic/report ID byte
    // All packets must start with 0xEC

    num_leds = 12;  // Default LED count
    led_colors.resize(num_leds * 3, 0);

    std::cout << "  [AuraMainboard] Ready with " << (int)num_leds << " LEDs\n";
    return true;
}

bool AsusAuraMainboardProtocol::SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < num_leds; i++)
    {
        led_colors[i * 3 + 0] = r;
        led_colors[i * 3 + 1] = g;
        led_colors[i * 3 + 2] = b;
    }
    return true;
}

bool AsusAuraMainboardProtocol::SetLEDColor(HIDController& hid, int led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= 0 && led_index < num_leds)
    {
        led_colors[led_index * 3 + 0] = r;
        led_colors[led_index * 3 + 1] = g;
        led_colors[led_index * 3 + 2] = b;
        return true;
    }
    return false;
}

bool AsusAuraMainboardProtocol::SetMode(HIDController& hid, int mode_index)
{
    current_mode = static_cast<uint8_t>(mode_index);
    return true;
}

bool AsusAuraMainboardProtocol::SetBrightness(HIDController& hid, uint8_t brightness)
{
    current_brightness = brightness;
    return true;
}

bool AsusAuraMainboardProtocol::Apply(HIDController& hid)
{
    // ASUS Aura USB protocol - all packets start with 0xEC magic byte
    unsigned char usb_buf[65] = {0};

    // Step 1: Send direct mode color packet
    // Format: 0xEC, 0x40, flags, start_led, count, R,G,B, R,G,B, ...
    memset(usb_buf, 0, 65);
    usb_buf[0] = 0xEC;  // Magic byte
    usb_buf[1] = 0x40;  // Direct mode command
    usb_buf[2] = 0x80;  // Apply flag (0x80 = final packet)
    usb_buf[3] = 0x00;  // Start LED
    usb_buf[4] = static_cast<uint8_t>(num_leds);  // LED count

    // Fill RGB data
    for (int i = 0; i < num_leds && i < 20; i++)
    {
        usb_buf[5 + i * 3 + 0] = led_colors[i * 3 + 0];  // R
        usb_buf[5 + i * 3 + 1] = led_colors[i * 3 + 1];  // G
        usb_buf[5 + i * 3 + 2] = led_colors[i * 3 + 2];  // B
    }

    bool result = hid.Write(usb_buf, 65);

    std::cout << "  [AuraMainboard] Applied RGB("
              << (int)led_colors[0] << "," << (int)led_colors[1] << "," << (int)led_colors[2]
              << ") to " << (int)num_leds << " LEDs - " << (result ? "OK" : "FAIL") << "\n";

    return result;
}

/*---------------------------------------------------------*\
| SteelSeriesRival600Protocol Implementation                |
| Based on OpenRGB SteelSeriesRivalController               |
\*---------------------------------------------------------*/
bool SteelSeriesRival600Protocol::Initialize(HIDController& hid)
{
    std::cout << "  [SteelSeriesRival600] Initializing...\n";
    // Initialize all zones to off
    memset(zone_colors, 0, sizeof(zone_colors));
    return true;
}

bool SteelSeriesRival600Protocol::SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < NUM_ZONES; i++)
    {
        zone_colors[i][0] = r;
        zone_colors[i][1] = g;
        zone_colors[i][2] = b;
    }
    return true;
}

bool SteelSeriesRival600Protocol::SetLEDColor(HIDController& hid, int led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= 0 && led_index < NUM_ZONES)
    {
        zone_colors[led_index][0] = r;
        zone_colors[led_index][1] = g;
        zone_colors[led_index][2] = b;
        return true;
    }
    return false;
}

bool SteelSeriesRival600Protocol::Apply(HIDController& hid)
{
    // SteelSeries Rival 600 protocol
    // 7-byte packet WITHOUT report ID prefix: 0x1C, 0x27, 0x00, zone_mask, R, G, B
    unsigned char usb_pkt[65] = {0};

    // Try feature report method - command 0x05 for LED control
    usb_pkt[0] = 0x05;  // Report ID / Command
    usb_pkt[1] = 0x00;  // Zone (all)
    usb_pkt[2] = zone_colors[0][0];  // R
    usb_pkt[3] = zone_colors[0][1];  // G
    usb_pkt[4] = zone_colors[0][2];  // B

    bool result = hid.SendFeatureReport(usb_pkt, 65);

    if (!result) {
        // Fallback: Try normal write with different format
        memset(usb_pkt, 0, 65);
        usb_pkt[0] = 0x00;  // Report ID
        usb_pkt[1] = 0x05;  // LED command
        usb_pkt[2] = 0x00;  // Zone
        usb_pkt[3] = zone_colors[0][0];  // R
        usb_pkt[4] = zone_colors[0][1];  // G
        usb_pkt[5] = zone_colors[0][2];  // B

        result = hid.Write(usb_pkt, 65);
    }

    std::cout << "  [SteelSeriesRival600] Applied RGB(" << (int)zone_colors[0][0]
              << "," << (int)zone_colors[0][1] << "," << (int)zone_colors[0][2]
              << ") - " << (result ? "OK" : "FAIL") << "\n";
    return result;
}

/*---------------------------------------------------------*\
| EVisionKeyboardProtocol Implementation                    |
| Based on OpenRGB EVisionKeyboardController                |
\*---------------------------------------------------------*/
bool EVisionKeyboardProtocol::Initialize(HIDController& hid)
{
    std::cout << "  [EVisionKeyboard] Initializing...\n";
    return true;
}

bool EVisionKeyboardProtocol::SetAllColors(HIDController& hid, uint8_t r, uint8_t g, uint8_t b)
{
    current_r = r;
    current_g = g;
    current_b = b;
    return true;
}

bool EVisionKeyboardProtocol::SetMode(HIDController& hid, int mode_index)
{
    switch (mode_index)
    {
        case 0: current_mode = EVISION_MODE_STATIC;    break;
        case 1: current_mode = EVISION_MODE_BREATHING; break;
        case 2: current_mode = EVISION_MODE_SPECTRUM;  break;
        case 3: current_mode = EVISION_MODE_WAVE;      break;
        case 4: current_mode = EVISION_MODE_REACTIVE;  break;
        default: current_mode = EVISION_MODE_STATIC;   break;
    }
    return true;
}

bool EVisionKeyboardProtocol::SetBrightness(HIDController& hid, uint8_t brightness)
{
    current_brightness = brightness;
    return true;
}

void EVisionKeyboardProtocol::ComputeChecksum(unsigned char* usb_buf)
{
    // EVision checksum: sum of bytes 0x03 to 0x3F, stored in bytes 0x01-0x02
    unsigned short checksum = 0;
    for (int i = 0x03; i < 64; i++)
    {
        checksum += usb_buf[i];
    }
    usb_buf[0x01] = checksum & 0xFF;
    usb_buf[0x02] = checksum >> 8;
}

bool EVisionKeyboardProtocol::SendCommand(HIDController& hid, uint8_t cmd, const uint8_t* data, size_t len)
{
    unsigned char usb_buf[64] = {0};
    usb_buf[0x00] = 0x04;  // EVision header
    usb_buf[0x03] = cmd;

    if (data && len > 0)
    {
        size_t copy_len = (std::min)(len, size_t(56));
        memcpy(&usb_buf[0x08], data, copy_len);
    }

    ComputeChecksum(usb_buf);
    return hid.Write(usb_buf, 64);
}

uint8_t EVisionKeyboardProtocol::CalculateChecksum(const uint8_t* data, size_t len)
{
    // Not used anymore - kept for interface compatibility
    return 0;
}

bool EVisionKeyboardProtocol::Apply(HIDController& hid)
{
    unsigned char usb_buf[64] = {0};
    unsigned char read_buf[64] = {0};

    // Send BEGIN command
    memset(usb_buf, 0, 64);
    usb_buf[0x00] = 0x04;
    usb_buf[0x01] = EVISION_CMD_BEGIN;
    usb_buf[0x02] = 0x00;
    usb_buf[0x03] = EVISION_CMD_BEGIN;
    hid.Write(usb_buf, 64);
    hid.Read(read_buf, 64, 100);

    // Set mode parameters - EVision format
    memset(usb_buf, 0, 64);
    usb_buf[0x00] = 0x04;
    usb_buf[0x03] = EVISION_CMD_SET_PARAMETER;
    usb_buf[0x04] = 0x08;  // Parameter size
    usb_buf[0x05] = 0x00;  // Parameter ID (0 = mode settings)

    usb_buf[0x08] = current_mode;
    usb_buf[0x09] = current_brightness * 4 / 100;  // 0-4 brightness range
    usb_buf[0x0A] = 0x03;  // Speed (normal)
    usb_buf[0x0B] = 0x00;  // Direction
    usb_buf[0x0C] = 0x00;  // Random color flag
    usb_buf[0x0D] = current_r;
    usb_buf[0x0E] = current_g;
    usb_buf[0x0F] = current_b;

    ComputeChecksum(usb_buf);
    hid.Write(usb_buf, 64);
    hid.Read(read_buf, 64, 100);

    // Send END command
    memset(usb_buf, 0, 64);
    usb_buf[0x00] = 0x04;
    usb_buf[0x01] = EVISION_CMD_END;
    usb_buf[0x02] = 0x00;
    usb_buf[0x03] = EVISION_CMD_END;
    hid.Write(usb_buf, 64);
    hid.Read(read_buf, 64, 100);

    std::cout << "  [EVisionKeyboard] Applied mode " << (int)current_mode
              << " brightness " << (int)(current_brightness * 4 / 100)
              << " RGB(" << (int)current_r << "," << (int)current_g << "," << (int)current_b << ")\n";
    return true;
}

/*---------------------------------------------------------*\
| ProtocolFactory Implementation                            |
\*---------------------------------------------------------*/
std::unique_ptr<RGBProtocol> ProtocolFactory::Create(const std::string& controller_name)
{
    if (controller_name == "AsusAuraMainboard")
    {
        return std::make_unique<AsusAuraMainboardProtocol>();
    }
    else if (controller_name == "SteelSeriesRival" || controller_name == "SteelSeriesRival600")
    {
        return std::make_unique<SteelSeriesRival600Protocol>();
    }
    else if (controller_name == "EVisionKeyboard")
    {
        return std::make_unique<EVisionKeyboardProtocol>();
    }

    // Default to generic
    std::cout << "  [ProtocolFactory] No specific protocol for '" << controller_name
              << "', using generic\n";
    return std::make_unique<GenericRGBProtocol>();
}

} // namespace OneClickRGB
