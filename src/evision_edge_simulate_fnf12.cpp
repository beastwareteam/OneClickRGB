// EVision Edge LED Control - Simulate FN+F12 (Rainbow mode) then set edge
// Based on user feedback: FN+F12 followed by evision_edge_final worked
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

    std::cout << "=== EVision Edge - Simulate FN+F12 + Edge Control ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Not found" << std::endl; return 1; }

    uint8_t profile = 0;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    uint16_t profile_base = 0x01 + (profile * 0x40);
    uint16_t edge_offset = profile_base + 0x1a;

    std::cout << "Profile: " << (int)profile << std::endl;

    // STEP 1: Set KEYBOARD to Rainbow mode (simulate FN+F12)
    // Keyboard mode values are different from edge mode values!
    // 0x01 = Color Wave Short (Rainbow)
    uint8_t kbd_rainbow[18] = {
        0x01,  // Mode = Color Wave Short (Rainbow) for keyboard
        0x04,  // Brightness
        0x02,  // Speed
        0x00,  // Direction
        0x00,  // Random
        r, g, b,  // Color (might be ignored for rainbow)
        0x00,  // Color offset
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Padding
        0x00   // LED mode
    };

    std::cout << "Step 1: Setting KEYBOARD to Rainbow mode..." << std::endl;
    EVisionQuery(dev, 0x06, profile_base, kbd_rainbow, 18, nullptr);

    // Also set edge to Rainbow
    uint8_t edge_rainbow[10] = {0x01, 0x04, 0x02, 0x00, 0x00, r, g, b, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, edge_rainbow, 10, nullptr);

    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "Waiting 500ms for Rainbow to take effect..." << std::endl;
    Sleep(500);

    // STEP 2: Now set to Static mode with our color
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Set keyboard to Static with our color
    uint8_t kbd_static[18] = {
        0x06,  // Mode = Static for keyboard
        0x04,  // Brightness
        0x00,  // Speed
        0x00,  // Direction
        0x00,  // Random
        r, g, b,  // Color
        0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00
    };

    std::cout << "Step 2: Setting KEYBOARD to Static blue..." << std::endl;
    EVisionQuery(dev, 0x06, profile_base, kbd_static, 18, nullptr);

    // Set edge to Static with our color
    uint8_t edge_static[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
    std::cout << "Step 3: Setting EDGE to Static blue..." << std::endl;
    EVisionQuery(dev, 0x06, edge_offset, edge_static, 10, nullptr);

    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify
    Sleep(50);
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    uint8_t verify[10];
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, verify);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "\nEdge config: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)verify[i] << " ";
    std::cout << std::dec << std::endl;

    hid_close(dev);
    hid_exit();
    std::cout << "\nDone! Both keyboard and edge should be blue now." << std::endl;
    return 0;
}
