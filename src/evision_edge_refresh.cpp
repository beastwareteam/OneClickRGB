// EVision Edge LED - Write + Profile Switch Test
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

#define EVISION_V2_CMD_BEGIN_CONFIGURE 0x01
#define EVISION_V2_CMD_END_CONFIGURE   0x02
#define EVISION_V2_CMD_READ_CONFIG     0x05
#define EVISION_V2_CMD_WRITE_CONFIG    0x06

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

    std::cout << "=== EVision Edge LED + Profile Refresh ===" << std::endl;
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
    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Read current profile
    uint8_t current_profile = 0;
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x00, nullptr, 1, &current_profile);
    std::cout << "Current profile: " << (int)current_profile << std::endl;

    uint16_t profile_offset = current_profile * 0x40 + 0x01;
    std::cout << "Profile offset: 0x" << std::hex << profile_offset << std::dec << std::endl;

    // Try ALL potential edge offsets within this profile
    // Profile structure from OpenRGB:
    // 0x00-0x07: Keyboard mode params
    // 0x1a-0x23: LOGO/Edge params (10 bytes)
    // 0x24-0x2b: Edge params (8 bytes)

    // Method 1: Write to LOGO offset (0x1a) - Endorfy style
    uint16_t logo_offset = profile_offset + 0x1a;
    uint8_t logo_config[10] = {
        0x06,  // Mode: Static (EVision)
        0x04,  // Brightness: max
        0x02,  // Speed
        0x00,  // Direction
        0x00,  // Random
        r, g, b,  // Color
        0x00,  // ?
        0x01   // On/Off
    };
    std::cout << "Writing LOGO/Edge at 0x" << std::hex << logo_offset << std::dec << std::endl;
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, logo_offset, logo_config, 10, nullptr);
    Sleep(10);

    // Method 2: Write to EDGE offset (0x24) - EVision style
    uint16_t edge_offset = profile_offset + 0x24;
    uint8_t edge_config[8] = {
        0x06,  // Mode: Static
        0x04,  // Brightness: max
        0x02,  // Speed
        0x00,  // Direction
        0x00,  // Random
        r, g, b   // Color
    };
    std::cout << "Writing EDGE at 0x" << std::hex << edge_offset << std::dec << std::endl;
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, edge_offset, edge_config, 8, nullptr);
    Sleep(10);

    // End configure - this should apply changes
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(50);

    std::cout << "\nTrying profile switch to refresh LEDs..." << std::endl;

    // Switch profile to 1 then back to current
    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(10);
    uint8_t new_profile = (current_profile == 0) ? 1 : 0;
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, 0x00, &new_profile, 1, nullptr);
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Switch back
    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(10);
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, 0x00, &current_profile, 1, nullptr);
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(50);

    std::cout << "Profile switched: " << (int)current_profile << " -> " << (int)new_profile << " -> " << (int)current_profile << std::endl;

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if side LEDs changed to target color." << std::endl;
    return 0;
}
