/**
 * OneClickRGB Complete - Full-featured RGB Control Application
 *
 * Features:
 * - All RGB devices (ASUS Aura, SteelSeries, EVision Keyboard, G.Skill RAM)
 * - Keyboard effects (Static, Breathing, Wave, Rainbow, Reactive, etc.)
 * - Edge LED modes (Static, Breathing, Wave, Spectrum, Off)
 * - Brightness and Speed control
 * - Profile save/load
 * - System tray with quick access
 * - Autostart option
 * - Hotkey support
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include "hidapi.h"
#include "channel_config.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

//=============================================================================
// CONSTANTS & LAYOUT
//=============================================================================

#define APP_NAME L"OneClickRGB"
#define APP_VERSION L"3.1"

// Layout constants (responsive)
#define WINDOW_WIDTH 540
#define WINDOW_HEIGHT 490
#define MARGIN 12           // Window margin
#define GROUP_MARGIN 8      // Space between groups
#define GROUP_TITLE_H 18    // Height for group title
#define GROUP_PADDING 10    // Padding inside group (after title)
#define ITEM_SPACING 6      // Vertical spacing between items
#define ITEM_H_SPACING 8    // Horizontal spacing between items

// Max widths for controls (prevents over-stretching)
#define MAX_SLIDER_W 200
#define MAX_BUTTON_W 80
#define MAX_COMBO_W 160
#define LABEL_W 70
#define CHECKBOX_W 85
#define SMALL_BTN_W 50
#define COLOR_BTN_W 44

// Control heights
#define CTRL_H 22
#define SLIDER_H 20
#define STATUS_H 50

// Tray icon
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 3000
#define ID_TRAY_SHOW 3001
#define ID_TRAY_BLUE 3010
#define ID_TRAY_RED 3011
#define ID_TRAY_GREEN 3012
#define ID_TRAY_WHITE 3013
#define ID_TRAY_OFF 3014

// Control IDs
#define ID_BTN_APPLY 1001
#define ID_BTN_PICK_COLOR 1002
#define ID_EDIT_HEX 1003
#define ID_SLIDER_R 1004
#define ID_SLIDER_G 1005
#define ID_SLIDER_B 1006
#define ID_SLIDER_BRIGHTNESS 1007
#define ID_SLIDER_SPEED 1008
#define ID_STATIC_PREVIEW 1009
#define ID_COMBO_KB_MODE 1020
#define ID_COMBO_EDGE_MODE 1021
#define ID_CHECK_AURA 1030
#define ID_CHECK_MOUSE 1031
#define ID_CHECK_KEYBOARD 1032
#define ID_CHECK_RAM 1033
#define ID_CHECK_EDGE 1034
#define ID_CHECK_AUTOSTART 1040
#define ID_CHECK_MINIMIZE_TRAY 1041
#define ID_BTN_SAVE_PROFILE 1050
#define ID_BTN_LOAD_PROFILE 1051
#define ID_COMBO_PROFILES 1052
#define ID_STATIC_STATUS 1060
#define ID_BTN_THEME 1070

// Presets (7 colors)
#define ID_BTN_PRESET_BLUE 1100
#define ID_BTN_PRESET_RED 1101
#define ID_BTN_PRESET_GREEN 1102
#define ID_BTN_PRESET_PURPLE 1103
#define ID_BTN_PRESET_WHITE 1104
#define ID_BTN_PRESET_OFF 1105
#define ID_BTN_PRESET_CYAN 1106
#define ID_BTN_CHANNEL_SETTINGS 1110

//=============================================================================
// THEME SYSTEM
//=============================================================================

struct Theme {
    COLORREF bgWindow;
    COLORREF bgControl;
    COLORREF bgButton;
    COLORREF bgButtonHover;
    COLORREF textPrimary;
    COLORREF textSecondary;
    COLORREF border;
    COLORREF accent;
    bool isDark;
};

Theme g_darkTheme = {
    RGB(30, 30, 30),      // bgWindow
    RGB(45, 45, 45),      // bgControl
    RGB(60, 60, 60),      // bgButton
    RGB(80, 80, 80),      // bgButtonHover
    RGB(255, 255, 255),   // textPrimary
    RGB(180, 180, 180),   // textSecondary
    RGB(70, 70, 70),      // border
    RGB(0, 120, 215),     // accent
    true
};

Theme g_lightTheme = {
    RGB(245, 245, 245),   // bgWindow
    RGB(255, 255, 255),   // bgControl
    RGB(225, 225, 225),   // bgButton
    RGB(200, 200, 200),   // bgButtonHover
    RGB(0, 0, 0),         // textPrimary
    RGB(80, 80, 80),      // textSecondary
    RGB(200, 200, 200),   // border
    RGB(0, 120, 215),     // accent
    false
};

Theme* g_theme = &g_darkTheme;
HBRUSH g_hBgBrush = NULL;
HBRUSH g_hCtrlBrush = NULL;
HBRUSH g_hBtnBrush = NULL;

//=============================================================================
// i18n LOCALIZATION
//=============================================================================

enum Lang { LANG_EN, LANG_DE };
Lang g_lang = LANG_EN;

struct Strings {
    const wchar_t* colorSelection;
    const wchar_t* effects;
    const wchar_t* devices;
    const wchar_t* profilesSettings;
    const wchar_t* red;
    const wchar_t* green;
    const wchar_t* blue;
    const wchar_t* pick;
    const wchar_t* keyboardEffect;
    const wchar_t* edgeEffect;
    const wchar_t* brightness;
    const wchar_t* speed;
    const wchar_t* channelCorrection;
    const wchar_t* profile;
    const wchar_t* save;
    const wchar_t* load;
    const wchar_t* autostart;
    const wchar_t* tray;
    const wchar_t* apply;
    const wchar_t* ready;
    const wchar_t* theme;
};

Strings g_strEN = {
    L"Color", L"Effects", L"Devices", L"Profiles",
    L"Red", L"Green", L"Blue", L"Pick",
    L"Keyboard", L"Edge", L"Brightness", L"Speed",
    L"Channels...", L"Profile", L"Save", L"Load",
    L"Autostart", L"Tray", L"APPLY",
    L"Ready - Select color and click Apply", L"Theme"
};

Strings g_strDE = {
    L"Farbe", L"Effekte", L"Geräte", L"Profile",
    L"Rot", L"Grün", L"Blau", L"Wählen",
    L"Tastatur", L"Rand", L"Helligkeit", L"Tempo",
    L"Kanäle...", L"Profil", L"Speichern", L"Laden",
    L"Autostart", L"Tray", L"ANWENDEN",
    L"Bereit - Farbe wählen und Anwenden klicken", L"Design"
};

Strings* g_str = &g_strEN;

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

// EVision constants
constexpr uint8_t EVISION_V2_REPORT_ID = 4;
constexpr uint8_t EVISION_V2_PACKET_SIZE = 64;

// Keyboard modes
enum KeyboardMode {
    KB_MODE_STATIC = 0x06,
    KB_MODE_BREATHING = 0x05,
    KB_MODE_SPECTRUM = 0x04,
    KB_MODE_WAVE_SHORT = 0x01,
    KB_MODE_WAVE_LONG = 0x02,
    KB_MODE_COLOR_WHEEL = 0x03,
    KB_MODE_REACTIVE = 0x07,
    KB_MODE_RIPPLE = 0x08,
    KB_MODE_STARLIGHT = 0x0A,
    KB_MODE_RAINBOW = 0x0C,
    KB_MODE_HURRICANE = 0x0D
};

// Edge modes (Endorfy)
enum EdgeMode {
    EDGE_MODE_FREEZE = 0x00,
    EDGE_MODE_WAVE = 0x01,
    EDGE_MODE_SPECTRUM = 0x02,
    EDGE_MODE_BREATHING = 0x03,
    EDGE_MODE_STATIC = 0x04,
    EDGE_MODE_OFF = 0x05
};

//=============================================================================
// GLOBAL STATE
//=============================================================================

struct AppState {
    // Window handles
    HWND hWnd = NULL;
    HWND hPreview = NULL;
    HWND hSliderR = NULL, hSliderG = NULL, hSliderB = NULL;
    HWND hSliderBrightness = NULL, hSliderSpeed = NULL;
    HWND hEditHex = NULL;
    HWND hComboKbMode = NULL, hComboEdgeMode = NULL;
    HWND hComboProfiles = NULL;
    HWND hStatus = NULL;
    HWND hCheckAura = NULL, hCheckMouse = NULL, hCheckKeyboard = NULL;
    HWND hCheckRAM = NULL, hCheckEdge = NULL;
    HWND hCheckAutostart = NULL, hCheckMinimizeTray = NULL;

    // Tray
    NOTIFYICONDATAW nid = {};
    bool minimizedToTray = false;

    // Color settings
    uint8_t red = 0, green = 34, blue = 255;
    uint8_t brightness = 4;  // 0-4
    uint8_t speed = 2;       // 0-5
    uint8_t kbMode = KB_MODE_STATIC;
    uint8_t edgeMode = EDGE_MODE_STATIC;

    // Device selection
    bool enableAura = true;
    bool enableMouse = true;
    bool enableKeyboard = true;
    bool enableRAM = true;
    bool enableEdge = true;

    // Settings
    bool autostart = false;
    bool minimizeToTray = true;

    // Status
    std::wstring statusLog;
    std::atomic<bool> applying{false};
    std::mutex statusMutex;

    // Profiles
    std::vector<std::wstring> profiles;
    std::wstring currentProfile;
} g_state;

// Channel correction manager
ChannelManager g_channels;

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

std::wstring GetAppDataPath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    std::wstring dir = std::wstring(path) + L"\\OneClickRGB";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir;
}

void AppendStatus(const wchar_t* text) {
    std::lock_guard<std::mutex> lock(g_state.statusMutex);
    g_state.statusLog += text;
    g_state.statusLog += L"\r\n";
    if (g_state.hStatus) {
        SetWindowTextW(g_state.hStatus, g_state.statusLog.c_str());
        SendMessage(g_state.hStatus, EM_SETSEL, g_state.statusLog.length(), g_state.statusLog.length());
        SendMessage(g_state.hStatus, EM_SCROLLCARET, 0, 0);
    }
}

void ClearStatus() {
    std::lock_guard<std::mutex> lock(g_state.statusMutex);
    g_state.statusLog.clear();
    if (g_state.hStatus) SetWindowTextW(g_state.hStatus, L"");
}

//=============================================================================
// AUTOSTART
//=============================================================================

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type, size = 0;
        bool exists = RegQueryValueExW(hKey, APP_NAME, NULL, &type, NULL, &size) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return exists;
    }
    return false;
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            std::wstring cmd = std::wstring(L"\"") + path + L"\" --minimized";
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (BYTE*)cmd.c_str(), (DWORD)(cmd.length() + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

//=============================================================================
// PROFILE MANAGEMENT
//=============================================================================

void SaveProfile(const std::wstring& name) {
    std::wstring path = GetAppDataPath() + L"\\profiles\\" + name + L".rgb";
    CreateDirectoryW((GetAppDataPath() + L"\\profiles").c_str(), NULL);

    std::ofstream file(path);
    if (file.is_open()) {
        file << "red=" << (int)g_state.red << "\n";
        file << "green=" << (int)g_state.green << "\n";
        file << "blue=" << (int)g_state.blue << "\n";
        file << "brightness=" << (int)g_state.brightness << "\n";
        file << "speed=" << (int)g_state.speed << "\n";
        file << "kbMode=" << (int)g_state.kbMode << "\n";
        file << "edgeMode=" << (int)g_state.edgeMode << "\n";
        file << "enableAura=" << g_state.enableAura << "\n";
        file << "enableMouse=" << g_state.enableMouse << "\n";
        file << "enableKeyboard=" << g_state.enableKeyboard << "\n";
        file << "enableRAM=" << g_state.enableRAM << "\n";
        file << "enableEdge=" << g_state.enableEdge << "\n";
        file.close();
        AppendStatus((L"Profile saved: " + name).c_str());
    }
}

void LoadProfile(const std::wstring& name) {
    std::wstring path = GetAppDataPath() + L"\\profiles\\" + name + L".rgb";
    std::ifstream file(path);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                int val = std::stoi(line.substr(pos + 1));
                if (key == "red") g_state.red = val;
                else if (key == "green") g_state.green = val;
                else if (key == "blue") g_state.blue = val;
                else if (key == "brightness") g_state.brightness = val;
                else if (key == "speed") g_state.speed = val;
                else if (key == "kbMode") g_state.kbMode = val;
                else if (key == "edgeMode") g_state.edgeMode = val;
                else if (key == "enableAura") g_state.enableAura = val;
                else if (key == "enableMouse") g_state.enableMouse = val;
                else if (key == "enableKeyboard") g_state.enableKeyboard = val;
                else if (key == "enableRAM") g_state.enableRAM = val;
                else if (key == "enableEdge") g_state.enableEdge = val;
            }
        }
        file.close();
        g_state.currentProfile = name;
        AppendStatus((L"Profile loaded: " + name).c_str());
    }
}

void RefreshProfileList() {
    g_state.profiles.clear();
    std::wstring searchPath = GetAppDataPath() + L"\\profiles\\*.rgb";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = fd.cFileName;
            name = name.substr(0, name.length() - 4);  // Remove .rgb
            g_state.profiles.push_back(name);
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    if (g_state.hComboProfiles) {
        SendMessage(g_state.hComboProfiles, CB_RESETCONTENT, 0, 0);
        for (const auto& p : g_state.profiles) {
            SendMessageW(g_state.hComboProfiles, CB_ADDSTRING, 0, (LPARAM)p.c_str());
        }
    }
}

//=============================================================================
// DEVICE CONTROL - ASUS AURA
//=============================================================================

bool SetAsusAura(uint8_t r, uint8_t g, uint8_t b) {
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
        AppendStatus(L"[ASUS Aura] Not found");
        return false;
    }

    constexpr int MAX_LEDS_PER_PACKET = 19;
    for (int channel = 0; channel < 8; channel++) {
        // Apply per-channel color correction
        uint8_t cr = r, cg = g, cb = b;
        g_channels.aura_channels[channel].ApplyCorrection(cr, cg, cb);

        if (!g_channels.aura_channels[channel].enabled) {
            cr = cg = cb = 0;  // Channel disabled
        }

        int led_count = 60, offset = 0;
        while (offset < led_count) {
            int send_count = (led_count - offset > MAX_LEDS_PER_PACKET) ? MAX_LEDS_PER_PACKET : (led_count - offset);
            bool is_last = (offset + send_count >= led_count);
            uint8_t buf[64] = {0};
            buf[0] = 0xEC; buf[1] = 0x40;
            buf[2] = channel | (is_last ? 0x80 : 0x00);
            buf[3] = offset; buf[4] = send_count;
            for (int i = 0; i < send_count; i++) {
                buf[5 + i*3] = cr; buf[5 + i*3 + 1] = cg; buf[5 + i*3 + 2] = cb;
            }
            hid_write(dev, buf, 64);
            Sleep(2);
            offset += send_count;
        }
    }
    hid_close(dev);
    AppendStatus(L"[ASUS Aura] 8 channels set (with correction)");
    return true;
}

//=============================================================================
// DEVICE CONTROL - STEELSERIES
//=============================================================================

bool SetSteelSeries(uint8_t r, uint8_t g, uint8_t b) {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->interface_number == 0) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) {
        AppendStatus(L"[SteelSeries] Not found");
        return false;
    }

    // Apply color correction for SteelSeries
    uint8_t cr = r, cg = g, cb = b;
    g_channels.steelseries.ApplyCorrection(cr, cg, cb);

    for (int i = 0; i < 8; i++) {
        uint8_t pkt[8] = {0x1C, 0x27, 0x00, (uint8_t)(1 << i), cr, cg, cb, 0};
        hid_write(dev, pkt, 7);
        Sleep(10);
    }
    uint8_t save[10] = {0x09};
    hid_write(dev, save, 9);
    hid_close(dev);
    AppendStatus(L"[SteelSeries] Rival 600 set");
    return true;
}

//=============================================================================
// DEVICE CONTROL - EVISION KEYBOARD
//=============================================================================

int EVisionQuery(hid_device* dev, uint8_t cmd, uint16_t offset, const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[EVISION_V2_PACKET_SIZE];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = EVISION_V2_REPORT_ID;
    buffer[3] = cmd; buffer[4] = size;
    buffer[5] = offset & 0xff; buffer[6] = (offset >> 8) & 0xff;
    if (idata && size > 0) memcpy(buffer + 8, idata, size);
    uint16_t chksum = 0;
    for (int i = 3; i < EVISION_V2_PACKET_SIZE; i++) chksum += buffer[i];
    buffer[1] = chksum & 0xff; buffer[2] = (chksum >> 8) & 0xff;
    if (hid_write(dev, buffer, sizeof(buffer)) < 0) return -1;
    int bytes_read, retries = 10;
    do { bytes_read = hid_read_timeout(dev, buffer, sizeof(buffer), 100); retries--; }
    while (bytes_read > 0 && buffer[0] != EVISION_V2_REPORT_ID && retries > 0);
    if (bytes_read != sizeof(buffer)) return -2;
    if (buffer[7] != 0) return -buffer[7];
    if (odata && buffer[4] > 0) memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

bool SetEVisionKeyboard(uint8_t r, uint8_t g, uint8_t b, uint8_t mode, uint8_t brightness, uint8_t speed) {
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
        AppendStatus(L"[EVision] Keyboard not found");
        return false;
    }

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);  // Begin configure
    Sleep(20);

    // Apply keyboard color correction
    uint8_t cr = r, cg = g, cb = b;
    g_channels.keyboard.ApplyCorrection(cr, cg, cb);

    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t profile_offset = profile * 0x40 + 0x01;

    // Build keyboard config (18 bytes)
    uint8_t config[18] = {0};
    config[0] = mode;           // Mode
    config[1] = brightness;     // Brightness (0-4)
    config[2] = speed;          // Speed (0-5, inverted)
    config[3] = 0;              // Direction
    config[4] = 0;              // Random color off
    config[5] = cr;             // Red (corrected)
    config[6] = cg;             // Green (corrected)
    config[7] = cb;             // Blue (corrected)
    config[8] = 0;              // Color offset

    EVisionQuery(dev, 0x06, profile_offset, config, 18, nullptr);
    Sleep(10);

    // Unlock Windows key
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);  // End configure
    hid_close(dev);

    wchar_t buf[64];
    swprintf(buf, 64, L"[EVision] Keyboard set (Mode: 0x%02X)", mode);
    AppendStatus(buf);
    return true;
}

bool SetEVisionEdge(uint8_t r, uint8_t g, uint8_t b, uint8_t mode) {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::EVISION_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    // Apply edge color correction
    uint8_t cr = r, cg = g, cb = b;
    g_channels.edge.ApplyCorrection(cr, cg, cb);

    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t edge_offset = profile * 0x40 + 0x01 + 0x1a;

    // Mode toggle trick: Set to different mode first, then target mode
    uint8_t firstMode = (mode == EDGE_MODE_STATIC) ? EDGE_MODE_WAVE : EDGE_MODE_STATIC;
    uint8_t edge1[10] = {firstMode, 0x04, 0x02, 0x00, 0x00, cr, cg, cb, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, edge1, 10, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    Sleep(100);

    // Now set target mode
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t edge2[10] = {mode, 0x04, 0x02, 0x00, 0x00, cr, cg, cb, 0x00, 0x01};
    EVisionQuery(dev, 0x06, edge_offset, edge2, 10, nullptr);

    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, 0x14, unlock, 2, nullptr);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);

    wchar_t buf[64];
    swprintf(buf, 64, L"[EVision] Edge set (Mode: 0x%02X)", mode);
    AppendStatus(buf);
    return true;
}

//=============================================================================
// DEVICE CONTROL - G.SKILL RAM
//=============================================================================

typedef HRESULT (__stdcall *pawnio_open_t)(PHANDLE);
typedef HRESULT (__stdcall *pawnio_load_t)(HANDLE, const UCHAR*, SIZE_T);
typedef HRESULT (__stdcall *pawnio_execute_t)(HANDLE, PCSTR, const ULONG64*, SIZE_T, PULONG64, SIZE_T, PSIZE_T);
typedef HRESULT (__stdcall *pawnio_close_t)(HANDLE);

union i2c_smbus_data { uint8_t byte; uint16_t word; uint8_t block[34]; };

bool SetGSkillRAM(uint8_t r, uint8_t g, uint8_t b) {
    HMODULE dll = LoadLibraryA("PawnIOLib.dll");
    if (!dll) {
        AppendStatus(L"[G.Skill] PawnIOLib.dll not found");
        return false;
    }

    auto p_open = (pawnio_open_t)GetProcAddress(dll, "pawnio_open");
    auto p_load = (pawnio_load_t)GetProcAddress(dll, "pawnio_load");
    auto p_exec = (pawnio_execute_t)GetProcAddress(dll, "pawnio_execute");
    auto p_close = (pawnio_close_t)GetProcAddress(dll, "pawnio_close");

    if (!p_open || !p_load || !p_exec || !p_close) {
        FreeLibrary(dll);
        return false;
    }

    HANDLE handle;
    if (p_open(&handle) != S_OK) {
        AppendStatus(L"[G.Skill] PawnIO driver not running");
        FreeLibrary(dll);
        return false;
    }

    HANDLE hFile = CreateFileA("SmbusI801.bin", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        p_close(handle);
        FreeLibrary(dll);
        return false;
    }

    DWORD size = GetFileSize(hFile, NULL);
    std::vector<uint8_t> blob(size);
    ReadFile(hFile, blob.data(), size, &size, NULL);
    CloseHandle(hFile);

    if (p_load(handle, blob.data(), blob.size()) != S_OK) {
        p_close(handle);
        FreeLibrary(dll);
        return false;
    }

    auto smbus_xfer = [&](uint8_t addr, char rw, uint8_t cmd, int sz, i2c_smbus_data* data) -> int {
        ULONG64 in[9] = {addr, (ULONG64)rw, cmd, (ULONG64)sz};
        if (data) memcpy(&in[4], data, sizeof(i2c_smbus_data));
        ULONG64 out[5] = {0}; SIZE_T ret_sz;
        HRESULT hr = p_exec(handle, "ioctl_smbus_xfer", in, 9, out, 5, &ret_sz);
        if (data) memcpy(data, &out[0], sizeof(i2c_smbus_data));
        return hr == S_OK ? 0 : -1;
    };

    auto read_byte = [&](uint8_t addr) -> int {
        i2c_smbus_data d; return smbus_xfer(addr, 1, 0, 1, &d) < 0 ? -1 : d.byte;
    };

    auto write_word = [&](uint8_t addr, uint8_t cmd, uint16_t val) {
        i2c_smbus_data d; d.word = val; smbus_xfer(addr, 0, cmd, 3, &d);
    };

    auto write_byte = [&](uint8_t addr, uint8_t cmd, uint8_t val) {
        i2c_smbus_data d; d.byte = val; smbus_xfer(addr, 0, cmd, 2, &d);
    };

    auto ene_write = [&](uint8_t addr, uint16_t reg, uint8_t val) {
        uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
        write_word(addr, 0x00, sw); Sleep(1);
        write_byte(addr, 0x01, val); Sleep(1);
    };

    auto ene_read = [&](uint8_t addr, uint16_t reg) -> uint8_t {
        uint16_t sw = ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF);
        write_word(addr, 0x00, sw); Sleep(1);
        i2c_smbus_data d;
        smbus_xfer(addr, 1, 0x81, 2, &d);
        return d.byte;
    };

    int found = 0;
    int slot = 0;  // Track which RAM slot (0-3)
    uint8_t addrs[] = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

    for (uint8_t addr : addrs) {
        if (read_byte(addr) < 0) continue;

        char name[17] = {0};
        for (int i = 0; i < 16; i++) name[i] = ene_read(addr, 0x1000 + i);

        if (strstr(name, "AUDA") || strstr(name, "DIMM") || strstr(name, "Trident")) {
            // Apply per-slot color correction
            uint8_t cr = r, cg = g, cb = b;
            if (slot < 4) {
                g_channels.ram_modules[slot].ApplyCorrection(cr, cg, cb);
            }

            uint8_t led_count = ene_read(addr, 0x1C02);
            if (led_count == 0 || led_count > 20) led_count = 8;

            ene_write(addr, 0x8020, 0x01); Sleep(5);

            for (int i = 0; i < led_count; i++) {
                uint16_t reg = 0x8100 + (i * 3);
                ene_write(addr, reg + 0, cr);
                ene_write(addr, reg + 1, cb);  // ENE uses RBG
                ene_write(addr, reg + 2, cg);
            }
            Sleep(5);
            ene_write(addr, 0x80A0, 0x01);
            found++;
            slot++;
        }
    }

    p_close(handle);
    FreeLibrary(dll);

    if (found > 0) {
        wchar_t buf[64];
        swprintf(buf, 64, L"[G.Skill] %d module(s) set", found);
        AppendStatus(buf);
        return true;
    }
    return false;
}

//=============================================================================
// APPLY ALL COLORS
//=============================================================================

void ApplyColors() {
    if (g_state.applying.exchange(true)) return;

    ClearStatus();
    AppendStatus(L"=== Applying RGB Settings ===");

    // Reload channel corrections before applying
    g_channels.Load();

    wchar_t buf[64];
    swprintf(buf, 64, L"Color: #%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
    AppendStatus(buf);
    AppendStatus(L"");

    hid_init();

    if (g_state.enableAura) {
        SetAsusAura(g_state.red, g_state.green, g_state.blue);
    }
    if (g_state.enableMouse) {
        SetSteelSeries(g_state.red, g_state.green, g_state.blue);
    }
    if (g_state.enableKeyboard) {
        SetEVisionKeyboard(g_state.red, g_state.green, g_state.blue,
                           g_state.kbMode, g_state.brightness, g_state.speed);
    }
    if (g_state.enableEdge) {
        SetEVisionEdge(g_state.red, g_state.green, g_state.blue, g_state.edgeMode);
    }
    if (g_state.enableRAM) {
        SetGSkillRAM(g_state.red, g_state.green, g_state.blue);
    }

    hid_exit();

    AppendStatus(L"");
    AppendStatus(L"=== Done! ===");

    g_state.applying = false;
}

//=============================================================================
// CHANNEL SETTINGS DIALOG (integrated)
//=============================================================================

#define ID_CS_TAB 5000
#define ID_CS_SAVE 5001
#define ID_CS_RESET 5002
#define ID_CS_SLIDER_BASE 5100
#define ID_CS_CHECK_BASE 5200

struct ChannelDialogData {
    HWND hTab;
    int currentTab;
    struct ControlRow {
        HWND hCheck;
        HWND hSliderR, hSliderG, hSliderB, hSliderBright;
        HWND hLabelR, hLabelG, hLabelB, hLabelBright;
    };
    std::vector<ControlRow> controls;
};

ChannelDialogData* g_csDlg = nullptr;

void UpdateChannelSliderLabel(int index) {
    if (!g_csDlg || index >= (int)g_csDlg->controls.size()) return;

    ChannelConfig* cfg = nullptr;
    if (g_csDlg->currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
    else if (g_csDlg->currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
    else if (g_csDlg->currentTab == 2) {
        if (index == 0) cfg = &g_channels.steelseries;
        else if (index == 1) cfg = &g_channels.keyboard;
        else if (index == 2) cfg = &g_channels.edge;
    }
    if (!cfg) return;

    wchar_t buf[16];
    swprintf(buf, 16, L"%d%%", cfg->red_adjust);
    SetWindowTextW(g_csDlg->controls[index].hLabelR, buf);
    swprintf(buf, 16, L"%d%%", cfg->green_adjust);
    SetWindowTextW(g_csDlg->controls[index].hLabelG, buf);
    swprintf(buf, 16, L"%d%%", cfg->blue_adjust);
    SetWindowTextW(g_csDlg->controls[index].hLabelB, buf);
    swprintf(buf, 16, L"%d%%", cfg->brightness);
    SetWindowTextW(g_csDlg->controls[index].hLabelBright, buf);
}

void CreateChannelRowInDialog(HWND hWnd, int index, int y, const wchar_t* name, ChannelConfig* cfg) {
    ChannelDialogData::ControlRow ctrl = {};

    ctrl.hCheck = CreateWindowW(L"BUTTON", name,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        15, y, 110, 20, hWnd, (HMENU)(INT_PTR)(ID_CS_CHECK_BASE + index), NULL, NULL);
    SendMessage(ctrl.hCheck, BM_SETCHECK, cfg->enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowW(L"STATIC", L"R:", WS_CHILD | WS_VISIBLE, 130, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        145, y, 80, 22, hWnd, (HMENU)(INT_PTR)(ID_CS_SLIDER_BASE + index * 10 + 0), NULL, NULL);
    SendMessage(ctrl.hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderR, TBM_SETPOS, TRUE, cfg->red_adjust);
    ctrl.hLabelR = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 225, y + 2, 35, 18, hWnd, NULL, NULL, NULL);

    CreateWindowW(L"STATIC", L"G:", WS_CHILD | WS_VISIBLE, 265, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        280, y, 80, 22, hWnd, (HMENU)(INT_PTR)(ID_CS_SLIDER_BASE + index * 10 + 1), NULL, NULL);
    SendMessage(ctrl.hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderG, TBM_SETPOS, TRUE, cfg->green_adjust);
    ctrl.hLabelG = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 360, y + 2, 35, 18, hWnd, NULL, NULL, NULL);

    CreateWindowW(L"STATIC", L"B:", WS_CHILD | WS_VISIBLE, 400, y + 2, 15, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        415, y, 80, 22, hWnd, (HMENU)(INT_PTR)(ID_CS_SLIDER_BASE + index * 10 + 2), NULL, NULL);
    SendMessage(ctrl.hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
    SendMessage(ctrl.hSliderB, TBM_SETPOS, TRUE, cfg->blue_adjust);
    ctrl.hLabelB = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 495, y + 2, 35, 18, hWnd, NULL, NULL, NULL);

    CreateWindowW(L"STATIC", L"Bright:", WS_CHILD | WS_VISIBLE, 535, y + 2, 40, 18, hWnd, NULL, NULL, NULL);
    ctrl.hSliderBright = CreateWindowW(TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        575, y, 70, 22, hWnd, (HMENU)(INT_PTR)(ID_CS_SLIDER_BASE + index * 10 + 3), NULL, NULL);
    SendMessage(ctrl.hSliderBright, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(ctrl.hSliderBright, TBM_SETPOS, TRUE, cfg->brightness);
    ctrl.hLabelBright = CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE, 645, y + 2, 35, 18, hWnd, NULL, NULL, NULL);

    g_csDlg->controls.push_back(ctrl);
    UpdateChannelSliderLabel(index);
}

void ClearChannelControls() {
    if (!g_csDlg) return;
    for (auto& ctrl : g_csDlg->controls) {
        DestroyWindow(ctrl.hCheck);
        DestroyWindow(ctrl.hSliderR);
        DestroyWindow(ctrl.hSliderG);
        DestroyWindow(ctrl.hSliderB);
        DestroyWindow(ctrl.hSliderBright);
        DestroyWindow(ctrl.hLabelR);
        DestroyWindow(ctrl.hLabelG);
        DestroyWindow(ctrl.hLabelB);
        DestroyWindow(ctrl.hLabelBright);
    }
    g_csDlg->controls.clear();
}

void CreateChannelTabContent(HWND hWnd, int tab) {
    ClearChannelControls();
    int y = 70;

    if (tab == 0) {
        for (int i = 0; i < 8; i++) {
            wchar_t name[32];
            swprintf(name, 32, L"ASUS Ch %d", i);
            CreateChannelRowInDialog(hWnd, i, y, name, &g_channels.aura_channels[i]);
            y += 28;
        }
    } else if (tab == 1) {
        const wchar_t* names[] = {L"RAM Slot 0", L"RAM Slot 1", L"RAM Slot 2", L"RAM Slot 3"};
        for (int i = 0; i < 4; i++) {
            CreateChannelRowInDialog(hWnd, i, y, names[i], &g_channels.ram_modules[i]);
            y += 28;
        }
    } else if (tab == 2) {
        CreateChannelRowInDialog(hWnd, 0, y, L"SteelSeries", &g_channels.steelseries); y += 28;
        CreateChannelRowInDialog(hWnd, 1, y, L"Keyboard", &g_channels.keyboard); y += 28;
        CreateChannelRowInDialog(hWnd, 2, y, L"Edge LEDs", &g_channels.edge);
    }
}

INT_PTR CALLBACK ChannelSettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_csDlg = new ChannelDialogData();
        g_csDlg->currentTab = 0;

        g_csDlg->hTab = CreateWindowW(WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            10, 10, 680, 28, hWnd, (HMENU)ID_CS_TAB, NULL, NULL);

        TCITEMW tie = {};
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)L"ASUS Aura (8)";
        SendMessage(g_csDlg->hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);
        tie.pszText = (LPWSTR)L"RAM (4)";
        SendMessage(g_csDlg->hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);
        tie.pszText = (LPWSTR)L"Other";
        SendMessage(g_csDlg->hTab, TCM_INSERTITEM, 2, (LPARAM)&tie);

        CreateWindowW(L"STATIC", L"100% = no change. Adjust to correct color deviation.",
            WS_CHILD | WS_VISIBLE, 15, 45, 400, 18, hWnd, NULL, NULL, NULL);

        CreateChannelTabContent(hWnd, 0);

        CreateWindowW(L"BUTTON", L"Save && Close",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            200, 320, 120, 30, hWnd, (HMENU)ID_CS_SAVE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Reset All",
            WS_CHILD | WS_VISIBLE,
            340, 320, 100, 30, hWnd, (HMENU)ID_CS_RESET, NULL, NULL);

        return TRUE;
    }

    case WM_NOTIFY: {
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->code == TCN_SELCHANGE && nmhdr->hwndFrom == g_csDlg->hTab) {
            g_csDlg->currentTab = TabCtrl_GetCurSel(g_csDlg->hTab);
            CreateChannelTabContent(hWnd, g_csDlg->currentTab);
        }
        break;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lParam;
        int pos = (int)SendMessage(slider, TBM_GETPOS, 0, 0);
        int id = GetDlgCtrlID(slider);

        if (id >= ID_CS_SLIDER_BASE) {
            int index = (id - ID_CS_SLIDER_BASE) / 10;
            int component = (id - ID_CS_SLIDER_BASE) % 10;

            ChannelConfig* cfg = nullptr;
            if (g_csDlg->currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
            else if (g_csDlg->currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
            else if (g_csDlg->currentTab == 2) {
                if (index == 0) cfg = &g_channels.steelseries;
                else if (index == 1) cfg = &g_channels.keyboard;
                else if (index == 2) cfg = &g_channels.edge;
            }

            if (cfg) {
                switch (component) {
                    case 0: cfg->red_adjust = pos; break;
                    case 1: cfg->green_adjust = pos; break;
                    case 2: cfg->blue_adjust = pos; break;
                    case 3: cfg->brightness = pos; break;
                }
                UpdateChannelSliderLabel(index);
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id >= ID_CS_CHECK_BASE && id < ID_CS_CHECK_BASE + 20) {
            int index = id - ID_CS_CHECK_BASE;
            bool checked = (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0) == BST_CHECKED);

            ChannelConfig* cfg = nullptr;
            if (g_csDlg->currentTab == 0 && index < 8) cfg = &g_channels.aura_channels[index];
            else if (g_csDlg->currentTab == 1 && index < 4) cfg = &g_channels.ram_modules[index];
            else if (g_csDlg->currentTab == 2) {
                if (index == 0) cfg = &g_channels.steelseries;
                else if (index == 1) cfg = &g_channels.keyboard;
                else if (index == 2) cfg = &g_channels.edge;
            }
            if (cfg) cfg->enabled = checked;
        }
        else if (id == ID_CS_SAVE) {
            g_channels.Save();
            ClearChannelControls();
            delete g_csDlg;
            g_csDlg = nullptr;
            EndDialog(hWnd, IDOK);
        }
        else if (id == ID_CS_RESET) {
            for (int i = 0; i < 8; i++) {
                g_channels.aura_channels[i] = ChannelConfig();
                g_channels.aura_channels[i].name = "ASUS Channel " + std::to_string(i);
            }
            for (int i = 0; i < 4; i++) {
                g_channels.ram_modules[i] = ChannelConfig();
                g_channels.ram_modules[i].name = "RAM Slot " + std::to_string(i);
            }
            g_channels.steelseries = ChannelConfig();
            g_channels.keyboard = ChannelConfig();
            g_channels.edge = ChannelConfig();
            CreateChannelTabContent(hWnd, g_csDlg->currentTab);
        }
        else if (id == IDCANCEL) {
            ClearChannelControls();
            delete g_csDlg;
            g_csDlg = nullptr;
            EndDialog(hWnd, IDCANCEL);
        }
        break;
    }

    case WM_CLOSE:
        ClearChannelControls();
        delete g_csDlg;
        g_csDlg = nullptr;
        EndDialog(hWnd, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void ShowChannelSettingsDialog(HWND hParent) {
    // Create dialog template in memory
    DLGTEMPLATE dlg = {};
    dlg.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER;
    dlg.dwExtendedStyle = 0;
    dlg.cdit = 0;
    dlg.x = 0; dlg.y = 0;
    dlg.cx = 350; dlg.cy = 190;  // Dialog units

    // We need to build a proper dialog template with title
    struct {
        DLGTEMPLATE tmpl;
        WORD menu;
        WORD wndClass;
        WCHAR title[32];
    } dlgData = {};

    dlgData.tmpl.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER;
    dlgData.tmpl.cx = 355;
    dlgData.tmpl.cy = 185;
    dlgData.menu = 0;
    dlgData.wndClass = 0;
    wcscpy_s(dlgData.title, L"Channel Color Correction");

    DialogBoxIndirectW(GetModuleHandle(NULL), &dlgData.tmpl, hParent, ChannelSettingsDlgProc);
}

//=============================================================================
// GUI HELPER FUNCTIONS
//=============================================================================

void UpdatePreview() {
    if (g_state.hPreview) InvalidateRect(g_state.hPreview, NULL, TRUE);
}

void UpdateSliders() {
    if (g_state.hSliderR) SendMessage(g_state.hSliderR, TBM_SETPOS, TRUE, g_state.red);
    if (g_state.hSliderG) SendMessage(g_state.hSliderG, TBM_SETPOS, TRUE, g_state.green);
    if (g_state.hSliderB) SendMessage(g_state.hSliderB, TBM_SETPOS, TRUE, g_state.blue);
    if (g_state.hSliderBrightness) SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, g_state.brightness);
    if (g_state.hSliderSpeed) SendMessage(g_state.hSliderSpeed, TBM_SETPOS, TRUE, g_state.speed);
}

void UpdateHexEdit() {
    wchar_t hex[8];
    swprintf(hex, 8, L"#%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
    SetWindowTextW(g_state.hEditHex, hex);
}

void UpdateAllControls() {
    UpdatePreview();
    UpdateSliders();
    UpdateHexEdit();

    SendMessage(g_state.hCheckAura, BM_SETCHECK, g_state.enableAura ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_state.hCheckMouse, BM_SETCHECK, g_state.enableMouse ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, g_state.enableKeyboard ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_state.hCheckRAM, BM_SETCHECK, g_state.enableRAM ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(g_state.hCheckEdge, BM_SETCHECK, g_state.enableEdge ? BST_CHECKED : BST_UNCHECKED, 0);
}

void ParseHexColor(const wchar_t* hex) {
    if (hex[0] == L'#') hex++;
    if (wcslen(hex) >= 6) {
        wchar_t r[3] = {hex[0], hex[1], 0};
        wchar_t g[3] = {hex[2], hex[3], 0};
        wchar_t b[3] = {hex[4], hex[5], 0};
        g_state.red = (uint8_t)wcstol(r, NULL, 16);
        g_state.green = (uint8_t)wcstol(g, NULL, 16);
        g_state.blue = (uint8_t)wcstol(b, NULL, 16);
    }
}

void SetPresetColor(uint8_t r, uint8_t g, uint8_t b) {
    g_state.red = r; g_state.green = g; g_state.blue = b;
    UpdatePreview();
    UpdateSliders();
    UpdateHexEdit();
}

void PickColor() {
    CHOOSECOLOR cc = {0};
    static COLORREF customColors[16] = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g_state.hWnd;
    cc.lpCustColors = customColors;
    cc.rgbResult = RGB(g_state.red, g_state.green, g_state.blue);
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc)) {
        g_state.red = GetRValue(cc.rgbResult);
        g_state.green = GetGValue(cc.rgbResult);
        g_state.blue = GetBValue(cc.rgbResult);
        UpdatePreview();
        UpdateSliders();
        UpdateHexEdit();
    }
}

//=============================================================================
// TRAY ICON
//=============================================================================

void CreateTrayIcon(HWND hWnd) {
    g_state.nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_state.nid.hWnd = hWnd;
    g_state.nid.uID = 1;
    g_state.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_state.nid.uCallbackMessage = WM_TRAYICON;
    g_state.nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_state.nid.szTip, L"OneClickRGB");
    Shell_NotifyIconW(NIM_ADD, &g_state.nid);
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_state.nid);
}

void ShowTrayMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show Window");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_BLUE, L"Blue");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RED, L"Red");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_GREEN, L"Green");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_WHITE, L"White");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OFF, L"Off");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

void MinimizeToTray() {
    ShowWindow(g_state.hWnd, SW_HIDE);
    g_state.minimizedToTray = true;
}

void RestoreFromTray() {
    ShowWindow(g_state.hWnd, SW_SHOW);
    SetForegroundWindow(g_state.hWnd);
    g_state.minimizedToTray = false;
}

//=============================================================================
// WINDOW PROCEDURES
//=============================================================================

LRESULT CALLBACK PreviewProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HBRUSH brush = CreateSolidBrush(RGB(g_state.red, g_state.green, g_state.blue));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Group box positions (calculated once, used for painting)
struct GroupRect { int x, y, w, h; const wchar_t* title; };
GroupRect g_groups[4];
int g_numGroups = 0;

// Helper to draw group box with proper title spacing
void DrawThemedGroupBox(HDC hdc, const GroupRect& g) {
    HPEN pen = CreatePen(PS_SOLID, 1, g_theme->border);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // Draw rounded rect starting below title
    RoundRect(hdc, g.x, g.y + GROUP_TITLE_H/2, g.x + g.w, g.y + g.h, 4, 4);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);

    // Title background (covers the line where title sits)
    SIZE textSize;
    GetTextExtentPoint32W(hdc, g.title, (int)wcslen(g.title), &textSize);
    RECT titleRect = {g.x + 12, g.y, g.x + 12 + textSize.cx + 8, g.y + GROUP_TITLE_H};
    FillRect(hdc, &titleRect, g_hBgBrush);

    // Title text
    SetTextColor(hdc, g_theme->accent);
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    TextOutW(hdc, g.x + 16, g.y + 1, g.title, (int)wcslen(g.title));
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Create theme brushes
        g_hBgBrush = CreateSolidBrush(g_theme->bgWindow);
        g_hCtrlBrush = CreateSolidBrush(g_theme->bgControl);
        g_hBtnBrush = CreateSolidBrush(g_theme->bgButton);

        // Register preview class
        WNDCLASS wc = {0};
        wc.lpfnWndProc = PreviewProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ColorPreview";
        wc.hbrBackground = g_hBgBrush;
        RegisterClass(&wc);

        // Layout variables
        const int M = MARGIN;
        const int GM = GROUP_MARGIN;
        const int GP = GROUP_PADDING;
        const int GTH = GROUP_TITLE_H;
        const int IS = ITEM_SPACING;
        const int IHS = ITEM_H_SPACING;
        const int CW = WINDOW_WIDTH - M * 2;  // Content width

        int y = M;
        int x = M;
        int gy;  // Group content y start

        // ═══════════════════════════════════════════════════════════════
        // GROUP 1: COLOR
        // ═══════════════════════════════════════════════════════════════
        int g1y = y;
        gy = y + GTH + GP;

        // Row 1: Preview + Hex + Pick + Theme toggle
        g_state.hPreview = CreateWindowW(L"ColorPreview", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x + GP, gy, 70, 50, hWnd, (HMENU)ID_STATIC_PREVIEW, NULL, NULL);

        CreateWindowW(L"STATIC", L"Hex:", WS_CHILD | WS_VISIBLE,
            x + GP + 78, gy + 2, 28, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hEditHex = CreateWindowW(L"EDIT", L"#0022FF",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE | ES_CENTER,
            x + GP + 106, gy, 68, CTRL_H, hWnd, (HMENU)ID_EDIT_HEX, NULL, NULL);

        CreateWindowW(L"BUTTON", g_str->pick,
            WS_CHILD | WS_VISIBLE,
            x + GP + 180, gy, SMALL_BTN_W, CTRL_H, hWnd, (HMENU)ID_BTN_PICK_COLOR, NULL, NULL);

        CreateWindowW(L"BUTTON", g_str->theme,
            WS_CHILD | WS_VISIBLE,
            x + CW - GP - 54, gy, 54, CTRL_H, hWnd, (HMENU)ID_BTN_THEME, NULL, NULL);

        // Row 2: 7 Color presets
        int py = gy + 28;
        int pw = COLOR_BTN_W;
        int px = x + GP + 78;
        CreateWindowW(L"BUTTON", L"Blue", WS_CHILD | WS_VISIBLE, px, py, pw, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_BLUE, NULL, NULL); px += pw + 2;
        CreateWindowW(L"BUTTON", L"Red", WS_CHILD | WS_VISIBLE, px, py, pw - 6, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_RED, NULL, NULL); px += pw - 4;
        CreateWindowW(L"BUTTON", L"Green", WS_CHILD | WS_VISIBLE, px, py, pw + 4, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_GREEN, NULL, NULL); px += pw + 6;
        CreateWindowW(L"BUTTON", L"Cyan", WS_CHILD | WS_VISIBLE, px, py, pw, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_CYAN, NULL, NULL); px += pw + 2;
        CreateWindowW(L"BUTTON", L"Purple", WS_CHILD | WS_VISIBLE, px, py, pw + 6, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_PURPLE, NULL, NULL); px += pw + 8;
        CreateWindowW(L"BUTTON", L"White", WS_CHILD | WS_VISIBLE, px, py, pw + 2, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_WHITE, NULL, NULL); px += pw + 4;
        CreateWindowW(L"BUTTON", L"Off", WS_CHILD | WS_VISIBLE, px, py, pw - 8, CTRL_H, hWnd, (HMENU)ID_BTN_PRESET_OFF, NULL, NULL);

        // Row 3-5: RGB Sliders (with max width)
        int sliderX = x + GP + LABEL_W;
        int sliderW = min(CW - GP * 2 - LABEL_W, MAX_SLIDER_W + 200);
        int sy = gy + 56;

        CreateWindowW(L"STATIC", g_str->red, WS_CHILD | WS_VISIBLE, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_R, NULL, NULL);
        SendMessage(g_state.hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        sy += SLIDER_H + IS;

        CreateWindowW(L"STATIC", g_str->green, WS_CHILD | WS_VISIBLE, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_G, NULL, NULL);
        SendMessage(g_state.hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        sy += SLIDER_H + IS;

        CreateWindowW(L"STATIC", g_str->blue, WS_CHILD | WS_VISIBLE, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_B, NULL, NULL);
        SendMessage(g_state.hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));

        int g1h = sy + SLIDER_H + GP - g1y;
        g_groups[0] = {x, g1y, CW, g1h, g_str->colorSelection};
        y = g1y + g1h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 2: EFFECTS
        // ═══════════════════════════════════════════════════════════════
        int g2y = y;
        gy = y + GTH + GP;

        // Row 1: Keyboard + Edge mode
        CreateWindowW(L"STATIC", g_str->keyboardEffect, WS_CHILD | WS_VISIBLE,
            x + GP, gy + 2, 56, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hComboKbMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            x + GP + 60, gy, MAX_COMBO_W, 200, hWnd, (HMENU)ID_COMBO_KB_MODE, NULL, NULL);
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Static");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Breathing");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Spectrum");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Wave");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Wave Long");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Wheel");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Reactive");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Ripple");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Starlight");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Rainbow");
        SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)L"Hurricane");
        SendMessage(g_state.hComboKbMode, CB_SETCURSEL, 0, 0);

        int ex = x + GP + 60 + MAX_COMBO_W + IHS * 2;
        CreateWindowW(L"STATIC", g_str->edgeEffect, WS_CHILD | WS_VISIBLE,
            ex, gy + 2, 36, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hComboEdgeMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            ex + 40, gy, 120, 200, hWnd, (HMENU)ID_COMBO_EDGE_MODE, NULL, NULL);
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Freeze");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Wave");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Spectrum");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Breath");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Static");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Off");
        SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, 4, 0);
        gy += CTRL_H + IS;

        // Row 2: Brightness + Speed
        CreateWindowW(L"STATIC", g_str->brightness, WS_CHILD | WS_VISIBLE,
            x + GP, gy + 2, 60, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hSliderBrightness = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x + GP + 64, gy, 120, SLIDER_H, hWnd, (HMENU)ID_SLIDER_BRIGHTNESS, NULL, NULL);
        SendMessage(g_state.hSliderBrightness, TBM_SETRANGE, TRUE, MAKELONG(0, 4));
        SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, 4);

        CreateWindowW(L"STATIC", g_str->speed, WS_CHILD | WS_VISIBLE,
            x + GP + 200, gy + 2, 40, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hSliderSpeed = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x + GP + 244, gy, 120, SLIDER_H, hWnd, (HMENU)ID_SLIDER_SPEED, NULL, NULL);
        SendMessage(g_state.hSliderSpeed, TBM_SETRANGE, TRUE, MAKELONG(0, 5));
        SendMessage(g_state.hSliderSpeed, TBM_SETPOS, TRUE, 2);

        int g2h = gy + SLIDER_H + GP - g2y;
        g_groups[1] = {x, g2y, CW, g2h, g_str->effects};
        y = g2y + g2h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 3: DEVICES
        // ═══════════════════════════════════════════════════════════════
        int g3y = y;
        gy = y + GTH + GP;

        g_state.hCheckAura = CreateWindowW(L"BUTTON", L"ASUS Aura",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP, gy, CHECKBOX_W, CTRL_H, hWnd, (HMENU)ID_CHECK_AURA, NULL, NULL);
        SendMessage(g_state.hCheckAura, BM_SETCHECK, BST_CHECKED, 0);

        g_state.hCheckMouse = CreateWindowW(L"BUTTON", L"SteelSeries",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + CHECKBOX_W + IHS, gy, CHECKBOX_W, CTRL_H, hWnd, (HMENU)ID_CHECK_MOUSE, NULL, NULL);
        SendMessage(g_state.hCheckMouse, BM_SETCHECK, BST_CHECKED, 0);

        g_state.hCheckKeyboard = CreateWindowW(L"BUTTON", L"Keyboard",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + (CHECKBOX_W + IHS) * 2, gy, CHECKBOX_W - 10, CTRL_H, hWnd, (HMENU)ID_CHECK_KEYBOARD, NULL, NULL);
        SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, BST_CHECKED, 0);

        g_state.hCheckEdge = CreateWindowW(L"BUTTON", L"Edge LEDs",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + (CHECKBOX_W + IHS) * 3 - 10, gy, CHECKBOX_W - 10, CTRL_H, hWnd, (HMENU)ID_CHECK_EDGE, NULL, NULL);
        SendMessage(g_state.hCheckEdge, BM_SETCHECK, BST_CHECKED, 0);

        g_state.hCheckRAM = CreateWindowW(L"BUTTON", L"RAM",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + (CHECKBOX_W + IHS) * 4 - 20, gy, 50, CTRL_H, hWnd, (HMENU)ID_CHECK_RAM, NULL, NULL);
        SendMessage(g_state.hCheckRAM, BM_SETCHECK, BST_CHECKED, 0);

        CreateWindowW(L"BUTTON", g_str->channelCorrection,
            WS_CHILD | WS_VISIBLE,
            x + CW - GP - 90, gy, 90, CTRL_H, hWnd, (HMENU)ID_BTN_CHANNEL_SETTINGS, NULL, NULL);

        int g3h = gy + CTRL_H + GP - g3y;
        g_groups[2] = {x, g3y, CW, g3h, g_str->devices};
        y = g3y + g3h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 4: PROFILES
        // ═══════════════════════════════════════════════════════════════
        int g4y = y;
        gy = y + GTH + GP;

        CreateWindowW(L"STATIC", g_str->profile, WS_CHILD | WS_VISIBLE,
            x + GP, gy + 2, 42, CTRL_H, hWnd, NULL, NULL, NULL);
        g_state.hComboProfiles = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
            x + GP + 46, gy, 130, 200, hWnd, (HMENU)ID_COMBO_PROFILES, NULL, NULL);

        CreateWindowW(L"BUTTON", g_str->save, WS_CHILD | WS_VISIBLE,
            x + GP + 182, gy, SMALL_BTN_W, CTRL_H, hWnd, (HMENU)ID_BTN_SAVE_PROFILE, NULL, NULL);
        CreateWindowW(L"BUTTON", g_str->load, WS_CHILD | WS_VISIBLE,
            x + GP + 182 + SMALL_BTN_W + 4, gy, SMALL_BTN_W, CTRL_H, hWnd, (HMENU)ID_BTN_LOAD_PROFILE, NULL, NULL);

        g_state.hCheckAutostart = CreateWindowW(L"BUTTON", g_str->autostart,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + 300, gy, 72, CTRL_H, hWnd, (HMENU)ID_CHECK_AUTOSTART, NULL, NULL);
        if (IsAutoStartEnabled()) {
            SendMessage(g_state.hCheckAutostart, BM_SETCHECK, BST_CHECKED, 0);
            g_state.autostart = true;
        }

        g_state.hCheckMinimizeTray = CreateWindowW(L"BUTTON", g_str->tray,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + GP + 376, gy, 44, CTRL_H, hWnd, (HMENU)ID_CHECK_MINIMIZE_TRAY, NULL, NULL);
        SendMessage(g_state.hCheckMinimizeTray, BM_SETCHECK, BST_CHECKED, 0);

        int g4h = gy + CTRL_H + GP - g4y;
        g_groups[3] = {x, g4y, CW, g4h, g_str->profilesSettings};
        g_numGroups = 4;
        y = g4y + g4h + GM;

        // ═══════════════════════════════════════════════════════════════
        // APPLY BUTTON
        // ═══════════════════════════════════════════════════════════════
        CreateWindowW(L"BUTTON", g_str->apply,
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            x, y, CW, 32, hWnd, (HMENU)ID_BTN_APPLY, NULL, NULL);
        y += 36;

        // ═══════════════════════════════════════════════════════════════
        // STATUS
        // ═══════════════════════════════════════════════════════════════
        g_state.hStatus = CreateWindowW(L"EDIT", g_str->ready,
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            x, y, CW, STATUS_H, hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);

        // Initialize
        UpdateSliders();
        UpdateHexEdit();
        RefreshProfileList();
        CreateTrayIcon(hWnd);
        g_channels.Load();
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Draw all group boxes
        for (int i = 0; i < g_numGroups; i++) {
            DrawThemedGroupBox(hdc, g_groups[i]);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, g_theme->textPrimary);
        SetBkColor(hdc, g_theme->bgWindow);
        return (LRESULT)g_hBgBrush;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hBgBrush);
        return 1;
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lParam;
        int pos = SendMessage(slider, TBM_GETPOS, 0, 0);

        if (slider == g_state.hSliderR) { g_state.red = pos; UpdatePreview(); UpdateHexEdit(); }
        else if (slider == g_state.hSliderG) { g_state.green = pos; UpdatePreview(); UpdateHexEdit(); }
        else if (slider == g_state.hSliderB) { g_state.blue = pos; UpdatePreview(); UpdateHexEdit(); }
        else if (slider == g_state.hSliderBrightness) g_state.brightness = pos;
        else if (slider == g_state.hSliderSpeed) g_state.speed = pos;
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_APPLY) {
            std::thread(ApplyColors).detach();
        }
        else if (id == ID_BTN_PICK_COLOR) {
            PickColor();
        }
        else if (id == ID_EDIT_HEX && code == EN_KILLFOCUS) {
            wchar_t hex[16];
            GetWindowTextW(g_state.hEditHex, hex, 16);
            ParseHexColor(hex);
            UpdatePreview();
            UpdateSliders();
        }
        else if (id == ID_COMBO_KB_MODE && code == CBN_SELCHANGE) {
            int sel = SendMessage(g_state.hComboKbMode, CB_GETCURSEL, 0, 0);
            uint8_t modes[] = {KB_MODE_STATIC, KB_MODE_BREATHING, KB_MODE_SPECTRUM, KB_MODE_WAVE_SHORT,
                               KB_MODE_WAVE_LONG, KB_MODE_COLOR_WHEEL, KB_MODE_REACTIVE, KB_MODE_RIPPLE,
                               KB_MODE_STARLIGHT, KB_MODE_RAINBOW, KB_MODE_HURRICANE};
            if (sel >= 0 && sel < 11) g_state.kbMode = modes[sel];
        }
        else if (id == ID_COMBO_EDGE_MODE && code == CBN_SELCHANGE) {
            int sel = SendMessage(g_state.hComboEdgeMode, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel <= 5) g_state.edgeMode = sel;
        }
        else if (id == ID_CHECK_AURA) g_state.enableAura = (SendMessage(g_state.hCheckAura, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (id == ID_CHECK_MOUSE) g_state.enableMouse = (SendMessage(g_state.hCheckMouse, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (id == ID_CHECK_KEYBOARD) g_state.enableKeyboard = (SendMessage(g_state.hCheckKeyboard, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (id == ID_CHECK_RAM) g_state.enableRAM = (SendMessage(g_state.hCheckRAM, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (id == ID_CHECK_EDGE) g_state.enableEdge = (SendMessage(g_state.hCheckEdge, BM_GETCHECK, 0, 0) == BST_CHECKED);
        else if (id == ID_CHECK_AUTOSTART) {
            g_state.autostart = (SendMessage(g_state.hCheckAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetAutoStart(g_state.autostart);
        }
        else if (id == ID_CHECK_MINIMIZE_TRAY) {
            g_state.minimizeToTray = (SendMessage(g_state.hCheckMinimizeTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        else if (id == ID_BTN_CHANNEL_SETTINGS) {
            // Open integrated Channel Settings dialog
            ShowChannelSettingsDialog(hWnd);
            g_channels.Load();  // Reload after dialog closes
            AppendStatus(L"Channel corrections updated");
        }
        else if (id == ID_BTN_SAVE_PROFILE) {
            wchar_t name[64];
            GetWindowTextW(g_state.hComboProfiles, name, 64);
            if (wcslen(name) > 0) {
                SaveProfile(name);
                RefreshProfileList();
            }
        }
        else if (id == ID_BTN_LOAD_PROFILE) {
            wchar_t name[64];
            GetWindowTextW(g_state.hComboProfiles, name, 64);
            if (wcslen(name) > 0) {
                LoadProfile(name);
                UpdateAllControls();
            }
        }
        // Presets
        else if (id == ID_BTN_PRESET_BLUE) SetPresetColor(0, 34, 255);
        else if (id == ID_BTN_PRESET_RED) SetPresetColor(255, 0, 0);
        else if (id == ID_BTN_PRESET_GREEN) SetPresetColor(0, 255, 0);
        else if (id == ID_BTN_PRESET_CYAN) SetPresetColor(0, 255, 255);
        else if (id == ID_BTN_PRESET_PURPLE) SetPresetColor(128, 0, 255);
        else if (id == ID_BTN_PRESET_WHITE) SetPresetColor(255, 255, 255);
        else if (id == ID_BTN_PRESET_OFF) SetPresetColor(0, 0, 0);
        else if (id == ID_BTN_THEME) {
            // Toggle theme
            g_theme = g_theme->isDark ? &g_lightTheme : &g_darkTheme;
            DeleteObject(g_hBgBrush);
            DeleteObject(g_hCtrlBrush);
            DeleteObject(g_hBtnBrush);
            g_hBgBrush = CreateSolidBrush(g_theme->bgWindow);
            g_hCtrlBrush = CreateSolidBrush(g_theme->bgControl);
            g_hBtnBrush = CreateSolidBrush(g_theme->bgButton);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        // Tray menu
        else if (id == ID_TRAY_SHOW) RestoreFromTray();
        else if (id == ID_TRAY_EXIT) {
            RemoveTrayIcon();
            PostQuitMessage(0);
        }
        else if (id == ID_TRAY_BLUE) { SetPresetColor(0, 34, 255); std::thread(ApplyColors).detach(); }
        else if (id == ID_TRAY_RED) { SetPresetColor(255, 0, 0); std::thread(ApplyColors).detach(); }
        else if (id == ID_TRAY_GREEN) { SetPresetColor(0, 255, 0); std::thread(ApplyColors).detach(); }
        else if (id == ID_TRAY_WHITE) { SetPresetColor(255, 255, 255); std::thread(ApplyColors).detach(); }
        else if (id == ID_TRAY_OFF) { SetPresetColor(0, 0, 0); std::thread(ApplyColors).detach(); }
        break;
    }

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            ShowTrayMenu(hWnd);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            RestoreFromTray();
        }
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_MINIMIZE && g_state.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        break;

    case WM_CLOSE:
        if (g_state.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        break;

    case WM_DESTROY:
        RemoveTrayIcon();
        if (g_hBgBrush) DeleteObject(g_hBgBrush);
        if (g_hCtrlBrush) DeleteObject(g_hCtrlBrush);
        if (g_hBtnBrush) DeleteObject(g_hBtnBrush);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

//=============================================================================
// MAIN
//=============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Check for --minimized flag
    bool startMinimized = (strstr(lpCmdLine, "--minimized") != nullptr);

    // Init common controls
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"OneClickRGBClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    // Create window
    g_state.hWnd = CreateWindowW(L"OneClickRGBClass", L"OneClickRGB v2.0 - Complete RGB Control",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (startMinimized) {
        MinimizeToTray();
    } else {
        ShowWindow(g_state.hWnd, nCmdShow);
    }
    UpdateWindow(g_state.hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
