/**
 * OneClickRGB - Complete Test Suite
 *
 * End-to-End Tests for all RGB devices and features
 *
 * Usage: test_suite.exe [options]
 *   --all         Run all tests
 *   --devices     Test device detection only
 *   --colors      Test color changes
 *   --modes       Test all keyboard/edge modes
 *   --profiles    Test profile save/load
 *   --stress      Run stress tests
 *   --verbose     Verbose output
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <functional>
#include <map>
#include "hidapi.h"

//=============================================================================
// TEST FRAMEWORK
//=============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    double duration_ms;
};

class TestSuite {
public:
    std::vector<TestResult> results;
    bool verbose = false;
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    void Run(const std::string& name, std::function<bool()> test) {
        std::cout << "[TEST] " << name << "... " << std::flush;

        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.name = name;

        try {
            result.passed = test();
            result.message = result.passed ? "OK" : "FAILED";
        } catch (const std::exception& e) {
            result.passed = false;
            result.message = std::string("EXCEPTION: ") + e.what();
        } catch (...) {
            result.passed = false;
            result.message = "UNKNOWN EXCEPTION";
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (result.passed) {
            std::cout << "\033[32mPASSED\033[0m";
            passed++;
        } else {
            std::cout << "\033[31mFAILED\033[0m";
            failed++;
        }
        std::cout << " (" << std::fixed << std::setprecision(1) << result.duration_ms << "ms)";

        if (!result.passed || verbose) {
            std::cout << " - " << result.message;
        }
        std::cout << std::endl;

        results.push_back(result);
    }

    void Skip(const std::string& name, const std::string& reason) {
        std::cout << "[SKIP] " << name << " - " << reason << std::endl;
        skipped++;
    }

    void PrintSummary() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Total:   " << (passed + failed + skipped) << std::endl;
        std::cout << "\033[32mPassed:  " << passed << "\033[0m" << std::endl;
        std::cout << "\033[31mFailed:  " << failed << "\033[0m" << std::endl;
        std::cout << "Skipped: " << skipped << std::endl;

        if (failed > 0) {
            std::cout << "\nFailed tests:" << std::endl;
            for (const auto& r : results) {
                if (!r.passed) {
                    std::cout << "  - " << r.name << ": " << r.message << std::endl;
                }
            }
        }

        double total_time = 0;
        for (const auto& r : results) total_time += r.duration_ms;
        std::cout << "\nTotal time: " << std::fixed << std::setprecision(1) << total_time << "ms" << std::endl;
    }
};

//=============================================================================
// DEVICE CONSTANTS
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
// DEVICE HANDLES (for testing)
//=============================================================================

struct DeviceState {
    bool aura_found = false;
    bool steelseries_found = false;
    bool evision_found = false;
    bool gskill_found = false;
    std::string aura_path;
    std::string steelseries_path;
    std::string evision_path;
};

DeviceState g_devices;

//=============================================================================
// EVISION HELPER
//=============================================================================

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

//=============================================================================
// DEVICE DETECTION TESTS
//=============================================================================

bool Test_HidApi_Init() {
    int result = hid_init();
    return result == 0;
}

bool Test_HidApi_Enumerate() {
    struct hid_device_info* devs = hid_enumerate(0, 0);
    bool found_any = (devs != nullptr);
    int count = 0;
    for (auto* cur = devs; cur; cur = cur->next) count++;
    hid_free_enumeration(devs);
    return found_any && count > 0;
}

bool Test_Asus_Aura_Detection() {
    struct hid_device_info* devs = hid_enumerate(Devices::ASUS_VID, Devices::ASUS_AURA_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::ASUS_USAGE_PAGE) {
            g_devices.aura_found = true;
            g_devices.aura_path = cur->path;
            break;
        }
    }
    hid_free_enumeration(devs);
    return g_devices.aura_found;
}

bool Test_SteelSeries_Detection() {
    struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->interface_number == 0) {
            g_devices.steelseries_found = true;
            g_devices.steelseries_path = cur->path;
            break;
        }
    }
    hid_free_enumeration(devs);
    return g_devices.steelseries_found;
}

bool Test_EVision_Detection() {
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            g_devices.evision_found = true;
            g_devices.evision_path = cur->path;
            break;
        }
    }
    hid_free_enumeration(devs);
    return g_devices.evision_found;
}

bool Test_GSkill_PawnIO_Available() {
    HMODULE dll = LoadLibraryA("PawnIOLib.dll");
    if (dll) {
        FreeLibrary(dll);
        return true;
    }
    return false;
}

bool Test_GSkill_SMBus_Module() {
    HANDLE hFile = CreateFileA("SmbusI801.bin", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
        return size > 0;
    }
    return false;
}

//=============================================================================
// ASUS AURA TESTS
//=============================================================================

bool Test_Asus_Open_Close() {
    if (!g_devices.aura_found) return false;

    hid_device* dev = hid_open_path(g_devices.aura_path.c_str());
    if (!dev) return false;
    hid_close(dev);
    return true;
}

bool Test_Asus_Set_Color() {
    if (!g_devices.aura_found) return false;

    hid_device* dev = hid_open_path(g_devices.aura_path.c_str());
    if (!dev) return false;

    // Set channel 0 to blue
    uint8_t buf[65] = {0};  // 65 bytes for report ID
    buf[0] = 0xEC;
    buf[1] = 0x40;
    buf[2] = 0x80;  // Channel 0, last packet
    buf[3] = 0;     // Offset
    buf[4] = 1;     // LED count
    buf[5] = 0;     // R
    buf[6] = 34;    // G
    buf[7] = 255;   // B

    int result = hid_write(dev, buf, 65);
    hid_close(dev);
    return result > 0;  // Just check if write succeeded
}

bool Test_Asus_All_Channels() {
    if (!g_devices.aura_found) return false;

    hid_device* dev = hid_open_path(g_devices.aura_path.c_str());
    if (!dev) return false;

    bool success = true;
    for (int channel = 0; channel < 8; channel++) {
        uint8_t buf[65] = {0};  // 65 bytes for report ID
        buf[0] = 0xEC;
        buf[1] = 0x40;
        buf[2] = channel | 0x80;
        buf[3] = 0;
        buf[4] = 1;
        buf[5] = 0; buf[6] = 34; buf[7] = 255;

        if (hid_write(dev, buf, 65) <= 0) {
            success = false;
            break;
        }
        Sleep(2);
    }

    hid_close(dev);
    return success;
}

//=============================================================================
// STEELSERIES TESTS
//=============================================================================

bool Test_SteelSeries_Open_Close() {
    if (!g_devices.steelseries_found) return false;

    hid_device* dev = hid_open_path(g_devices.steelseries_path.c_str());
    if (!dev) return false;
    hid_close(dev);
    return true;
}

bool Test_SteelSeries_Set_Color() {
    if (!g_devices.steelseries_found) return false;

    hid_device* dev = hid_open_path(g_devices.steelseries_path.c_str());
    if (!dev) return false;

    // Set zone 0 to blue
    uint8_t pkt[8] = {0x1C, 0x27, 0x00, 0x01, 0, 34, 255, 0};
    int result = hid_write(dev, pkt, 7);

    hid_close(dev);
    return result > 0;
}

bool Test_SteelSeries_All_Zones() {
    if (!g_devices.steelseries_found) return false;

    hid_device* dev = hid_open_path(g_devices.steelseries_path.c_str());
    if (!dev) return false;

    bool success = true;
    for (int i = 0; i < 8; i++) {
        uint8_t pkt[8] = {0x1C, 0x27, 0x00, (uint8_t)(1 << i), 0, 34, 255, 0};
        if (hid_write(dev, pkt, 7) <= 0) {
            success = false;
            break;
        }
        Sleep(10);
    }

    // Save
    uint8_t save[10] = {0x09};
    hid_write(dev, save, 9);

    hid_close(dev);
    return success;
}

//=============================================================================
// EVISION KEYBOARD TESTS
//=============================================================================

bool Test_EVision_Open_Close() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;
    hid_close(dev);
    return true;
}

bool Test_EVision_Begin_End_Configure() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    int res1 = EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);  // Begin
    Sleep(20);
    int res2 = EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);  // End

    hid_close(dev);
    return res1 >= 0 && res2 >= 0;
}

bool Test_EVision_Read_Profile() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    uint8_t profile = 255;
    int res = EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return res > 0 && profile <= 2;
}

bool Test_EVision_Read_Config() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    uint8_t config[18];
    int res = EVisionQuery(dev, 0x05, 0x01, nullptr, 18, config);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return res == 18;
}

bool Test_EVision_Write_Color() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Read current config
    uint8_t config[18];
    EVisionQuery(dev, 0x05, 0x01, nullptr, 18, config);

    // Modify color to blue
    config[5] = 0;    // R
    config[6] = 34;   // G
    config[7] = 255;  // B

    // Write back
    int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return res >= 0;
}

bool Test_EVision_All_Modes() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    uint8_t modes[] = {0x06, 0x05, 0x04, 0x01, 0x02, 0x03, 0x07};  // Static, Breathing, etc.
    bool success = true;

    for (uint8_t mode : modes) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t config[18] = {0};
        config[0] = mode;
        config[1] = 0x04;  // Brightness
        config[5] = 0; config[6] = 34; config[7] = 255;  // Blue

        int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res < 0) {
            success = false;
            break;
        }
        Sleep(100);
    }

    // Restore to Static
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t config[18] = {0x06, 0x04, 0, 0, 0, 0, 34, 255};
    EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    return success;
}

bool Test_EVision_Edge_Config() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Read edge config at offset 0x1b
    uint8_t edge[10];
    int res = EVisionQuery(dev, 0x05, 0x1b, nullptr, 10, edge);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return res == 10;
}

bool Test_EVision_Edge_Write() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Write edge config
    uint8_t edge[10] = {0x04, 0x04, 0x00, 0x00, 0x00, 0, 34, 255, 0x00, 0x01};
    int res = EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);

    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return res >= 0;
}

bool Test_EVision_Edge_All_Modes() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    uint8_t modes[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};  // All edge modes
    bool success = true;

    for (uint8_t mode : modes) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t edge[10] = {mode, 0x04, 0x02, 0x00, 0x00, 0, 34, 255, 0x00, 0x01};
        int res = EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res < 0) {
            success = false;
            break;
        }
        Sleep(200);
    }

    // Restore to Static
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t edge[10] = {0x04, 0x04, 0x00, 0x00, 0x00, 0, 34, 255, 0x00, 0x01};
    EVisionQuery(dev, 0x06, 0x1b, edge, 10, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    return success;
}

//=============================================================================
// PROFILE TESTS
//=============================================================================

std::wstring GetTestProfilePath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    return std::wstring(path) + L"\\OneClickRGB\\profiles";
}

bool Test_Profile_Directory_Create() {
    std::wstring dir = GetTestProfilePath();
    CreateDirectoryW((GetTestProfilePath().substr(0, GetTestProfilePath().length() - 9)).c_str(), NULL);
    BOOL result = CreateDirectoryW(dir.c_str(), NULL);
    return result || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool Test_Profile_Save() {
    std::wstring path = GetTestProfilePath() + L"\\test_profile.rgb";
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "red=0\n";
    file << "green=34\n";
    file << "blue=255\n";
    file << "brightness=4\n";
    file << "speed=2\n";
    file << "kbMode=6\n";
    file << "edgeMode=4\n";
    file.close();

    // Verify file exists
    std::ifstream check(path);
    return check.good();
}

bool Test_Profile_Load() {
    std::wstring path = GetTestProfilePath() + L"\\test_profile.rgb";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::map<std::string, int> values;
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            int val = std::stoi(line.substr(pos + 1));
            values[key] = val;
        }
    }
    file.close();

    return values["red"] == 0 && values["green"] == 34 && values["blue"] == 255;
}

bool Test_Profile_Delete() {
    std::wstring path = GetTestProfilePath() + L"\\test_profile.rgb";
    return DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
}

//=============================================================================
// COLOR TESTS
//=============================================================================

bool Test_Color_Sequence_RGB() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    // Test Red, Green, Blue sequence
    uint8_t colors[][3] = {{255,0,0}, {0,255,0}, {0,0,255}};
    bool success = true;

    for (auto& color : colors) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t config[18] = {0x06, 0x04, 0, 0, 0, color[0], color[1], color[2]};
        int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res < 0) {
            success = false;
            break;
        }
        Sleep(300);
    }

    // Restore to blue
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t config[18] = {0x06, 0x04, 0, 0, 0, 0, 34, 255};
    EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    return success;
}

bool Test_Color_Brightness_Levels() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    bool success = true;

    for (uint8_t brightness = 0; brightness <= 4; brightness++) {
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(20);

        uint8_t config[18] = {0x06, brightness, 0, 0, 0, 0, 34, 255};
        int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

        uint8_t unlock[2] = {0x00, 0x00};
        EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res < 0) {
            success = false;
            break;
        }
        Sleep(200);
    }

    // Restore to max brightness
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t config[18] = {0x06, 0x04, 0, 0, 0, 0, 34, 255};
    EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    return success;
}

//=============================================================================
// STRESS TESTS
//=============================================================================

bool Test_Stress_Rapid_Color_Changes() {
    if (!g_devices.evision_found) return false;

    hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
    if (!dev) return false;

    bool success = true;

    for (int i = 0; i < 50; i++) {
        uint8_t r = (i * 5) % 256;
        uint8_t g = (i * 7) % 256;
        uint8_t b = (i * 11) % 256;

        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        Sleep(10);

        uint8_t config[18] = {0x06, 0x04, 0, 0, 0, r, g, b};
        int res = EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);

        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        if (res < 0) {
            success = false;
            break;
        }
        Sleep(20);
    }

    // Restore to blue
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t config[18] = {0x06, 0x04, 0, 0, 0, 0, 34, 255};
    EVisionQuery(dev, 0x06, 0x01, config, 18, nullptr);
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);
    return success;
}

bool Test_Stress_Open_Close_Cycles() {
    if (!g_devices.evision_found) return false;

    bool success = true;

    for (int i = 0; i < 20; i++) {
        hid_device* dev = hid_open_path(g_devices.evision_path.c_str());
        if (!dev) {
            success = false;
            break;
        }

        // Quick query
        EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
        EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

        hid_close(dev);
        Sleep(10);
    }

    return success;
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "OneClickRGB Test Suite v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Parse arguments
    bool runAll = true;
    bool runDevices = false;
    bool runColors = false;
    bool runModes = false;
    bool runProfiles = false;
    bool runStress = false;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--all") { runAll = true; }
        else if (arg == "--devices") { runDevices = true; runAll = false; }
        else if (arg == "--colors") { runColors = true; runAll = false; }
        else if (arg == "--modes") { runModes = true; runAll = false; }
        else if (arg == "--profiles") { runProfiles = true; runAll = false; }
        else if (arg == "--stress") { runStress = true; runAll = false; }
        else if (arg == "--verbose") { verbose = true; }
    }

    TestSuite suite;
    suite.verbose = verbose;

    // Initialize HID
    std::cout << "=== Initialization ===" << std::endl;
    suite.Run("HID API Init", Test_HidApi_Init);
    suite.Run("HID Enumerate", Test_HidApi_Enumerate);

    // Device Detection
    if (runAll || runDevices) {
        std::cout << "\n=== Device Detection ===" << std::endl;
        suite.Run("ASUS Aura Detection", Test_Asus_Aura_Detection);
        suite.Run("SteelSeries Detection", Test_SteelSeries_Detection);
        suite.Run("EVision Keyboard Detection", Test_EVision_Detection);
        suite.Run("G.Skill PawnIO Available", Test_GSkill_PawnIO_Available);
        suite.Run("G.Skill SMBus Module", Test_GSkill_SMBus_Module);
    }

    // ASUS Aura Tests
    if (runAll || runDevices || runColors) {
        std::cout << "\n=== ASUS Aura Tests ===" << std::endl;
        if (g_devices.aura_found) {
            suite.Run("ASUS Open/Close", Test_Asus_Open_Close);
            suite.Run("ASUS Set Color", Test_Asus_Set_Color);
            suite.Run("ASUS All Channels", Test_Asus_All_Channels);
        } else {
            suite.Skip("ASUS Tests", "Device not found");
        }
    }

    // SteelSeries Tests
    if (runAll || runDevices || runColors) {
        std::cout << "\n=== SteelSeries Tests ===" << std::endl;
        if (g_devices.steelseries_found) {
            suite.Run("SteelSeries Open/Close", Test_SteelSeries_Open_Close);
            suite.Run("SteelSeries Set Color", Test_SteelSeries_Set_Color);
            suite.Run("SteelSeries All Zones", Test_SteelSeries_All_Zones);
        } else {
            suite.Skip("SteelSeries Tests", "Device not found");
        }
    }

    // EVision Keyboard Tests
    if (runAll || runDevices || runColors || runModes) {
        std::cout << "\n=== EVision Keyboard Tests ===" << std::endl;
        if (g_devices.evision_found) {
            suite.Run("EVision Open/Close", Test_EVision_Open_Close);
            suite.Run("EVision Begin/End Configure", Test_EVision_Begin_End_Configure);
            suite.Run("EVision Read Profile", Test_EVision_Read_Profile);
            suite.Run("EVision Read Config", Test_EVision_Read_Config);
            suite.Run("EVision Write Color", Test_EVision_Write_Color);

            if (runAll || runModes) {
                suite.Run("EVision All Keyboard Modes", Test_EVision_All_Modes);
                suite.Run("EVision Edge Config Read", Test_EVision_Edge_Config);
                suite.Run("EVision Edge Write", Test_EVision_Edge_Write);
                suite.Run("EVision Edge All Modes", Test_EVision_Edge_All_Modes);
            }
        } else {
            suite.Skip("EVision Tests", "Device not found");
        }
    }

    // Profile Tests
    if (runAll || runProfiles) {
        std::cout << "\n=== Profile Tests ===" << std::endl;
        suite.Run("Profile Directory Create", Test_Profile_Directory_Create);
        suite.Run("Profile Save", Test_Profile_Save);
        suite.Run("Profile Load", Test_Profile_Load);
        suite.Run("Profile Delete", Test_Profile_Delete);
    }

    // Color Tests
    if (runAll || runColors) {
        std::cout << "\n=== Color Tests ===" << std::endl;
        if (g_devices.evision_found) {
            suite.Run("Color Sequence RGB", Test_Color_Sequence_RGB);
            suite.Run("Color Brightness Levels", Test_Color_Brightness_Levels);
        } else {
            suite.Skip("Color Tests", "EVision device not found");
        }
    }

    // Stress Tests
    if (runAll || runStress) {
        std::cout << "\n=== Stress Tests ===" << std::endl;
        if (g_devices.evision_found) {
            suite.Run("Stress: Rapid Color Changes (50x)", Test_Stress_Rapid_Color_Changes);
            suite.Run("Stress: Open/Close Cycles (20x)", Test_Stress_Open_Close_Cycles);
        } else {
            suite.Skip("Stress Tests", "EVision device not found");
        }
    }

    // Cleanup
    hid_exit();

    // Print summary
    suite.PrintSummary();

    return suite.failed > 0 ? 1 : 0;
}
