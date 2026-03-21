/**
 * OneClickRGB - Set all RGB devices to one color
 *
 * Usage: oneclick_rgb <r> <g> <b>
 * Example: oneclick_rgb 0 34 255  (Blue #0022FF)
 *
 * Supported devices:
 * - ASUS Aura (Mainboard, Fans, LED strips)
 * - SteelSeries Rival 600
 * - EVision Keyboard (GK650)
 * - G.Skill RAM (via PawnIO SMBus)
 * - PowerColor Red Devil GPU (via AMD ADL)
 */

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include "hidapi.h"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

// Device VID/PIDs
namespace Devices {
    // ASUS Aura
    constexpr uint16_t ASUS_VID = 0x0B05;
    constexpr uint16_t ASUS_AURA_PID = 0x19AF;
    constexpr uint16_t ASUS_USAGE_PAGE = 0xFF72;

    // SteelSeries Rival 600
    constexpr uint16_t STEELSERIES_VID = 0x1038;
    constexpr uint16_t RIVAL_600_PID = 0x1724;

    // EVision Keyboard
    constexpr uint16_t EVISION_VID = 0x3299;
    constexpr uint16_t EVISION_PID = 0x4E9F;
    constexpr uint16_t EVISION_USAGE_PAGE = 0xFF1C;
}

// Global color
uint8_t g_red = 0, g_green = 34, g_blue = 255;

//=============================================================================
// ASUS AURA
//=============================================================================

void SetAsusAura() {
    std::cout << "\n[ASUS Aura] Scanning..." << std::endl;

    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::ASUS_VID, Devices::ASUS_AURA_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::ASUS_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "[ASUS Aura] Not found" << std::endl;
        return;
    }

    std::cout << "[ASUS Aura] Found! Setting all 8 channels..." << std::endl;

    // Set all 8 channels with 60 LEDs each
    // Max LEDs per packet: (64 - 5) / 3 = 19 LEDs
    constexpr int MAX_LEDS_PER_PACKET = 19;

    for (int channel = 0; channel < 8; channel++) {
        int led_count = 60;
        int offset = 0;

        while (offset < led_count) {
            int send_count = (led_count - offset > MAX_LEDS_PER_PACKET) ? MAX_LEDS_PER_PACKET : (led_count - offset);
            bool is_last = (offset + send_count >= led_count);

            uint8_t buf[64] = {0};
            buf[0] = 0xEC;
            buf[1] = 0x40;
            buf[2] = channel | (is_last ? 0x80 : 0x00);
            buf[3] = offset;
            buf[4] = send_count;

            for (int i = 0; i < send_count; i++) {
                buf[5 + i*3 + 0] = g_red;
                buf[5 + i*3 + 1] = g_green;
                buf[5 + i*3 + 2] = g_blue;
            }

            hid_write(dev, buf, 64);
            Sleep(2);
            offset += send_count;
        }
    }

    hid_close(dev);
    std::cout << "[ASUS Aura] Done!" << std::endl;
}

//=============================================================================
// STEELSERIES RIVAL 600
//=============================================================================

void SetSteelSeriesRival600() {
    std::cout << "\n[SteelSeries] Scanning..." << std::endl;

    // Find device on interface 0 (like OpenRGB does)
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->interface_number == 0) {
            std::cout << "[SteelSeries] Found on interface 0: " << cur->path << std::endl;
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        std::cout << "[SteelSeries] Rival 600 not found" << std::endl;
        return;
    }

    std::cout << "[SteelSeries] Rival 600 found! Setting colors..." << std::endl;

    // Rival 600 LED zones (from OpenRGB):
    // Logo: 0x01, Wheel: 0x00, Left: 0x02/0x04/0x06, Right: 0x03/0x05/0x07
    // SetRival600Color uses (1 << zone_id) as bitmask
    uint8_t led_zones[] = {0, 1, 2, 3, 4, 5, 6, 7};

    for (int i = 0; i < 8; i++) {
        uint8_t pkt[8] = {0};  // Extra byte for potential Report ID
        pkt[0] = 0x1C;         // Command
        pkt[1] = 0x27;
        pkt[2] = 0x00;
        pkt[3] = 1 << led_zones[i];  // Bitmask for zone
        pkt[4] = g_red;
        pkt[5] = g_green;
        pkt[6] = g_blue;

        int res = hid_write(dev, pkt, 7);
        if (res < 0) {
            std::wcout << L"[SteelSeries] Write failed: " << hid_error(dev) << std::endl;
        }
        Sleep(10);
    }

    // Save to device (optional, makes it persistent)
    uint8_t save[10] = {0};
    save[0] = 0x09;
    hid_write(dev, save, 9);

    hid_close(dev);
    std::cout << "[SteelSeries] Done!" << std::endl;
}

//=============================================================================
// EVISION KEYBOARD V2 (OpenRGB Protocol)
//=============================================================================

// EVision V2 constants from OpenRGB
constexpr uint8_t EVISION_V2_REPORT_ID = 4;
constexpr uint8_t EVISION_V2_PACKET_SIZE = 64;

// Commands
constexpr uint8_t EVISION_V2_CMD_BEGIN_CONFIGURE = 0x01;
constexpr uint8_t EVISION_V2_CMD_END_CONFIGURE = 0x02;
constexpr uint8_t EVISION_V2_CMD_READ_CONFIG = 0x05;
constexpr uint8_t EVISION_V2_CMD_WRITE_CONFIG = 0x06;
constexpr uint8_t EVISION_V2_CMD_SEND_DYNAMIC_COLORS = 0x12;  // Direct mode - no profile change!
constexpr uint8_t EVISION_V2_CMD_END_DYNAMIC_COLORS = 0x13;

// GK650 has approximately 104 keys + extras
constexpr int EVISION_LED_COUNT = 126;  // Total LED buffer size

// EVision V2 Query function (matches OpenRGB implementation)
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

    // Calculate checksum (sum of bytes 3 to end)
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) {
        chksum += buffer[i];
    }
    buffer[1] = chksum & 0xff;
    buffer[2] = (chksum >> 8) & 0xff;

    // Send
    int res = hid_write(dev, buffer, sizeof(buffer));
    if (res < 0) {
        return -1;
    }

    // Read response
    int bytes_read;
    int retries = 10;
    do {
        bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100);
        retries--;
    } while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);

    if (bytes_read != sizeof(buffer) || buffer[0] != EVISION_V2_REPORT_ID) {
        return -2;
    }

    // Check for error
    if (buffer[7] != 0) {
        return -buffer[7];
    }

    // Copy output data if requested
    if (odata && buffer[4] > 0) {
        memcpy(odata, buffer + 8, buffer[4]);
    }

    return buffer[4];  // Return size
}

// Read multiple bytes from EVision config
int EVisionRead(hid_device* dev, uint16_t offset, uint8_t* data, uint16_t size) {
    uint8_t buffer[56];  // Max data per packet
    uint16_t bytes_read = 0;

    while (bytes_read < size) {
        uint8_t chunk_size = (size - bytes_read > 56) ? 56 : (size - bytes_read);
        int res = EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, offset + bytes_read, nullptr, chunk_size, buffer);
        if (res < 0) return res;
        memcpy(data + bytes_read, buffer, chunk_size);
        bytes_read += chunk_size;
    }
    return bytes_read;
}

// Write multiple bytes to EVision config
int EVisionWrite(hid_device* dev, uint16_t offset, const uint8_t* data, uint16_t size) {
    uint16_t bytes_written = 0;

    while (bytes_written < size) {
        uint8_t chunk_size = (size - bytes_written > 56) ? 56 : (size - bytes_written);
        int res = EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, offset + bytes_written,
                               data + bytes_written, chunk_size, nullptr);
        if (res < 0) return res;
        bytes_written += chunk_size;
        Sleep(5);
    }
    return bytes_written;
}

// Endorfy Edge Mode values (different from keyboard!)
constexpr uint8_t ENDORFY_MODE2_STATIC = 0x04;
constexpr uint8_t EVISION_V2_EDGE_OFFSET = 0x1a;  // Edge uses LOGO offset

void SetEVisionKeyboard() {
    std::cout << "\n[EVision] Scanning..." << std::endl;

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
        std::cout << "[EVision] Keyboard not found" << std::endl;
        return;
    }

    std::cout << "[EVision] Keyboard found! Setting all zones..." << std::endl;

    int res;

    // Begin configure
    res = EVisionQuery(dev, EVISION_V2_CMD_BEGIN_CONFIGURE, 0, nullptr, 0, nullptr);
    if (res < 0) {
        std::cout << "[EVision] Begin configure failed" << std::endl;
    }
    Sleep(20);

    // Read current profile number
    uint8_t current_profile = 0;
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, 0x00, nullptr, 1, &current_profile);
    if (current_profile > 2) current_profile = 0;

    uint16_t profile_offset = current_profile * 0x40 + 0x01;

    //=== 1. SET KEYBOARD KEYS ===
    uint8_t config[18];
    memset(config, 0, sizeof(config));
    res = EVisionRead(dev, profile_offset, config, 18);

    // Set keyboard to static mode with target color
    config[0] = 0x06;  // Static mode (EVision)
    if (config[1] == 0) config[1] = 0x04;  // Brightness max
    config[4] = 0x00;  // Random color off
    config[5] = g_red;
    config[6] = g_green;
    config[7] = g_blue;

    res = EVisionWrite(dev, profile_offset, config, 18);
    if (res < 0) {
        std::cout << "[EVision] Keyboard write failed" << std::endl;
    } else {
        std::cout << "[EVision] Keyboard: RGB(" << (int)g_red << "," << (int)g_green << "," << (int)g_blue << ")" << std::endl;
    }
    Sleep(10);

    //=== 2. SET EDGE LEDs (side lighting) ===
    uint16_t edge_offset = profile_offset + EVISION_V2_EDGE_OFFSET;
    std::cout << "[EVision] Edge offset: 0x" << std::hex << edge_offset << std::dec << std::endl;
    uint8_t edge_config[10];
    memset(edge_config, 0, sizeof(edge_config));
    EVisionQuery(dev, EVISION_V2_CMD_READ_CONFIG, edge_offset, nullptr, 10, edge_config);
    std::cout << "[EVision] Current Edge: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_config[i] << " ";
    std::cout << std::dec << std::endl;

    // Set edge to static mode with target color (Endorfy protocol)
    // WICHTIG: Speed und Direction MÜSSEN 0 sein für Static Mode!
    edge_config[0] = ENDORFY_MODE2_STATIC;  // Static mode = 0x04 (Endorfy)
    edge_config[1] = 0x04;  // Brightness max
    edge_config[2] = 0x00;  // Speed = 0 (WICHTIG!)
    edge_config[3] = 0x00;  // Direction = 0
    edge_config[4] = 0x00;  // Random off
    edge_config[5] = g_red;
    edge_config[6] = g_green;
    edge_config[7] = g_blue;
    edge_config[8] = 0x00;  // Unknown
    edge_config[9] = 0x01;  // On

    std::cout << "[EVision] Writing Edge: ";
    for (int i = 0; i < 10; i++) std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)edge_config[i] << " ";
    std::cout << std::dec << std::endl;

    res = EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, edge_offset, edge_config, 10, nullptr);
    if (res < 0) {
        std::cout << "[EVision] Edge write failed: " << res << std::endl;
    } else {
        std::cout << "[EVision] Edge LEDs: RGB(" << (int)g_red << "," << (int)g_green << "," << (int)g_blue << ")" << std::endl;
    }
    Sleep(10);

    //=== 3. UNLOCK WINDOWS KEY ===
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, EVISION_V2_CMD_WRITE_CONFIG, 0x14, unlock, 2, nullptr);
    std::cout << "[EVision] Windows key unlocked" << std::endl;

    // End configure (applies changes)
    res = EVisionQuery(dev, EVISION_V2_CMD_END_CONFIGURE, 0, nullptr, 0, nullptr);
    if (res < 0) {
        std::cout << "[EVision] End configure failed" << std::endl;
    } else {
        std::cout << "[EVision] Profile updated to RGB(" << (int)g_red << "," << (int)g_green << "," << (int)g_blue << ")" << std::endl;
    }

    hid_close(dev);
    std::cout << "[EVision] Done!" << std::endl;
}

//=============================================================================
// G.SKILL RAM (via PawnIO SMBus - Direct Hardware Access)
//=============================================================================

#include <fstream>
#include <filesystem>

// PawnIO function pointers (dynamically loaded)
typedef HRESULT (__stdcall *pawnio_version_t)(PULONG version);
typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE handle);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE handle, const UCHAR* blob, SIZE_T size);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE handle, PCSTR name, const ULONG64* in, SIZE_T in_size, PULONG64 out, SIZE_T out_size, PSIZE_T return_size);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE handle);

pawnio_version_t p_pawnio_version = nullptr;
pawnio_open_t p_pawnio_open = nullptr;
pawnio_load_t p_pawnio_load = nullptr;
pawnio_execute_t p_pawnio_execute = nullptr;
pawnio_close_t p_pawnio_close = nullptr;

HANDLE g_pawnio_handle = nullptr;
HMODULE g_pawnio_dll = nullptr;

// SMBus constants
#define I2C_SMBUS_READ  1
#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_BYTE      1
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA 3

//=== DDR5 SPD5 Hub Protocol (for Trident Z5) ===
// SPD5 Hub addresses: 0x50-0x57 (one per DIMM slot)
constexpr uint8_t SPD5_HUB_PAGE_REG = 0x0B;
constexpr uint8_t SPD5_PAGE_SPD     = 0x00;  // SPD EEPROM
constexpr uint8_t SPD5_PAGE_VENDOR  = 0x04;  // G.Skill RGB page

// G.Skill DDR5 RGB registers (on vendor page)
constexpr uint8_t GSKILL_REG_MODE       = 0x20;
constexpr uint8_t GSKILL_REG_SPEED      = 0x21;
constexpr uint8_t GSKILL_REG_DIRECTION  = 0x22;
constexpr uint8_t GSKILL_REG_BRIGHTNESS = 0x23;
constexpr uint8_t GSKILL_REG_COLOR_START= 0x30;  // 10 LEDs x 3 bytes = 30 bytes
constexpr uint8_t GSKILL_REG_APPLY      = 0x60;
constexpr uint8_t GSKILL_REG_SAVE       = 0x61;

// G.Skill modes
constexpr uint8_t GSKILL_MODE_OFF    = 0x00;
constexpr uint8_t GSKILL_MODE_STATIC = 0x01;
constexpr uint8_t GSKILL_LED_COUNT   = 10;

// DDR5 SPD5 Hub addresses to scan (DIMM slots 0-7)
static const uint8_t ddr5_spd_addresses[] = {
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57
};

// DDR4 ENE DRAM addresses
static const uint8_t ddr4_ene_addresses[] = {
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F
};

union i2c_smbus_data {
    uint8_t  byte;
    uint16_t word;
    uint8_t  block[34];
};

// Check if PawnIO driver is installed and loaded
bool IsPawnIODriverInstalled() {
    // Try to open the device directly
    HANDLE hDevice = CreateFileA("\\\\.\\PawnIO", GENERIC_READ | GENERIC_WRITE,
                                  0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        return true;
    }

    // Try to start the service in case it exists but isn't running
    std::cout << "[G.Skill RAM] Trying to start PawnIO service..." << std::endl;
    system("sc start PawnIO >nul 2>&1");
    Sleep(1000);

    // Check again
    hDevice = CreateFileA("\\\\.\\PawnIO", GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        std::cout << "[G.Skill RAM] PawnIO service started!" << std::endl;
        return true;
    }

    return false;
}

// Install PawnIO driver silently
bool InstallPawnIODriver() {
    std::cout << "[G.Skill RAM] PawnIO driver not detected. Installing automatically..." << std::endl;

    // Check for installer in same directory as executable
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = std::string(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of("\\/"));

    std::string installer_paths[] = {
        exeDir + "\\PawnIO_setup.exe",
        exeDir + "\\dependencies\\PawnIO\\PawnIO_setup.exe",
        "PawnIO_setup.exe"
    };

    std::string installer_path;
    for (const auto& path : installer_paths) {
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            installer_path = path;
            break;
        }
    }

    if (installer_path.empty()) {
        // Download the installer
        std::cout << "[G.Skill RAM] Downloading PawnIO driver..." << std::endl;

        std::string download_path = exeDir + "\\PawnIO_setup.exe";
        std::string download_cmd =
            "powershell -ExecutionPolicy Bypass -Command \""
            "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
            "Invoke-WebRequest -Uri 'https://github.com/namazso/PawnIO.Setup/releases/latest/download/PawnIO_setup.exe' "
            "-OutFile '" + download_path + "'\" 2>nul";

        int result = system(download_cmd.c_str());
        if (result != 0 || GetFileAttributesA(download_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            std::cout << "[G.Skill RAM] Download failed. Please install PawnIO manually from https://pawnio.eu/" << std::endl;
            return false;
        }
        installer_path = download_path;
    }

    std::cout << "[G.Skill RAM] Installing PawnIO driver (requires Administrator)..." << std::endl;

    // Run installer silently with admin privileges
    // The PawnIO installer should support silent mode
    SHELLEXECUTEINFOA sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = "runas";
    sei.lpFile = installer_path.c_str();
    sei.lpParameters = NULL;  // Will try interactive mode
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            std::cout << "[G.Skill RAM] Installation cancelled by user" << std::endl;
        } else {
            std::cout << "[G.Skill RAM] Failed to start installer (error " << err << ")" << std::endl;
        }
        return false;
    }

    std::cout << "[G.Skill RAM] Please click 'Install' in the PawnIO window..." << std::endl;

    // Wait for installer to finish (max 2 minutes)
    DWORD waitResult = WaitForSingleObject(sei.hProcess, 120000);
    CloseHandle(sei.hProcess);

    if (waitResult == WAIT_TIMEOUT) {
        std::cout << "[G.Skill RAM] Installation timed out" << std::endl;
        return false;
    }

    // Give the driver a moment to load
    Sleep(2000);

    // Verify installation
    HANDLE hDevice = CreateFileA("\\\\.\\PawnIO", GENERIC_READ | GENERIC_WRITE,
                                  0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        std::cout << "[G.Skill RAM] PawnIO driver installed and ready!" << std::endl;
        return true;
    }

    // Try starting the service
    system("sc start PawnIO >nul 2>&1");
    Sleep(1000);

    hDevice = CreateFileA("\\\\.\\PawnIO", GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        std::cout << "[G.Skill RAM] PawnIO driver started!" << std::endl;
        return true;
    }

    std::cout << "[G.Skill RAM] Driver installed but may need a reboot to activate" << std::endl;
    return false;
}

// Helper to get executable directory
static std::string GetExeDirectory() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string dir = std::string(exePath);
    return dir.substr(0, dir.find_last_of("\\/"));
}

bool LoadPawnIO() {
    // Try to find PawnIOLib.dll relative to executable
    std::string exeDir = GetExeDirectory();
    std::string dll_paths[] = {
        exeDir + "\\PawnIOLib.dll",
        exeDir + "\\dependencies\\PawnIO\\PawnIOLib.dll",
        "PawnIOLib.dll"
    };

    for (const auto& path : dll_paths) {
        g_pawnio_dll = LoadLibraryA(path.c_str());
        if (g_pawnio_dll) {
            std::cout << "[G.Skill RAM] Loaded PawnIO from: " << path << std::endl;
            break;
        }
    }

    if (!g_pawnio_dll) {
        return false;
    }

    p_pawnio_version = (pawnio_version_t)GetProcAddress(g_pawnio_dll, "pawnio_version");
    p_pawnio_open = (pawnio_open_t)GetProcAddress(g_pawnio_dll, "pawnio_open");
    p_pawnio_load = (pawnio_load_t)GetProcAddress(g_pawnio_dll, "pawnio_load");
    p_pawnio_execute = (pawnio_execute_t)GetProcAddress(g_pawnio_dll, "pawnio_execute");
    p_pawnio_close = (pawnio_close_t)GetProcAddress(g_pawnio_dll, "pawnio_close");

    if (!p_pawnio_version || !p_pawnio_open || !p_pawnio_load || !p_pawnio_execute || !p_pawnio_close) {
        FreeLibrary(g_pawnio_dll);
        g_pawnio_dll = nullptr;
        return false;
    }

    ULONG version;
    if (p_pawnio_version(&version) != S_OK) {
        return false;
    }
    std::cout << "[G.Skill RAM] PawnIO version: " << version << std::endl;

    return true;
}

bool LoadSMBusModule() {
    // Find SMBus module relative to executable
    std::string exeDir = GetExeDirectory();
    std::string module_paths[] = {
        exeDir + "\\SmbusI801.bin",
        exeDir + "\\dependencies\\PawnIO\\modules\\SmbusI801.bin",
        exeDir + "\\modules\\SmbusI801.bin",
        "SmbusI801.bin"
    };

    std::string module_path;
    for (const auto& path : module_paths) {
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            module_path = path;
            break;
        }
    }

    if (module_path.empty()) {
        std::cout << "[G.Skill RAM] SmbusI801.bin not found" << std::endl;
        return false;
    }

    // Open PawnIO
    HRESULT hr = p_pawnio_open(&g_pawnio_handle);
    if (hr != S_OK) {
        if (hr == E_ACCESSDENIED) {
            std::cout << "[G.Skill RAM] Access denied - need Administrator" << std::endl;
        } else if (hr == 0x80070002) {
            // ERROR_FILE_NOT_FOUND - driver not installed
            std::cout << "[G.Skill RAM] PawnIO driver not found" << std::endl;
            if (InstallPawnIODriver()) {
                // Try again after installation
                hr = p_pawnio_open(&g_pawnio_handle);
                if (hr != S_OK) {
                    std::cout << "[G.Skill RAM] Still can't open PawnIO after install: 0x"
                              << std::hex << hr << std::dec << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        } else {
            std::cout << "[G.Skill RAM] PawnIO open failed: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }
    }

    // Load SMBus module
    std::ifstream file(module_path, std::ios::binary);
    if (!file.is_open()) {
        p_pawnio_close(g_pawnio_handle);
        return false;
    }

    std::vector<char> blob((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    hr = p_pawnio_load(g_pawnio_handle, (const UCHAR*)blob.data(), blob.size());
    if (hr != S_OK) {
        std::cout << "[G.Skill RAM] SMBus module load failed: 0x" << std::hex << hr << std::dec << std::endl;
        p_pawnio_close(g_pawnio_handle);
        return false;
    }

    std::cout << "[G.Skill RAM] SMBus module loaded: " << module_path << std::endl;
    return true;
}

int SMBusXfer(uint8_t addr, char read_write, uint8_t command, int size, i2c_smbus_data* data) {
    const SIZE_T in_size = 9;
    ULONG64 in[in_size] = {0};
    const SIZE_T out_size = 5;
    ULONG64 out[out_size] = {0};
    SIZE_T return_size;

    in[0] = addr;
    in[1] = read_write;
    in[2] = command;
    in[3] = size;

    if (data) {
        memcpy(&in[4], data, sizeof(i2c_smbus_data));
    }

    HRESULT hr = p_pawnio_execute(g_pawnio_handle, "ioctl_smbus_xfer", in, in_size, out, out_size, &return_size);

    if (data) {
        memcpy(data, &out[0], sizeof(i2c_smbus_data));
    }

    return hr == S_OK ? 0 : -1;
}

int SMBusReadByte(uint8_t addr) {
    i2c_smbus_data data;
    if (SMBusXfer(addr, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data) < 0) return -1;
    return data.byte;
}

int SMBusReadByteData(uint8_t addr, uint8_t cmd) {
    i2c_smbus_data data;
    if (SMBusXfer(addr, I2C_SMBUS_READ, cmd, I2C_SMBUS_BYTE_DATA, &data) < 0) return -1;
    return data.byte;
}

int SMBusWriteByteData(uint8_t addr, uint8_t cmd, uint8_t value) {
    i2c_smbus_data data;
    data.byte = value;
    return SMBusXfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_BYTE_DATA, &data);
}

int SMBusWriteWordData(uint8_t addr, uint8_t cmd, uint16_t value) {
    i2c_smbus_data data;
    data.word = value;
    return SMBusXfer(addr, I2C_SMBUS_WRITE, cmd, I2C_SMBUS_WORD_DATA, &data);
}

// Note: ENE register operations moved to SetRAMColorDDR4 section

//=== DDR5 G.Skill Trident Z5 RGB Control ===

// Select RGB vendor page on SPD5 Hub
bool SelectGSkillRGBPage(uint8_t spd_addr) {
    return SMBusWriteByteData(spd_addr, SPD5_HUB_PAGE_REG, SPD5_PAGE_VENDOR) == 0;
}

// Return to SPD page
bool SelectSPDPage(uint8_t spd_addr) {
    return SMBusWriteByteData(spd_addr, SPD5_HUB_PAGE_REG, SPD5_PAGE_SPD) == 0;
}

// Check if this is a G.Skill RGB module
bool IsGSkillDDR5(uint8_t spd_addr) {
    // Switch to vendor page
    if (!SelectGSkillRGBPage(spd_addr)) {
        return false;
    }

    // Try to read a signature or mode register
    int mode = SMBusReadByteData(spd_addr, GSKILL_REG_MODE);

    // Return to SPD page
    SelectSPDPage(spd_addr);

    // If we got a valid read, it's likely a G.Skill module
    return (mode >= 0 && mode <= 0x0B);  // Valid mode range
}

// Set DDR5 RAM color using G.Skill protocol
void SetGSkillDDR5Color(uint8_t spd_addr, uint8_t r, uint8_t g, uint8_t b) {
    std::cout << "[G.Skill RAM] Setting DDR5 at 0x" << std::hex << (int)spd_addr
              << std::dec << " to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;

    // Switch to RGB vendor page
    if (!SelectGSkillRGBPage(spd_addr)) {
        std::cout << "[G.Skill RAM] Failed to select RGB page" << std::endl;
        return;
    }

    // Set mode to static
    SMBusWriteByteData(spd_addr, GSKILL_REG_MODE, GSKILL_MODE_STATIC);
    Sleep(5);

    // Set brightness to max
    SMBusWriteByteData(spd_addr, GSKILL_REG_BRIGHTNESS, 0x04);
    Sleep(5);

    // Set color for all 10 LEDs
    for (int i = 0; i < GSKILL_LED_COUNT; i++) {
        uint8_t color_reg = GSKILL_REG_COLOR_START + (i * 3);
        SMBusWriteByteData(spd_addr, color_reg + 0, r);  // Red
        SMBusWriteByteData(spd_addr, color_reg + 1, g);  // Green
        SMBusWriteByteData(spd_addr, color_reg + 2, b);  // Blue
        Sleep(2);
    }

    // Apply changes
    SMBusWriteByteData(spd_addr, GSKILL_REG_APPLY, 0x01);
    Sleep(10);

    // Return to SPD page
    SelectSPDPage(spd_addr);

    std::cout << "[G.Skill RAM] Applied!" << std::endl;
}

//=== DDR4 ENE Protocol (AUDA0-E6K5-0101) ===
// ENE SMBus uses a special protocol:
// 1. Write 16-bit register address to command 0x00 (byte-swapped)
// 2. Read value from command 0x81, or write value to command 0x01

// ENE Register addresses (16-bit)
constexpr uint16_t ENE_REG_DEVICE_NAME      = 0x1000;
constexpr uint16_t ENE_REG_CONFIG_TABLE     = 0x1C00;  // Configuration table (64 bytes)
constexpr uint16_t ENE_REG_COLORS_DIRECT_V2 = 0x8100;  // Direct colors for AUDA0
constexpr uint16_t ENE_REG_DIRECT           = 0x8020;  // Direct mode enable
constexpr uint16_t ENE_REG_APPLY            = 0x80A0;  // Apply changes

// Config table offsets
constexpr uint8_t ENE_CONFIG_LED_COUNT = 0x02;  // LED count in config table

void ENERegisterWrite(uint8_t addr, uint16_t reg, uint8_t val) {
    // Write register address (byte-swapped)
    uint16_t reg_swapped = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    SMBusWriteWordData(addr, 0x00, reg_swapped);
    Sleep(1);

    // Write value
    SMBusWriteByteData(addr, 0x01, val);
    Sleep(1);
}

uint8_t ENERegisterRead(uint8_t addr, uint16_t reg) {
    // Write register address (byte-swapped)
    uint16_t reg_swapped = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
    SMBusWriteWordData(addr, 0x00, reg_swapped);
    Sleep(1);

    // Read value
    int res = SMBusReadByteData(addr, 0x81);
    return (res >= 0) ? (uint8_t)res : 0;
}

// Read LED count from ENE config table
int ENEGetLEDCount(uint8_t addr) {
    uint8_t led_count = ENERegisterRead(addr, ENE_REG_CONFIG_TABLE + ENE_CONFIG_LED_COUNT);

    // Sanity check - DRAM modules typically have 5-10 LEDs per side
    if (led_count == 0 || led_count > 20) {
        // Fallback: read from offset 0x03 (some devices store it there)
        led_count = ENERegisterRead(addr, ENE_REG_CONFIG_TABLE + 0x03);
    }

    // If still invalid, default to 10 (common for Trident Z5)
    if (led_count == 0 || led_count > 20) {
        led_count = 10;
    }

    return led_count;
}

void SetRAMColorDDR4(uint8_t addr, uint8_t r, uint8_t g, uint8_t b) {
    // Read actual LED count from device
    int led_count = ENEGetLEDCount(addr);

    std::cout << "[G.Skill RAM] Setting ENE AUDA0 at 0x" << std::hex << (int)addr << std::dec
              << " (" << led_count << " LEDs) to RGB(" << (int)r << "," << (int)g << "," << (int)b << ")..." << std::endl;

    // Enable direct mode (register 0x8020 = 0x01)
    ENERegisterWrite(addr, ENE_REG_DIRECT, 0x01);
    Sleep(5);

    // Write colors to direct register (0x8100+)
    // ENE uses RBG order (Red, Blue, Green)
    for (int i = 0; i < led_count; i++) {
        uint16_t color_reg = ENE_REG_COLORS_DIRECT_V2 + (i * 3);
        ENERegisterWrite(addr, color_reg + 0, r);  // Red
        ENERegisterWrite(addr, color_reg + 1, b);  // Blue (swapped for ENE!)
        ENERegisterWrite(addr, color_reg + 2, g);  // Green
    }
    Sleep(5);

    // Apply changes (register 0x80A0 = 0x01)
    ENERegisterWrite(addr, ENE_REG_APPLY, 0x01);
    Sleep(10);

    std::cout << "[G.Skill RAM] Applied to 0x" << std::hex << (int)addr << std::dec << std::endl;
}

void SetGSkillRAM() {
    std::cout << "\n[G.Skill RAM] Initializing PawnIO SMBus..." << std::endl;

    if (!LoadPawnIO()) {
        std::cout << "[G.Skill RAM] PawnIO DLL not found" << std::endl;
        std::cout << "[G.Skill RAM] Copy PawnIOLib.dll and SmbusI801.bin to program directory" << std::endl;
        return;
    }

    if (!LoadSMBusModule()) {
        std::cout << "[G.Skill RAM] Failed to initialize SMBus" << std::endl;
        if (g_pawnio_dll) FreeLibrary(g_pawnio_dll);
        return;
    }

    bool found_ram = false;

    //=== SCAN DDR5 SPD5 Hub addresses (0x50-0x57) ===
    std::cout << "[G.Skill RAM] Scanning DDR5 SPD5 hubs (0x50-0x57)..." << std::endl;

    for (uint8_t addr : ddr5_spd_addresses) {
        int res = SMBusReadByte(addr);
        if (res >= 0) {
            std::cout << "[G.Skill RAM] Found SPD5 hub at 0x" << std::hex << (int)addr << std::dec
                      << " (DIMM slot " << (addr - 0x50) << ")" << std::endl;

            // Check if this has a G.Skill RGB controller
            if (IsGSkillDDR5(addr)) {
                found_ram = true;
                SetGSkillDDR5Color(addr, g_red, g_green, g_blue);
            } else {
                std::cout << "[G.Skill RAM] Not a G.Skill RGB module" << std::endl;
            }
        }
    }

    //=== SCAN DDR4 ENE addresses (0x70-0x7F) - legacy support ===
    if (!found_ram) {
        std::cout << "[G.Skill RAM] Scanning DDR4 ENE addresses (0x70-0x7F)..." << std::endl;

        for (uint8_t addr : ddr4_ene_addresses) {
            int res = SMBusReadByte(addr);
            if (res >= 0) {
                // Read device name using ENE protocol
                char name[17] = {0};
                for (int i = 0; i < 16; i++) {
                    name[i] = ENERegisterRead(addr, ENE_REG_DEVICE_NAME + i);
                }

                std::cout << "[G.Skill RAM] Found ENE device at 0x" << std::hex << (int)addr << std::dec
                          << ": " << name << std::endl;

                // Skip Micron (incompatible)
                if (strstr(name, "Micron") != nullptr) {
                    continue;
                }

                // Check for G.Skill compatible
                if (strstr(name, "DIMM_LED") != nullptr ||
                    strstr(name, "AUDA") != nullptr ||
                    strstr(name, "Trident") != nullptr) {

                    found_ram = true;
                    std::cout << "[G.Skill RAM] Setting DDR4 to RGB("
                              << (int)g_red << "," << (int)g_green << "," << (int)g_blue << ")" << std::endl;
                    SetRAMColorDDR4(addr, g_red, g_green, g_blue);
                }
            }
        }
    }

    if (!found_ram) {
        std::cout << "[G.Skill RAM] No compatible RAM found on SMBus" << std::endl;
        std::cout << "[G.Skill RAM] Note: PawnIO driver must be installed and running" << std::endl;
    }

    // Cleanup
    p_pawnio_close(g_pawnio_handle);
    FreeLibrary(g_pawnio_dll);
    std::cout << "[G.Skill RAM] Scan complete!" << std::endl;
}

//=============================================================================
// POWERCOLOR RED DEVIL GPU (via AMD ADL I2C)
//=============================================================================

// AMD ADL definitions
#define ADL_OK                            0
#define ADL_ERR                          -1
#define ADL_ERR_NOT_INIT                 -2
#define ADL_ERR_INVALID_PARAM            -3
#define ADL_ERR_INVALID_ADL_IDX          -4
#define ADL_MAX_PATH                    256

#define ADL_DL_I2C_ACTIONREAD             0x00000001
#define ADL_DL_I2C_ACTIONWRITE            0x00000002

// ADL types
typedef void* ADL_CONTEXT_HANDLE;
typedef void* (__stdcall *ADL_MAIN_MALLOC_CALLBACK)(int);

struct AdapterInfo {
    int iSize;
    int iAdapterIndex;
    char strUDID[ADL_MAX_PATH];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[ADL_MAX_PATH];
    char strDisplayName[ADL_MAX_PATH];
    int iPresent;
    int iExist;
    char strDriverPath[ADL_MAX_PATH];
    char strDriverPathExt[ADL_MAX_PATH];
    char strPNPString[ADL_MAX_PATH];
    int iOSDisplayIndex;
};

struct AdapterInfoX2 {
    int iSize;
    int iAdapterIndex;
    char strUDID[ADL_MAX_PATH];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[ADL_MAX_PATH];
    char strDisplayName[ADL_MAX_PATH];
    int iPresent;
    int iExist;
    char strDriverPath[ADL_MAX_PATH];
    char strDriverPathExt[ADL_MAX_PATH];
    char strPNPString[ADL_MAX_PATH];
    int iOSDisplayIndex;
    int iInfoMask;
    int iInfoValue;
};

struct ADLI2C {
    int iSize;
    int iLine;
    int iAddress;
    int iOffset;
    int iAction;
    int iSpeed;
    int iDataSize;
    char *pcData;
};

// ADL function pointers
typedef int (*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int (*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int (*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_PRIMARY_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int (*ADL2_ADAPTER_ADAPTERINFOX4_GET)(ADL_CONTEXT_HANDLE, int, int*, AdapterInfoX2**);
typedef int (*ADL2_DISPLAY_WRITEANDREADI2C)(ADL_CONTEXT_HANDLE, int, ADLI2C*);

ADL2_MAIN_CONTROL_CREATE          g_ADL2_Main_Control_Create = nullptr;
ADL2_MAIN_CONTROL_DESTROY         g_ADL2_Main_Control_Destroy = nullptr;
ADL2_ADAPTER_NUMBEROFADAPTERS_GET g_ADL2_Adapter_NumberOfAdapters_Get = nullptr;
ADL2_ADAPTER_PRIMARY_GET          g_ADL2_Adapter_Primary_Get = nullptr;
ADL2_ADAPTER_ADAPTERINFOX4_GET    g_ADL2_Adapter_AdapterInfoX4_Get = nullptr;
ADL2_DISPLAY_WRITEANDREADI2C      g_ADL2_Display_WriteAndReadI2C = nullptr;

HMODULE g_adl_dll = nullptr;
ADL_CONTEXT_HANDLE g_adl_context = nullptr;

// ADL memory allocation
void* __stdcall ADL_Main_Memory_Alloc(int iSize) {
    return malloc(iSize);
}

// PowerColor Red Devil V2 protocol
constexpr uint8_t RED_DEVIL_I2C_ADDR = 0x22;

// Register definitions (from OpenRGB)
constexpr uint8_t RED_DEVIL_V2_WRITE_REG_MODE = 0x01;
constexpr uint8_t RED_DEVIL_V2_WRITE_REG_RGB1 = 0x30;
constexpr uint8_t RED_DEVIL_V2_WRITE_REG_RGB2 = 0x31;
constexpr uint8_t RED_DEVIL_V2_READ_REG_MAGIC = 0x82;

// Modes
constexpr uint8_t RED_DEVIL_V2_MODE_STATIC = 0x01;
constexpr uint8_t RED_DEVIL_V2_BRIGHTNESS_MAX = 0xFF;
constexpr uint8_t RED_DEVIL_V2_SPEED_DEFAULT = 0x32;

bool LoadADL() {
    g_adl_dll = LoadLibraryA("atiadlxx.dll");
    if (!g_adl_dll) {
        // Try 32-bit version
        g_adl_dll = LoadLibraryA("atiadlxy.dll");
    }

    if (!g_adl_dll) {
        return false;
    }

    g_ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(g_adl_dll, "ADL2_Main_Control_Create");
    g_ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(g_adl_dll, "ADL2_Main_Control_Destroy");
    g_ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(g_adl_dll, "ADL2_Adapter_NumberOfAdapters_Get");
    g_ADL2_Adapter_Primary_Get = (ADL2_ADAPTER_PRIMARY_GET)GetProcAddress(g_adl_dll, "ADL2_Adapter_Primary_Get");
    g_ADL2_Adapter_AdapterInfoX4_Get = (ADL2_ADAPTER_ADAPTERINFOX4_GET)GetProcAddress(g_adl_dll, "ADL2_Adapter_AdapterInfoX4_Get");
    g_ADL2_Display_WriteAndReadI2C = (ADL2_DISPLAY_WRITEANDREADI2C)GetProcAddress(g_adl_dll, "ADL2_Display_WriteAndReadI2C");

    if (!g_ADL2_Main_Control_Create || !g_ADL2_Main_Control_Destroy ||
        !g_ADL2_Adapter_NumberOfAdapters_Get || !g_ADL2_Adapter_Primary_Get ||
        !g_ADL2_Adapter_AdapterInfoX4_Get || !g_ADL2_Display_WriteAndReadI2C) {
        FreeLibrary(g_adl_dll);
        g_adl_dll = nullptr;
        return false;
    }

    return true;
}

int ADL_I2C_Write(int adapter_index, uint8_t addr, uint8_t reg, uint8_t* data, int size) {
    char buf[64];
    buf[0] = reg;
    memcpy(buf + 1, data, size);

    ADLI2C i2c;
    i2c.iSize = sizeof(ADLI2C);
    i2c.iSpeed = 100;
    i2c.iLine = 1;  // AMD GPU I2C line
    i2c.iAddress = addr << 1;
    i2c.iOffset = 0;
    i2c.iAction = ADL_DL_I2C_ACTIONWRITE;
    i2c.iDataSize = size + 1;
    i2c.pcData = buf;

    return g_ADL2_Display_WriteAndReadI2C(g_adl_context, adapter_index, &i2c);
}

int ADL_I2C_Read(int adapter_index, uint8_t addr, uint8_t reg, uint8_t* data, int size) {
    ADLI2C i2c;
    i2c.iSize = sizeof(ADLI2C);
    i2c.iSpeed = 100;
    i2c.iLine = 1;
    i2c.iAddress = addr << 1;
    i2c.iOffset = reg;
    i2c.iAction = ADL_DL_I2C_ACTIONREAD;
    i2c.iDataSize = size;
    i2c.pcData = (char*)data;

    return g_ADL2_Display_WriteAndReadI2C(g_adl_context, adapter_index, &i2c);
}

void SetPowerColorGPU() {
    std::cout << "\n[PowerColor GPU] Scanning..." << std::endl;

    if (!LoadADL()) {
        std::cout << "[PowerColor GPU] AMD ADL not found (no AMD GPU driver?)" << std::endl;
        return;
    }

    // Initialize ADL
    int ret = g_ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &g_adl_context);
    if (ret != ADL_OK) {
        std::cout << "[PowerColor GPU] ADL init failed: " << ret << std::endl;
        FreeLibrary(g_adl_dll);
        return;
    }

    // Get adapter info
    int num_devices = 0;
    AdapterInfoX2* info = nullptr;
    if (g_ADL2_Adapter_AdapterInfoX4_Get(g_adl_context, -1, &num_devices, &info) == ADL_OK && num_devices > 0) {
        std::cout << "[PowerColor GPU] GPU: " << info[0].strAdapterName << std::endl;

        // Extract subsystem IDs from PNP string
        std::string pnp = info[0].strPNPString;
        std::cout << "[PowerColor GPU] PNP: " << pnp << std::endl;
    }

    // Get primary adapter
    int primary_adapter = 0;
    ret = g_ADL2_Adapter_Primary_Get(g_adl_context, &primary_adapter);
    if (ret != ADL_OK) {
        std::cout << "[PowerColor GPU] Failed to get primary adapter" << std::endl;
        g_ADL2_Main_Control_Destroy(g_adl_context);
        FreeLibrary(g_adl_dll);
        return;
    }

    std::cout << "[PowerColor GPU] Adapter index: " << primary_adapter << std::endl;

    // Scan multiple I2C addresses on different lines
    std::cout << "[PowerColor GPU] Scanning GPU I2C bus..." << std::endl;

    bool found_rgb = false;
    int found_addr = 0;
    int found_line = 0;

    // Known RGB controller addresses
    uint8_t rgb_addrs[] = {0x22, 0x29, 0x2A, 0x48, 0x49, 0x4A, 0x50, 0x60, 0x68};

    for (int line = 0; line <= 2 && !found_rgb; line++) {
        for (uint8_t addr : rgb_addrs) {
            uint8_t data[4] = {0};

            ADLI2C i2c;
            i2c.iSize = sizeof(ADLI2C);
            i2c.iSpeed = 100;
            i2c.iLine = line;
            i2c.iAddress = addr << 1;
            i2c.iOffset = 0x00;
            i2c.iAction = ADL_DL_I2C_ACTIONREAD;
            i2c.iDataSize = 1;
            i2c.pcData = (char*)data;

            ret = g_ADL2_Display_WriteAndReadI2C(g_adl_context, primary_adapter, &i2c);

            if (ret == ADL_OK) {
                std::cout << "[PowerColor GPU] Found device at 0x" << std::hex << (int)addr
                          << " (line " << line << ")" << std::dec << std::endl;

                // Try to read more data
                i2c.iOffset = RED_DEVIL_V2_READ_REG_MAGIC;
                i2c.iDataSize = 3;
                ret = g_ADL2_Display_WriteAndReadI2C(g_adl_context, primary_adapter, &i2c);

                if (ret == ADL_OK) {
                    std::cout << "[PowerColor GPU]   Magic bytes: "
                              << std::hex << (int)data[0] << " " << (int)data[1] << " " << (int)data[2]
                              << std::dec << std::endl;

                    // Check for known signatures
                    if ((data[0] == 0x01 && data[1] == 0x05) ||
                        (data[0] == 0x01 && data[1] == 0x32)) {
                        std::cout << "[PowerColor GPU]   -> PowerColor Red Devil RGB detected!" << std::endl;
                        found_rgb = true;
                        found_addr = addr;
                        found_line = line;
                    }
                }
            }
        }
    }

    if (!found_rgb) {
        std::cout << "[PowerColor GPU] No RGB controller found on GPU I2C" << std::endl;
        std::cout << "[PowerColor GPU] Note: Hellhound models may not have I2C RGB control" << std::endl;
        g_ADL2_Main_Control_Destroy(g_adl_context);
        FreeLibrary(g_adl_dll);
        return;
    }

    // Set mode to static using found address
    std::cout << "[PowerColor GPU] Setting RGB via 0x" << std::hex << found_addr << std::dec << "..." << std::endl;

    // Helper lambda to write on found line
    auto write_rgb = [&](uint8_t reg, uint8_t* data, int size) -> int {
        char buf[64];
        buf[0] = reg;
        memcpy(buf + 1, data, size);

        ADLI2C i2c;
        i2c.iSize = sizeof(ADLI2C);
        i2c.iSpeed = 100;
        i2c.iLine = found_line;
        i2c.iAddress = found_addr << 1;
        i2c.iOffset = 0;
        i2c.iAction = ADL_DL_I2C_ACTIONWRITE;
        i2c.iDataSize = size + 1;
        i2c.pcData = buf;

        return g_ADL2_Display_WriteAndReadI2C(g_adl_context, primary_adapter, &i2c);
    };

    uint8_t mode_data[3] = {
        RED_DEVIL_V2_MODE_STATIC,
        RED_DEVIL_V2_BRIGHTNESS_MAX,
        RED_DEVIL_V2_SPEED_DEFAULT
    };
    ret = write_rgb(RED_DEVIL_V2_WRITE_REG_MODE, mode_data, 3);
    Sleep(50);

    if (ret != ADL_OK) {
        std::cout << "[PowerColor GPU] Mode write failed: " << ret << std::endl;
    }

    // Set color
    uint8_t color_data[3] = {g_red, g_green, g_blue};

    ret = write_rgb(RED_DEVIL_V2_WRITE_REG_RGB1, color_data, 3);
    Sleep(50);
    ret = write_rgb(RED_DEVIL_V2_WRITE_REG_RGB2, color_data, 3);
    Sleep(50);

    std::cout << "[PowerColor GPU] Set to RGB(" << (int)g_red << "," << (int)g_green << "," << (int)g_blue << ")" << std::endl;

    // Cleanup
    g_ADL2_Main_Control_Destroy(g_adl_context);
    FreeLibrary(g_adl_dll);

    std::cout << "[PowerColor GPU] Done!" << std::endl;
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "============================================" << std::endl;
    std::cout << "       OneClickRGB - All Devices Tool       " << std::endl;
    std::cout << "============================================" << std::endl;

    // Parse arguments
    if (argc >= 4) {
        g_red = (uint8_t)atoi(argv[1]);
        g_green = (uint8_t)atoi(argv[2]);
        g_blue = (uint8_t)atoi(argv[3]);
    }

    std::cout << "\nTarget color: RGB(" << (int)g_red << ", " << (int)g_green << ", " << (int)g_blue << ")" << std::endl;
    std::cout << "             #" << std::hex << std::setfill('0')
              << std::setw(2) << (int)g_red
              << std::setw(2) << (int)g_green
              << std::setw(2) << (int)g_blue
              << std::dec << std::endl;

    // Initialize HIDAPI
    if (hid_init() != 0) {
        std::cerr << "Failed to initialize HIDAPI" << std::endl;
        return 1;
    }

    // Set all devices
    SetAsusAura();
    SetSteelSeriesRival600();
    SetEVisionKeyboard();
    SetGSkillRAM();
    SetPowerColorGPU();

    hid_exit();

    std::cout << "\n============================================" << std::endl;
    std::cout << "              All done!" << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
