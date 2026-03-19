// Set Edge LEDs in zone 0x02c0+
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

    std::cout << "=== Setting Edge LEDs at 0x02c0+ ===" << std::endl;
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

    // The pattern at 0x02c0 is: 07 00 98 (mode, r, gb?)
    // Let's change ALL these entries to: 06 R G (static mode with our color)
    // The format might be: Mode(1) + Color(2 bytes packed?)

    // Try setting entries from 0x02c0 to 0x0440
    std::cout << "\nWriting static color to zone 0x02c0-0x0440..." << std::endl;

    for (uint16_t addr = 0x02c0; addr < 0x0440; addr += 3) {
        // Try format: Mode(06=static), R, G  or Mode, combined color
        uint8_t data[3] = { 0x06, r, g };  // Static mode with RG
        EVisionQuery(dev, 0x06, addr, data, 3, nullptr);
    }

    Sleep(50);

    // Also try writing full RGB at different offsets
    std::cout << "Writing RGB to specific zones..." << std::endl;

    // Try writing 16-byte blocks with RGB pattern
    for (uint16_t addr = 0x02c0; addr < 0x0440; addr += 16) {
        uint8_t block[16];
        for (int i = 0; i < 16; i += 3) {
            if (i + 2 < 16) {
                block[i] = 0x06;  // Static mode
                block[i+1] = r;
                block[i+2] = g;
            }
        }
        block[15] = b;  // Put blue somewhere
        EVisionQuery(dev, 0x06, addr, block, 16, nullptr);
    }

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    Sleep(50);

    // Read back
    std::cout << "\nAfter changes (0x02c0-0x02f0):" << std::endl;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    for (uint16_t addr = 0x02c0; addr < 0x0300; addr += 16) {
        uint8_t data[16];
        EVisionQuery(dev, 0x05, addr, nullptr, 16, data);
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << addr << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs changed." << std::endl;
    return 0;
}
