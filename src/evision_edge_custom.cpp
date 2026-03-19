// EVision Edge - Try writing to custom color memory (cmd 0x0B)
// Maybe edge LEDs read color from custom color slots
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

    std::cout << "=== EVision Edge - Custom Color Test ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Not found" << std::endl; return 1; }

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Write to custom color memory - fill first few bytes with our color
    // Pattern seems to be 3 bytes per entry: R G B
    uint8_t colors[48];
    for (int i = 0; i < 48; i += 3) {
        colors[i] = r;
        colors[i+1] = g;
        colors[i+2] = b;
    }

    std::cout << "Writing custom colors (cmd 0x0B)..." << std::endl;
    // Write to colorset 0 (offset 0)
    EVisionQuery(dev, 0x0B, 0, colors, 48, nullptr);
    Sleep(10);

    // Also try setting edge to use custom mode
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    uint16_t edge_offset = 0x01 + (profile * 0x40) + 0x1a;

    // Set edge to static with our color AND write custom colors
    uint8_t edge[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, edge, 10, nullptr);

    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify custom colors
    std::cout << "\nVerifying custom colors:" << std::endl;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    uint8_t verify[16];
    EVisionQuery(dev, 0x0A, 0, nullptr, 16, verify);
    std::cout << "Custom slot 0: ";
    for (int i = 0; i < 16; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();
    std::cout << "\nDone!" << std::endl;
    return 0;
}
