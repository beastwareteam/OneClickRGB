/**
 * ASUS Aura Config Analyzer
 * Reads and analyzes the config table in detail
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi/hidapi.h>
#define NOMINMAX
#include <windows.h>

namespace AuraUSB {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t AURA_CONTROLLER_PID = 0x19AF;
    constexpr uint8_t MAGIC = 0xEC;
    constexpr uint8_t CMD_CONFIG = 0xB0;
    constexpr uint8_t CMD_DIRECT = 0x40;
    constexpr uint8_t APPLY_FLAG = 0x80;
}

hid_device* g_dev = nullptr;

void PrintHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) std::cout << std::endl << "     ";
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

bool SendPacket(const uint8_t* data, size_t len = 64) {
    int res = hid_write(g_dev, data, len);
    return res > 0;
}

void AnalyzeConfig() {
    std::cout << "=== Analyzing Config Table ===" << std::endl;

    uint8_t buf[64] = {0};
    buf[0] = AuraUSB::MAGIC;
    buf[1] = AuraUSB::CMD_CONFIG;

    SendPacket(buf);

    uint8_t response[64] = {0};
    int res = hid_read_timeout(g_dev, response, 64, 1000);

    if (res <= 0) {
        std::cout << "No response" << std::endl;
        return;
    }

    std::cout << "Full response (" << res << " bytes):" << std::endl;
    PrintHex(response, res);

    std::cout << "\n=== Parsing Config ===" << std::endl;

    // The response format depends on the device
    // Let's print each byte with its interpretation

    std::cout << "\nByte-by-byte analysis:" << std::endl;
    std::cout << "  [0] Magic echo: 0x" << std::hex << (int)response[0] << std::dec << std::endl;
    std::cout << "  [1] Command echo: 0x" << std::hex << (int)response[1] << std::dec << std::endl;

    // Based on the response: ec 30 00 00 1e 9f 03 01 00 00 78 3c 00 01 00 00 78 3c 00 01 00 00 78 3c 00 00 00 00 00 00 00 04
    // Offset 4-5: 0x1e 0x9f = some config values
    // Offset 6: 0x03 = might be number of channels!
    // Offset 7: 0x01 = LED count for channel?

    std::cout << "  [2] Unknown: 0x" << std::hex << (int)response[2] << std::dec << std::endl;
    std::cout << "  [3] Unknown: 0x" << std::hex << (int)response[3] << std::dec << std::endl;
    std::cout << "  [4] Config byte 1: 0x" << std::hex << (int)response[4] << std::dec << " (" << (int)response[4] << ")" << std::endl;
    std::cout << "  [5] Config byte 2: 0x" << std::hex << (int)response[5] << std::dec << " (" << (int)response[5] << ")" << std::endl;
    std::cout << "  [6] Possible num channels: " << (int)response[6] << std::endl;
    std::cout << "  [7] Possible LED count: " << (int)response[7] << std::endl;

    // Look for patterns - repeating structure
    std::cout << "\nLooking for channel structures:" << std::endl;
    for (int i = 4; i < 32; i += 8) {
        std::cout << "  Block at offset " << i << ": ";
        for (int j = 0; j < 8 && (i+j) < res; j++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)response[i+j] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // From the pattern: 1e 9f 03 01 00 00 78 3c | 00 01 00 00 78 3c | 00 01 00 00 78 3c
    // It looks like there's a header (1e 9f) then repeating 6-byte structures

    std::cout << "\n=== Attempting Channel Structure Parse ===" << std::endl;
    // Header: bytes 4-5 (0x1e, 0x9f)
    // Channel count: byte 6 (0x03 = 3 channels!)

    int num_channels = response[6];
    std::cout << "Detected " << num_channels << " channels" << std::endl;

    // Each channel structure seems to be: type, led_count, ??, ??, max_leds_low, max_leds_high
    int offset = 7;
    for (int ch = 0; ch < num_channels && offset < res - 5; ch++) {
        std::cout << "\nChannel " << ch << ":" << std::endl;
        std::cout << "  Type/Flags: 0x" << std::hex << (int)response[offset] << std::dec << std::endl;
        std::cout << "  LED Count: " << (int)response[offset + 1] << std::endl;
        std::cout << "  Byte 3: 0x" << std::hex << (int)response[offset + 2] << std::dec << std::endl;
        std::cout << "  Byte 4: 0x" << std::hex << (int)response[offset + 3] << std::dec << std::endl;
        uint16_t max_leds = response[offset + 4] | (response[offset + 5] << 8);
        std::cout << "  Max LEDs: " << max_leds << " (0x" << std::hex << max_leds << std::dec << ")" << std::endl;
        offset += 6;
    }
}

void TestDirectOnAllChannels() {
    std::cout << "\n=== Testing Direct Mode on All Channels ===" << std::endl;

    // Based on config: 3 channels (0, 1, 2)
    // Try different device IDs to find which respond

    uint8_t test_devices[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t colors_red[24] = {255,0,0, 255,0,0, 255,0,0, 255,0,0, 255,0,0, 255,0,0, 255,0,0, 255,0,0};

    for (uint8_t dev : test_devices) {
        std::cout << "\nTesting device 0x" << std::hex << (int)dev << std::dec << " with RED (8 LEDs)..." << std::endl;

        uint8_t buf[64] = {0};
        buf[0] = AuraUSB::MAGIC;
        buf[1] = AuraUSB::CMD_DIRECT;
        buf[2] = dev | AuraUSB::APPLY_FLAG;  // Device + Apply
        buf[3] = 0;    // Start LED
        buf[4] = 8;    // LED count

        // 8 LEDs = 24 bytes of RGB data
        for (int i = 0; i < 8; i++) {
            buf[5 + i*3 + 0] = 255;  // R
            buf[5 + i*3 + 1] = 0;    // G
            buf[5 + i*3 + 2] = 0;    // B
        }

        std::cout << "TX: ";
        for (int i = 0; i < 32; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << " ";
        }
        std::cout << std::dec << std::endl;

        int res = hid_write(g_dev, buf, 64);
        std::cout << "Result: " << res << std::endl;

        Sleep(500);
    }

    std::cout << "\nDone testing devices. Which ones lit up?" << std::endl;
}

int main() {
    std::cout << "=== ASUS Aura Config Analyzer ===" << std::endl;

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    struct hid_device_info* cur = devs;

    while (cur) {
        if (cur->usage_page == 0xFF72) {
            g_dev = hid_open_path(cur->path);
            if (g_dev) {
                std::wcout << L"Opened: " << cur->product_string << std::endl;
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!g_dev) {
        std::cerr << "Failed to open device" << std::endl;
        hid_exit();
        return 1;
    }

    hid_set_nonblocking(g_dev, 1);

    AnalyzeConfig();
    TestDirectOnAllChannels();

    hid_close(g_dev);
    hid_exit();

    return 0;
}
