/**
 * ASUS Aura Detection v3 - with error diagnostics
 * Tests different write methods and packet formats
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi/hidapi.h>
#include <windows.h>

namespace AuraUSB {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t AURA_CONTROLLER_PID = 0x19AF;
    constexpr uint8_t MAGIC = 0xEC;
}

void PrintHex(const uint8_t* data, size_t len, const std::string& label) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len && i < 32; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "=== ASUS Aura Diagnostic Test v3 ===" << std::endl;
    std::cout << "Checking HID access methods..." << std::endl << std::endl;

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    // Enumerate all interfaces for this device
    std::cout << "=== All Device Interfaces ===" << std::endl;
    struct hid_device_info* devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    struct hid_device_info* cur = devs;

    while (cur) {
        std::cout << "\nInterface " << cur->interface_number << ":" << std::endl;
        std::cout << "  Path: " << cur->path << std::endl;
        std::cout << "  Usage Page: 0x" << std::hex << cur->usage_page << std::dec << std::endl;
        std::cout << "  Usage: 0x" << std::hex << cur->usage << std::dec << std::endl;
        if (cur->product_string) {
            std::wcout << L"  Product: " << cur->product_string << std::endl;
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    std::cout << "\n=== Trying to open different interfaces ===" << std::endl;

    // Try all possible interfaces
    devs = hid_enumerate(AuraUSB::ASUS_VID, AuraUSB::AURA_CONTROLLER_PID);
    cur = devs;

    while (cur) {
        std::cout << "\n--- Testing Interface " << cur->interface_number
                  << " (UsagePage 0x" << std::hex << cur->usage_page << std::dec << ") ---" << std::endl;

        hid_device* dev = hid_open_path(cur->path);
        if (!dev) {
            std::wcout << L"  FAILED to open: " << hid_error(nullptr) << std::endl;
            cur = cur->next;
            continue;
        }

        std::cout << "  Opened successfully!" << std::endl;

        // Test 1: hid_write with 65 bytes (Report ID 0x00 + 64 data)
        {
            uint8_t buf[65] = {0};
            buf[0] = 0x00;  // Report ID
            buf[1] = AuraUSB::MAGIC;
            buf[2] = 0x35;  // CMD
            buf[3] = 0x00;  // Channel 0
            buf[4] = 0x01;  // Static mode
            buf[5] = 0xFF;  // R
            buf[6] = 0x00;  // G
            buf[7] = 0x00;  // B

            std::cout << "\n  Test 1: hid_write(65 bytes, Report ID 0x00)" << std::endl;
            PrintHex(buf, 16, "  TX");
            int res = hid_write(dev, buf, 65);
            std::cout << "  Result: " << res;
            if (res < 0) {
                std::wcout << L" Error: " << hid_error(dev) << std::endl;
            } else {
                std::cout << " OK" << std::endl;
            }
        }

        // Test 2: hid_write with 64 bytes (no Report ID)
        {
            uint8_t buf[64] = {0};
            buf[0] = AuraUSB::MAGIC;
            buf[1] = 0x35;
            buf[2] = 0x00;
            buf[3] = 0x01;
            buf[4] = 0xFF;
            buf[5] = 0x00;
            buf[6] = 0x00;

            std::cout << "\n  Test 2: hid_write(64 bytes, no Report ID)" << std::endl;
            PrintHex(buf, 16, "  TX");
            int res = hid_write(dev, buf, 64);
            std::cout << "  Result: " << res;
            if (res < 0) {
                std::wcout << L" Error: " << hid_error(dev) << std::endl;
            } else {
                std::cout << " OK" << std::endl;
            }
        }

        // Test 3: hid_send_feature_report
        {
            uint8_t buf[65] = {0};
            buf[0] = 0x00;
            buf[1] = AuraUSB::MAGIC;
            buf[2] = 0x35;
            buf[3] = 0x00;
            buf[4] = 0x01;
            buf[5] = 0xFF;
            buf[6] = 0x00;
            buf[7] = 0x00;

            std::cout << "\n  Test 3: hid_send_feature_report(65 bytes)" << std::endl;
            PrintHex(buf, 16, "  TX");
            int res = hid_send_feature_report(dev, buf, 65);
            std::cout << "  Result: " << res;
            if (res < 0) {
                std::wcout << L" Error: " << hid_error(dev) << std::endl;
            } else {
                std::cout << " OK" << std::endl;
            }
        }

        // Test 4: Different packet sizes
        std::cout << "\n  Test 4: Testing different packet sizes..." << std::endl;
        for (int size : {8, 16, 32, 64, 65}) {
            uint8_t buf[65] = {0};
            buf[0] = 0x00;
            buf[1] = AuraUSB::MAGIC;
            buf[2] = 0x35;

            int res = hid_write(dev, buf, size);
            std::cout << "  Size " << size << ": " << res;
            if (res < 0) {
                std::wcout << L" - " << hid_error(dev);
            }
            std::cout << std::endl;
        }

        // Test 5: Get Feature Report (try to read config)
        {
            uint8_t buf[65] = {0};
            buf[0] = 0xB0;  // Report ID for config?

            std::cout << "\n  Test 5: hid_get_feature_report(0xB0)" << std::endl;
            int res = hid_get_feature_report(dev, buf, 65);
            std::cout << "  Result: " << res;
            if (res > 0) {
                PrintHex(buf, res > 32 ? 32 : res, "\n  RX");
            } else if (res < 0) {
                std::wcout << L" Error: " << hid_error(dev) << std::endl;
            }
        }

        // Test 6: Try reading
        {
            std::cout << "\n  Test 6: hid_read_timeout (500ms)" << std::endl;
            uint8_t buf[65] = {0};
            int res = hid_read_timeout(dev, buf, 65, 500);
            std::cout << "  Result: " << res;
            if (res > 0) {
                PrintHex(buf, res > 32 ? 32 : res, "\n  RX");
            } else if (res < 0) {
                std::wcout << L" Error: " << hid_error(dev) << std::endl;
            } else {
                std::cout << " (timeout/no data)" << std::endl;
            }
        }

        hid_close(dev);
        cur = cur->next;
    }

    hid_free_enumeration(devs);
    hid_exit();

    std::cout << "\n=== Diagnostics Complete ===" << std::endl;
    std::cout << "\nNOTE: If all writes fail, ASUS Armoury Crate or Aura software" << std::endl;
    std::cout << "may have exclusive access to the device. Try:" << std::endl;
    std::cout << "  1. Close ASUS Armoury Crate" << std::endl;
    std::cout << "  2. Stop 'ArmouryCrateService' and 'LightingService' in services.msc" << std::endl;
    std::cout << "  3. Run this test again" << std::endl;

    return 0;
}
