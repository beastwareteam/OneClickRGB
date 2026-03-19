// EVision Edge LED Control v2 - Try alternative offsets
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
    if (argc < 4) {
        std::cout << "Usage: evision_edge2 <R> <G> <B>" << std::endl;
        return 1;
    }

    uint8_t r = atoi(argv[1]);
    uint8_t g = atoi(argv[2]);
    uint8_t b = atoi(argv[3]);

    std::cout << "=== EVision Edge LED Control v2 ===" << std::endl;
    std::cout << "Setting all zones to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

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

    // The memory scan showed:
    // 0x01: Main keyboard (we already set this)
    // 0x41: Another zone with ff ff ff color (could be Edge)
    // 0x81: Another zone with ff ff ff color (could be Logo/secondary)

    // Zone structure seems to be:
    // Byte 0: Mode
    // Byte 1: Brightness
    // Byte 2: Speed
    // Byte 3: Direction
    // Byte 4: Random
    // Byte 5-7 or 5-8: Color

    // Set static mode for offset 0x41 (potential Edge)
    std::cout << "\n--- Zone at 0x41 ---" << std::endl;
    uint8_t zone1[16];
    EVisionQuery(dev, 0x05, 0x41, nullptr, 16, zone1);
    std::cout << "Current: ";
    for (int i = 0; i < 16; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)zone1[i] << " ";
    std::cout << std::dec << std::endl;

    // Set to static blue
    uint8_t config1[8] = { 0x06, 0x04, 0x02, 0x00, 0x00, r, g, b };
    EVisionQuery(dev, 0x06, 0x41, config1, 8, nullptr);
    std::cout << "Set to static mode" << std::endl;
    Sleep(10);

    // Set static mode for offset 0x81 (potential Logo)
    std::cout << "\n--- Zone at 0x81 ---" << std::endl;
    uint8_t zone2[16];
    EVisionQuery(dev, 0x05, 0x81, nullptr, 16, zone2);
    std::cout << "Current: ";
    for (int i = 0; i < 16; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)zone2[i] << " ";
    std::cout << std::dec << std::endl;

    uint8_t config2[8] = { 0x06, 0x04, 0x02, 0x00, 0x00, r, g, b };
    EVisionQuery(dev, 0x06, 0x81, config2, 8, nullptr);
    std::cout << "Set to static mode" << std::endl;
    Sleep(10);

    // Also ensure Windows key is unlocked
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    // End configure
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if edge LEDs changed." << std::endl;
    return 0;
}
