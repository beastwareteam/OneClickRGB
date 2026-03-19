// EVision Edge LED Control - Test offset 0x24 (EVISION_V2_PARAMETER_EDGE)
// Based on OpenRGB discovery: GK650 might use 0x24 offset, not 0x1a
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

    std::cout << "=== EVision Edge LED - Test offset 0x24 ===" << std::endl;
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

    // Calculate offsets for both EDGE types
    uint16_t profile_base = 0x01 + (profile * 0x40);
    uint16_t offset_1a = profile_base + 0x1a;  // ENDORFY_KEYBOARD_PART_EDGE / LOGO
    uint16_t offset_24 = profile_base + 0x24;  // EVISION_V2_KEYBOARD_PART_EDGE

    std::cout << "Profile " << (int)profile << std::endl;
    std::cout << "Offset 0x1a: 0x" << std::hex << offset_1a << std::dec << std::endl;
    std::cout << "Offset 0x24: 0x" << std::hex << offset_24 << std::dec << std::endl;

    // Read current state from both offsets
    uint8_t data_1a[10], data_24[8];
    EVisionQuery(dev, 0x05, offset_1a, nullptr, 10, data_1a);
    EVisionQuery(dev, 0x05, offset_24, nullptr, 7, data_24);  // 0x2b - 0x24 = 7 bytes

    std::cout << "\nCurrent 0x1a (LOGO/Endorfy Edge): ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data_1a[i] << " ";
    std::cout << std::dec << std::endl;

    std::cout << "Current 0x24 (EVision Edge):      ";
    for (int i = 0; i < 7; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data_24[i] << " ";
    std::cout << std::dec << std::endl;

    // Write to OFFSET 0x24 (EVISION_V2_PARAMETER_EDGE)
    // Format: [Mode, Brightness, Speed, Direction, RandomFlag, R, G, B, ...]
    // Based on header: size = 0x2b - 0x24 = 7 bytes
    uint8_t edge_config[7] = {
        0x04,  // Mode = Static (using MODE2 values since it's edge)
        0x04,  // Brightness = max
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r,     // Red
        g      // Green - only 7 bytes, Blue might be at another position?
    };

    std::cout << "\nWriting to 0x24 with RGB..." << std::endl;
    int res = EVisionQuery(dev, 0x06, offset_24, edge_config, 7, nullptr);
    std::cout << "Write result: " << res << std::endl;

    // Also try writing full 10 bytes to 0x24 (same structure as 0x1a)
    uint8_t edge_full[10] = {
        0x04,  // Mode = Static
        0x04,  // Brightness = max
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r,     // Red
        g,     // Green
        b,     // Blue
        0x00,  // Unknown
        0x01   // On
    };

    std::cout << "Writing 10 bytes to 0x24..." << std::endl;
    res = EVisionQuery(dev, 0x06, offset_24, edge_full, 10, nullptr);
    std::cout << "Write result: " << res << std::endl;

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Read back to verify
    Sleep(50);
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    EVisionQuery(dev, 0x05, offset_24, nullptr, 10, data_24);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "\nAfter 0x24: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data_24[i] << " ";
    std::cout << std::dec << std::endl;

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs changed." << std::endl;
    return 0;
}
