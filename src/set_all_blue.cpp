/**
 * Set All ASUS Aura LEDs to Blue
 * Quick test - no output buffering
 */

#include <cstring>
#include <cstdint>
#include <hidapi/hidapi.h>

int main() {
    if (hid_init() != 0) return 1;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(0x0B05, 0x19AF);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == 0xFF72) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) return 1;

    // Set all 3 channels with 60 LEDs each to BLUE (#0022FF)
    for (int channel = 0; channel < 3; channel++) {
        int led_count = 60;
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
                buf[5 + i*3 + 0] = 0x00;  // R
                buf[5 + i*3 + 1] = 0x22;  // G
                buf[5 + i*3 + 2] = 0xFF;  // B
            }

            hid_write(dev, buf, 64);
            offset += send_count;
        }
    }

    hid_close(dev);
    hid_exit();
    return 0;
}
