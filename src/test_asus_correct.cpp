/**
 * ASUS Aura Correct Protocol Test
 * Uses 64-byte packets WITHOUT Report ID (as discovered in diagnostics)
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <hidapi/hidapi.h>
#include <windows.h>

namespace AuraUSB {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t AURA_CONTROLLER_PID = 0x19AF;
    constexpr uint8_t MAGIC = 0xEC;

    // Commands (from OpenRGB)
    constexpr uint8_t CMD_SET_DIRECT     = 0x40;  // Direct LED control
    constexpr uint8_t CMD_SET_EFFECT     = 0x35;  // Set effect/color
    constexpr uint8_t CMD_FIRMWARE       = 0x82;  // Get firmware
    constexpr uint8_t CMD_CONFIG_GET     = 0x30;  // Get config
    constexpr uint8_t CMD_DIRECT_APPLY   = 0x80;  // Apply direct mode

    // Effect modes
    constexpr uint8_t MODE_OFF           = 0x00;
    constexpr uint8_t MODE_STATIC        = 0x01;
    constexpr uint8_t MODE_BREATHING     = 0x02;
    constexpr uint8_t MODE_COLOR_CYCLE   = 0x03;
    constexpr uint8_t MODE_RAINBOW       = 0x04;
    constexpr uint8_t MODE_FLASH         = 0x05;
}

void PrintHex(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len && i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// IMPORTANT: This device requires 64-byte packets WITHOUT Report ID!
bool SendPacket(hid_device* dev, const uint8_t* data, const std::string& desc) {
    std::cout << desc << std::endl;
    PrintHex(data, 16, "TX");
    int res = hid_write(dev, data, 64);  // 64 bytes, NOT 65!
    std::cout << "Result: " << res;
    if (res < 0) {
        std::wcout << L" Error: " << hid_error(dev);
    }
    std::cout << std::endl;
    return res > 0;
}

// Enable/disable direct LED control mode
void SetDirectMode(hid_device* dev, bool enable) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;      // 0xEC
    buf[1] = AuraUSB::CMD_SET_DIRECT;  // 0x40
    buf[2] = enable ? 0x00 : 0xFF;

    SendPacket(dev, buf, enable ? "Enabling direct mode" : "Disabling direct mode");
    Sleep(50);
}

// Set color for a channel in direct mode
// channel: 0-7
// num_leds: number of LEDs on this channel (default 8 for ARGB strips)
void SetDirectColor(hid_device* dev, uint8_t channel, uint8_t r, uint8_t g, uint8_t b, int num_leds = 8) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;      // 0xEC
    buf[1] = AuraUSB::CMD_SET_DIRECT;  // 0x40
    buf[2] = channel;
    buf[3] = 0x00;  // More packets follow (not last)

    // Fill LED colors (R,G,B per LED)
    int offset = 4;
    for (int i = 0; i < num_leds && offset + 2 < 64; i++) {
        buf[offset++] = r;
        buf[offset++] = g;
        buf[offset++] = b;
    }

    std::stringstream ss;
    ss << "Channel " << (int)channel << " -> RGB(" << (int)r << "," << (int)g << "," << (int)b << ") x" << num_leds << " LEDs";
    SendPacket(dev, buf, ss.str());
}

// Apply direct mode changes
void ApplyDirect(hid_device* dev) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;      // 0xEC
    buf[1] = AuraUSB::CMD_SET_DIRECT;  // 0x40
    buf[2] = AuraUSB::CMD_DIRECT_APPLY;  // 0x80 = Apply

    SendPacket(dev, buf, "Applying direct mode");
    Sleep(50);
}

// Set effect mode (static, breathing, rainbow, etc.)
void SetEffect(hid_device* dev, uint8_t channel, uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t speed = 2) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;      // 0xEC
    buf[1] = AuraUSB::CMD_SET_EFFECT;  // 0x35
    buf[2] = channel;
    buf[3] = mode;      // Effect mode
    buf[4] = r;         // Red
    buf[5] = g;         // Green
    buf[6] = b;         // Blue
    buf[7] = 0x00;      // Direction
    buf[8] = speed;     // Speed (0-5)

    std::stringstream ss;
    ss << "Channel " << (int)channel << " -> Mode " << (int)mode << " RGB(" << (int)r << "," << (int)g << "," << (int)b << ")";
    SendPacket(dev, buf, ss.str());
}

void GetFirmware(hid_device* dev) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_FIRMWARE;

    SendPacket(dev, buf, "Getting firmware info");

    // Try to read response
    uint8_t response[64] = {0};
    int res = hid_read_timeout(dev, response, 64, 500);
    if (res > 0) {
        PrintHex(response, 32, "RX");
        std::cout << "Firmware: ";
        for (int i = 2; i < 32 && response[i] != 0; i++) {
            std::cout << (char)response[i];
        }
        std::cout << std::endl;
    }
}

void GetConfig(hid_device* dev) {
    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_CONFIG_GET;

    SendPacket(dev, buf, "Getting config");

    uint8_t response[64] = {0};
    int res = hid_read_timeout(dev, response, 64, 500);
    if (res > 0) {
        PrintHex(response, 32, "RX");
        std::cout << "Number of channels: " << (int)response[0] << std::endl;
    }
}

int main() {
    std::cout << "=== ASUS Aura Protocol Test (64-byte packets) ===" << std::endl;

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    // Open device with correct usage page
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    struct hid_device_info* cur = devs;

    while (cur) {
        if (cur->usage_page == 0xFF72) {
            dev = hid_open_path(cur->path);
            if (dev) {
                std::wcout << L"Opened: " << cur->product_string << std::endl;
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cerr << "Failed to open device" << std::endl;
        hid_exit();
        return 1;
    }

    hid_set_nonblocking(dev, 1);

    std::cout << "\n=== Device Info ===" << std::endl;
    GetFirmware(dev);
    GetConfig(dev);

    std::cout << "\n=== Direct Mode Test ===" << std::endl;
    std::cout << "Setting all channels to RED..." << std::endl;

    SetDirectMode(dev, true);
    Sleep(100);

    // Test all 8 possible channels
    for (int ch = 0; ch < 8; ch++) {
        SetDirectColor(dev, ch, 255, 0, 0, 8);  // Red, 8 LEDs per channel
        Sleep(20);
    }
    ApplyDirect(dev);

    std::cout << "\nChannels should now be RED." << std::endl;
    std::cout << "Waiting 2 seconds then changing to GREEN..." << std::endl;
    Sleep(2000);

    // Green
    for (int ch = 0; ch < 8; ch++) {
        SetDirectColor(dev, ch, 0, 255, 0, 8);
        Sleep(20);
    }
    ApplyDirect(dev);

    std::cout << "Channels should now be GREEN." << std::endl;
    std::cout << "Waiting 2 seconds then changing to BLUE..." << std::endl;
    Sleep(2000);

    // Blue
    for (int ch = 0; ch < 8; ch++) {
        SetDirectColor(dev, ch, 0, 0, 255, 8);
        Sleep(20);
    }
    ApplyDirect(dev);

    std::cout << "Channels should now be BLUE." << std::endl;
    std::cout << "\nWaiting 2 seconds then testing effect mode..." << std::endl;
    Sleep(2000);

    // Disable direct mode, use effect mode
    SetDirectMode(dev, false);
    Sleep(100);

    std::cout << "\n=== Effect Mode Test ===" << std::endl;
    std::cout << "Setting static PURPLE on all channels..." << std::endl;

    for (int ch = 0; ch < 8; ch++) {
        SetEffect(dev, ch, AuraUSB::MODE_STATIC, 128, 0, 255, 2);  // Purple static
        Sleep(50);
    }

    std::cout << "\nTest complete!" << std::endl;
    std::cout << "You should have seen: RED -> GREEN -> BLUE -> PURPLE" << std::endl;

    hid_close(dev);
    hid_exit();

    return 0;
}
