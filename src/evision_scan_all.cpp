// Scan ALL HID devices for potential RGB controllers
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>

int main() {
    std::cout << "=== Scanning ALL HID Devices ===" << std::endl << std::endl;

    hid_init();

    // Enumerate ALL HID devices
    struct hid_device_info* devs = hid_enumerate(0, 0);
    int count = 0;

    for (auto* cur = devs; cur; cur = cur->next) {
        // Look for vendor-specific usage pages (0xFF00+) or SONiX devices
        bool interesting = (cur->usage_page >= 0xFF00) ||
                          (cur->vendor_id == 0x3299) ||
                          (cur->vendor_id == 0x0C45);  // SONiX VID

        if (interesting) {
            count++;
            std::cout << "Device #" << count << ":" << std::endl;
            std::cout << "  VID: 0x" << std::hex << std::setfill('0') << std::setw(4) << cur->vendor_id;
            std::cout << "  PID: 0x" << std::setw(4) << cur->product_id << std::dec << std::endl;
            std::cout << "  Path: " << cur->path << std::endl;
            std::cout << "  Usage Page: 0x" << std::hex << std::setfill('0') << std::setw(4) << cur->usage_page << std::dec << std::endl;
            std::cout << "  Usage: 0x" << std::hex << std::setfill('0') << std::setw(4) << cur->usage << std::dec << std::endl;
            std::cout << "  Interface: " << cur->interface_number << std::endl;
            if (cur->product_string) {
                std::wcout << L"  Product: " << cur->product_string << std::endl;
            }
            std::cout << std::endl;
        }
    }

    hid_free_enumeration(devs);

    std::cout << "Total interesting devices: " << count << std::endl;

    hid_exit();
    return 0;
}
