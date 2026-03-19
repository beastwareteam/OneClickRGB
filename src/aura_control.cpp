/**
 * ASUS Aura Control Tool
 * Usage: aura_control <r> <g> <b> [leds_per_channel]
 * Default: aura_control 0 34 255 60 (Blue #0022FF, 60 LEDs)
 */

#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <hidapi/hidapi.h>

int main(int argc, char* argv[]) {
    // Parse arguments
    uint8_t r = 0x00, g = 0x22, b = 0xFF;  // Default: Blue #0022FF
    int leds_per_channel = 60;

    if (argc >= 4) {
        r = (uint8_t)atoi(argv[1]);
        g = (uint8_t)atoi(argv[2]);
        b = (uint8_t)atoi(argv[3]);
    }
    if (argc >= 5) {
        leds_per_channel = atoi(argv[4]);
    }

    printf("Setting RGB(%d,%d,%d) on %d LEDs x 3 channels\n", r, g, b, leds_per_channel);
    fflush(stdout);

    if (hid_init() != 0) {
        printf("HID init failed\n");
        return 1;
    }

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(0x0B05, 0x19AF);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == 0xFF72) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        printf("Device not found\n");
        return 1;
    }

    printf("Device opened\n");
    fflush(stdout);

    // Set all 3 channels
    for (int channel = 0; channel < 3; channel++) {
        int offset = 0;
        while (offset < leds_per_channel) {
            int send_count = (leds_per_channel - offset > 20) ? 20 : (leds_per_channel - offset);
            bool is_last = (offset + send_count >= leds_per_channel);

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

            hid_write(dev, buf, 64);
            offset += send_count;
        }
        printf("Channel %d: %d LEDs\n", channel, leds_per_channel);
        fflush(stdout);
    }

    printf("Total: %d LEDs set\n", leds_per_channel * 3);

    hid_close(dev);
    hid_exit();
    return 0;
}
