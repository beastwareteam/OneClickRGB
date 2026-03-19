// EVision Full Memory Scan - Find Windows Key Lock setting
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

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, uint8_t size, uint8_t* odata) {
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));

    buffer[0] = EVISION_V2_REPORT_ID;
    buffer[3] = cmd;
    buffer[4] = size;
    buffer[5] = offset & 0xff;
    buffer[6] = (offset >> 8) & 0xff;

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
    std::cout << "=== EVision Full Memory Scan ===" << std::endl;
    std::cout << "Looking for Windows Key Lock setting...\n" << std::endl;

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
    EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, 0, nullptr);
    Sleep(20);

    // Scan memory ranges looking for non-zero bytes
    std::cout << "Scanning memory 0x0000-0x0200..." << std::endl;

    for (uint16_t base = 0; base < 0x200; base += 16) {
        uint8_t data[16];
        memset(data, 0xFF, sizeof(data));  // Fill with 0xFF to detect actual reads

        int res = EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, base, 16, data);

        // Check if any byte is non-zero (and not our 0xFF fill)
        bool has_data = false;
        for (int i = 0; i < 16; i++) {
            if (data[i] != 0x00 && data[i] != 0xFF) {
                has_data = true;
                break;
            }
        }

        if (has_data || base < 0x10 || (base >= 0x01 && base < 0x100)) {
            std::cout << std::hex << std::setfill('0') << std::setw(4) << base << ": ";
            for (int i = 0; i < 16; i++) {
                std::cout << std::setw(2) << (int)data[i] << " ";
            }

            // Annotate known offsets
            if (base == 0x00) std::cout << " <- Current profile";
            if (base == 0x01) std::cout << " <- Profile 0 start";
            std::cout << std::dec << std::endl;
        }
    }

    // End configure
    EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, 0, nullptr);

    hid_close(dev);
    hid_exit();

    std::cout << "\nLook for bytes that might control keyboard functions." << std::endl;
    std::cout << "The Windows Lock is likely a single bit somewhere." << std::endl;
    return 0;
}
