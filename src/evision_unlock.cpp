// EVision Windows Key Unlock Tool
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

    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;

    if (hid_write(dev, buffer, sizeof(buffer)) < 0) return -1;

    int bytes_read;
    int retries = 10;
    do {
        bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100);
        retries--;
    } while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);

    if (bytes_read != sizeof(buffer)) return -2;
    if (buffer[7] != 0) return -buffer[7];

    if (odata && buffer[4] > 0) {
        memcpy(odata, buffer + 8, buffer[4]);
    }
    return buffer[4];
}

int main() {
    std::cout << "=== EVision Windows Key Unlock ===" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
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

    // Read current state at offset 0x10-0x1F
    uint8_t data[16];
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x10, nullptr, 16, data);

    std::cout << "Current 0x10-0x1F: ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // Try setting bytes 0x14 and 0x15 to 0x00 (potential Win Lock disable)
    std::cout << "\nTrying to clear potential Win Lock bytes at 0x14-0x15..." << std::endl;

    uint8_t unlock[2] = {0x00, 0x00};
    int res = EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, 0x14, unlock, 2, nullptr);
    if (res < 0) {
        std::cout << "Write failed: " << res << std::endl;
    } else {
        std::cout << "Write OK" << std::endl;
    }
    Sleep(10);

    // Read back
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x10, nullptr, 16, data);
    std::cout << "After:   0x10-0x1F: ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;

    // End configure
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nTry pressing the Windows key now!" << std::endl;
    return 0;
}
