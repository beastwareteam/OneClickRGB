// EVision Edge LED Control - Combined approach: Write to keyboard, 0x1a AND 0x24
// Based on OpenRGB: Edge may inherit color from keyboard profile
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>
#include <cstdlib>

#define EVISION_VID 0x3299
#define EVISION_PID 0x4E9F
#define EVISION_USAGE_PAGE 0xFF1C
#define EVISION_V2_REPORT_ID 0x04
#define EVISION_V2_PACKET_SIZE 64

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = EVISION_V2_REPORT_ID;
    buffer[3] = cmd;
    buffer[4] = size;
    buffer[5] = offset & 0xff;
    buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, sizeof(buffer)) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100); retries--; }
    while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);
    if (bytes_read != sizeof(buffer)) return -2;
    if (buffer[7] != 0) return -buffer[7];
    if (odata && buffer[4] > 0) memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

int main(int argc, char* argv[]) {
    uint8_t r = 0, g = 34, b = 255;
    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }

    std::cout << "=== EVision Combined RGB Control ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    // Begin configure
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Get current profile
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    uint16_t profile_base = 0x01 + (profile * 0x40);

    std::cout << "Profile: " << (int)profile << std::endl;

    // 1. Write KEYBOARD main config (18 bytes starting at profile_base)
    // This sets the keyboard's mode color which edge may inherit
    uint8_t kbd_config[18] = {
        0x06,  // Mode = Static (EVISION_V2_MODE_STATIC)
        0x04,  // Brightness = max
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r,     // Red (byte 5)
        g,     // Green (byte 6)
        b,     // Blue (byte 7)
        0x00,  // Color offset (byte 8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Padding
        0x00   // LED mode color (byte 17)
    };

    std::cout << "Step 1: Setting keyboard main config at 0x" << std::hex << profile_base << std::dec << std::endl;
    EVisionQuery(dev, 0x06, profile_base, kbd_config, 18, nullptr);
    Sleep(10);

    // 2. Write to offset 0x1a (LOGO/Edge for Endorfy)
    uint16_t offset_1a = profile_base + 0x1a;
    uint8_t edge_1a[10] = {
        0x04,  // Mode = Static
        0x04,  // Brightness = max
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r, g, b,  // RGB
        0x00,  // Unknown
        0x01   // On
    };

    std::cout << "Step 2: Setting edge at 0x" << std::hex << offset_1a << std::dec << std::endl;
    EVisionQuery(dev, 0x06, offset_1a, edge_1a, 10, nullptr);
    Sleep(10);

    // 3. Write to offset 0x24 (EVision Edge)
    uint16_t offset_24 = profile_base + 0x24;
    uint8_t edge_24[7] = {
        0x04,  // Mode = Static
        0x04,  // Brightness = max
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r, g   // R, G (no room for B in 7 bytes - will try 10)
    };

    std::cout << "Step 3: Setting edge at 0x" << std::hex << offset_24 << std::dec << std::endl;
    EVisionQuery(dev, 0x06, offset_24, edge_1a, 10, nullptr);  // Use same 10-byte format
    Sleep(10);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Read back all to verify
    Sleep(50);
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);

    uint8_t verify[20];
    EVisionQuery(dev, 0x05, profile_base, nullptr, 18, verify);
    std::cout << "\nKbd config: ";
    for (int i = 0; i < 18; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;

    EVisionQuery(dev, 0x05, offset_1a, nullptr, 10, verify);
    std::cout << "Edge 0x1a:  ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;

    EVisionQuery(dev, 0x05, offset_24, nullptr, 10, verify);
    std::cout << "Edge 0x24:  ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs show blue." << std::endl;
    return 0;
}
