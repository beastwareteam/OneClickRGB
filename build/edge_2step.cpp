#include <windows.h>
#include <iostream>
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
    uint8_t r = 255, g = 0, b = 0;
    if (argc >= 4) { r = atoi(argv[1]); g = atoi(argv[2]); b = atoi(argv[3]); }

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Not found" << std::endl; return 1; }

    uint16_t edge_offset = 0x1b;

    // STEP 1: Set to Rainbow mode and APPLY
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t rainbow[10] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, rainbow, 10, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    std::cout << "Rainbow mode set - waiting 500ms..." << std::endl;
    Sleep(500);

    // STEP 2: Now set to Static with color
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t stat[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, stat, 10, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    std::cout << "Static RGB(" << (int)r << "," << (int)g << "," << (int)b << ") set" << std::endl;

    hid_close(dev);
    hid_exit();
    return 0;
}
