/**
 * ASUS Aura Debug Test
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi/hidapi.h>
#define NOMINMAX
#include <windows.h>

int main() {
    std::cout << "Starting..." << std::endl;

    if (hid_init() != 0) {
        std::cerr << "HID init failed" << std::endl;
        return 1;
    }
    std::cout << "HID initialized" << std::endl;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(0x0B05, 0x19AF);
    std::cout << "Enumerated" << std::endl;

    for (auto* cur = devs; cur; cur = cur->next) {
        std::cout << "Found interface " << cur->interface_number
                  << " usage 0x" << std::hex << cur->usage_page << std::dec << std::endl;
        if (cur->usage_page == 0xFF72) {
            std::cout << "Opening..." << std::endl;
            dev = hid_open_path(cur->path);
            if (dev) {
                std::cout << "Opened!" << std::endl;
                break;
            }
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cerr << "No device" << std::endl;
        hid_exit();
        return 1;
    }

    // Single packet test
    uint8_t buf[64] = {0};
    buf[0] = 0xEC;
    buf[1] = 0x40;
    buf[2] = 0x80;  // Channel 0 + Apply
    buf[3] = 0;     // Start LED
    buf[4] = 8;     // 8 LEDs

    // Red
    for (int i = 0; i < 8; i++) {
        buf[5 + i*3] = 255;
    }

    std::cout << "Sending..." << std::endl;
    std::cout << "TX: ";
    for (int i = 0; i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << " ";
    }
    std::cout << std::dec << std::endl;

    int res = hid_write(dev, buf, 64);
    std::cout << "Result: " << res << std::endl;

    if (res < 0) {
        std::wcout << L"Error: " << hid_error(dev) << std::endl;
    }

    std::cout << "Closing..." << std::endl;
    hid_close(dev);
    hid_exit();
    std::cout << "Done!" << std::endl;

    return 0;
}
