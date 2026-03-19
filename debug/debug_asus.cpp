/*
 * ASUS Aura Debug - Testet verschiedene Kommunikationsmethoden
 */

#include <iostream>
#include <iomanip>
#include <cstring>
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
    return "";
#endif
}

void PrintHex(const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "=== ASUS Aura Debug Tool ===\n";

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return 1;
    }

    // Liste alle ASUS Interfaces
    std::cout << "\nAll ASUS interfaces:\n";
    hid_device_info* devices = hid_enumerate(0x0B05, 0x19AF);
    hid_device_info* current = devices;

    int idx = 0;
    while (current) {
        std::cout << "\n[" << idx++ << "] Interface " << current->interface_number
                  << ", Usage Page 0x" << std::hex << current->usage_page
                  << ", Usage 0x" << current->usage << std::dec << "\n";
        std::cout << "    Product: " << WideToString(current->product_string) << "\n";
        std::cout << "    Path: " << current->path << "\n";

        hid_device* dev = hid_open_path(current->path);
        if (dev) {
            std::cout << "    Status: OPENED\n";

            unsigned char buf[65] = {0};
            int res;

            // Test 1: Normal write
            std::cout << "    Test write: ";
            buf[0] = 0x00;
            buf[1] = 0xB0;
            res = hid_write(dev, buf, 65);
            std::cout << res << "\n";

            // Test 2: Feature report
            std::cout << "    Test feature report send: ";
            buf[0] = 0xB0;  // Report ID = first byte for feature reports
            res = hid_send_feature_report(dev, buf, 65);
            std::cout << res << "\n";

            if (res > 0) {
                std::cout << "    Feature report get: ";
                memset(buf, 0, 65);
                buf[0] = 0xB0;
                res = hid_get_feature_report(dev, buf, 65);
                std::cout << res << "\n";
                if (res > 0) {
                    std::cout << "    Response: ";
                    PrintHex(buf, 20);
                }
            }

            // Test 3: Try setting a color via write
            std::cout << "\n    Testing color via write...\n";
            memset(buf, 0, 65);
            buf[0] = 0xEC;  // Some ASUS devices use 0xEC
            buf[1] = 0x35;
            buf[2] = 0x00;
            buf[3] = 0x00;
            buf[4] = 0xFF;  // Red
            buf[5] = 0x00;  // Green
            buf[6] = 0x00;  // Blue

            res = hid_write(dev, buf, 65);
            std::cout << "    Result: " << res << "\n";

            // Test 4: OpenRGB Aura USB protocol
            std::cout << "\n    Testing OpenRGB Aura USB protocol...\n";

            // Set Direct Mode
            memset(buf, 0, 65);
            buf[0] = 0x00;
            buf[1] = 0x35;  // Control mode
            buf[2] = 0x00;  // Apply
            buf[3] = 0xFF;  // Brightness
            res = hid_write(dev, buf, 65);
            std::cout << "    Set mode: " << res << "\n";

            // Set colors - try 0x36 command
            memset(buf, 0, 65);
            buf[0] = 0x00;
            buf[1] = 0x36;  // Set color
            buf[2] = 0x00;  // Start LED
            buf[3] = 0x01;  // Count
            buf[4] = 0xFF;  // R
            buf[5] = 0x00;  // G
            buf[6] = 0x00;  // B
            res = hid_write(dev, buf, 65);
            std::cout << "    Set color: " << res << "\n";

            // Apply
            memset(buf, 0, 65);
            buf[0] = 0x00;
            buf[1] = 0x35;
            buf[2] = 0x00;
            buf[3] = 0x00;
            buf[4] = 0x80;  // Apply flag
            res = hid_write(dev, buf, 65);
            std::cout << "    Apply: " << res << "\n";

            // Fehler anzeigen
            const wchar_t* err = hid_error(dev);
            if (err) {
                std::cout << "    Last error: " << WideToString(err) << "\n";
            }

            hid_close(dev);
        } else {
            std::cout << "    Status: CANNOT OPEN\n";
        }

        current = current->next;
    }
    hid_free_enumeration(devices);

    hid_exit();
    return 0;
}
