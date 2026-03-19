/*---------------------------------------------------------*\
| SteelSeries Rival 600 Protocol Debug Tool                 |
| Based on OpenRGB SteelSeriesRivalController               |
\*---------------------------------------------------------*/
#include <iostream>
#include <cstring>
#include <hidapi.h>
#include <windows.h>

hid_device* dev = nullptr;

// Rival 600 SetColor - EXACTLY like OpenRGB (7 bytes, NO Report ID)
// OpenRGB line 240: hid_write(dev, usb_pkt, 0x07);
void SetRival600Color(uint8_t zone_id, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t usb_pkt[7];
    memset(usb_pkt, 0x00, sizeof(usb_pkt));

    usb_pkt[0] = 0x1C;           // Command
    usb_pkt[1] = 0x27;           // Sub-command
    usb_pkt[2] = 0x00;           // Reserved
    usb_pkt[3] = 1 << zone_id;   // Zone bitmask
    usb_pkt[4] = r;
    usb_pkt[5] = g;
    usb_pkt[6] = b;

    printf("Zone %d (mask 0x%02X): TX [%02X %02X %02X %02X %02X %02X %02X]\n",
           zone_id, 1 << zone_id,
           usb_pkt[0], usb_pkt[1], usb_pkt[2], usb_pkt[3],
           usb_pkt[4], usb_pkt[5], usb_pkt[6]);

    int result = hid_write(dev, usb_pkt, 7);
    printf("  Result: %d\n", result);
}

// Set all 8 zones
void SetAllZones(uint8_t r, uint8_t g, uint8_t b)
{
    for (int zone = 0; zone < 8; zone++) {
        SetRival600Color(zone, r, g, b);
    }
}

// Alternative: Set all at once with bitmask 0xFF (7 bytes, no Report ID)
void SetAllZonesAtOnce(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t usb_pkt[7];
    memset(usb_pkt, 0x00, sizeof(usb_pkt));

    usb_pkt[0] = 0x1C;
    usb_pkt[1] = 0x27;
    usb_pkt[2] = 0x00;
    usb_pkt[3] = 0xFF;  // All zones
    usb_pkt[4] = r;
    usb_pkt[5] = g;
    usb_pkt[6] = b;

    printf("All zones (0xFF): TX [%02X %02X %02X %02X %02X %02X %02X]\n",
           usb_pkt[0], usb_pkt[1], usb_pkt[2], usb_pkt[3],
           usb_pkt[4], usb_pkt[5], usb_pkt[6]);

    int result = hid_write(dev, usb_pkt, 7);
    printf("  Result: %d\n", result);
}

// SaveMode - uses send_usb_msg which adds Report ID internally
// So we send 10 bytes: [0x00 (report ID) + 0x09 + 8 zeros]
void SaveMode()
{
    uint8_t usb_pkt[10];
    memset(usb_pkt, 0x00, sizeof(usb_pkt));

    usb_pkt[0] = 0x00;  // Report ID (hidapi on Windows needs this)
    usb_pkt[1] = 0x09;  // Save command

    printf("SaveMode: TX [%02X %02X ...]\n", usb_pkt[0], usb_pkt[1]);

    int result = hid_write(dev, usb_pkt, 10);
    printf("  Result: %d\n", result);
}

// Test without SaveMode - maybe Rival 600 doesn't need it?
void TestWithoutSave(uint8_t r, uint8_t g, uint8_t b)
{
    printf("\n--- Setting color WITHOUT SaveMode ---\n");
    SetAllZones(r, g, b);
    printf("--- Done (no save) ---\n");
}

int main()
{
    printf("=== SteelSeries Rival 600 Debug ===\n\n");

    hid_init();

    // Find mouse - VID 0x1038, PID 0x1724
    hid_device_info* devs = hid_enumerate(0x1038, 0x1724);
    hid_device_info* cur = devs;

    printf("Searching for Rival 600...\n");
    while (cur) {
        printf("Found: Interface %d, Usage Page 0x%04X, Usage 0x%04X\n",
               cur->interface_number, cur->usage_page, cur->usage);

        // Try interface 0 with vendor usage page
        if (cur->interface_number == 0) {
            dev = hid_open_path(cur->path);
            if (dev) {
                printf("Opened: %s\n\n", cur->path);
                break;
            }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);

    if (!dev) {
        printf("ERROR: Could not open mouse!\n");
        return 1;
    }

    // Test 1: RED - hold for 5 seconds
    printf("\n=== Setting RED - watch the mouse! ===\n");
    SetAllZones(255, 0, 0);
    printf("\n>>> MOUSE SHOULD BE RED NOW - waiting 5 seconds <<<\n");
    Sleep(5000);

    // Test 2: GREEN - hold for 5 seconds
    printf("\n=== Setting GREEN - watch the mouse! ===\n");
    SetAllZones(0, 255, 0);
    printf("\n>>> MOUSE SHOULD BE GREEN NOW - waiting 5 seconds <<<\n");
    Sleep(5000);

    // Test 3: Restore original BLUE
    printf("\n=== Restoring BLUE (#0022FF) ===\n");
    SetAllZones(0, 0x22, 0xFF);
    SaveMode();
    printf("\n>>> DONE - Mouse should be BLUE <<<\n");

    hid_close(dev);
    hid_exit();

    return 0;
}
