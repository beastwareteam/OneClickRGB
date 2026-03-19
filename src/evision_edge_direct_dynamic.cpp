// EVision Edge - Try direct dynamic color command (0x12)
// This command is used for real-time LED updates
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>
#include <cstdlib>

#define EVISION_VID 0x3299
#define EVISION_PID 0x4E9F
#define EVISION_USAGE_PAGE 0xFF1C

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[64];
    memset(buffer, 0, 64);
    buffer[0] = 0x04;
    buffer[3] = cmd;
    buffer[4] = size;
    buffer[5] = offset & 0xff;
    buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < 64; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, 64) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, 64, 100); retries--; }
    while (bytes_read > 0 && buffer[0] != 0x04 && retries > 0);
    if (bytes_read != 64) return -2;
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

    std::cout << "=== EVision Edge - Direct Dynamic Colors ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Not found" << std::endl; return 1; }

    // First set keyboard to "Direct" mode (0xFF) which enables dynamic color control
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    // Set keyboard mode to direct control - this might enable edge control too
    uint16_t profile_base = 0x01 + (profile * 0x40);

    // According to OpenRGB, map_size is around 126 LEDs
    // Edge LEDs might be at the end of the LED map
    // Let's try writing colors to various LED positions

    // First, let's just set ALL LEDs to blue using dynamic colors
    // The format is R G B for each LED, starting at offset 0
    uint8_t colors[54];  // 18 LEDs worth
    for (int i = 0; i < 54; i += 3) {
        colors[i] = r;
        colors[i+1] = g;
        colors[i+2] = b;
    }

    std::cout << "Sending dynamic colors (cmd 0x12)..." << std::endl;

    // Try different offset ranges that might control edge LEDs
    // Standard keyboard LEDs: 0-105 (times 3 for RGB = 0-315)
    // Edge might be after that

    // Try offset 0 (first batch of LEDs)
    int res = EVisionQuery(dev, 0x12, 0, colors, 54, nullptr);
    std::cout << "Offset 0: " << res << std::endl;

    // Try offset 315 (after keyboard LEDs, possible edge position)
    res = EVisionQuery(dev, 0x12, 315, colors, 54, nullptr);
    std::cout << "Offset 315: " << res << std::endl;

    // Try offset 378 (126 LEDs * 3)
    res = EVisionQuery(dev, 0x12, 378, colors, 54, nullptr);
    std::cout << "Offset 378: " << res << std::endl;

    // Try some other positions
    res = EVisionQuery(dev, 0x12, 400, colors, 54, nullptr);
    std::cout << "Offset 400: " << res << std::endl;

    // Keep the colors active by sending refresh
    for (int i = 0; i < 10; i++) {
        Sleep(100);
        // Refresh command (write to blank space position 6*3=18)
        EVisionQuery(dev, 0x12, 18, nullptr, 1, nullptr);
    }

    std::cout << "Done! Did edge LEDs change?" << std::endl;

    // End dynamic mode
    EVisionQuery(dev, 0x13, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();
    return 0;
}
