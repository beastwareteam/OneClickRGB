/**
 * ASUS Aura Channel Detection Tool v2
 * Uses interrupt transfers instead of feature reports
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <hidapi/hidapi.h>
#include <windows.h>

// ASUS Aura USB Protocol Constants (from OpenRGB)
namespace AuraUSB {
    // Vendor/Product IDs
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t AURA_CONTROLLER_PID = 0x19AF;

    // Commands
    constexpr uint8_t CMD_DIRECT      = 0x40;  // Direct mode control
    constexpr uint8_t CMD_SET         = 0x35;  // Set effect/color
    constexpr uint8_t CMD_APPLY       = 0x35;  // Apply changes
    constexpr uint8_t CMD_FIRMWARE    = 0x82;  // Firmware info
    constexpr uint8_t CMD_CONFIG      = 0xB0;  // Config read/write

    // Magic byte
    constexpr uint8_t MAGIC           = 0xEC;

    // Direct mode
    constexpr uint8_t DIRECT_APPLY    = 0x80;

    // Effect modes
    constexpr uint8_t MODE_OFF        = 0x00;
    constexpr uint8_t MODE_STATIC     = 0x01;
    constexpr uint8_t MODE_BREATHING  = 0x02;
    constexpr uint8_t MODE_COLOR_CYCLE= 0x03;
    constexpr uint8_t MODE_RAINBOW    = 0x04;
}

void PrintHex(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

int WriteInterrupt(hid_device* dev, const uint8_t* data, size_t len) {
    // ASUS uses interrupt transfers, not feature reports
    // hid_write sends to OUT endpoint
    return hid_write(dev, data, len);
}

int ReadInterrupt(hid_device* dev, uint8_t* data, size_t len, int timeout_ms = 500) {
    // hid_read reads from IN endpoint
    return hid_read_timeout(dev, data, len, timeout_ms);
}

void SetDirectMode(hid_device* dev, bool enable) {
    uint8_t buf[65] = {0};
    buf[0] = 0x00;  // Report ID
    buf[1] = AuraUSB::MAGIC;
    buf[2] = AuraUSB::CMD_DIRECT;
    buf[3] = enable ? 0x00 : 0xFF;  // 0x00 = enable direct, 0xFF = disable

    std::cout << (enable ? "Enabling" : "Disabling") << " direct mode..." << std::endl;
    PrintHex(buf, 8, "TX");
    int res = WriteInterrupt(dev, buf, 65);
    std::cout << "Write result: " << res << std::endl;
    Sleep(50);
}

void SetChannelColor(hid_device* dev, uint8_t channel, uint8_t r, uint8_t g, uint8_t b, int num_leds = 8) {
    // According to OpenRGB: Direct mode packet format
    // Byte 0: Report ID (0x00)
    // Byte 1: 0xEC (magic)
    // Byte 2: 0x40 (direct mode command)
    // Byte 3: Channel (0x00-0x07)
    // Byte 4: Apply flag (0x00 or 0x80)
    // Byte 5+: LED colors (R,G,B per LED)

    uint8_t buf[65] = {0};
    buf[0] = 0x00;  // Report ID
    buf[1] = AuraUSB::MAGIC;
    buf[2] = AuraUSB::CMD_DIRECT;
    buf[3] = channel;
    buf[4] = 0x00;  // More packets follow

    // Set colors for each LED
    int offset = 5;
    for (int i = 0; i < num_leds && offset + 2 < 65; i++) {
        buf[offset++] = r;
        buf[offset++] = g;
        buf[offset++] = b;
    }

    std::cout << "\nChannel " << (int)channel << " -> RGB(" << (int)r << "," << (int)g << "," << (int)b << "), LEDs: " << num_leds << std::endl;
    PrintHex(buf, 32, "TX");
    int res = WriteInterrupt(dev, buf, 65);
    std::cout << "Write result: " << res << std::endl;
}

void ApplyDirectMode(hid_device* dev) {
    // Apply/commit direct mode changes
    uint8_t buf[65] = {0};
    buf[0] = 0x00;
    buf[1] = AuraUSB::MAGIC;
    buf[2] = AuraUSB::CMD_DIRECT;
    buf[3] = 0x80;  // Apply flag

    std::cout << "\nApplying changes..." << std::endl;
    PrintHex(buf, 8, "TX");
    int res = WriteInterrupt(dev, buf, 65);
    std::cout << "Write result: " << res << std::endl;
}

void SetEffectMode(hid_device* dev, uint8_t channel, uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t speed = 2) {
    // Effect mode packet (from OpenRGB AuraUSBController.cpp)
    // This sets a hardware effect like static, breathing, rainbow, etc.

    uint8_t buf[65] = {0};
    buf[0] = 0x00;  // Report ID
    buf[1] = AuraUSB::MAGIC;
    buf[2] = AuraUSB::CMD_SET;
    buf[3] = channel;
    buf[4] = mode;      // Effect mode
    buf[5] = r;         // Red
    buf[6] = g;         // Green
    buf[7] = b;         // Blue
    buf[8] = 0x00;      // Direction (for rainbow)
    buf[9] = speed;     // Speed (0-5)

    std::cout << "\nSetting effect mode " << (int)mode << " on channel " << (int)channel << std::endl;
    PrintHex(buf, 16, "TX");
    int res = WriteInterrupt(dev, buf, 65);
    std::cout << "Write result: " << res << std::endl;
}

void TestAllChannels(hid_device* dev) {
    std::cout << "\n=== Testing All Channels with Direct Mode ===" << std::endl;

    // Enable direct mode
    SetDirectMode(dev, true);
    Sleep(100);

    // Test channels 0-7 with RED
    for (int ch = 0; ch < 8; ch++) {
        std::cout << "\n--- Testing Channel " << ch << " ---" << std::endl;
        SetChannelColor(dev, ch, 255, 0, 0, 8);  // Red, 8 LEDs
        Sleep(50);
    }

    ApplyDirectMode(dev);

    std::cout << "\n\nAll channels should now be RED." << std::endl;
    std::cout << "Press Enter to test GREEN..." << std::endl;
    std::cin.get();

    // Green
    for (int ch = 0; ch < 8; ch++) {
        SetChannelColor(dev, ch, 0, 255, 0, 8);  // Green
        Sleep(50);
    }
    ApplyDirectMode(dev);

    std::cout << "All channels should now be GREEN." << std::endl;
    std::cout << "Press Enter to test BLUE..." << std::endl;
    std::cin.get();

    // Blue
    for (int ch = 0; ch < 8; ch++) {
        SetChannelColor(dev, ch, 0, 0, 255, 8);  // Blue
        Sleep(50);
    }
    ApplyDirectMode(dev);

    std::cout << "All channels should now be BLUE." << std::endl;
    std::cout << "Press Enter to test static effect mode..." << std::endl;
    std::cin.get();

    // Disable direct mode and set static effect
    SetDirectMode(dev, false);
    Sleep(100);

    // Set static purple on all channels using effect mode
    for (int ch = 0; ch < 8; ch++) {
        SetEffectMode(dev, ch, AuraUSB::MODE_STATIC, 128, 0, 255, 2);  // Purple
        Sleep(50);
    }

    std::cout << "All channels should now be PURPLE (static effect)." << std::endl;
}

int main() {
    std::cout << "=== ASUS Aura Direct Mode Test v2 ===" << std::endl;

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    // Find ASUS Aura Controller with correct usage page
    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    struct hid_device_info* cur = devs;
    hid_device* dev = nullptr;

    while (cur) {
        std::cout << "Found: Interface=" << cur->interface_number
                  << " UsagePage=0x" << std::hex << cur->usage_page << std::dec << std::endl;

        // Look for vendor-specific usage page (0xFF72 for this device)
        if (cur->usage_page == 0xFF72) {
            std::cout << "Opening vendor-specific interface..." << std::endl;
            dev = hid_open_path(cur->path);
            if (dev) {
                std::wcout << L"Device: " << cur->product_string << std::endl;
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cerr << "Failed to open ASUS Aura Controller" << std::endl;
        hid_exit();
        return 1;
    }

    // Set non-blocking for reads
    hid_set_nonblocking(dev, 1);

    std::cout << "\nDevice opened successfully!" << std::endl;
    std::cout << "Press Enter to start channel test..." << std::endl;
    std::cin.get();

    TestAllChannels(dev);

    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();

    hid_close(dev);
    hid_exit();

    return 0;
}
