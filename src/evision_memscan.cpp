// Scan keyboard memory for Edge LED config
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>

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

int main() {
    std::cout << "=== EVision Memory Scan - Looking for Edge LEDs ===" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    std::cout << "\nScanning memory 0x0000-0x0500..." << std::endl;
    std::cout << "Looking for potential LED configs (mode bytes 0x01-0x0D, followed by brightness 0x00-0x04)" << std::endl;
    std::cout << std::endl;

    for (uint16_t addr = 0x0000; addr < 0x0500; addr += 16) {
        uint8_t data[16];
        int res = EVisionQuery(dev, 0x05, addr, nullptr, 16, data);
        if (res < 0) continue;

        // Check if this looks like an LED config
        bool has_data = false;
        bool looks_like_config = false;
        for (int i = 0; i < 16; i++) {
            if (data[i] != 0) has_data = true;
        }

        // Check for LED config patterns: mode (01-0D), brightness (00-04)
        if (data[0] >= 0x01 && data[0] <= 0x0D && data[1] <= 0x04) {
            looks_like_config = true;
        }
        // Also check for color patterns (any byte sequence with valid RGB)
        if (has_data && (data[0] == 0x07 || data[0] == 0x06 || data[0] == 0x04 || data[0] == 0x01)) {
            looks_like_config = true;
        }

        if (has_data) {
            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << addr << ": ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::setw(2) << (int)data[i] << " ";
            }
            if (looks_like_config) {
                std::cout << " <- LED config?";
            }
            std::cout << std::dec << std::endl;
        }
    }

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone!" << std::endl;
    return 0;
}
