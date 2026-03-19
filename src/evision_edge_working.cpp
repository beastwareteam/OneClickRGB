// EVision Edge LED Control - Working version
// Key insight: Must change MODE to trigger LED update
// If already in mode 0x04, change to 0x06 first, then back to 0x04
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
    if (argc >= 4) { r = atoi(argv[1]); g = atoi(argv[2]); b = atoi(argv[3]); }

    std::cout << "=== EVision Edge LED Control (Working) ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    uint16_t edge_offset = 0x1b;  // Profile 0 base (0x01) + LOGO offset (0x1a)

    // Step 1: Begin configure
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Step 2: Read current state
    uint8_t current[10];
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, current);

    std::cout << "Current: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)current[i] << " ";
    std::cout << std::dec << std::endl;

    // Step 3: FIRST set to mode 0x01 (Rainbow) to force mode change
    // This is the KEY - must use mode 0x01, not 0x06!
    uint8_t mode01[10] = {0x01, 0x00, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, mode01, 10, nullptr);
    std::cout << "Set mode 0x01 (Rainbow)" << std::endl;

    // End configure to apply
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Step 4: Begin configure again
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Step 5: NOW set to mode 0x04 (Static) with color - this triggers the change!
    uint8_t mode04[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, mode04, 10, nullptr);
    std::cout << "Set mode 0x04 (Static) with color" << std::endl;

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify
    Sleep(50);
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, current);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "After:   ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)current[i] << " ";
    std::cout << std::dec << std::endl;

    hid_close(dev);
    hid_exit();
    std::cout << "Done!" << std::endl;
    return 0;
}
