// EVision Edge LED Control - Test BGR order instead of RGB
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
    // Blue in RGB = 0, 34, 255
    // Blue in BGR = 255, 34, 0
    uint8_t r = 0, g = 34, b = 255;
    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }

    // Swap for BGR
    uint8_t bgr_b = r;  // Blue position gets Red value
    uint8_t bgr_g = g;  // Green stays
    uint8_t bgr_r = b;  // Red position gets Blue value

    std::cout << "=== EVision Edge LED - BGR Test ===" << std::endl;
    std::cout << "Target RGB: (" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;
    std::cout << "Sending BGR: (" << (int)bgr_r << "," << (int)bgr_g << "," << (int)bgr_b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    uint8_t profile = 0;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    uint16_t profile_base = 0x01 + (profile * 0x40);
    uint16_t edge_offset = profile_base + 0x1a;

    // Step 1: Set Rainbow mode first
    uint8_t rainbow[10] = {0x01, 0x04, 0x00, 0x00, 0x00, bgr_r, bgr_g, bgr_b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, rainbow, 10, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Step 2: Set Static mode with BGR color
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    uint8_t config[10] = {0x04, 0x04, 0x00, 0x00, 0x00, bgr_r, bgr_g, bgr_b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, config, 10, nullptr);

    // Also set keyboard config with BGR
    uint8_t kbd[18] = {0x06, 0x04, 0x00, 0x00, 0x00, bgr_r, bgr_g, bgr_b, 0x00, 0,0,0,0,0,0,0,0, 0x00};
    EVisionQuery(dev, 0x06, profile_base, kbd, 18, nullptr);

    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify
    Sleep(50);
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    uint8_t verify[10];
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, verify);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "Stored: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;

    hid_close(dev);
    hid_exit();
    std::cout << "Done! Check if side LEDs are blue now." << std::endl;
    return 0;
}
