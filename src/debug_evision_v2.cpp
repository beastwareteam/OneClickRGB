/*---------------------------------------------------------*\
| EVision V2 Keyboard Protocol Debug Tool                   |
| Based directly on OpenRGB EVisionV2KeyboardController     |
\*---------------------------------------------------------*/
#include <iostream>
#include <cstring>
#include <hidapi.h>
#include <windows.h>

// Protocol constants from OpenRGB
#define EVISION_V2_PACKET_SIZE      64
#define EVISION_V2_REPORT_ID        0x04

// Commands
#define CMD_BEGIN_CONFIGURE         0x01
#define CMD_END_CONFIGURE           0x02
#define CMD_READ_CONFIG             0x05
#define CMD_WRITE_CONFIG            0x06

// Offsets
#define OFFSET_CURRENT_PROFILE      0x00
#define OFFSET_FIRST_PROFILE        0x01

// Parameters
#define PARAM_MODE                  0x00
#define PARAM_BRIGHTNESS            0x01
#define PARAM_SPEED                 0x02
#define PARAM_DIRECTION             0x03
#define PARAM_RANDOM_COLOR_FLAG     0x04
#define PARAM_MODE_COLOR            0x05  // RGB at 0x05, 0x06, 0x07

// Modes
#define MODE_STATIC                 0x06  // "Normal" - single solid color

hid_device* dev = nullptr;

int Query(uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata)
{
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));

    buffer[0] = EVISION_V2_REPORT_ID;  // 0x04
    buffer[3] = cmd;
    buffer[4] = size;
    buffer[5] = offset & 0xFF;
    buffer[6] = (offset >> 8) & 0xFF;

    if (idata && size > 0) {
        memcpy(buffer + 8, idata, size);
    }

    // Compute checksum (sum of bytes 3-63)
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) {
        chksum += buffer[i];
    }
    buffer[1] = chksum & 0xFF;
    buffer[2] = (chksum >> 8) & 0xFF;

    // Debug output
    printf("TX: ");
    for (int i = 0; i < 16; i++) printf("%02X ", buffer[i]);
    printf("...\n");

    // Send
    int result = hid_write(dev, buffer, EVISION_V2_PACKET_SIZE);
    if (result < 0) {
        printf("Write failed!\n");
        return -1;
    }

    // Read response
    int bytes_read = hid_read_timeout(dev, buffer, EVISION_V2_PACKET_SIZE, 1000);
    if (bytes_read > 0) {
        printf("RX: ");
        for (int i = 0; i < 16; i++) printf("%02X ", buffer[i]);
        printf("...\n");

        if (odata && buffer[4] > 0) {
            memcpy(odata, buffer + 8, buffer[4]);
        }
        return buffer[4];  // Return data size
    }

    return 0;
}

bool SetStaticColor(uint8_t r, uint8_t g, uint8_t b)
{
    printf("\n=== Setting Static Color: RGB(%d, %d, %d) ===\n", r, g, b);

    // Step 1: Begin configure
    printf("\n[1] Begin Configure\n");
    Query(CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);

    // Step 2: Read current profile
    printf("\n[2] Read Current Profile\n");
    uint8_t current_profile = 0;
    Query(CMD_READ_CONFIG, OFFSET_CURRENT_PROFILE, nullptr, 1, &current_profile);
    printf("    Current profile: %d\n", current_profile);
    if (current_profile > 2) {
        current_profile = 0;
        printf("    Invalid profile, setting to 0\n");
        // Write correct profile
        Query(CMD_WRITE_CONFIG, OFFSET_CURRENT_PROFILE, &current_profile, 1, nullptr);
    }

    // Step 3: Write mode settings to profile
    printf("\n[3] Write Mode Settings\n");
    uint8_t config[18];
    memset(config, 0, sizeof(config));

    config[PARAM_MODE]              = MODE_STATIC;  // 0x06 = Static
    config[PARAM_BRIGHTNESS]        = 0x04;         // Max brightness
    config[PARAM_SPEED]             = 0x03;         // Normal speed
    config[PARAM_DIRECTION]         = 0x00;
    config[PARAM_RANDOM_COLOR_FLAG] = 0x00;
    config[PARAM_MODE_COLOR + 0]    = r;            // Red
    config[PARAM_MODE_COLOR + 1]    = g;            // Green
    config[PARAM_MODE_COLOR + 2]    = b;            // Blue

    uint16_t offset = current_profile * 0x40 + OFFSET_FIRST_PROFILE;
    printf("    Writing to offset: 0x%04X\n", offset);

    Query(CMD_WRITE_CONFIG, offset, config, sizeof(config), nullptr);

    // Step 4: End configure
    printf("\n[4] End Configure\n");
    Query(CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);

    return true;
}

int main()
{
    printf("=== EVision V2 Protocol Debug (OpenRGB-based) ===\n\n");

    hid_init();

    // Find keyboard with usage page 0xFF1C
    hid_device_info* devs = hid_enumerate(0x3299, 0x4E9F);
    hid_device_info* cur = devs;

    while (cur) {
        printf("Found: Interface %d, Usage Page 0x%04X\n",
               cur->interface_number, cur->usage_page);

        if (cur->usage_page == 0xFF1C) {
            dev = hid_open_path(cur->path);
            if (dev) {
                printf("Opened: %s\n", cur->path);
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!dev) {
        printf("ERROR: Could not open keyboard!\n");
        return 1;
    }

    // Test 1: Set RED
    SetStaticColor(255, 0, 0);
    printf("\n>>> Check keyboard - should be RED <<<\n");
    Sleep(3000);

    // Test 2: Set GREEN
    SetStaticColor(0, 255, 0);
    printf("\n>>> Check keyboard - should be GREEN <<<\n");
    Sleep(3000);

    // Test 3: Set BLUE (restore original)
    SetStaticColor(0, 21, 214);  // Your original blue
    printf("\n>>> Check keyboard - should be BLUE <<<\n");

    hid_close(dev);
    hid_exit();

    return 0;
}
