// EVision Memory Scanner - Find where edge LED colors are stored
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>

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

int main() {
    std::cout << "=== EVision Memory Scanner ===" << std::endl;

    hid_init();
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    if (!dev) { std::cout << "Not found" << std::endl; return 1; }

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    std::cout << "Scanning config memory (cmd 0x05)..." << std::endl;
    std::cout << "Looking for potential edge/color data..." << std::endl << std::endl;

    // Scan first 256 bytes of config memory
    for (uint16_t base = 0; base < 0x100; base += 0x10) {
        uint8_t data[16];
        int ret = EVisionQuery(dev, 0x05, base, nullptr, 16, data);
        if (ret > 0) {
            // Check if this row contains potential RGB data (non-zero values)
            bool has_data = false;
            for (int i = 0; i < 16; i++) {
                if (data[i] != 0) { has_data = true; break; }
            }

            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << base << ": ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::setw(2) << (int)data[i] << " ";
            }

            // Annotate known offsets
            if (base == 0x00) std::cout << " <- Profile select";
            else if (base == 0x00 && data[0] == 0) std::cout << " <- Profile 0";
            else if (base >= 0x01 && base < 0x40) std::cout << " <- Profile 0 config";
            else if (base >= 0x41 && base < 0x80) std::cout << " <- Profile 1 config";
            else if (base >= 0x81 && base < 0xC0) std::cout << " <- Profile 2 config";

            std::cout << std::dec << std::endl;
        }
    }

    // Also scan custom color memory (command 0x0A)
    std::cout << "\nScanning custom color memory (cmd 0x0A)..." << std::endl;
    for (uint16_t base = 0; base < 0x100; base += 0x10) {
        uint8_t data[16];
        int ret = EVisionQuery(dev, 0x0A, base, nullptr, 16, data);
        if (ret > 0) {
            bool has_data = false;
            for (int i = 0; i < 16; i++) {
                if (data[i] != 0) { has_data = true; break; }
            }
            if (has_data) {
                std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) << base << ": ";
                for (int i = 0; i < 16; i++) {
                    std::cout << std::setw(2) << (int)data[i] << " ";
                }
                std::cout << std::dec << std::endl;
            }
        }
    }

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();
    return 0;
}
