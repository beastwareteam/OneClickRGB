/*
 * HID Debug Tool - Zeigt alle HID-Interfaces mit Details
 */

#include <iostream>
#include <iomanip>
#include <hidapi.h>

#ifdef _WIN32
#include <windows.h>
#endif

std::string WideToString(const wchar_t* wstr) {
    if (!wstr || !wstr[0]) return "";
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
    return result;
#else
    std::string result;
    while (*wstr) {
        if (*wstr < 128) result += static_cast<char>(*wstr);
        wstr++;
    }
    return result;
#endif
}

int main() {
    std::cout << "=== HID Debug Tool ===\n\n";

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return 1;
    }

    // Enumerate all HID devices
    hid_device_info* devices = hid_enumerate(0, 0);
    hid_device_info* current = devices;

    int count = 0;
    while (current) {
        // Filter for our devices
        bool is_target =
            (current->vendor_id == 0x0B05 && current->product_id == 0x19AF) ||  // ASUS Aura
            (current->vendor_id == 0x3299 && current->product_id == 0x4E9F) ||  // EVision KB
            (current->vendor_id == 0x1038 && current->product_id == 0x1724);    // SteelSeries

        if (is_target) {
            std::cout << "----------------------------------------\n";
            std::cout << "Device #" << count++ << "\n";
            std::cout << "  VID:PID:      " << std::hex << std::setfill('0')
                      << std::setw(4) << current->vendor_id << ":"
                      << std::setw(4) << current->product_id << std::dec << "\n";
            std::cout << "  Interface:    " << current->interface_number << "\n";
            std::cout << "  Usage Page:   0x" << std::hex << current->usage_page << std::dec << "\n";
            std::cout << "  Usage:        0x" << std::hex << current->usage << std::dec << "\n";
            std::cout << "  Manufacturer: " << WideToString(current->manufacturer_string) << "\n";
            std::cout << "  Product:      " << WideToString(current->product_string) << "\n";
            std::cout << "  Path:         " << current->path << "\n";

            // Try to open and get report descriptor info
            hid_device* dev = hid_open_path(current->path);
            if (dev) {
                std::cout << "  Status:       OPENABLE\n";

                // Try to read current state
                unsigned char buf[65] = {0};
                hid_set_nonblocking(dev, 1);
                int res = hid_read_timeout(dev, buf, 65, 100);
                if (res > 0) {
                    std::cout << "  Read " << res << " bytes: ";
                    for (int i = 0; i < res && i < 16; i++) {
                        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)buf[i] << " ";
                    }
                    std::cout << std::dec << "\n";
                }

                hid_close(dev);
            } else {
                std::cout << "  Status:       CANNOT OPEN\n";
            }
        }
        current = current->next;
    }

    hid_free_enumeration(devices);
    hid_exit();

    std::cout << "\n=== " << count << " target devices found ===\n";
    return 0;
}
