/*---------------------------------------------------------*\
| EVision Keyboard Protocol Debug Tool                      |
| Tests different packet formats to find working protocol   |
\*---------------------------------------------------------*/
#include <iostream>
#include <cstring>
#include <hidapi.h>
#include <windows.h>

// Protocol constants
constexpr uint8_t HEADER = 0x04;
constexpr uint8_t CMD_BEGIN = 0x01;
constexpr uint8_t CMD_END = 0x02;
constexpr uint8_t CMD_SET_PARAM = 0x06;

void ComputeChecksum(uint8_t* buf) {
    uint16_t sum = 0;
    for (int i = 0x03; i < 64; i++) {
        sum += buf[i];
    }
    buf[0x01] = sum & 0xFF;
    buf[0x02] = (sum >> 8) & 0xFF;
}

void PrintPacket(const char* name, uint8_t* buf, int len) {
    std::cout << name << ": ";
    for (int i = 0; i < len && i < 20; i++) {
        printf("%02X ", buf[i]);
    }
    std::cout << "...\n";
}

int main() {
    std::cout << "=== EVision Keyboard Debug ===\n\n";

    hid_init();

    // Find keyboard - VID 0x3299, PID 0x4E9F, Interface 1, Usage Page 0xFF1C
    hid_device_info* devs = hid_enumerate(0x3299, 0x4E9F);
    hid_device_info* cur = devs;
    hid_device* dev = nullptr;

    // First pass: find 0xFF1C
    while (cur) {
        std::cout << "Found: Interface " << cur->interface_number
                  << ", Usage Page 0x" << std::hex << cur->usage_page << std::dec << "\n";

        // MUST be usage page 0xFF1C for RGB control
        if (cur->usage_page == 0xFF1C) {
            dev = hid_open_path(cur->path);
            if (dev) {
                std::cout << "Opened RGB interface: " << cur->path << "\n";
                break;
            }
        }
        cur = cur->next;
    }

    // If not found, try any vendor-specific page
    if (!dev) {
        cur = hid_enumerate(0x3299, 0x4E9F);
        while (cur) {
            if (cur->usage_page >= 0xFF00) {  // Vendor-specific pages
                dev = hid_open_path(cur->path);
                if (dev) {
                    std::cout << "Opened vendor interface (0x" << std::hex << cur->usage_page << std::dec << "): " << cur->path << "\n";
                    break;
                }
            }
            cur = cur->next;
        }
        hid_free_enumeration(cur);
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cerr << "ERROR: Could not open keyboard!\n";
        return 1;
    }

    uint8_t buf[64] = {0};
    uint8_t response[64] = {0};

    std::cout << "\n=== Test 1: Original Protocol (Mode 6 = Static, RED) ===\n";

    // BEGIN
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_BEGIN;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_BEGIN;
    PrintPacket("BEGIN", buf, 10);
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    // SET PARAMETER - Mode settings
    // Format from profile: SelectItem=6, Light=4, Speed=2, R=255, G=0, B=0
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x03] = CMD_SET_PARAM;
    buf[0x04] = 0x08;  // data size
    buf[0x05] = 0x00;  // parameter ID (mode settings)

    // Parameter data at offset 0x08
    buf[0x08] = 0x06;  // Mode = 6 (Static/Normal)
    buf[0x09] = 0x04;  // Brightness = 4 (max)
    buf[0x0A] = 0x02;  // Speed = 2
    buf[0x0B] = 0x00;  // Direction
    buf[0x0C] = 0x00;  // Random color flag
    buf[0x0D] = 0xFF;  // Red
    buf[0x0E] = 0x00;  // Green
    buf[0x0F] = 0x00;  // Blue

    ComputeChecksum(buf);
    PrintPacket("SET_PARAM", buf, 20);
    int result = hid_write(dev, buf, 64);
    std::cout << "Write result: " << result << "\n";
    hid_read_timeout(dev, response, 64, 100);

    // END
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_END;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_END;
    PrintPacket("END", buf, 10);
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    Sleep(2000);

    std::cout << "\n=== Test 2: Try GREEN ===\n";

    // BEGIN
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_BEGIN;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_BEGIN;
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    // SET PARAMETER - GREEN
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x03] = CMD_SET_PARAM;
    buf[0x04] = 0x08;
    buf[0x05] = 0x00;

    buf[0x08] = 0x06;  // Mode = Static
    buf[0x09] = 0x04;  // Brightness max
    buf[0x0A] = 0x02;  // Speed
    buf[0x0B] = 0x00;
    buf[0x0C] = 0x00;
    buf[0x0D] = 0x00;  // R
    buf[0x0E] = 0xFF;  // G
    buf[0x0F] = 0x00;  // B

    ComputeChecksum(buf);
    PrintPacket("SET_PARAM GREEN", buf, 20);
    result = hid_write(dev, buf, 64);
    std::cout << "Write result: " << result << "\n";
    hid_read_timeout(dev, response, 64, 100);

    // END
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_END;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_END;
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    Sleep(2000);

    std::cout << "\n=== Test 3: Restore BLUE (from your profile) ===\n";

    // BEGIN
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_BEGIN;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_BEGIN;
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    // From profile: Red=0, Green=21, Blue=214
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x03] = CMD_SET_PARAM;
    buf[0x04] = 0x08;
    buf[0x05] = 0x00;

    buf[0x08] = 0x06;  // Mode = Static (SelectItem=6)
    buf[0x09] = 0x04;  // Brightness = 4 (Light=4)
    buf[0x0A] = 0x02;  // Speed = 2
    buf[0x0B] = 0x00;
    buf[0x0C] = 0x00;
    buf[0x0D] = 0x00;  // Red = 0
    buf[0x0E] = 0x15;  // Green = 21 (0x15)
    buf[0x0F] = 0xD6;  // Blue = 214 (0xD6)

    ComputeChecksum(buf);
    PrintPacket("SET_PARAM BLUE", buf, 20);
    result = hid_write(dev, buf, 64);
    std::cout << "Write result: " << result << "\n";
    hid_read_timeout(dev, response, 64, 100);

    // END
    memset(buf, 0, 64);
    buf[0x00] = HEADER;
    buf[0x01] = CMD_END;
    buf[0x02] = 0x00;
    buf[0x03] = CMD_END;
    hid_write(dev, buf, 64);
    hid_read_timeout(dev, response, 64, 100);

    std::cout << "\n=== Check your keyboard! ===\n";
    std::cout << "Did the colors change? (RED -> GREEN -> BLUE)\n";

    hid_close(dev);
    hid_exit();

    return 0;
}
