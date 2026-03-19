/**
 * ASUS Aura Quick Test - No delays
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
}

hid_device* g_dev = nullptr;

void SetChannelColor(uint8_t channel, uint8_t r, uint8_t g, uint8_t b, int led_count) {
    int offset = 0;
    while (offset < led_count) {
        int send_count = (led_count - offset > 20) ? 20 : (led_count - offset);
        bool is_last = (offset + send_count >= led_count);

        uint8_t buf[64] = {0};
        buf[0] = AuraUSB::MAGIC;
        buf[1] = AuraUSB::CMD_DIRECT;
        buf[2] = channel | (is_last ? AuraUSB::APPLY_FLAG : 0x00);
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

int main(int argc, char* argv[]) {
    // Parse command line: test_asus_quick <r> <g> <b> [leds_per_channel]
    uint8_t r = 255, g = 0, b = 0;
    int leds = 24;

    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }
    if (argc >= 5) {
        leds = atoi(argv[4]);
    }

    std::cout << "Setting RGB(" << (int)r << "," << (int)g << "," << (int)b << ") on " << leds << " LEDs per channel" << std::endl;

    if (hid_init() != 0) return 1;

    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
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

    // Set all 3 channels
    SetChannelColor(0, r, g, b, leds);
    SetChannelColor(1, r, g, b, leds);
    SetChannelColor(2, r, g, b, leds);

    std::cout << "Done!" << std::endl;

    hid_close(g_dev);
    hid_exit();
    return 0;
}
