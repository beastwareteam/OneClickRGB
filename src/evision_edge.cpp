// EVision Edge LED Control - Set side lighting
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

// Profile structure (from OpenRGB):
// 0x00: Mode
// 0x01: Brightness
// 0x02: Speed
// 0x03: Direction
// 0x04: Random color flag
// 0x05-0x07: RGB Color
// ...
// 0x1a-0x22: Logo parameters (9 bytes: mode, brightness, speed, dir, random, R, G, B, on/off)
// 0x24-0x2a: Edge parameters (7 bytes: mode, brightness, speed, dir, random, R, G, B)

#define EVISION_EDGE_OFFSET 0x24
#define EVISION_LOGO_OFFSET 0x1a

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
    if (argc < 4) {
        std::cout << "Usage: evision_edge <R> <G> <B>" << std::endl;
        std::cout << "Example: evision_edge 0 34 255  (Blue)" << std::endl;
        return 1;
    }

    uint8_t r = atoi(argv[1]);
    uint8_t g = atoi(argv[2]);
    uint8_t b = atoi(argv[3]);

    std::cout << "=== EVision Edge LED Control ===" << std::endl;
    std::cout << "Setting Edge to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

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

    uint16_t profile_base = 0x01 + (profile * 0x40);
    std::cout << "Profile " << (int)profile << ", base offset 0x" << std::hex << profile_base << std::dec << std::endl;

    // Read current Edge config (offset 0x24 within profile = 7 bytes)
    uint16_t edge_offset = profile_base + EVISION_EDGE_OFFSET;
    uint8_t edge_config[7];
    EVisionQuery(dev, 0x05, edge_offset, nullptr, 7, edge_config);

    std::cout << "Current Edge config: ";
    for (int i = 0; i < 7; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_config[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Set to Static mode with our color
    // Byte 0: Mode (0x06 = Static)
    // Byte 1: Brightness (0x04 = max)
    // Byte 2: Speed
    // Byte 3: Direction
    // Byte 4: Random color (0x00 = off)
    // Byte 5-7: RGB
    edge_config[0] = 0x06;  // Static mode
    edge_config[1] = 0x04;  // Max brightness
    edge_config[4] = 0x00;  // Random off
    edge_config[5] = r;
    edge_config[6] = g;
    // Note: Edge might only have 7 bytes, B might be at different position

    // Let's try writing 8 bytes to include B
    uint8_t edge_full[8] = { 0x06, 0x04, edge_config[2], edge_config[3], 0x00, r, g, b };

    std::cout << "Writing Edge config: ";
    for (int i = 0; i < 8; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_full[i] << " ";
    }
    std::cout << std::dec << std::endl;

    int res = EVisionQuery(dev, 0x06, edge_offset, edge_full, 8, nullptr);
    if (res < 0) {
        std::cout << "Write failed: " << res << std::endl;
    }
    Sleep(10);

    // Also try Logo (offset 0x1a within profile)
    uint16_t logo_offset = profile_base + EVISION_LOGO_OFFSET;
    uint8_t logo_config[10];
    EVisionQuery(dev, 0x05, logo_offset, nullptr, 10, logo_config);

    std::cout << "\nCurrent Logo config: ";
    for (int i = 0; i < 10; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)logo_config[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Set Logo to static color
    uint8_t logo_full[10] = { 0x06, 0x04, logo_config[2], logo_config[3], 0x00, r, g, b, 0x00, 0x01 };

    std::cout << "Writing Logo config: ";
    for (int i = 0; i < 10; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)logo_full[i] << " ";
    }
    std::cout << std::dec << std::endl;

    res = EVisionQuery(dev, 0x06, logo_offset, logo_full, 10, nullptr);
    if (res < 0) {
        std::cout << "Logo write failed: " << res << std::endl;
    }
    Sleep(10);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check the edge and logo LEDs." << std::endl;
    return 0;
}
