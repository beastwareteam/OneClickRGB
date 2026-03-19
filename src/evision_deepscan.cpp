// EVision Deep Scan - Find ALL LED zones
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
    std::cout << "=== EVision Deep Memory Scan ===" << std::endl;
    std::cout << "Scanning 0x0000-0x0400 for LED zones...\n" << std::endl;

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

    // Scan larger range
    std::cout << "Addr  | Data (showing non-empty rows)" << std::endl;
    std::cout << "------+--------------------------------------------------" << std::endl;

    for (uint16_t base = 0; base < 0x400; base += 16) {
        uint8_t data[16];
        memset(data, 0, sizeof(data));

        int res = EVisionQuery(dev, 0x05, base, nullptr, 16, data);
        if (res < 0) continue;

        // Check if row has interesting data
        bool has_data = false;
        int nonzero = 0;
        for (int i = 0; i < 16; i++) {
            if (data[i] != 0x00 && data[i] != 0x20) nonzero++;
        }

        // Show rows with significant data
        if (nonzero > 2) {
            std::cout << std::hex << std::setfill('0') << std::setw(4) << base << ": ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::setw(2) << (int)data[i] << " ";
            }

            // Try to identify what this might be
            if (base == 0x00) std::cout << " <- Profile selector";
            else if (base == 0x10) std::cout << " <- Global settings?";
            else if (data[0] >= 0x01 && data[0] <= 0x0D && data[1] <= 0x04) {
                std::cout << " <- LED Zone? Mode=" << (int)data[0];
            }

            std::cout << std::dec << std::endl;
        }
    }

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nLook for rows that look like LED configs:" << std::endl;
    std::cout << "  Mode (01-0D), Brightness (00-04), Speed, Dir, Random, R, G, B" << std::endl;
    return 0;
}
