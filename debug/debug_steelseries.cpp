/*
 * SteelSeries Rival 600 Debug
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <hidapi.h>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

void PrintHex(const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "=== SteelSeries Rival 600 Debug ===\n";

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return 1;
    }

    // Find SteelSeries (Interface 0, Usage 0xFFC0)
    hid_device_info* devices = hid_enumerate(0x1038, 0x1724);
    hid_device_info* current = devices;
    hid_device* dev = nullptr;

    std::cout << "\nAll SteelSeries interfaces:\n";
    while (current) {
        std::cout << "  Interface " << current->interface_number
                  << ", Usage 0x" << std::hex << current->usage_page << std::dec << "\n";

        if (current->interface_number == 0 && current->usage_page == 0xFFC0) {
            std::cout << "  -> Using this one!\n";
            dev = hid_open_path(current->path);
        }
        current = current->next;
    }
    hid_free_enumeration(devices);

    if (!dev) {
        std::cerr << "Failed to open device!\n";
        hid_exit();
        return 1;
    }

    unsigned char buf[65] = {0};
    int res;

    // Test verschiedene Paketgrößen und Formate
    std::cout << "\n=== Test 1: 8-byte packet (original) ===\n";
    memset(buf, 0, 8);
    buf[0] = 0x00;  // Report ID
    buf[1] = 0x05;  // Set LED
    buf[2] = 0x00;  // Zone 0
    buf[3] = 0xFF;  // R
    buf[4] = 0x00;  // G
    buf[5] = 0x00;  // B

    std::cout << "Sending: "; PrintHex(buf, 8);
    res = hid_write(dev, buf, 8);
    std::cout << "Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 2: Full RGB command ===\n";
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x05;  // LED command
    buf[2] = 0x00;  // Zone
    buf[3] = 0x01;  // Count
    buf[4] = 0xFF;  // R
    buf[5] = 0x00;  // G
    buf[6] = 0x00;  // B

    std::cout << "Sending: "; PrintHex(buf, 16);
    res = hid_write(dev, buf, 65);
    std::cout << "Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 3: SteelSeries Engine 3 Protocol ===\n";
    // Based on reverse engineering
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x09;  // RGB command for Engine 3
    buf[2] = 0x00;  // Zone ID

    // RGB values
    buf[3] = 0xFF;  // R
    buf[4] = 0x00;  // G
    buf[5] = 0x00;  // B

    std::cout << "Sending: "; PrintHex(buf, 16);
    res = hid_write(dev, buf, 65);
    std::cout << "Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 4: prismSync Protocol ===\n";
    // Alternative protocol
    memset(buf, 0, 65);
    buf[0] = 0x00;
    buf[1] = 0x07;  // prismSync command
    buf[2] = 0x00;
    buf[3] = 0xFF;  // R
    buf[4] = 0x00;  // G
    buf[5] = 0x00;  // B
    buf[6] = 0x64;  // Duration (100ms)

    std::cout << "Sending: "; PrintHex(buf, 16);
    res = hid_write(dev, buf, 65);
    std::cout << "Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 5: OpenRGB Rival Protocol ===\n";
    // From OpenRGB SteelSeriesRivalController
    // Set color for all zones
    for (int zone = 0; zone < 8; zone++) {
        memset(buf, 0, 8);
        buf[0] = 0x05;  // LED command (no report ID prefix for some devices)
        buf[1] = zone;  // Zone
        buf[2] = 0xFF;  // R
        buf[3] = 0x00;  // G
        buf[4] = 0x00;  // B

        std::cout << "Zone " << zone << ": ";
        res = hid_write(dev, buf, 8);
        std::cout << res << "  ";
    }
    std::cout << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 6: Feature Report ===\n";
    memset(buf, 0, 65);
    buf[0] = 0x05;  // Report ID
    buf[1] = 0x00;  // Zone
    buf[2] = 0xFF;  // R
    buf[3] = 0x00;  // G
    buf[4] = 0x00;  // B

    res = hid_send_feature_report(dev, buf, 65);
    std::cout << "Result: " << res << "\n";

    // Fehler anzeigen
    const wchar_t* err = hid_error(dev);
    if (err) {
        char errbuf[256];
        wcstombs(errbuf, err, 256);
        std::cout << "Last error: " << errbuf << "\n";
    }

    hid_close(dev);
    hid_exit();

    std::cout << "\n=== Check your mouse LEDs! ===\n";
    return 0;
}
