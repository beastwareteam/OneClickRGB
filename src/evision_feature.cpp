// Try Feature Reports for Edge LEDs
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <hidapi.h>
#include <cstring>
#include <cstdlib>

#define EVISION_VID 0x3299
#define EVISION_PID 0x4E9F
#define EVISION_USAGE_PAGE 0xFF1C

int main(int argc, char* argv[]) {
    uint8_t r = 0, g = 34, b = 255;
    if (argc >= 4) {
        r = atoi(argv[1]);
        g = atoi(argv[2]);
        b = atoi(argv[3]);
    }

    std::cout << "=== Testing Feature Reports for Edge LEDs ===" << std::endl;
    std::cout << "Target: RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    hid_init();

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(EVISION_VID, EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            std::cout << "Opened: " << cur->path << std::endl;
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) { std::cout << "Keyboard not found!" << std::endl; return 1; }

    // Try different Report IDs for Feature Reports
    uint8_t report_ids[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    for (uint8_t rid : report_ids) {
        uint8_t buffer[65];
        memset(buffer, 0, sizeof(buffer));
        buffer[0] = rid;

        int res = hid_get_feature_report(dev, buffer, sizeof(buffer));
        if (res > 0) {
            std::cout << "Feature Report " << std::hex << (int)rid << ": ";
            for (int i = 0; i < (res > 20 ? 20 : res); i++) {
                std::cout << std::setfill('0') << std::setw(2) << (int)buffer[i] << " ";
            }
            if (res > 20) std::cout << "...";
            std::cout << std::dec << " (" << res << " bytes)" << std::endl;
        }
    }

    // Try sending a Feature Report with edge color
    std::cout << "\nTrying to set Feature Report with edge color..." << std::endl;

    // Try Report ID 0x04 (same as the normal protocol)
    uint8_t feature[65];
    memset(feature, 0, sizeof(feature));
    feature[0] = 0x04;  // Report ID
    feature[1] = 0x06;  // Mode = Static
    feature[2] = 0x04;  // Brightness
    feature[3] = r;
    feature[4] = g;
    feature[5] = b;

    int res = hid_send_feature_report(dev, feature, sizeof(feature));
    std::cout << "Send Feature Report 0x04: " << res << std::endl;

    // Try Report ID 0x07
    feature[0] = 0x07;
    res = hid_send_feature_report(dev, feature, sizeof(feature));
    std::cout << "Send Feature Report 0x07: " << res << std::endl;

    // Try Report ID 0x00
    feature[0] = 0x00;
    res = hid_send_feature_report(dev, feature, 65);
    std::cout << "Send Feature Report 0x00: " << res << std::endl;

    hid_close(dev);
    hid_exit();

    std::cout << "\nDone!" << std::endl;
    return 0;
}
