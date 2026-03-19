// Direct LED control using SEND_DYNAMIC_COLORS
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

#define EVISION_V2_CMD_SEND_DYNAMIC_COLORS 0x12
#define EVISION_V2_CMD_END_DYNAMIC_COLORS  0x13

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

    std::cout << "=== Direct LED Control (cmd 0x12) ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    // Send direct colors to ALL LEDs (keyboard has ~106 keys + edge LEDs)
    // Format: RGB * number of LEDs
    // Let's send 150 LEDs worth of data to cover everything

    const int NUM_LEDS = 150;
    uint8_t* colors = new uint8_t[NUM_LEDS * 3];

    // Fill all LEDs with our color
    for (int i = 0; i < NUM_LEDS; i++) {
        colors[i * 3 + 0] = r;
        colors[i * 3 + 1] = g;
        colors[i * 3 + 2] = b;
    }

    std::cout << "Sending " << NUM_LEDS << " LEDs via SEND_DYNAMIC_COLORS..." << std::endl;

    // Send in chunks (max 50 bytes payload per packet)
    const int CHUNK_SIZE = 48;  // 16 LEDs per chunk (48 bytes)
    for (int offset = 0; offset < NUM_LEDS * 3; offset += CHUNK_SIZE) {
        int remaining = NUM_LEDS * 3 - offset;
        int size = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        int res = EVisionQuery(dev, EVISION_V2_CMD_SEND_DYNAMIC_COLORS, offset, colors + offset, size, nullptr);
        if (res < 0) {
            std::cout << "Chunk at offset " << offset << " failed: " << res << std::endl;
        }
        Sleep(5);
    }

    std::cout << "Sending END_DYNAMIC_COLORS..." << std::endl;
    EVisionQuery(dev, EVISION_V2_CMD_END_DYNAMIC_COLORS, 0, nullptr, 0, nullptr);

    delete[] colors;

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Check if ALL LEDs changed to the target color." << std::endl;
    return 0;
}
