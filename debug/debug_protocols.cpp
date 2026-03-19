/*
 * Protocol Debug Tool - Testet jeden Befehl einzeln
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi.h>

#ifdef _WIN32
#include <windows.h>
#endif

void PrintHex(const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << std::dec << "\n";
}

// Test ASUS Aura Mainboard
void TestAsusAura(hid_device* dev) {
    std::cout << "\n=== Testing ASUS Aura Mainboard ===\n";

    unsigned char buf[65] = {0};
    int res;

    // Test 1: Get Firmware Version (0x82)
    std::cout << "\n[1] Requesting Firmware Version...\n";
    memset(buf, 0, 65);
    buf[0] = 0x00;  // Report ID
    buf[1] = 0x82;  // AURA_REQUEST_FIRMWARE_VERSION

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Send result: " << res << "\n";

    memset(buf, 0, 65);
    buf[0] = 0x82;
    res = hid_get_feature_report(dev, buf, 65);
    std::cout << "  Get result: " << res << "\n";
    if (res > 0) {
        std::cout << "  Response: ";
        PrintHex(buf, 20);
        std::cout << "  Version string: " << (char*)&buf[2] << "\n";
    }

    // Test 2: Get Config Table (0xB0)
    std::cout << "\n[2] Requesting Config Table...\n";
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0xB0;  // AURA_REQUEST_CONFIG_TABLE

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Send result: " << res << "\n";

    memset(buf, 0, 65);
    buf[0] = 0xB0;
    res = hid_get_feature_report(dev, buf, 65);
    std::cout << "  Get result: " << res << "\n";
    if (res > 0) {
        std::cout << "  Response: ";
        PrintHex(buf, 20);
        std::cout << "  LED Count (offset 0x02): " << (int)buf[2] << "\n";
    }

    // Test 3: Set Direct Mode with RED
    std::cout << "\n[3] Setting Direct Mode RED...\n";

    // First packet: Enable direct mode
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x35;  // AURA_CONTROL_MODE_EFFECT - set mode
    buf[2] = 0x00;  // Channel
    buf[3] = 0x00;  // Unknown
    buf[4] = 0x01;  // Direct mode

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Mode set result: " << res << "\n";

    // Color packet
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x36;  // Direct color command
    buf[2] = 0x00;  // Channel
    buf[3] = 0x00;  // Start LED
    buf[4] = 0x01;  // LED count
    buf[5] = 0xFF;  // Red
    buf[6] = 0x00;  // Green
    buf[7] = 0x00;  // Blue

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Color set result: " << res << "\n";

    // Apply
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x35;  // Apply
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x80;  // Apply flag

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Apply result: " << res << "\n";
}

// Test SteelSeries Rival 600
void TestSteelSeries(hid_device* dev) {
    std::cout << "\n=== Testing SteelSeries Rival 600 ===\n";

    unsigned char buf[65] = {0};
    int res;

    // Try multiple packet sizes
    std::cout << "\n[1] Trying 8-byte packet (OpenRGB style)...\n";
    buf[0] = 0x00;  // Report ID (muss bei manchen Geräten 0 sein)
    buf[1] = 0x05;  // Set LED
    buf[2] = 0x00;  // Zone
    buf[3] = 0xFF;  // Red
    buf[4] = 0x00;  // Green
    buf[5] = 0x00;  // Blue

    res = hid_write(dev, buf, 65);
    std::cout << "  Result: " << res << "\n";

    // Try SteelSeries Engine protocol
    std::cout << "\n[2] Trying SteelSeries Engine protocol...\n";
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x08;  // Set color command
    buf[2] = 0x00;  // Zone
    buf[3] = 0x00;
    buf[4] = 0xFF;  // Red
    buf[5] = 0x00;  // Green
    buf[6] = 0x00;  // Blue

    res = hid_write(dev, buf, 65);
    std::cout << "  Result: " << res << "\n";

    // Try feature report
    std::cout << "\n[3] Trying feature report...\n";
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x05;
    buf[2] = 0x00;
    buf[3] = 0xFF;
    buf[4] = 0x00;
    buf[5] = 0x00;

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "  Result: " << res << "\n";
}

int main() {
    std::cout << "=== Protocol Debug Tool ===\n";

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return 1;
    }

    // Find and test ASUS Aura (0x0B05:0x19AF, Interface 2, Usage 0xFF72)
    std::cout << "\nSearching for ASUS Aura...\n";
    hid_device_info* devices = hid_enumerate(0x0B05, 0x19AF);
    hid_device_info* current = devices;

    while (current) {
        if (current->interface_number == 2 && current->usage_page == 0xFF72) {
            std::cout << "Found ASUS Aura at: " << current->path << "\n";
            hid_device* dev = hid_open_path(current->path);
            if (dev) {
                TestAsusAura(dev);
                hid_close(dev);
            } else {
                std::cout << "Failed to open device!\n";
            }
            break;
        }
        current = current->next;
    }
    hid_free_enumeration(devices);

    // Find and test SteelSeries (0x1038:0x1724, Interface 0, Usage 0xFFC0)
    std::cout << "\nSearching for SteelSeries Rival 600...\n";
    devices = hid_enumerate(0x1038, 0x1724);
    current = devices;

    while (current) {
        if (current->interface_number == 0 && current->usage_page == 0xFFC0) {
            std::cout << "Found SteelSeries at: " << current->path << "\n";
            hid_device* dev = hid_open_path(current->path);
            if (dev) {
                TestSteelSeries(dev);
                hid_close(dev);
            } else {
                std::cout << "Failed to open device!\n";
            }
            break;
        }
        current = current->next;
    }
    hid_free_enumeration(devices);

    hid_exit();

    std::cout << "\n=== Debug Complete ===\n";
    std::cout << "Check if any LEDs changed color!\n";

    return 0;
}
