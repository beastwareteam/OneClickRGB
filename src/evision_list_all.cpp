// List ALL HID interfaces for EVision keyboard
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>

#define EVISION_VID 0x3299
#define EVISION_PID 0x4E9F

int main() {
    std::cout << "=== All EVision HID Interfaces ===" << std::endl;

    hid_init();

    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    int count = 0;
    for (auto* cur = devs; cur; cur = cur->next) {
        count++;
        std::cout << "\nInterface #" << count << ":" << std::endl;
        std::cout << "  Path: " << cur->path << std::endl;
        std::cout << "  Usage Page: 0x" << std::hex << cur->usage_page << std::dec << std::endl;
        std::cout << "  Usage: 0x" << std::hex << cur->usage << std::dec << std::endl;
        std::cout << "  Interface: " << cur->interface_number << std::endl;
        if (cur->product_string) {
            std::wcout << L"  Product: " << cur->product_string << std::endl;
        }
    }
    hid_free_enumeration(devs);

    std::cout << "\nTotal interfaces: " << count << std::endl;

    hid_exit();
    return 0;
}
