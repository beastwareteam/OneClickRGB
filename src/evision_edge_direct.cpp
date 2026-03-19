// EVision Edge LED - Direct Mode Test
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

// Commands
#define CMD_BEGIN_CONFIGURE     0x01
#define CMD_END_CONFIGURE       0x02
#define CMD_READ_CONFIG         0x05
#define CMD_WRITE_CONFIG        0x06
#define CMD_SEND_DYNAMIC        0x12  // Direct LED control
#define CMD_END_DYNAMIC         0x13

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

    std::cout << "  TX: cmd=0x" << std::hex << (int)cmd << " off=0x" << offset << std::dec;

    if (hid_write(dev, buffer, sizeof(buffer)) < 0) {
        std::cout << " WRITE FAILED" << std::endl;
        return -1;
    }

    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100); retries--; }
    while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);

    if (bytes_read != sizeof(buffer)) {
        std::cout << " READ FAILED" << std::endl;
        return -2;
    }
    if (buffer[7] != 0) {
        std::cout << " ERROR=" << (int)buffer[7] << std::endl;
        return -buffer[7];
    }

    std::cout << " OK" << std::endl;

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

    std::cout << "=== EVision Edge - Direct Mode Test ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    // Method 1: Try Direct Mode (0x12) for edge LEDs
    std::cout << "\n=== Method 1: Direct Mode (cmd 0x12) ===" << std::endl;

    // Send direct color command - some keyboards use this for immediate effect
    // Format might be: offset = LED index, data = RGB values
    for (int led = 0; led < 16; led++) {
        uint8_t color[3] = {r, g, b};
        EVisionQuery(dev, CMD_SEND_DYNAMIC, led, color, 3, nullptr);
        Sleep(5);
    }

    // End direct mode
    EVisionQuery(dev, CMD_END_DYNAMIC, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Method 2: Write to all three profiles
    std::cout << "\n=== Method 2: Write to ALL profiles ===" << std::endl;

    EVisionQuery(dev, CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Edge config for Endorfy
    uint8_t edge_config[10] = {
        0x04,  // Mode = Static (Endorfy)
        0x04,  // Brightness = max
        0x02,  // Speed
        0x00,  // Direction
        0x00,  // Random = off
        r, g, b,  // RGB
        0x00,  // Unknown
        0x01   // On
    };

    // Write to all 3 profiles (offsets 0x1b, 0x5b, 0x9b)
    uint16_t profile_offsets[] = {0x1b, 0x5b, 0x9b};
    for (uint16_t offset : profile_offsets) {
        std::cout << "Writing profile at 0x" << std::hex << offset << std::dec << std::endl;
        EVisionQuery(dev, CMD_WRITE_CONFIG, offset, edge_config, 10, nullptr);
        Sleep(10);
    }

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, CMD_WRITE_CONFIG, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Method 3: Try different data formats at 0x20
    std::cout << "\n=== Method 3: Different formats at 0x20 ===" << std::endl;

    EVisionQuery(dev, CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Try format: Mode Brightness Speed Dir Random R G B (like main keyboard)
    uint8_t format_a[10] = {0x06, 0x04, 0x02, 0x00, 0x00, r, g, b, 0x00, 0x01};
    std::cout << "Format A (keyboard-style) at 0x20:" << std::endl;
    EVisionQuery(dev, CMD_WRITE_CONFIG, 0x20, format_a, 10, nullptr);
    Sleep(10);

    // Also write at 0x25 (within the data we saw)
    std::cout << "Format A at 0x25:" << std::endl;
    EVisionQuery(dev, CMD_WRITE_CONFIG, 0x25, format_a, 10, nullptr);
    Sleep(10);

    EVisionQuery(dev, CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check side LEDs now." << std::endl;
    return 0;
}
