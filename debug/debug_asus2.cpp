/*
 * ASUS Aura Debug v2 - Mit Report ID 0xEC
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
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << std::dec << "\n";
}

int main() {
    std::cout << "=== ASUS Aura Debug v2 ===\n";
    std::cout << "Using Report ID 0xEC\n\n";

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return 1;
    }

    // Find ASUS Aura (Interface 2, Usage 0xFF72)
    hid_device_info* devices = hid_enumerate(0x0B05, 0x19AF);
    hid_device_info* current = devices;
    hid_device* dev = nullptr;

    while (current) {
        if (current->interface_number == 2 && current->usage_page == 0xFF72) {
            std::cout << "Found ASUS Aura at: " << current->path << "\n";
            dev = hid_open_path(current->path);
            break;
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

    std::cout << "\n=== Test 1: Direct Control with 0xEC ===\n";

    // OpenRGB AuraUSBController uses 0xEC as the report ID
    // Command structure: [0xEC] [CMD] [PARAM1] [PARAM2] ...

    // Step 1: Enter direct mode
    std::cout << "[1] Entering direct mode...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;  // Report ID
    buf[1] = 0x35;  // Set control mode
    buf[2] = 0x00;  // Zone
    buf[3] = 0x00;
    buf[4] = 0x01;  // Direct mode

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Step 2: Set RED color
    std::cout << "[2] Setting RED color...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;  // Report ID
    buf[1] = 0x36;  // Set LED color
    buf[2] = 0x00;  // Start LED index
    buf[3] = 0x10;  // Number of LEDs (16)

    // RGB for each LED
    for (int i = 0; i < 16; i++) {
        buf[4 + i * 3 + 0] = 0xFF;  // R
        buf[4 + i * 3 + 1] = 0x00;  // G
        buf[4 + i * 3 + 2] = 0x00;  // B
    }

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Step 3: Apply
    std::cout << "[3] Applying...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;  // Report ID
    buf[1] = 0x35;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x80;  // Apply bit

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    std::cout << "\n=== Test 2: Alternative - Static Mode ===\n";

    // Some Aura devices need static mode instead of direct
    std::cout << "[1] Setting static RED...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;
    buf[1] = 0x35;  // Control
    buf[2] = 0x00;  // Zone
    buf[3] = 0x00;  // Static mode (0)
    buf[4] = 0xFF;  // R
    buf[5] = 0x00;  // G
    buf[6] = 0x00;  // B
    buf[7] = 0x00;  // Speed
    buf[8] = 0x00;  // Direction

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Apply
    std::cout << "[2] Applying...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;
    buf[1] = 0x35;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x80;

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    std::cout << "\n=== Test 3: Aura Addressable Header Protocol ===\n";

    // Different protocol variant
    std::cout << "[1] Setting color with 0xB3...\n";
    memset(buf, 0, 65);
    buf[0] = 0xEC;
    buf[1] = 0xB3;  // Addressable LED command
    buf[2] = 0x00;  // Channel
    buf[3] = 0x00;  // Start
    buf[4] = 0x78;  // Length (120 LEDs)

    // Fill with red
    for (int i = 0; i < 30; i++) {
        buf[5 + i * 2] = 0xFF;  // High byte (RG)
        buf[6 + i * 2] = 0x00;  // Low byte (B+next R)
    }

    res = hid_write(dev, buf, 65);
    std::cout << "    Result: " << res << "\n";

    hid_close(dev);
    hid_exit();

    std::cout << "\n=== Check your LEDs! ===\n";
    return 0;
}
