// EVision Edge LED Control - Final version using correct Endorfy protocol
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

// Endorfy Edge Mode values (different from keyboard!)
#define ENDORFY_MODE2_FREEZE         0x00
#define ENDORFY_MODE2_COLOR_WAVE     0x01  // Rainbow
#define ENDORFY_MODE2_SPECTRUM_CYCLE 0x02
#define ENDORFY_MODE2_BREATHING      0x03
#define ENDORFY_MODE2_STATIC         0x04
#define ENDORFY_MODE2_OFF            0x05

// Edge uses LOGO offset within profile
#define EVISION_V2_PARAMETER_LOGO    0x1a

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

    std::cout << "=== EVision/Endorfy Edge LED Control ===" << std::endl;
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

    // Get current profile
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;

    // Edge offset = profile_base + 0x1a (LOGO offset for Endorfy)
    uint16_t profile_base = 0x01 + (profile * 0x40);
    uint16_t edge_offset = profile_base + EVISION_V2_PARAMETER_LOGO;

    std::cout << "Profile " << (int)profile << ", Edge offset 0x" << std::hex << edge_offset << std::dec << std::endl;

    // Read current Edge config (10 bytes)
    uint8_t edge_config[10];
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, edge_config);

    std::cout << "Current Edge: ";
    for (int i = 0; i < 10; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_config[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Set Edge to Static mode with our color
    // Buffer format for Endorfy Edge (10 bytes):
    // [0] Mode (0x04 = Static)
    // [1] Brightness (0x04 = max)
    // [2] Speed
    // [3] Direction
    // [4] Random color flag
    // [5] Red
    // [6] Green
    // [7] Blue
    // [8] Unknown
    // [9] On/Off (0x01 = on)

    uint8_t new_config[10] = {
        ENDORFY_MODE2_STATIC,  // Mode = Static (0x04)
        0x04,                   // Brightness = max
        0x00,                   // Speed = 0 (WICHTIG!)
        0x00,                   // Direction = 0
        0x00,                   // Random = off
        r,                      // Red
        g,                      // Green
        b,                      // Blue
        0x00,                   // Unknown
        0x01                    // On
    };

    std::cout << "Writing Edge: ";
    for (int i = 0; i < 10; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)new_config[i] << " ";
    }
    std::cout << std::dec << std::endl;

    int res = EVisionQuery(dev, 0x06, edge_offset, new_config, 10, nullptr);
    if (res < 0) {
        std::cout << "Write failed: " << res << std::endl;
    } else {
        std::cout << "Write OK" << std::endl;
    }
    Sleep(10);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    // Verify
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(10);
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 10, edge_config);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    std::cout << "After:   Edge: ";
    for (int i = 0; i < 10; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_config[i] << " ";
    }
    std::cout << std::dec << std::endl;

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs changed to blue." << std::endl;
    return 0;
}
