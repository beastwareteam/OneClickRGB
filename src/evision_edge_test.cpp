// EVision Edge LED Test - Try offset 0x20
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

    std::cout << "=== EVision Edge Test - Multiple Offsets ===" << std::endl;
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

    // Test multiple potential edge offsets
    uint16_t test_offsets[] = {0x1b, 0x20, 0x24, 0x25, 0x5b, 0x9b};

    for (uint16_t offset : test_offsets) {
        uint8_t data[16];
        EVisionQuery(dev, 0x05, offset, nullptr, 16, data);

        std::cout << "\nOffset 0x" << std::hex << offset << std::dec << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // Try writing to offset 0x20 (where we saw ff 00 00)
    std::cout << "\n=== Writing to offset 0x20 ===" << std::endl;

    // Format at 0x20 appears to be: R G B ? Mode Brightness Speed Dir ? ? R G B ...
    uint8_t edge_0x20[16] = {
        r, g, b,      // RGB at start
        0x00,         // unknown
        0x06,         // Mode = Static (EVision mode)
        0x04,         // Brightness
        0x02,         // Speed
        0x00,         // Direction
        0x00, 0x00,   // padding
        r, g, b,      // RGB again
        0x00, 0x00, 0x00
    };

    int res = EVisionQuery(dev, 0x06, 0x20, edge_0x20, 16, nullptr);
    std::cout << "Write 0x20 result: " << res << std::endl;
    Sleep(10);

    // Also try offset 0x1b with the color directly at bytes 0-2
    std::cout << "\n=== Writing to offset 0x1b (color first) ===" << std::endl;
    uint8_t edge_0x1b[10] = {
        0x04,  // Mode = Static (Endorfy)
        0x04,  // Brightness
        0x02,  // Speed
        0x00,  // Direction
        0x00,  // Random
        r, g, b,  // RGB
        0x00,  // unknown
        0x01   // On
    };

    res = EVisionQuery(dev, 0x06, 0x1b, edge_0x1b, 10, nullptr);
    std::cout << "Write 0x1b result: " << res << std::endl;
    Sleep(10);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify
    std::cout << "\n=== After write ===" << std::endl;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);

    for (uint16_t offset : test_offsets) {
        uint8_t data[16];
        EVisionQuery(dev, 0x05, offset, nullptr, 16, data);

        std::cout << "Offset 0x" << std::hex << offset << std::dec << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check side LEDs." << std::endl;
    return 0;
}
