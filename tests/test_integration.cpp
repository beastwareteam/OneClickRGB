/**
 * OneClickRGB - Integration Tests
 *
 * Tests complete workflows and multi-device scenarios
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include "hidapi.h"

//=============================================================================
// CONSTANTS
//=============================================================================

namespace Devices {
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t ASUS_AURA_PID = 0x19AF;
    constexpr uint16_t ASUS_USAGE_PAGE = 0xFF72;
    constexpr uint16_t STEELSERIES_VID = 0x1038;
    constexpr uint16_t RIVAL_600_PID = 0x1724;
    constexpr uint16_t EVISION_VID = 0x3299;
    constexpr uint16_t EVISION_PID = 0x4E9F;
    constexpr uint16_t EVISION_USAGE_PAGE = 0xFF1C;
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[64];
    memset(buffer, 0, 64);
    buffer[0] = 0x04; buffer[3] = cmd; buffer[4] = size;
    buffer[5] = offset & 0xff; buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < 64; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff; buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, 64) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, 64, 100); retries--; }
    while (bytes_read > 0 && buffer[0] != 0x04 && retries > 0);
    if (bytes_read != 64) return -2;
    if (buffer[7] != 0) return -buffer[7];
    if (odata && buffer[4] > 0) memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

struct Color { uint8_t r, g, b; const char* name; };

Color TEST_COLORS[] = {
    {0, 34, 255, "Blue"},
    {255, 0, 0, "Red"},
    {0, 255, 0, "Green"},
    {255, 255, 0, "Yellow"},
    {255, 0, 255, "Magenta"},
    {0, 255, 255, "Cyan"},
    {255, 255, 255, "White"},
    {0, 0, 0, "Off"}
};

//=============================================================================
// INTEGRATION TEST 1: Full System Color Sync
//=============================================================================

bool Test_FullSystem_ColorSync(uint8_t r, uint8_t g, uint8_t b) {
    std::cout << "Testing full system sync to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")..." << std::endl;

    int devices_set = 0;

    // ASUS Aura
    {
        hid_device* dev = nullptr;
        struct hid_device_info* devs = hid_enumerate(Devices::ASUS_VID, Devices::ASUS_AURA_PID);
        for (auto* cur = devs; cur; cur = cur->next) {
            if (cur->usage_page == Devices::ASUS_USAGE_PAGE) {
                dev = hid_open_path(cur->path);
                break;
            }
        }
        hid_free_enumeration(devs);

        if (dev) {
            for (int channel = 0; channel < 8; channel++) {
                uint8_t buf[64] = {0};
                buf[0] = 0xEC; buf[1] = 0x40;
                buf[2] = channel | 0x80;
                buf[3] = 0; buf[4] = 1;
                buf[5] = r; buf[6] = g; buf[7] = b;
                hid_write(dev, buf, 64);
                Sleep(2);
            }
            hid_close(dev);
            devices_set++;
            std::cout << "  [OK] ASUS Aura" << std::endl;
        }
    }

    // SteelSeries
    {
        hid_device* dev = nullptr;
        struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
        for (auto* cur = devs; cur; cur = cur->next) {
            if (cur->interface_number == 0) {
                dev = hid_open_path(cur->path);
                break;
            }
        }
        hid_free_enumeration(devs);

        if (dev) {
            for (int i = 0; i < 8; i++) {
                uint8_t pkt[8] = {0x1C, 0x27, 0x00, (uint8_t)(1 << i), r, g, b, 0};
                hid_write(dev, pkt, 7);
                Sleep(10);
            }
            uint8_t save[10] = {0x09};
            hid_write(dev, save, 9);
            hid_close(dev);
            devices_set++;
            std::cout << "  [OK] SteelSeries" << std::endl;
        }
    }

    // EVision Keyboard
    {
        hid_device* dev = nullptr;
        struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
        for (auto* cur = devs; cur; cur = cur->next) {
            if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
                dev = hid_open_path(cur->path);
                break;
            }
        }
        hid_free_enumeration(devs);

        if (dev) {
            EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
            Sleep(20);

            // Keyboard
            uint8_t config[18] = {0x06, 0x04, 0, 0, 0, r, g, b};
            EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
            Sleep(10);

            // Edge
            uint8_t edge[10] = {0x04, 0x04, 0x00, 0x00, 0x00, r, g, b, 0x00, 0x01};
            EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);

            uint8_t unlock[2] = {0x00, 0x00};
            EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
            EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
            hid_close(dev);
            devices_set++;
            std::cout << "  [OK] EVision Keyboard + Edge" << std::endl;
        }
    }

    std::cout << "  Total devices: " << devices_set << std::endl;
    return devices_set > 0;
}

//=============================================================================
// INTEGRATION TEST 2: Color Cycle Animation
//=============================================================================

bool Test_ColorCycle_Animation() {
    std::cout << "\nRunning color cycle animation test..." << std::endl;

    for (const auto& color : TEST_COLORS) {
        std::cout << "  Setting " << color.name << "..." << std::flush;
        if (Test_FullSystem_ColorSync(color.r, color.g, color.b)) {
            std::cout << " OK" << std::endl;
        } else {
            std::cout << " FAILED" << std::endl;
            return false;
        }
        Sleep(500);
    }

    // Restore to blue
    std::cout << "  Restoring to Blue..." << std::endl;
    Test_FullSystem_ColorSync(0, 34, 255);

    return true;
}

//=============================================================================
// INTEGRATION TEST 3: Keyboard Mode Cycle
//=============================================================================

bool Test_KeyboardMode_Cycle() {
    std::cout << "\nRunning keyboard mode cycle test..." << std::endl;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "  [SKIP] Keyboard not found" << std::endl;
        return true;
    }

    struct Mode { uint8_t value; const char* name; };
    Mode modes[] = {
        {0x06, "Static"},
        {0x05, "Breathing"},
        {0x04, "Spectrum"},
        {0x01, "Wave Short"},
        {0x0C, "Rainbow Wave"},
        {0x07, "Reactive"},
        {0x06, "Static (restore)"}
    };

    bool success = true;
    for (const auto& mode : modes) {
        std::cout << "  Mode: " << mode.name << "..." << std::flush;

        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t config[18] = {mode.value, 0x04, 0x02, 0, 0, 0, 34, 255};
        int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res >= 0) {
            std::cout << " OK" << std::endl;
        } else {
            std::cout << " FAILED" << std::endl;
            success = false;
        }
        Sleep(800);
    }

    hid_close(dev);
    return success;
}

//=============================================================================
// INTEGRATION TEST 4: Edge Mode Cycle
//=============================================================================

bool Test_EdgeMode_Cycle() {
    std::cout << "\nRunning edge mode cycle test..." << std::endl;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "  [SKIP] Keyboard not found" << std::endl;
        return true;
    }

    struct Mode { uint8_t value; const char* name; };
    Mode modes[] = {
        {0x04, "Static"},
        {0x03, "Breathing"},
        {0x02, "Spectrum"},
        {0x01, "Wave"},
        {0x00, "Freeze"},
        {0x05, "Off"},
        {0x04, "Static (restore)"}
    };

    bool success = true;
    for (const auto& mode : modes) {
        std::cout << "  Edge Mode: " << mode.name << "..." << std::flush;

        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t edge[10] = {mode.value, 0x04, 0x02, 0x00, 0x00, 0, 34, 255, 0x00, 0x01};
        int res = EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res >= 0) {
            std::cout << " OK" << std::endl;
        } else {
            std::cout << " FAILED" << std::endl;
            success = false;
        }
        Sleep(800);
    }

    hid_close(dev);
    return success;
}

//=============================================================================
// INTEGRATION TEST 5: Brightness Fade
//=============================================================================

bool Test_Brightness_Fade() {
    std::cout << "\nRunning brightness fade test..." << std::endl;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "  [SKIP] Keyboard not found" << std::endl;
        return true;
    }

    // Fade down
    std::cout << "  Fading down..." << std::endl;
    for (int brightness = 4; brightness >= 0; brightness--) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(10);

        uint8_t config[18] = {0x06, (uint8_t)brightness, 0, 0, 0, 0, 34, 255};
        EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
        Sleep(200);
    }

    // Fade up
    std::cout << "  Fading up..." << std::endl;
    for (int brightness = 0; brightness <= 4; brightness++) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(10);

        uint8_t config[18] = {0x06, (uint8_t)brightness, 0, 0, 0, 0, 34, 255};
        EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
        Sleep(200);
    }

    hid_close(dev);
    return true;
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "OneClickRGB Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    hid_init();

    int passed = 0, failed = 0;

    // Test 1: Full System Color Sync
    std::cout << "=== Test 1: Full System Color Sync ===" << std::endl;
    if (Test_FullSystem_ColorSync(0, 34, 255)) {
        std::cout << "PASSED\n" << std::endl;
        passed++;
    } else {
        std::cout << "FAILED\n" << std::endl;
        failed++;
    }

    // Test 2: Color Cycle Animation
    std::cout << "=== Test 2: Color Cycle Animation ===" << std::endl;
    if (Test_ColorCycle_Animation()) {
        std::cout << "PASSED\n" << std::endl;
        passed++;
    } else {
        std::cout << "FAILED\n" << std::endl;
        failed++;
    }

    // Test 3: Keyboard Mode Cycle
    std::cout << "=== Test 3: Keyboard Mode Cycle ===" << std::endl;
    if (Test_KeyboardMode_Cycle()) {
        std::cout << "PASSED\n" << std::endl;
        passed++;
    } else {
        std::cout << "FAILED\n" << std::endl;
        failed++;
    }

    // Test 4: Edge Mode Cycle
    std::cout << "=== Test 4: Edge Mode Cycle ===" << std::endl;
    if (Test_EdgeMode_Cycle()) {
        std::cout << "PASSED\n" << std::endl;
        passed++;
    } else {
        std::cout << "FAILED\n" << std::endl;
        failed++;
    }

    // Test 5: Brightness Fade
    std::cout << "=== Test 5: Brightness Fade ===" << std::endl;
    if (Test_Brightness_Fade()) {
        std::cout << "PASSED\n" << std::endl;
        passed++;
    } else {
        std::cout << "FAILED\n" << std::endl;
        failed++;
    }

    hid_exit();

    // Summary
    std::cout << "========================================" << std::endl;
    std::cout << "Integration Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    return failed > 0 ? 1 : 0;
}
