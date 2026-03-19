// EVision Keyboard Debug Tool - Read all config data
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

    if (idata && size > 0) {
        memcpy(buffer + 8, idata, size);
    }

    // Calculate checksum (sum of bytes 3-63)
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) {
        chksum += buffer[i];
    }
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;

    int res = hid_send_feature_report(dev, buffer, sizeof(buffer));
    if (res < 0) return -1;

    Sleep(10);

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = EVISION_V2_REPORT_ID;
    int bytes_read = hid_get_feature_report(dev, buffer, sizeof(buffer));
    if (bytes_read != sizeof(buffer) || buffer[0] != EVISION_V2_REPORT_ID) return -2;
    if (buffer[7] != 0) return -buffer[7];

    if (odata && buffer[4] > 0) {
        memcpy(odata, buffer + 8, buffer[4]);
    }
    return buffer[4];
}

int main() {
    std::cout << "=== EVision Keyboard Debug Tool ===" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            std::cout << "Found keyboard at: " << cur->path << std::endl;
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "Keyboard not found!" << std::endl;
        return 1;
    }

    // Begin configure
    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Read current profile number
    uint8_t current_profile = 0;
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x00, nullptr, 1, &current_profile);
    std::cout << "\nCurrent profile: " << (int)current_profile << std::endl;

    // Read all 3 profiles (each 0x40 = 64 bytes apart, starting at 0x01)
    for (int profile = 0; profile < 3; profile++) {
        uint16_t offset = 0x01 + (profile * 0x40);
        std::cout << "\n=== Profile " << profile << " (offset 0x" << std::hex << offset << std::dec << ") ===" << std::endl;

        // Read 64 bytes
        uint8_t data[64];
        memset(data, 0, sizeof(data));
        for (int chunk = 0; chunk < 64; chunk += 32) {
            uint8_t buf[56];
            memset(buf, 0, sizeof(buf));
            int res = EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, offset + chunk, nullptr, 32, buf);
            if (res > 0) {
                memcpy(data + chunk, buf, (res < 32) ? res : 32);
            }
        }

        // Print hex dump
        for (int row = 0; row < 4; row++) {
            std::cout << "  " << std::hex << std::setfill('0') << std::setw(2) << (row * 16) << ": ";
            for (int col = 0; col < 16; col++) {
                std::cout << std::setw(2) << (int)data[row * 16 + col] << " ";
            }
            std::cout << std::endl;
        }
    }

    // Also read some global settings that might contain Win Lock
    std::cout << "\n=== Global Settings (offset 0xC0-0xFF) ===" << std::endl;
    for (int block = 0; block < 4; block++) {
        uint16_t offset = 0xC0 + (block * 16);
        uint8_t data[16];
        int res = EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, offset, nullptr, 16, data);

        std::cout << "  " << std::hex << std::setfill('0') << std::setw(2) << offset << ": ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }

    // End configure
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone! Look for non-zero bytes that might control Win Lock." << std::endl;
    return 0;
}
