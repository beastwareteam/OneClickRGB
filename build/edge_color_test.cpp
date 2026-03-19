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
    uint8_t r = 255, g = 0, b = 0;
    if (argc >= 4) { r = atoi(argv[1]); g = atoi(argv[2]); b = atoi(argv[3]); }

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

    // Write color to multiple potential locations
    // Keyboard profile color at 0x01+5 = 0x06
    uint8_t kb_color[3] = {r, g, b};
    EVisionQuery(dev, 0x06, 0x06, kb_color, 3, nullptr);
    std::cout << "Wrote color to 0x06" << std::endl;

    // Edge config at 0x1b with color
    uint8_t edge[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);
    std::cout << "Wrote edge config to 0x1b" << std::endl;

    // Also write to 0x01 (profile base) with full config
    uint8_t profile[18];
    memset(profile, 0, 18);
    profile[0] = 0x06;  // Mode = static
    profile[1] = 0x04;  // Brightness
    profile[5] = r;
    profile[6] = g;
    profile[7] = b;
    EVisionQuery(dev, 0x06, 0x01, profile, 18, nullptr);
    std::cout << "Wrote profile to 0x01" << std::endl;

    // Unlock
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();
    std::cout << "Done!" << std::endl;
    return 0;
}
