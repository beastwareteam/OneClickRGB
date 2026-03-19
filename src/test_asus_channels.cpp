/**
 * ASUS Aura Channel Test
 * Tests each channel individually with different colors
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi/hidapi.h>
#define NOMINMAX
#include <windows.h>

hid_device* g_dev = nullptr;

void SetChannel(uint8_t channel, uint8_t r, uint8_t g, uint8_t b, int led_count) {
    int offset = 0;
    while (offset < led_count) {
        int send_count = (led_count - offset > 20) ? 20 : (led_count - offset);
        bool is_last = (offset + send_count >= led_count);

        uint8_t buf[64] = {0};
        buf[0] = 0xEC;
        buf[1] = 0x40;
        buf[2] = channel | (is_last ? 0x80 : 0x00);
        buf[3] = offset;
        buf[4] = send_count;

        for (int i = 0; i < send_count; i++) {
            buf[5 + i*3 + 0] = r;
            buf[5 + i*3 + 1] = g;
            buf[5 + i*3 + 2] = b;
        }

        hid_write(g_dev, buf, 64);
        offset += send_count;
    }
}

int main() {
    std::cout << "=== ASUS Aura Channel Test ===" << std::endl;

    if (hid_init() != 0) return 1;

    struct hid_device_info* devs = hid_enumerate(0x0B05, 0x19AF);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == 0xFF72) {
            g_dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!g_dev) {
        std::cerr << "Device not found" << std::endl;
        return 1;
    }

    std::cout << "Device opened!" << std::endl;

    // Test each channel separately with different colors
    // This helps identify which channel controls which fans/LEDs

    std::cout << "\n=== Channel 0: RED (60 LEDs) ===" << std::endl;
    SetChannel(0, 255, 0, 0, 60);
    std::cout << "Channel 0 should be RED now. Press Enter for next channel..." << std::endl;
    std::cin.get();

    std::cout << "\n=== Channel 1: GREEN (60 LEDs) ===" << std::endl;
    SetChannel(1, 0, 255, 0, 60);
    std::cout << "Channel 1 should be GREEN now. Press Enter for next channel..." << std::endl;
    std::cin.get();

    std::cout << "\n=== Channel 2: BLUE (60 LEDs) ===" << std::endl;
    SetChannel(2, 0, 0, 255, 60);
    std::cout << "Channel 2 should be BLUE now. Press Enter to set all to BLUE..." << std::endl;
    std::cin.get();

    std::cout << "\n=== All channels: BLUE #0022FF (60 LEDs each) ===" << std::endl;
    SetChannel(0, 0x00, 0x22, 0xFF, 60);
    SetChannel(1, 0x00, 0x22, 0xFF, 60);
    SetChannel(2, 0x00, 0x22, 0xFF, 60);
    std::cout << "All channels should be BLUE now!" << std::endl;

    hid_close(g_dev);
    hid_exit();

    return 0;
}
