// EVision Edge LED Control v3 - Target 0x20 zone
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

    std::cout << "=== EVision Edge LED Control v3 ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Read and show current state of potential edge zones
    uint16_t zones[] = {0x20, 0x24, 0x28, 0x2C, 0x30};

    for (uint16_t offset : zones) {
        uint8_t data[16];
        EVisionQuery(dev, 0x05, offset, nullptr, 16, data);
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << offset << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // Based on scan, 0x20 contains:
    // 00 22 ff 00 01 06 04 00 00 00 00 22 ff 00 00 00
    // Bytes 0-2: Color (R G B)
    // Byte 4: Mode (01 = rainbow?)
    // Bytes 5-7: Mode params?
    // Bytes 10-12: Another color?

    // Let's try setting the entire zone to static blue
    std::cout << "\nSetting zones to static blue..." << std::endl;

    // Try offset 0x20 - write static mode config
    // Format might be: R G B ? Mode Brightness Speed Dir ? ? R G B
    uint8_t config20[16] = { r, g, b, 0x00, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x00, 0x00 };
    EVisionQuery(dev, 0x06, 0x20, config20, 16, nullptr);
    Sleep(10);

    // Also try 0x24 (next potential zone)
    // From scan: 00 01 06 04 00 00 00 00 22 ff 00 00 00 00 00 00
    // This looks like: ? Mode=01 then Mode=06?
    uint8_t config24[16] = { 0x00, 0x06, 0x04, 0x00, 0x00, r, g, b, r, g, b, 0x00, 0x00, 0x00, 0x00, 0x00 };
    EVisionQuery(dev, 0x06, 0x24, config24, 16, nullptr);
    Sleep(10);

    // Try 0x04-0x08 within first profile - edge params at 0x24 offset from profile
    // Profile 0 starts at 0x01, edge at +0x24 = 0x25
    uint8_t edge_config[8] = { 0x06, 0x04, 0x02, 0x00, 0x00, r, g, b };
    EVisionQuery(dev, 0x06, 0x25, edge_config, 8, nullptr);
    Sleep(10);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Read back
    std::cout << "\nAfter changes:" << std::endl;
    for (uint16_t offset : zones) {
        uint8_t data[16];
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(10);
        EVisionQuery(dev, 0x05, offset, nullptr, 16, data);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << offset << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs changed." << std::endl;
    return 0;
}
