/**
 * ASUS Aura Full Test
 * Tests all channels with more LEDs
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
    constexpr uint8_t CMD_DIRECT = 0x40;
    constexpr uint8_t APPLY_FLAG = 0x80;
    constexpr uint8_t MAX_LEDS_PER_PACKET = 20;
}

hid_device* g_dev = nullptr;

void PrintHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len && i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// Send direct color to a channel with multiple packets if needed
void SetChannelColor(uint8_t channel, uint8_t r, uint8_t g, uint8_t b, int led_count) {
    std::cout << "Channel " << (int)channel << ": " << led_count << " LEDs -> RGB("
              << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    int offset = 0;
    while (offset < led_count) {
        int send_count = led_count - offset;
        if (send_count > AuraUSB::MAX_LEDS_PER_PACKET) {
            send_count = AuraUSB::MAX_LEDS_PER_PACKET;
        }

        bool is_last = (offset + send_count >= led_count);

        uint8_t buf[64] = {0};
        buf[0] = AuraUSB::MAGIC;
        buf[1] = AuraUSB::CMD_DIRECT;
        buf[2] = channel | (is_last ? AuraUSB::APPLY_FLAG : 0x00);
        buf[3] = offset;       // Start LED
        buf[4] = send_count;   // LED count

        for (int i = 0; i < send_count; i++) {
            buf[5 + i*3 + 0] = r;
            buf[5 + i*3 + 1] = g;
            buf[5 + i*3 + 2] = b;
        }

        std::cout << "  Packet: LEDs " << offset << "-" << (offset + send_count - 1);
        if (is_last) std::cout << " [APPLY]";
        std::cout << " -> ";
        PrintHex(buf, 16);

        hid_write(g_dev, buf, 64);
        offset += send_count;
        Sleep(10);
    }
}

int main() {
    std::cout << "=== ASUS Aura Full LED Test ===" << std::endl;
    std::cout << "Testing all 3 channels with more LEDs" << std::endl << std::endl;

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

    // Test configuration:
    // Channel 0, 1, 2 = ARGB Headers (each can have up to 60 LEDs)
    // Typical fan: 8-12 LEDs
    // LED strip: 30-60 LEDs

    // Based on your setup (7 fans + LED strips):
    // Let's test with different LED counts

    std::cout << "\n=== Test 1: Setting all channels to RED ===" << std::endl;
    std::cout << "Testing with 24 LEDs per channel (3 fans x 8 LEDs each)" << std::endl << std::endl;

    // Channel 0 - First ARGB header (e.g., 3 fans = 24 LEDs)
    SetChannelColor(0, 255, 0, 0, 24);
    Sleep(100);

    // Channel 1 - Second ARGB header (e.g., 3 fans = 24 LEDs)
    SetChannelColor(1, 255, 0, 0, 24);
    Sleep(100);

    // Channel 2 - Third ARGB header (e.g., LED strip = 30 LEDs)
    SetChannelColor(2, 255, 0, 0, 30);
    Sleep(100);

    std::cout << "\nAll channels set to RED. Waiting 2 seconds..." << std::endl;
    Sleep(2000);

    std::cout << "\n=== Test 2: Setting all channels to GREEN ===" << std::endl;
    SetChannelColor(0, 0, 255, 0, 24);
    SetChannelColor(1, 0, 255, 0, 24);
    SetChannelColor(2, 0, 255, 0, 30);

    std::cout << "\nAll channels set to GREEN. Waiting 2 seconds..." << std::endl;
    Sleep(2000);

    std::cout << "\n=== Test 3: Setting all channels to BLUE (#0022FF) ===" << std::endl;
    SetChannelColor(0, 0x00, 0x22, 0xFF, 24);
    SetChannelColor(1, 0x00, 0x22, 0xFF, 24);
    SetChannelColor(2, 0x00, 0x22, 0xFF, 30);

    std::cout << "\nAll channels set to BLUE (#0022FF)." << std::endl;

    std::cout << "\n=== Test 4: Extended test with 60 LEDs per channel ===" << std::endl;
    std::cout << "Waiting 2 seconds then testing maximum capacity..." << std::endl;
    Sleep(2000);

    // Test with maximum LEDs (60 per channel as reported by config)
    SetChannelColor(0, 255, 128, 0, 60);  // Orange
    SetChannelColor(1, 255, 128, 0, 60);
    SetChannelColor(2, 255, 128, 0, 60);

    std::cout << "\nAll channels set to ORANGE with 60 LEDs each." << std::endl;
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "\nBitte sag mir:" << std::endl;
    std::cout << "1. Welche Fans/LEDs haben auf ROT reagiert?" << std::endl;
    std::cout << "2. Haben alle 7 Fans aufgeleuchtet?" << std::endl;
    std::cout << "3. Haben die LED-Streifen reagiert?" << std::endl;

    hid_close(g_dev);
    hid_exit();

    return 0;
}
