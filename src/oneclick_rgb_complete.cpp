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
#include <windowsx.h>
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
#include <algorithm>
#include <objidl.h>
#include <gdiplus.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include "hidapi.h"
#include "channel_config.h"
#include "modern_ui.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

//=============================================================================
// CONSTANTS & LAYOUT
//=============================================================================

#define APP_NAME L"OneClickRGB"
#define APP_VERSION L"3.4"

// Layout constants (responsive)
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 820
#define TITLEBAR_H 32       // Custom titlebar height
#define MARGIN 12           // Window margin
#define GROUP_MARGIN 8      // Space between groups
#define GROUP_TITLE_H 32    // Height for group title badge + spacing below
#define GROUP_PADDING 12    // Padding inside group (uniform: left, top, bottom)
#define ITEM_SPACING 6      // Vertical spacing between items
#define ITEM_H_SPACING 8    // Horizontal spacing between items
#define BORDER_RADIUS 8     // Rounded corners for groups

// Max widths for controls (prevents over-stretching)
#define MAX_SLIDER_W 200
#define MAX_BUTTON_W 80
#define MAX_COMBO_W 160
#define LABEL_W 70
#define CHECKBOX_W 85
#define SMALL_BTN_W 50
#define COLOR_BTN_W 48      // Minimum width for color preset buttons
#define BTN_GAP 6           // Gap between buttons

// Control heights
#define CTRL_H 28           // Minimum height for buttons (includes 2px inset on each side)
#define BTN_H 28            // Explicit button height
#define SLIDER_H 20
#define STATUS_H 120

// Tray icon
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 3000
#define ID_TRAY_SHOW 3001
#define ID_TRAY_BLUE 3010
#define ID_TRAY_RED 3011
#define ID_TRAY_GREEN 3012
#define ID_TRAY_WHITE 3013
#define ID_TRAY_OFF 3014
#define ID_TRAY_STANDBY 3015
#define ID_TRAY_SHUTDOWN 3016
#define ID_TRAY_RESTART 3017
#define ID_TIMER_RESUME 3020
#define ID_TIMER_DEBOUNCE 3021

// Custom titlebar buttons
#define ID_BTN_MINIMIZE 3030
#define ID_BTN_MAXIMIZE 3031
#define ID_BTN_CLOSE 3032

// Global Hotkeys
#define ID_HOTKEY_BLUE 4001
#define ID_HOTKEY_RED 4002
#define ID_HOTKEY_GREEN 4003
#define ID_HOTKEY_WHITE 4004
#define ID_HOTKEY_OFF 4005
#define ID_HOTKEY_TOGGLE 4006

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
#define ID_CHECK_AUTO_APPLY 1042
#define ID_BTN_SAVE_PROFILE 1050
#define ID_BTN_LOAD_PROFILE 1051
#define ID_COMBO_PROFILES 1052
#define ID_STATIC_STATUS 1060
#define ID_BTN_THEME 1070
#define ID_BTN_LANG 1071

// Presets (7 colors)
#define ID_BTN_PRESET_BLUE 1100
#define ID_BTN_PRESET_RED 1101
#define ID_BTN_PRESET_GREEN 1102
#define ID_BTN_PRESET_PURPLE 1103
#define ID_BTN_PRESET_WHITE 1104
#define ID_BTN_PRESET_OFF 1105
#define ID_BTN_PRESET_CYAN 1106
#define ID_BTN_CHANNEL_SETTINGS 1110
#define ID_BTN_ASUS_TEST 1111
#define ID_BTN_HID_RESET 1112

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

// Trackbar background brush matching the dark theme
HBRUSH g_hTrackbarBrush = NULL;

void InitTrackbarBrush() {
    if (!g_hTrackbarBrush) {
        // Use average color from gradient (27, 31, 42) - blends well with background
        g_hTrackbarBrush = CreateSolidBrush(RGB(27, 31, 42));
    }
}
HBITMAP g_hLogoBitmap = NULL;
Gdiplus::Image* g_pLogoImage = NULL;  // Keep GDI+ image for proper alpha
int g_logoWidth = 0;
int g_logoHeight = 0;
bool g_skipApplyOnStart = false;  // Skip applying colors after theme/language restart

//=============================================================================
// MODERN UI COMPONENTS
//=============================================================================

// Custom slider data for RGB
struct CustomSliderData {
    ModernSlider slider;
    bool registered;
};

CustomSliderData g_sliderR = {{}, false};
CustomSliderData g_sliderG = {{}, false};
CustomSliderData g_sliderB = {{}, false};
CustomSliderData g_sliderBrightness = {{}, false};
CustomSliderData g_sliderSpeed = {{}, false};

// Modern cards (group boxes)
ModernCard g_cards[4];
int g_numCards = 0;

// Modern color preview
ModernColorPreview g_colorPreview;

// Track mouse for hover effects
HWND g_hoverWnd = NULL;
TRACKMOUSEEVENT g_trackMouse = {sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_HOVER, NULL, 10};

// Animation timer
#define ID_TIMER_ANIMATION 5000
float g_animPhase = 0.0f;

// Semi-transparent control background brush
HBRUSH g_hTransparentBrush = NULL;

//=============================================================================
// i18n LOCALIZATION
//=============================================================================

enum Lang { LANG_EN, LANG_DE };
Lang g_lang = LANG_EN;

struct Strings {
    // Group titles
    const wchar_t* colorSelection;
    const wchar_t* effects;
    const wchar_t* devices;
    const wchar_t* profilesSettings;
    // Color section
    const wchar_t* red;
    const wchar_t* green;
    const wchar_t* blue;
    const wchar_t* pick;
    const wchar_t* hex;
    // Effects section
    const wchar_t* keyboardEffect;
    const wchar_t* edgeEffect;
    const wchar_t* brightness;
    const wchar_t* speed;
    // Devices section
    const wchar_t* channelCorrection;
    // Profiles section
    const wchar_t* profile;
    const wchar_t* save;
    const wchar_t* load;
    const wchar_t* autostart;
    const wchar_t* tray;
    const wchar_t* autoApply;
    // Buttons
    const wchar_t* apply;
    const wchar_t* theme;
    // Status
    const wchar_t* statusTitle;
    const wchar_t* ready;
    // Window title
    const wchar_t* windowTitle;
    // Color presets
    const wchar_t* presetBlue;
    const wchar_t* presetRed;
    const wchar_t* presetGreen;
    const wchar_t* presetCyan;
    const wchar_t* presetPurple;
    const wchar_t* presetWhite;
    const wchar_t* presetOff;
    // Keyboard modes
    const wchar_t* modeStatic;
    const wchar_t* modeBreathing;
    const wchar_t* modeWave;
    const wchar_t* modeReactive;
    const wchar_t* modeRainbow;
    // Edge modes
    const wchar_t* edgeStatic;
    const wchar_t* edgeBreathing;
    const wchar_t* edgeWave;
    const wchar_t* edgeSpectrum;
    const wchar_t* edgeOff;
    // Channel settings dialog
    const wchar_t* csTitle;
    const wchar_t* csSaveClose;
    const wchar_t* csResetAll;
    const wchar_t* csHint;
};

Strings g_strEN = {
    // Group titles
    L"Color", L"Effects", L"Devices", L"Profiles",
    // Color section
    L"Red", L"Green", L"Blue", L"Pick", L"Hex:",
    // Effects section
    L"Keyboard", L"Edge", L"Brightness", L"Speed",
    // Devices section
    L"Channels...",
    // Profiles section
    L"Profile", L"Save", L"Load", L"Autostart", L"Tray", L"Live",
    // Buttons
    L"APPLY", L"Theme",
    // Status
    L"Status", L"Ready - Select color and click Apply",
    // Window title
    L"OneClickRGB v3.4 - Complete RGB Control [Admin: %s]",
    // Color presets
    L"Blue", L"Red", L"Green", L"Cyan", L"Purple", L"White", L"Off",
    // Keyboard modes
    L"Static", L"Breathing", L"Wave", L"Reactive", L"Rainbow",
    // Edge modes
    L"Static", L"Breathing", L"Wave", L"Spectrum", L"Off",
    // Channel settings dialog
    L"Channel Color Correction", L"Save && Close", L"Reset All",
    L"100% = no change. Adjust to correct color deviation."
};

Strings g_strDE = {
    // Group titles (ä=\x00E4, ö=\x00F6, ü=\x00FC, ß=\x00DF, Ä=\x00C4, Ö=\x00D6, Ü=\x00DC)
    L"Farbe", L"Effekte", L"Ger\x00E4te", L"Profile",
    // Color section
    L"Rot", L"Gr\x00FCn", L"Blau", L"W\x00E4hlen", L"Hex:",
    // Effects section
    L"Tastatur", L"Rand-LEDs", L"Helligkeit", L"Tempo",
    // Devices section
    L"Kan\x00E4le...",
    // Profiles section
    L"Profil", L"Speichern", L"Laden", L"Autostart", L"Tray", L"Live",
    // Buttons
    L"ANWENDEN", L"Design",
    // Status
    L"Status", L"Bereit - Farbe w\x00E4hlen und Anwenden klicken",
    // Window title
    L"OneClickRGB v3.4 - Komplette RGB-Steuerung [Admin: %s]",
    // Color presets
    L"Blau", L"Rot", L"Gr\x00FCn", L"Cyan", L"Lila", L"Wei\x00DF", L"Aus",
    // Keyboard modes
    L"Statisch", L"Atmend", L"Welle", L"Reaktiv", L"Regenbogen",
    // Edge modes
    L"Statisch", L"Atmend", L"Welle", L"Spektrum", L"Aus",
    // Channel settings dialog
    L"Kanal-Farbkorrektur", L"Speichern && Schlie\x00DFen", L"Alle zur\x00FCcksetzen",
    L"100% = keine \x00C4nderung. Anpassen um Farbabweichungen zu korrigieren."
};

Strings* g_str = &g_strEN;

//=============================================================================
// ADMIN CHECK
//=============================================================================

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

wchar_t g_windowTitle[256] = {0};

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
    HWND hStatusBorder = NULL;  // Rounded border container for status log
    HWND hLogo = NULL;  // Logo control (always on top)
    HWND hCheckAura = NULL, hCheckMouse = NULL, hCheckKeyboard = NULL;
    HWND hCheckRAM = NULL, hCheckEdge = NULL;
    HWND hCheckAutostart = NULL, hCheckMinimizeTray = NULL, hCheckAutoApply = NULL;
    // Titlebar buttons
    HWND hBtnClose = NULL, hBtnMaximize = NULL, hBtnMinimize = NULL;

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
    bool autoApply = true;
    bool dryRun = false;  // Dry-run mode: skip hardware communication

    // Status
    std::wstring statusLog;
    std::atomic<bool> applying{false};
    std::mutex statusMutex;

    // Profiles
    std::vector<std::wstring> profiles;
    std::wstring currentProfile;
    std::wstring lastProfile;
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

// Window position storage
int g_windowX = CW_USEDEFAULT, g_windowY = CW_USEDEFAULT;

void SaveAppSettings() {
    std::wstring path = GetAppDataPath() + L"\\app_settings.cfg";
    std::ofstream file(path);
    if (file.is_open()) {
        file << "lang=" << (g_lang == LANG_DE ? "de" : "en") << "\n";
        file << "dark=" << (g_theme->isDark ? 1 : 0) << "\n";
        // Save last profile
        if (!g_state.lastProfile.empty()) {
            std::string profileName(g_state.lastProfile.begin(), g_state.lastProfile.end());
            file << "lastProfile=" << profileName << "\n";
        }
        // Save window position
        if (g_state.hWnd) {
            RECT rc;
            GetWindowRect(g_state.hWnd, &rc);
            file << "windowX=" << rc.left << "\n";
            file << "windowY=" << rc.top << "\n";
        }
        file.close();
    }
}

void LoadAppSettings() {
    std::wstring path = GetAppDataPath() + L"\\app_settings.cfg";
    std::ifstream file(path);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("lang=de") != std::string::npos) {
                g_lang = LANG_DE;
                g_str = &g_strDE;
            }
            if (line.find("dark=1") != std::string::npos) {
                g_theme = &g_darkTheme;
            } else if (line.find("dark=0") != std::string::npos) {
                g_theme = &g_lightTheme;
            }
            // Load last profile name
            if (line.find("lastProfile=") == 0) {
                std::string profileName = line.substr(12);
                g_state.lastProfile = std::wstring(profileName.begin(), profileName.end());
            }
            // Load window position
            if (line.find("windowX=") == 0) {
                g_windowX = std::stoi(line.substr(8));
            }
            if (line.find("windowY=") == 0) {
                g_windowY = std::stoi(line.substr(8));
            }
        }
        file.close();
    }
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
        g_state.lastProfile = name;
        SaveAppSettings();  // Remember last profile
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
// Direct HID control with 65-byte buffer (Report ID 0xEC)
//=============================================================================

#define ASUS_LEDS_PER_PACKET 20
#define AURA_REQUEST_FIRMWARE_VERSION 0x82
#define AURA_REQUEST_CONFIG_TABLE 0xB0

// Hardware configuration from device scan
struct AsusHardwareConfig {
    bool valid = false;
    char firmware[17] = {0};
    uint8_t configTable[60] = {0};
    int numMainboardLEDs = 0;
    int numRGBHeaders = 0;
    int numAddressableHeaders = 0;

    struct Channel {
        bool present = false;
        int ledCount = 0;
        bool addressable = false;
        int directChannel = 0;  // The actual channel number to send to device
        char name[64] = {0};
        // Saved color settings
        uint8_t colorR = 0;
        uint8_t colorG = 34;
        uint8_t colorB = 255;
        bool enabled = true;
    };
    Channel channels[8];
    int numChannels = 0;
};

AsusHardwareConfig g_asusHwConfig;

// Read firmware version from device
bool ReadAsusFirmware(hid_device* dev, char* firmware) {
    uint8_t buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0x00] = 0xEC;
    buf[0x01] = AURA_REQUEST_FIRMWARE_VERSION;

    if (hid_write(dev, buf, 65) < 0) return false;
    if (hid_read_timeout(dev, buf, 65, 1000) < 0) return false;

    if (buf[1] == 0x02) {
        memcpy(firmware, &buf[2], 16);
        firmware[16] = 0;
        return true;
    }
    return false;
}

// Read config table from device
bool ReadAsusConfigTable(hid_device* dev, uint8_t* configTable) {
    uint8_t buf[65];
    memset(buf, 0, sizeof(buf));
    buf[0x00] = 0xEC;
    buf[0x01] = AURA_REQUEST_CONFIG_TABLE;

    if (hid_write(dev, buf, 65) < 0) return false;
    if (hid_read_timeout(dev, buf, 65, 1000) < 0) return false;

    if (buf[1] == 0x30) {
        memcpy(configTable, &buf[4], 60);
        return true;
    }
    return false;
}

// Parse config table to determine channels (like OpenRGB)
void ParseAsusConfig(AsusHardwareConfig& cfg) {
    // From OpenRGB AsusAuraMainboardController:
    // config_table[0x1B] = num_total_mainboard_leds
    // config_table[0x1D] = num_rgb_headers
    // config_table[0x02] = num_addressable_headers

    cfg.numMainboardLEDs = cfg.configTable[0x1B];
    cfg.numRGBHeaders = cfg.configTable[0x1D];
    cfg.numAddressableHeaders = cfg.configTable[0x02];

    if (cfg.numMainboardLEDs < cfg.numRGBHeaders) {
        cfg.numRGBHeaders = 0;
    }

    cfg.numChannels = 0;

    // Mainboard fixed LEDs - uses direct_channel 0x04 (from OpenRGB)
    if (cfg.numMainboardLEDs > 0) {
        cfg.channels[cfg.numChannels].present = true;
        cfg.channels[cfg.numChannels].ledCount = cfg.numMainboardLEDs;
        cfg.channels[cfg.numChannels].addressable = false;
        cfg.channels[cfg.numChannels].directChannel = 0x04;  // Mainboard uses channel 4
        cfg.channels[cfg.numChannels].colorR = 0;
        cfg.channels[cfg.numChannels].colorG = 34;
        cfg.channels[cfg.numChannels].colorB = 255;
        cfg.channels[cfg.numChannels].enabled = true;
        sprintf(cfg.channels[cfg.numChannels].name, "Mainboard (%d LEDs)", cfg.numMainboardLEDs);
        cfg.numChannels++;
    }

    // Addressable headers - use their index as direct_channel
    for (int i = 0; i < cfg.numAddressableHeaders && cfg.numChannels < 8; i++) {
        // Addressable channels typically support up to 120 LEDs
        cfg.channels[cfg.numChannels].present = true;
        cfg.channels[cfg.numChannels].ledCount = 120;  // Max per addressable header
        cfg.channels[cfg.numChannels].addressable = true;
        cfg.channels[cfg.numChannels].directChannel = i;  // Addressable uses 0, 1, 2...
        cfg.channels[cfg.numChannels].colorR = 0;
        cfg.channels[cfg.numChannels].colorG = 34;
        cfg.channels[cfg.numChannels].colorB = 255;
        cfg.channels[cfg.numChannels].enabled = true;
        sprintf(cfg.channels[cfg.numChannels].name, "Addressable %d (max 120 LEDs)", i + 1);
        cfg.numChannels++;
    }

    cfg.valid = (cfg.numChannels > 0);
}

// Full hardware scan
bool ScanAsusHardware() {
    hid_init();

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
        hid_exit();
        g_asusHwConfig.valid = false;
        return false;
    }

    // Read firmware
    if (!ReadAsusFirmware(dev, g_asusHwConfig.firmware)) {
        hid_close(dev);
        hid_exit();
        g_asusHwConfig.valid = false;
        return false;
    }

    // Read config table
    if (!ReadAsusConfigTable(dev, g_asusHwConfig.configTable)) {
        hid_close(dev);
        hid_exit();
        g_asusHwConfig.valid = false;
        return false;
    }

    hid_close(dev);
    hid_exit();

    // Parse configuration
    ParseAsusConfig(g_asusHwConfig);

    return g_asusHwConfig.valid;
}

// Save hardware config to file
void SaveAsusHardwareConfig() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        wcscat(path, L"\\OneClickRGB");
        CreateDirectoryW(path, NULL);
        wcscat(path, L"\\asus_hw_config.bin");

        FILE* f = _wfopen(path, L"wb");
        if (f) {
            fwrite(&g_asusHwConfig, sizeof(g_asusHwConfig), 1, f);
            fclose(f);
        }
    }
}

// Load hardware config from file
bool LoadAsusHardwareConfig() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        wcscat(path, L"\\OneClickRGB\\asus_hw_config.bin");

        FILE* f = _wfopen(path, L"rb");
        if (f) {
            size_t read = fread(&g_asusHwConfig, sizeof(g_asusHwConfig), 1, f);
            fclose(f);
            return (read == 1 && g_asusHwConfig.valid);
        }
    }
    return false;
}

// Check if hardware config has changed
bool HasAsusHardwareChanged() {
    AsusHardwareConfig current;

    hid_init();
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
        hid_exit();
        return !g_asusHwConfig.valid;  // Changed if we had valid config but no device now
    }

    ReadAsusFirmware(dev, current.firmware);
    ReadAsusConfigTable(dev, current.configTable);
    hid_close(dev);
    hid_exit();

    // Compare firmware and config table
    if (strcmp(g_asusHwConfig.firmware, current.firmware) != 0) return true;
    if (memcmp(g_asusHwConfig.configTable, current.configTable, 60) != 0) return true;

    return false;
}

// Initialize ASUS hardware on startup
void InitAsusHardware() {
    bool needScan = true;

    // Try to load saved config
    if (LoadAsusHardwareConfig()) {
        // Check if hardware changed
        if (!HasAsusHardwareChanged()) {
            needScan = false;  // Config still valid
        }
    }

    if (needScan) {
        if (ScanAsusHardware()) {
            SaveAsusHardwareConfig();
        }
    }
}

hid_device* OpenAsusAura() {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::ASUS_VID, Devices::ASUS_AURA_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->usage_page == Devices::ASUS_USAGE_PAGE) {
            dev = hid_open_path(cur->path);
            break;
        }
    }
    hid_free_enumeration(devs);

    // SetGen1() - Required initialization before Direct Mode (from OpenRGB)
    if (dev) {
        uint8_t buf[65];
        memset(buf, 0, sizeof(buf));
        buf[0x00] = 0xEC;
        buf[0x01] = 0x52;
        buf[0x02] = 0x53;
        buf[0x03] = 0x00;
        buf[0x04] = 0x01;
        hid_write(dev, buf, 65);
        Sleep(5);
    }

    return dev;
}

void SetAsusChannel(hid_device* dev, int channel, int numLEDs, uint8_t r, uint8_t g, uint8_t b) {
    if (!dev) return;
    int offset = 0;

    while (offset < numLEDs) {
        int count = ASUS_LEDS_PER_PACKET;
        if (offset + count > numLEDs) count = numLEDs - offset;
        bool last = (offset + count >= numLEDs);

        // OpenRGB-style: 65-byte buffer with 0xEC as Report ID
        uint8_t buf[65];
        memset(buf, 0, sizeof(buf));

        buf[0x00] = 0xEC;  // Report ID
        buf[0x01] = 0x40;  // Direct mode
        buf[0x02] = (last ? 0x80 : 0x00) | channel;
        buf[0x03] = offset;
        buf[0x04] = count;

        for (int i = 0; i < count; i++) {
            buf[0x05 + i*3 + 0] = r;
            buf[0x05 + i*3 + 1] = g;
            buf[0x05 + i*3 + 2] = b;
        }

        hid_write(dev, buf, 65);
        Sleep(2);
        offset += count;
    }
}

bool SetAsusAura(uint8_t r, uint8_t g, uint8_t b) {
    hid_device* dev = OpenAsusAura();
    if (!dev) {
        AppendStatus(L"[ASUS Aura] Not found");
        return false;
    }

    int setCount = 0;

    // Use hardware config if available
    if (g_asusHwConfig.valid) {
        for (int i = 0; i < g_asusHwConfig.numChannels; i++) {
            if (g_channels.aura_channels[i].enabled) {
                uint8_t cr = r, cg = g, cb = b;
                g_channels.aura_channels[i].ApplyCorrection(cr, cg, cb);
                SetAsusChannel(dev, g_asusHwConfig.channels[i].directChannel,
                              g_asusHwConfig.channels[i].ledCount, cr, cg, cb);
                setCount++;
            }
        }
    } else {
        // Fallback: old static config
        struct { int channel; int leds; } channels[] = {
            {0, 60}, {1, 120}, {2, 120}, {3, 60}, {4, 60}, {5, 60}, {6, 60}, {7, 60}
        };
        for (int i = 0; i < 8; i++) {
            if (g_channels.aura_channels[i].enabled) {
                uint8_t cr = r, cg = g, cb = b;
                g_channels.aura_channels[i].ApplyCorrection(cr, cg, cb);
                SetAsusChannel(dev, channels[i].channel, channels[i].leds, cr, cg, cb);
                setCount++;
            }
        }
    }

    hid_close(dev);

    wchar_t buf[64];
    swprintf(buf, 64, L"[ASUS Aura] %d channels set", setCount);
    AppendStatus(buf);
    return true;
}

// Quick update for live preview (single call, no status messages)
void SetAsusAuraQuick(uint8_t r, uint8_t g, uint8_t b) {
    hid_init();
    hid_device* dev = OpenAsusAura();
    if (!dev) {
        hid_exit();
        return;
    }

    // Use hardware config if available
    if (g_asusHwConfig.valid) {
        for (int i = 0; i < g_asusHwConfig.numChannels; i++) {
            if (g_channels.aura_channels[i].enabled) {
                uint8_t cr = r, cg = g, cb = b;
                g_channels.aura_channels[i].ApplyCorrection(cr, cg, cb);
                SetAsusChannel(dev, g_asusHwConfig.channels[i].directChannel,
                              g_asusHwConfig.channels[i].ledCount, cr, cg, cb);
            }
        }
    } else {
        // Fallback
        struct { int channel; int leds; } channels[] = {
            {0, 60}, {1, 120}, {2, 120}, {3, 60}, {4, 60}, {5, 60}, {6, 60}, {7, 60}
        };
        for (int i = 0; i < 8; i++) {
            if (g_channels.aura_channels[i].enabled) {
                uint8_t cr = r, cg = g, cb = b;
                g_channels.aura_channels[i].ApplyCorrection(cr, cg, cb);
                SetAsusChannel(dev, channels[i].channel, channels[i].leds, cr, cg, cb);
            }
        }
    }

    hid_close(dev);
    hid_exit();
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

// Helper to get exe directory as narrow string
static std::string GetExeDirA() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string dir(path);
    size_t pos = dir.find_last_of("\\/");
    return pos != std::string::npos ? dir.substr(0, pos) : ".";
}

// Reset G.Skill RAM to a known state (turn off LEDs)
bool ResetGSkillRAM() {
    return SetGSkillRAM(0, 0, 0);  // Turn off all LEDs
}

bool SetGSkillRAM(uint8_t r, uint8_t g, uint8_t b) {
    std::string exeDir = GetExeDirA();

    // Try multiple paths for PawnIOLib.dll
    HMODULE dll = NULL;
    std::string dllPaths[] = {
        exeDir + "\\PawnIOLib.dll",
        exeDir + "\\dependencies\\PawnIO\\PawnIOLib.dll",
        "PawnIOLib.dll"
    };

    for (const auto& path : dllPaths) {
        dll = LoadLibraryA(path.c_str());
        if (dll) break;
    }

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

    // Try multiple paths for SmbusI801.bin
    HANDLE hFile = INVALID_HANDLE_VALUE;
    std::string binPaths[] = {
        exeDir + "\\SmbusI801.bin",
        exeDir + "\\modules\\SmbusI801.bin",
        exeDir + "\\dependencies\\PawnIO\\modules\\SmbusI801.bin",
        "SmbusI801.bin"
    };

    for (const auto& path : binPaths) {
        hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE) break;
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        AppendStatus(L"[G.Skill] SmbusI801.bin not found");
        p_close(handle);
        FreeLibrary(dll);
        return false;
    }

    DWORD size = GetFileSize(hFile, NULL);
    std::vector<uint8_t> blob(size);
    ReadFile(hFile, blob.data(), size, &size, NULL);
    CloseHandle(hFile);

    if (p_load(handle, blob.data(), blob.size()) != S_OK) {
        AppendStatus(L"[G.Skill] Failed to load SMBus module");
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

    AppendStatus(L"[G.Skill] No RAM modules found on SMBus");
    return false;
}

//=============================================================================
// SYSTEM POWER CONTROL (Standby, Shutdown, Restart)
//=============================================================================

bool EnableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    CloseHandle(hToken);

    return (GetLastError() == ERROR_SUCCESS);
}

// Global flag for resume detection
std::atomic<bool> g_resumeDetected{false};
std::atomic<bool> g_watcherRunning{true};

// Watchdog thread that detects resume by monitoring time jumps
void ResumeWatcherThread() {
    ULONGLONG lastTick = GetTickCount64();

    while (g_watcherRunning) {
        Sleep(1000);  // Check every second

        ULONGLONG currentTick = GetTickCount64();
        ULONGLONG elapsed = currentTick - lastTick;

        // If more than 5 seconds passed in what should be 1 second,
        // we likely just resumed from standby
        if (elapsed > 5000) {
            g_resumeDetected = true;
            // Post message to main window
            if (g_state.hWnd) {
                PostMessage(g_state.hWnd, WM_USER + 100, 0, 0);  // Custom resume message
            }
        }

        lastTick = currentTick;
    }
}

void SystemStandby() {
    AppendStatus(L"Initiating system standby...");
    SetSuspendState(FALSE, FALSE, FALSE);  // Standby (not hibernate)
}

void SystemShutdown() {
    if (EnableShutdownPrivilege()) {
        AppendStatus(L"Initiating system shutdown...");
        ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER);
    } else {
        AppendStatus(L"Failed to get shutdown privilege");
    }
}

void SystemRestart() {
    if (EnableShutdownPrivilege()) {
        AppendStatus(L"Initiating system restart...");
        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER);
    } else {
        AppendStatus(L"Failed to get shutdown privilege");
    }
}

//=============================================================================
// FULL SYSTEM RESET (after standby/resume)
//=============================================================================

void FullHIDReset() {
    AppendStatus(L"Resetting all RGB devices...");

    // === 1. Reset ASUS Aura (HID) ===
    hid_exit();
    Sleep(500);
    if (hid_init() != 0) {
        AppendStatus(L"[ERROR] HID init failed");
        return;
    }
    Sleep(200);

    // Open ASUS Aura device
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
        uint8_t buf[65];

        // Request Config Table (0xB0)
        memset(buf, 0, sizeof(buf));
        buf[0x00] = 0xEC;
        buf[0x01] = 0xB0;
        hid_write(dev, buf, 65);
        hid_read_timeout(dev, buf, 65, 500);
        Sleep(20);

        // SetGen1 - Required before Direct Mode
        memset(buf, 0, sizeof(buf));
        buf[0x00] = 0xEC;
        buf[0x01] = 0x52;
        buf[0x02] = 0x53;
        buf[0x03] = 0x00;
        buf[0x04] = 0x01;
        hid_write(dev, buf, 65);
        Sleep(50);

        // Switch all channels to Direct Mode (0x35 with mode 0xFF)
        for (int ch = 0; ch < 8; ch++) {
            memset(buf, 0, sizeof(buf));
            buf[0x00] = 0xEC;
            buf[0x01] = 0x35;
            buf[0x02] = ch;
            buf[0x03] = 0x00;
            buf[0x04] = 0x00;
            buf[0x05] = 0xFF;
            hid_write(dev, buf, 65);
            Sleep(5);
        }

        hid_close(dev);
        AppendStatus(L"ASUS Aura reset OK");
    } else {
        AppendStatus(L"[WARN] ASUS Aura not found");
    }

    hid_exit();

    // === 2. Reset G.Skill RAM (SMBus) ===
    // G.Skill RAM uses SMBus, not HID - reset separately
    if (g_state.enableRAM) {
        AppendStatus(L"Resetting G.Skill RAM...");
        // Brief reset pulse: turn off, wait, then ApplyColors will set correct color
        SetGSkillRAM(0, 0, 0);
        Sleep(100);
        AppendStatus(L"G.Skill RAM reset OK");
    }
}

//=============================================================================
// APPLY ALL COLORS
//=============================================================================

void ApplyColors() {
    if (g_state.applying.exchange(true)) return;

    // Copy values to local vars to avoid thread issues
    uint8_t r = g_state.red;
    uint8_t g = g_state.green;
    uint8_t b = g_state.blue;
    bool doAura = g_state.enableAura;
    bool doMouse = g_state.enableMouse;
    bool doKeyboard = g_state.enableKeyboard;
    bool doEdge = g_state.enableEdge;
    bool doRAM = g_state.enableRAM;
    int kbMode = g_state.kbMode;
    int brightness = g_state.brightness;
    int speed = g_state.speed;
    int edgeMode = g_state.edgeMode;
    bool dryRun = g_state.dryRun;

    ClearStatus();

    if (dryRun) {
        AppendStatus(L"=== DRY RUN MODE ===");
    }
    AppendStatus(L"=== Applying RGB Settings ===");

    wchar_t buf[128];
    swprintf(buf, 128, L"Color: #%02X%02X%02X", r, g, b);
    AppendStatus(buf);

    // Skip hardware communication in dry-run mode
    if (dryRun) {
        if (doAura) AppendStatus(L"[DRY] ASUS Aura: skipped");
        if (doMouse) AppendStatus(L"[DRY] SteelSeries: skipped");
        if (doKeyboard) {
            swprintf(buf, 128, L"[DRY] Keyboard mode %d: skipped", kbMode);
            AppendStatus(buf);
        }
        if (doEdge) AppendStatus(L"[DRY] Edge LEDs: skipped");
        if (doRAM) AppendStatus(L"[DRY] G.Skill RAM: skipped");
        AppendStatus(L"=== DRY RUN Complete ===");
        g_state.applying = false;
        return;
    }

    hid_init();

    // ASUS Aura - direct HID control
    if (doAura) {
        SetAsusAura(r, g, b);
    }
    if (doMouse) {
        SetSteelSeries(r, g, b);
    }
    if (doKeyboard) {
        SetEVisionKeyboard(r, g, b, kbMode, brightness, speed);
    }
    if (doEdge) {
        SetEVisionEdge(r, g, b, edgeMode);
    }
    if (doRAM) {
        SetGSkillRAM(r, g, b);
    }

    hid_exit();

    AppendStatus(L"=== Done! ===");

    g_state.applying = false;
}

//=============================================================================
// ASUS AURA TEST DIALOG - Visual channel testing with live feedback
//=============================================================================

#define ID_ASUS_TEST_BASE 6000
#define ID_ASUS_CH_CHECK_BASE 6100
#define ID_ASUS_CH_TEST_BASE 6200
#define ID_ASUS_CH_PICK_BASE 6250
#define ID_ASUS_TEST_ALL 6300
#define ID_ASUS_SCAN 6301
#define ID_ASUS_RESET 6302
#define ID_ASUS_CLOSE 6303

struct AsusTestDialog {
    HWND hDlg;
    HWND hStatus;
    HWND hFirmwareLabel;
    HWND hColorPreview[8];
    HWND hCheckBox[8];
    HWND hSliderR[8];
    HWND hSliderG[8];
    HWND hSliderB[8];
    HWND hLabelR[8];
    HWND hLabelG[8];
    HWND hLabelB[8];
    bool channelActive[8];
    uint8_t channelR[8];
    uint8_t channelG[8];
    uint8_t channelB[8];
    int numChannels;
};

AsusTestDialog* g_asusTest = nullptr;

// Test a single ASUS channel with a specific color
// 'channel' is the UI index (0, 1, 2...), we map it to the actual direct_channel
bool TestAsusChannel(int channel, uint8_t r, uint8_t g, uint8_t b) {
    hid_init();
    hid_device* dev = OpenAsusAura();
    if (!dev) {
        hid_exit();
        return false;
    }

    // Get LED count and direct channel from hardware config
    int ledCount = 120;
    int directChannel = channel;  // Default: use index as channel

    if (g_asusHwConfig.valid && channel < g_asusHwConfig.numChannels) {
        ledCount = g_asusHwConfig.channels[channel].ledCount;
        directChannel = g_asusHwConfig.channels[channel].directChannel;
    }

    SetAsusChannel(dev, directChannel, ledCount, r, g, b);

    hid_close(dev);
    hid_exit();
    return true;
}

// Color preview subclass
LRESULT CALLBACK AsusColorPreviewProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        int index = (int)dwRefData;
        COLORREF color = RGB(128, 128, 128);
        if (g_asusTest && index < g_asusTest->numChannels) {
            color = RGB(g_asusTest->channelR[index],
                       g_asusTest->channelG[index],
                       g_asusTest->channelB[index]);
        }

        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        // Border
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        DeleteObject(pen);

        EndPaint(hWnd, &ps);
        return 0;
    }
    else if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, AsusColorPreviewProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

INT_PTR CALLBACK AsusTestDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_asusTest = new AsusTestDialog();
        memset(g_asusTest, 0, sizeof(AsusTestDialog));
        g_asusTest->hDlg = hWnd;

        // Use scanned hardware config or fallback
        int numCh = g_asusHwConfig.valid ? g_asusHwConfig.numChannels : 3;
        if (numCh > 8) numCh = 8;
        if (numCh < 1) numCh = 1;
        g_asusTest->numChannels = numCh;

        // Load saved colors from hardware config
        for (int i = 0; i < numCh; i++) {
            if (g_asusHwConfig.valid && i < g_asusHwConfig.numChannels) {
                g_asusTest->channelR[i] = g_asusHwConfig.channels[i].colorR;
                g_asusTest->channelG[i] = g_asusHwConfig.channels[i].colorG;
                g_asusTest->channelB[i] = g_asusHwConfig.channels[i].colorB;
                g_asusTest->channelActive[i] = g_asusHwConfig.channels[i].enabled;
            } else {
                g_asusTest->channelR[i] = g_state.red;
                g_asusTest->channelG[i] = g_state.green;
                g_asusTest->channelB[i] = g_state.blue;
                g_asusTest->channelActive[i] = true;
            }
        }

        // Title with firmware info
        wchar_t title[128];
        if (g_asusHwConfig.valid) {
            wchar_t fw[32];
            MultiByteToWideChar(CP_UTF8, 0, g_asusHwConfig.firmware, -1, fw, 32);
            swprintf(title, 128, L"ASUS Aura - %s (%d Kan\x00E4le)", fw, numCh);
        } else {
            wcscpy(title, L"ASUS Aura Kanalsteuerung");
        }
        CreateWindowW(L"STATIC", title,
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 8, 460, 20, hWnd, NULL, NULL, NULL);

        int y = 35;
        for (int i = 0; i < numCh; i++) {
            // Build channel name from hardware config
            wchar_t chName[128];
            if (g_asusHwConfig.valid && i < g_asusHwConfig.numChannels) {
                wchar_t name[64];
                MultiByteToWideChar(CP_ACP, 0, g_asusHwConfig.channels[i].name, -1, name, 64);
                swprintf(chName, 128, L"Kanal %d - %s", i, name);
            } else {
                swprintf(chName, 128, L"Kanal %d", i);
            }

            // Channel header with checkbox
            g_asusTest->hCheckBox[i] = CreateWindowW(L"BUTTON", chName,
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                15, y, 350, 20, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_CHECK_BASE + i), NULL, NULL);
            SendMessage(g_asusTest->hCheckBox[i], BM_SETCHECK,
                g_asusTest->channelActive[i] ? BST_CHECKED : BST_UNCHECKED, 0);

            // Color preview
            g_asusTest->hColorPreview[i] = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                420, y + 25, 50, 50, hWnd, NULL, NULL, NULL);
            SetWindowSubclass(g_asusTest->hColorPreview[i], AsusColorPreviewProc, 0, (DWORD_PTR)i);

            // Prepare value labels
            wchar_t valR[8], valG[8], valB[8];
            swprintf(valR, 8, L"%d", g_asusTest->channelR[i]);
            swprintf(valG, 8, L"%d", g_asusTest->channelG[i]);
            swprintf(valB, 8, L"%d", g_asusTest->channelB[i]);

            // R slider row
            CreateWindowW(L"STATIC", L"R:", WS_CHILD | WS_VISIBLE,
                25, y + 25, 20, 20, hWnd, NULL, NULL, NULL);
            g_asusTest->hSliderR[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                50, y + 22, 300, 25, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_TEST_BASE + i * 10 + 0), NULL, NULL);
            SendMessage(g_asusTest->hSliderR[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
            SendMessage(g_asusTest->hSliderR[i], TBM_SETPOS, TRUE, g_asusTest->channelR[i]);
                        g_asusTest->hLabelR[i] = CreateWindowW(L"STATIC", valR,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                355, y + 25, 35, 18, hWnd, NULL, NULL, NULL);

            // G slider row
            CreateWindowW(L"STATIC", L"G:", WS_CHILD | WS_VISIBLE,
                25, y + 50, 20, 20, hWnd, NULL, NULL, NULL);
            g_asusTest->hSliderG[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                50, y + 47, 300, 25, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_TEST_BASE + i * 10 + 1), NULL, NULL);
            SendMessage(g_asusTest->hSliderG[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
            SendMessage(g_asusTest->hSliderG[i], TBM_SETPOS, TRUE, g_asusTest->channelG[i]);
                        g_asusTest->hLabelG[i] = CreateWindowW(L"STATIC", valG,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                355, y + 50, 35, 18, hWnd, NULL, NULL, NULL);

            // B slider row
            CreateWindowW(L"STATIC", L"B:", WS_CHILD | WS_VISIBLE,
                25, y + 75, 20, 20, hWnd, NULL, NULL, NULL);
            g_asusTest->hSliderB[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                50, y + 72, 300, 25, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_TEST_BASE + i * 10 + 2), NULL, NULL);
            SendMessage(g_asusTest->hSliderB[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
                        SendMessage(g_asusTest->hSliderB[i], TBM_SETPOS, TRUE, g_asusTest->channelB[i]);
            g_asusTest->hLabelB[i] = CreateWindowW(L"STATIC", valB,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                355, y + 75, 35, 18, hWnd, NULL, NULL, NULL);

            y += 105;
        }

        // Action buttons
        int btnY = y + 10;
        CreateWindowW(L"BUTTON", L"Alle anwenden",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            15, btnY, 110, 28, hWnd, (HMENU)ID_ASUS_TEST_ALL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Alle aus",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            135, btnY, 80, 28, hWnd, (HMENU)ID_ASUS_RESET, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Schliessen",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            390, btnY, 80, 28, hWnd, (HMENU)ID_ASUS_CLOSE, NULL, NULL);

        // Status bar
        g_asusTest->hStatus = CreateWindowW(L"STATIC",
            L"Slider bewegen um Farben live anzupassen",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, btnY + 35, 450, 20, hWnd, NULL, NULL, NULL);

        return TRUE;
    }

    case WM_HSCROLL: {
        HWND hSlider = (HWND)lParam;
        if (!g_asusTest) break;

        // Find which channel and color component this slider belongs to
        for (int ch = 0; ch < g_asusTest->numChannels; ch++) {
            bool updated = false;
            wchar_t val[8];

            if (hSlider == g_asusTest->hSliderR[ch]) {
                g_asusTest->channelR[ch] = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                swprintf(val, 8, L"%d", g_asusTest->channelR[ch]);
                SetWindowTextW(g_asusTest->hLabelR[ch], val);
                updated = true;
            }
            else if (hSlider == g_asusTest->hSliderG[ch]) {
                g_asusTest->channelG[ch] = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                swprintf(val, 8, L"%d", g_asusTest->channelG[ch]);
                SetWindowTextW(g_asusTest->hLabelG[ch], val);
                updated = true;
            }
            else if (hSlider == g_asusTest->hSliderB[ch]) {
                g_asusTest->channelB[ch] = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                swprintf(val, 8, L"%d", g_asusTest->channelB[ch]);
                SetWindowTextW(g_asusTest->hLabelB[ch], val);
                updated = true;
            }

            if (updated) {
                // Update color preview
                InvalidateRect(g_asusTest->hColorPreview[ch], NULL, TRUE);

                // Live apply with debouncing (150ms delay)
                static int s_pendingChannel = -1;
                if (g_asusTest->channelActive[ch]) {
                    s_pendingChannel = ch;
                    KillTimer(hWnd, ID_TIMER_DEBOUNCE);
                    SetTimer(hWnd, ID_TIMER_DEBOUNCE, 150, NULL);
                }

                wchar_t buf[64];
                swprintf(buf, 64, L"Kanal %d: RGB(%d, %d, %d)", ch,
                        g_asusTest->channelR[ch], g_asusTest->channelG[ch], g_asusTest->channelB[ch]);
                SetWindowTextW(g_asusTest->hStatus, buf);
                break;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int numCh = g_asusTest ? g_asusTest->numChannels : 3;

        // Checkbox changed
        if (id >= ID_ASUS_CH_CHECK_BASE && id < ID_ASUS_CH_CHECK_BASE + numCh) {
            int ch = id - ID_ASUS_CH_CHECK_BASE;
            g_asusTest->channelActive[ch] = (SendMessage(g_asusTest->hCheckBox[ch], BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_channels.aura_channels[ch].enabled = g_asusTest->channelActive[ch];

            // Apply immediately if enabled
            if (g_asusTest->channelActive[ch]) {
                TestAsusChannel(ch, g_asusTest->channelR[ch],
                               g_asusTest->channelG[ch],
                               g_asusTest->channelB[ch]);
            }

            wchar_t buf[64];
            swprintf(buf, 64, L"Kanal %d %s", ch, g_asusTest->channelActive[ch] ? L"aktiv" : L"aus");
            SetWindowTextW(g_asusTest->hStatus, buf);
        }
        else if (id == ID_ASUS_TEST_ALL) {
            hid_init();
            hid_device* dev = OpenAsusAura();
            if (dev) {
                int count = 0;

                for (int i = 0; i < numCh; i++) {
                    if (g_asusTest->channelActive[i]) {
                        int leds = 120;
                        int directCh = i;
                        if (g_asusHwConfig.valid && i < g_asusHwConfig.numChannels) {
                            leds = g_asusHwConfig.channels[i].ledCount;
                            directCh = g_asusHwConfig.channels[i].directChannel;
                        }
                        SetAsusChannel(dev, directCh, leds,
                                      g_asusTest->channelR[i],
                                      g_asusTest->channelG[i],
                                      g_asusTest->channelB[i]);
                        count++;
                    }
                }

                hid_close(dev);
                wchar_t buf[64];
                swprintf(buf, 64, L"%d Kan\x00E4le angewendet", count);
                SetWindowTextW(g_asusTest->hStatus, buf);
            } else {
                SetWindowTextW(g_asusTest->hStatus, L"Ger\x00E4t nicht gefunden");
            }
            hid_exit();
        }
        else if (id == ID_ASUS_RESET) {
            hid_init();
            hid_device* dev = OpenAsusAura();
            if (dev) {
                for (int i = 0; i < numCh; i++) {
                    int leds = 120;
                    int directCh = i;
                    if (g_asusHwConfig.valid && i < g_asusHwConfig.numChannels) {
                        leds = g_asusHwConfig.channels[i].ledCount;
                        directCh = g_asusHwConfig.channels[i].directChannel;
                    }
                    SetAsusChannel(dev, directCh, leds, 0, 0, 0);
                    g_asusTest->channelR[i] = 0;
                    g_asusTest->channelG[i] = 0;
                    g_asusTest->channelB[i] = 0;
                    SendMessage(g_asusTest->hSliderR[i], TBM_SETPOS, TRUE, 0);
                    SendMessage(g_asusTest->hSliderG[i], TBM_SETPOS, TRUE, 0);
                    SendMessage(g_asusTest->hSliderB[i], TBM_SETPOS, TRUE, 0);
                    SetWindowTextW(g_asusTest->hLabelR[i], L"0");
                    SetWindowTextW(g_asusTest->hLabelG[i], L"0");
                    SetWindowTextW(g_asusTest->hLabelB[i], L"0");
                    InvalidateRect(g_asusTest->hColorPreview[i], NULL, TRUE);
                }
                hid_close(dev);
                SetWindowTextW(g_asusTest->hStatus, L"Alle Kan\x00E4le aus");
            }
            hid_exit();
        }
        else if (id == ID_ASUS_CLOSE || id == IDCANCEL) {
            // Save channel colors and states to hardware config
            for (int i = 0; i < numCh; i++) {
                if (g_asusHwConfig.valid && i < g_asusHwConfig.numChannels) {
                    g_asusHwConfig.channels[i].colorR = g_asusTest->channelR[i];
                    g_asusHwConfig.channels[i].colorG = g_asusTest->channelG[i];
                    g_asusHwConfig.channels[i].colorB = g_asusTest->channelB[i];
                    g_asusHwConfig.channels[i].enabled = g_asusTest->channelActive[i];
                }
                g_channels.aura_channels[i].enabled = g_asusTest->channelActive[i];
            }
            SaveAsusHardwareConfig();
            g_channels.Save();

            delete g_asusTest;
            g_asusTest = nullptr;
            EndDialog(hWnd, IDOK);
        }
        break;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            // Apply pending channel change
            for (int ch = 0; ch < g_asusTest->numChannels; ch++) {
                if (g_asusTest->channelActive[ch]) {
                    TestAsusChannel(ch, g_asusTest->channelR[ch],
                                   g_asusTest->channelG[ch],
                                   g_asusTest->channelB[ch]);
                }
            }
        }
        break;

    case WM_CLOSE:
        SendMessage(hWnd, WM_COMMAND, ID_ASUS_CLOSE, 0);
        return TRUE;
    }

    return FALSE;
}

void ShowAsusTestDialog(HWND hParent) {
    // Calculate dialog height based on number of channels
    int numCh = g_asusHwConfig.valid ? g_asusHwConfig.numChannels : 3;
    if (numCh > 8) numCh = 8;
    if (numCh < 1) numCh = 1;

    // Height: ~70 dialog units per channel + 50 for header/buttons
    int dlgHeight = 50 + numCh * 55;

    struct {
        DLGTEMPLATE tmpl;
        WORD menu;
        WORD wndClass;
        WCHAR title[32];
    } dlgData = {};

    dlgData.tmpl.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER;
    dlgData.tmpl.cx = 260;  // Dialog units - wider for RGB sliders
    dlgData.tmpl.cy = dlgHeight;
    dlgData.menu = 0;
    dlgData.wndClass = 0;
    wcscpy_s(dlgData.title, L"ASUS Aura Kanalsteuerung");

    DialogBoxIndirectW(GetModuleHandle(NULL), &dlgData.tmpl, hParent, AsusTestDlgProc);
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

        CreateWindowW(L"STATIC", g_str->csHint,
            WS_CHILD | WS_VISIBLE, 15, 45, 400, 18, hWnd, NULL, NULL, NULL);

        CreateChannelTabContent(hWnd, 0);

        CreateWindowW(L"BUTTON", g_str->csSaveClose,
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            200, 320, 120, 30, hWnd, (HMENU)ID_CS_SAVE, NULL, NULL);
        CreateWindowW(L"BUTTON", g_str->csResetAll,
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

                // Live update with debouncing (150ms delay)
                if (g_csDlg->currentTab == 0) {
                    KillTimer(hWnd, ID_TIMER_DEBOUNCE);
                    SetTimer(hWnd, ID_TIMER_DEBOUNCE, 150, NULL);
                }
            }
        }
        break;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            if (g_csDlg && g_csDlg->currentTab == 0) {
                SetAsusAuraQuick(g_state.red, g_state.green, g_state.blue);
            }
        }
        break;

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
            if (cfg) {
                cfg->enabled = checked;
                // Live update on checkbox change
                if (g_csDlg->currentTab == 0) {
                    SetAsusAuraQuick(g_state.red, g_state.green, g_state.blue);
                }
            }
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
    wcscpy_s(dlgData.title, g_str->csTitle);

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
    g_state.nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));  // Custom icon
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

    // Quick colors submenu
    HMENU hColorMenu = CreatePopupMenu();
    AppendMenuW(hColorMenu, MF_STRING, ID_TRAY_BLUE, L"Blue");
    AppendMenuW(hColorMenu, MF_STRING, ID_TRAY_RED, L"Red");
    AppendMenuW(hColorMenu, MF_STRING, ID_TRAY_GREEN, L"Green");
    AppendMenuW(hColorMenu, MF_STRING, ID_TRAY_WHITE, L"White");
    AppendMenuW(hColorMenu, MF_STRING, ID_TRAY_OFF, L"Off");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorMenu, L"Quick Colors");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Power submenu
    HMENU hPowerMenu = CreatePopupMenu();
    AppendMenuW(hPowerMenu, MF_STRING, ID_TRAY_STANDBY, L"Standby");
    AppendMenuW(hPowerMenu, MF_STRING, ID_TRAY_SHUTDOWN, L"Shutdown");
    AppendMenuW(hPowerMenu, MF_STRING, ID_TRAY_RESTART, L"Restart");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPowerMenu, L"Power");

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
GroupRect g_groups[5];
int g_numGroups = 0;

// Helper to draw modern group box
void DrawThemedGroupBox(HDC hdc, const GroupRect& g) {
    Gdiplus::Graphics gfx(hdc);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    gfx.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);  // Crisp lines

    // Use 0.5f offset for crisp 1px lines
    float x = (float)g.x + 0.5f;
    float y = (float)g.y + 0.5f;
    float w = (float)g.w - 1.0f;
    float h = (float)g.h - 1.0f;
    float radius = 8.0f;

    // Rounded rectangle path
    Gdiplus::GraphicsPath path;
    float d = radius * 2;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();

    // Semi-transparent fill
    Gdiplus::SolidBrush fillBrush(Gdiplus::Color(20, 255, 255, 255));
    gfx.FillPath(&fillBrush, &path);

    // Border - consistent 1px line
    Gdiplus::Pen borderPen(Gdiplus::Color(80, 100, 160, 220), 1.0f);
    borderPen.SetAlignment(Gdiplus::PenAlignmentCenter);
    gfx.DrawPath(&borderPen, &path);

    // Title badge
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

    // Measure text for badge size
    Gdiplus::RectF textBounds;
    gfx.MeasureString(g.title, -1, &font, Gdiplus::PointF(0, 0), &textBounds);

    float badgeX = (float)g.x + GROUP_PADDING;  // Same as left padding
    float badgeY = (float)g.y + GROUP_PADDING; // Same as top padding
    float badgeW = textBounds.Width + 18;
    float badgeH = 20;
    float badgeR = 5.0f;

    // Badge rounded rect
    Gdiplus::GraphicsPath badgePath;
    float bd = badgeR * 2;
    badgePath.AddArc(badgeX, badgeY, bd, bd, 180, 90);
    badgePath.AddArc(badgeX + badgeW - bd, badgeY, bd, bd, 270, 90);
    badgePath.AddArc(badgeX + badgeW - bd, badgeY + badgeH - bd, bd, bd, 0, 90);
    badgePath.AddArc(badgeX, badgeY + badgeH - bd, bd, bd, 90, 90);
    badgePath.CloseFigure();

    // Badge fill (subtle gradient feel)
    Gdiplus::SolidBrush badgeFill(Gdiplus::Color(255, 35, 80, 140));
    gfx.FillPath(&badgeFill, &badgePath);

    // Badge border
    Gdiplus::Pen badgeBorderPen(Gdiplus::Color(255, 60, 120, 200), 1.0f);
    gfx.DrawPath(&badgeBorderPen, &badgePath);

    // Title text (centered in badge)
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 180, 220, 255));
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::RectF badgeRect(badgeX, badgeY, badgeW, badgeH);
    gfx.DrawString(g.title, -1, &font, badgeRect, &format, &textBrush);
}

// Modern button drawing helper
void DrawModernButton(HDC hdc, RECT* rc, const wchar_t* text, bool isHovered, bool isPressed, bool isAccent = false) {
    Gdiplus::Graphics gfx(hdc);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    // Inset by 2px so border (1px pen width) isn't clipped at control edges
    float inset = 2.0f;
    float x = (float)rc->left + inset;
    float y = (float)rc->top + inset;
    float w = (float)(rc->right - rc->left) - inset * 2;
    float h = (float)(rc->bottom - rc->top) - inset * 2;
    float radius = 4.0f;  // Slightly smaller radius for better fit

    // Rounded rectangle path
    Gdiplus::GraphicsPath path;
    float d = radius * 2;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();

    // Background color based on state
    Gdiplus::Color bgColor;
    Gdiplus::Color borderColor;
    Gdiplus::Color textColor;

    if (isAccent) {
        // Accent button (Apply, etc.)
        if (isPressed) {
            bgColor = Gdiplus::Color(255, 30, 100, 180);
            borderColor = Gdiplus::Color(255, 50, 130, 220);
        } else if (isHovered) {
            bgColor = Gdiplus::Color(255, 50, 130, 220);
            borderColor = Gdiplus::Color(255, 80, 160, 255);
        } else {
            bgColor = Gdiplus::Color(255, 40, 110, 200);
            borderColor = Gdiplus::Color(255, 60, 140, 230);
        }
        textColor = Gdiplus::Color(255, 255, 255, 255);
    } else {
        // Normal button
        if (isPressed) {
            bgColor = Gdiplus::Color(255, 50, 55, 70);
            borderColor = Gdiplus::Color(255, 80, 140, 200);
        } else if (isHovered) {
            bgColor = Gdiplus::Color(255, 55, 60, 80);
            borderColor = Gdiplus::Color(255, 100, 160, 220);
        } else {
            bgColor = Gdiplus::Color(255, 40, 45, 60);
            borderColor = Gdiplus::Color(255, 70, 80, 100);
        }
        textColor = Gdiplus::Color(255, 220, 225, 235);
    }

    // Fill
    Gdiplus::SolidBrush fillBrush(bgColor);
    gfx.FillPath(&fillBrush, &path);

    // Border
    Gdiplus::Pen borderPen(borderColor, 1.0f);
    gfx.DrawPath(&borderPen, &path);

    // Text
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(textColor);

    Gdiplus::RectF textRect(x, y, w, h);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    gfx.DrawString(text, -1, &font, textRect, &format, &textBrush);
}

// Draw modern checkbox with badge-style background
void DrawModernCheckbox(HDC hdc, RECT* rc, const wchar_t* text, bool isChecked, bool isHovered) {
    Gdiplus::Graphics gfx(hdc);
    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

    int w = rc->right - rc->left;
    int h = rc->bottom - rc->top;

    // Badge background (rounded rect for entire control) - no rectangular clear!
    int padding = 2;
    float badgeRadius = 5.0f;
    Gdiplus::GraphicsPath badgePath;
    float bd = badgeRadius * 2;
    badgePath.AddArc((float)padding, (float)padding, bd, bd, 180, 90);
    badgePath.AddArc((float)(w - padding) - bd, (float)padding, bd, bd, 270, 90);
    badgePath.AddArc((float)(w - padding) - bd, (float)(h - padding) - bd, bd, bd, 0, 90);
    badgePath.AddArc((float)padding, (float)(h - padding) - bd, bd, bd, 90, 90);
    badgePath.CloseFigure();

    // Badge fill - same colors as buttons
    Gdiplus::Color badgeBg = isHovered ?
        Gdiplus::Color(255, 55, 60, 80) :   // Hover: same as button bg hover
        Gdiplus::Color(255, 40, 45, 60);    // Normal: same as button bg
    Gdiplus::SolidBrush badgeBrush(badgeBg);
    gfx.FillPath(&badgeBrush, &badgePath);

    // Badge border - same colors as buttons
    Gdiplus::Color badgeBorderColor = isHovered ?
        Gdiplus::Color(255, 100, 160, 220) :  // Hover: same as button border hover
        Gdiplus::Color(255, 70, 80, 100);     // Normal: same as button border
    Gdiplus::Pen badgeBorder(badgeBorderColor, 1.0f);
    gfx.DrawPath(&badgeBorder, &badgePath);

    // Checkbox square
    int boxSize = 14;
    int boxX = padding + 8;
    int boxY = (h - boxSize) / 2;

    Gdiplus::GraphicsPath boxPath;
    float radius = 3.0f;
    float d = radius * 2;
    boxPath.AddArc((float)boxX, (float)boxY, d, d, 180, 90);
    boxPath.AddArc((float)boxX + boxSize - d, (float)boxY, d, d, 270, 90);
    boxPath.AddArc((float)boxX + boxSize - d, (float)boxY + boxSize - d, d, d, 0, 90);
    boxPath.AddArc((float)boxX, (float)boxY + boxSize - d, d, d, 90, 90);
    boxPath.CloseFigure();

    // Checkbox background
    Gdiplus::Color boxBg = isChecked ?
        Gdiplus::Color(255, 50, 130, 210) : Gdiplus::Color(255, 30, 35, 48);
    Gdiplus::SolidBrush boxBrush(boxBg);
    gfx.FillPath(&boxBrush, &boxPath);

    // Checkbox border
    Gdiplus::Color boxBorder = isChecked ?
        Gdiplus::Color(255, 80, 160, 240) : Gdiplus::Color(255, 70, 80, 100);
    Gdiplus::Pen boxPen(boxBorder, 1.0f);
    gfx.DrawPath(&boxPen, &boxPath);

    // Checkmark
    if (isChecked) {
        Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
        checkPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        int cx = boxX + boxSize / 2;
        int cy = boxY + boxSize / 2;
        Gdiplus::Point pts[3] = {
            {cx - 3, cy},
            {cx - 1, cy + 3},
            {cx + 4, cy - 3}
        };
        gfx.DrawLines(&checkPen, pts, 3);
    }

    // Text with more padding
    int textX = boxX + boxSize + 10;
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 220, 225, 235));
    Gdiplus::RectF textRect((float)textX, 0.0f, (float)(w - textX - 6), (float)h);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentNear);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);  // Prevent text wrapping
    gfx.DrawString(text, -1, &font, textRect, &format, &textBrush);
}

// Track button hover states
std::map<HWND, bool> g_buttonHover;
std::map<HWND, bool> g_buttonPressed;
std::map<HWND, bool> g_checkboxHover;
std::map<HWND, bool> g_checkboxChecked;
std::map<HWND, bool> g_labelHover;
std::map<HWND, bool> g_comboHover;

// Button subclass procedure for hover tracking
LRESULT CALLBACK ModernButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_MOUSEMOVE: {
            if (!g_buttonHover[hWnd]) {
                g_buttonHover[hWnd] = true;
                // Request mouse leave tracking
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            g_buttonHover[hWnd] = false;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_NCDESTROY: {
            g_buttonHover.erase(hWnd);
            g_buttonPressed.erase(hWnd);
            RemoveWindowSubclass(hWnd, ModernButtonProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Forward declarations
void ApplyModernCheckboxStyle(HWND hCheck);

// Helper to create a modern owner-draw button with hover tracking
HWND CreateModernButton(LPCWSTR text, DWORD style, int x, int y, int w, int h, HWND parent, int id) {
    HWND hBtn = CreateWindowW(L"BUTTON", text,
        style | BS_OWNERDRAW,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    if (hBtn) {
        SetWindowSubclass(hBtn, ModernButtonProc, 0, 0);
    }
    return hBtn;
}

// Helper to create modern checkbox with auto-calculated width based on text
// Returns the created HWND and sets outWidth to the calculated width
HWND CreateModernCheckbox(LPCWSTR text, int x, int y, int h, HWND parent, int id, int* outWidth) {
    // Calculate text width
    HDC hdc = GetDC(parent);
    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &textSize);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(parent, hdc);

    // Width = padding(10) + checkbox(14) + gap(10) + text + padding(16) + extra(8)
    int width = 10 + 14 + 10 + textSize.cx + 16 + 8;
    if (outWidth) *outWidth = width;

    HWND hCheck = CreateWindowW(L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, width, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);

    if (hCheck) {
        ApplyModernCheckboxStyle(hCheck);
    }
    return hCheck;
}

// Checkbox subclass for owner-draw behavior
LRESULT CALLBACK ModernCheckboxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case BM_GETCHECK: {
            // Return our tracked state
            return g_checkboxChecked[hWnd] ? BST_CHECKED : BST_UNCHECKED;
        }
        case BM_SETCHECK: {
            // Set our tracked state
            g_checkboxChecked[hWnd] = (wParam == BST_CHECKED);
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }
        case WM_ERASEBKGND: {
            // Copy parent background to simulate transparency
            HWND hParent = GetParent(hWnd);
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);

            // Get position relative to parent
            POINT pt = {0, 0};
            MapWindowPoints(hWnd, hParent, &pt, 1);

            // Get parent DC and copy background
            HDC hParentDC = GetDC(hParent);
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hParentDC, pt.x, pt.y, SRCCOPY);
            ReleaseDC(hParent, hParentDC);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            wchar_t text[64] = {0};
            GetWindowTextW(hWnd, text, 64);
            bool isChecked = g_checkboxChecked[hWnd];
            bool isHovered = g_checkboxHover[hWnd];

            DrawModernCheckbox(hdc, &rc, text, isChecked, isHovered);

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!g_checkboxHover[hWnd]) {
                g_checkboxHover[hWnd] = true;
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            g_checkboxHover[hWnd] = false;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_LBUTTONDOWN: {
            SetCapture(hWnd);
            break;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
            // Check if mouse is still over the control
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            RECT rc;
            GetClientRect(hWnd, &rc);
            if (PtInRect(&rc, pt)) {
                // Toggle state
                g_checkboxChecked[hWnd] = !g_checkboxChecked[hWnd];
                InvalidateRect(hWnd, NULL, TRUE);
                // Notify parent of state change (send BN_CLICKED)
                SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), (LPARAM)hWnd);
            }
            return 0;
        }
        case WM_NCDESTROY: {
            g_checkboxHover.erase(hWnd);
            g_checkboxChecked.erase(hWnd);
            RemoveWindowSubclass(hWnd, ModernCheckboxProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Helper to apply modern checkbox style
void ApplyModernCheckboxStyle(HWND hCheck) {
    g_checkboxChecked[hCheck] = false; // Initialize unchecked
    SetWindowSubclass(hCheck, ModernCheckboxProc, 0, 0);
}

// Combobox subclass for modern border styling with hover
LRESULT CALLBACK ModernComboProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_PAINT: {
            // Let default paint happen first
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Then draw our border on top
            HDC hdc = GetDC(hWnd);
            RECT rc;
            GetClientRect(hWnd, &rc);

            bool isHovered = g_comboHover[hWnd];

            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            // Draw rounded border - brighter on hover
            float radius = 4.0f;
            float x = 0.5f, y = 0.5f;
            float w = (float)(rc.right - rc.left) - 1.0f;
            float h = (float)(rc.bottom - rc.top) - 1.0f;

            Gdiplus::GraphicsPath path;
            float d = radius * 2;
            path.AddArc(x, y, d, d, 180, 90);
            path.AddArc(x + w - d, y, d, d, 270, 90);
            path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
            path.AddArc(x, y + h - d, d, d, 90, 90);
            path.CloseFigure();

            // Border color (same as buttons)
            Gdiplus::Color borderColor = isHovered ?
                Gdiplus::Color(255, 100, 160, 220) :  // Hover: same as button border hover
                Gdiplus::Color(255, 70, 80, 100);     // Normal: same as button border
            Gdiplus::Pen borderPen(borderColor, 1.0f);
            g.DrawPath(&borderPen, &path);

            ReleaseDC(hWnd, hdc);
            return result;
        }
        case WM_MOUSEMOVE: {
            if (!g_comboHover[hWnd]) {
                g_comboHover[hWnd] = true;
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            g_comboHover[hWnd] = false;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_NCDESTROY: {
            g_comboHover.erase(hWnd);
            RemoveWindowSubclass(hWnd, ModernComboProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Helper to apply modern combobox style
void ApplyModernComboStyle(HWND hCombo) {
    SetWindowSubclass(hCombo, ModernComboProc, 0, 0);
}

// Label subclass for hover effect
LRESULT CALLBACK ModernLabelProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            wchar_t text[128] = {0};
            GetWindowTextW(hWnd, text, 128);
            bool isHovered = g_labelHover[hWnd];

            // Draw with GDI+
            Gdiplus::Graphics gfx(hdc);
            gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

            // Text color changes on hover (same colors as buttons)
            Gdiplus::Color textColor = isHovered ?
                Gdiplus::Color(255, 100, 160, 220) :  // Hover: same as button border hover
                Gdiplus::Color(255, 220, 225, 235);   // Normal: same as button text

            Gdiplus::FontFamily fontFamily(L"Segoe UI");
            Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush textBrush(textColor);
            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentNear);
            format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

            Gdiplus::RectF textRect((float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top));
            gfx.DrawString(text, -1, &font, textRect, &format, &textBrush);

            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: {
            // Copy parent background
            HWND hParent = GetParent(hWnd);
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            POINT pt = {0, 0};
            MapWindowPoints(hWnd, hParent, &pt, 1);
            HDC hParentDC = GetDC(hParent);
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hParentDC, pt.x, pt.y, SRCCOPY);
            ReleaseDC(hParent, hParentDC);
            return 1;
        }
        case WM_MOUSEMOVE: {
            if (!g_labelHover[hWnd]) {
                g_labelHover[hWnd] = true;
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        }
        case WM_MOUSELEAVE: {
            g_labelHover[hWnd] = false;
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case WM_NCDESTROY: {
            g_labelHover.erase(hWnd);
            RemoveWindowSubclass(hWnd, ModernLabelProc, uIdSubclass);
            break;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Helper to apply modern label style with hover
void ApplyModernLabelStyle(HWND hLabel) {
    SetWindowSubclass(hLabel, ModernLabelProc, 0, 0);
}

// Helper to create a modern label with hover effect
HWND CreateModernLabel(LPCWSTR text, int x, int y, int w, int h, HWND parent) {
    HWND hLabel = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, NULL, NULL, NULL);
    if (hLabel) {
        ApplyModernLabelStyle(hLabel);
    }
    return hLabel;
}

// No extra border drawing needed - use clean WS_BORDER styling

// GDI+ token
ULONG_PTR g_gdiplusToken = 0;

// Load logo PNG from exe directory - keep as GDI+ Image for proper rendering
void LoadLogoBitmap() {
    // Cleanup old
    if (g_pLogoImage) {
        delete g_pLogoImage;
        g_pLogoImage = NULL;
    }
    if (g_hLogoBitmap) {
        DeleteObject(g_hLogoBitmap);
        g_hLogoBitmap = NULL;
    }

    // Get path to icon.png next to exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat(exePath, L"icon.png");

    // Load PNG with GDI+ and keep as Image (not HBITMAP)
    g_pLogoImage = Gdiplus::Image::FromFile(exePath);
    if (!g_pLogoImage || g_pLogoImage->GetLastStatus() != Gdiplus::Ok) {
        // Try src folder
        if (g_pLogoImage) delete g_pLogoImage;
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *(lastSlash + 1) = L'\0';
        wcscat(exePath, L"..\\src\\icon.png");
        g_pLogoImage = Gdiplus::Image::FromFile(exePath);
    }

    if (!g_pLogoImage || g_pLogoImage->GetLastStatus() != Gdiplus::Ok) {
        if (g_pLogoImage) {
            delete g_pLogoImage;
            g_pLogoImage = NULL;
        }
        return;
    }

    g_logoWidth = g_pLogoImage->GetWidth();
    g_logoHeight = g_pLogoImage->GetHeight();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Create theme brushes
        g_hBgBrush = CreateSolidBrush(g_theme->bgWindow);
        g_hCtrlBrush = CreateSolidBrush(g_theme->bgControl);
        g_hBtnBrush = CreateSolidBrush(g_theme->bgButton);
        InitTrackbarBrush();

        // Load logo
        LoadLogoBitmap();

        // Register preview class
        WNDCLASS wc = {0};
        wc.lpfnWndProc = PreviewProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"ColorPreview";
        wc.hbrBackground = g_hBgBrush;
        RegisterClass(&wc);

        // Layout variables - mathematically correct
        const int M = MARGIN;           // 12 - outer margin from window edge
        const int GM = GROUP_MARGIN;    // 8 - space between groups
        const int GP = GROUP_PADDING;   // 10 - padding inside group box
        const int GTH = GROUP_TITLE_H;  // 18 - group title height
        const int IS = ITEM_SPACING;    // 6 - vertical item spacing
        const int IHS = ITEM_H_SPACING; // 8 - horizontal item spacing

        // Window client area = WINDOW_WIDTH (620)
        // Group box width = WINDOW_WIDTH - 2*MARGIN = 620 - 24 = 596
        const int CW = WINDOW_WIDTH - M * 2;  // 531 = Content/Group width
        // Inner content width = CW - 2*GP = 531 - 20 = 511
        const int ICW = CW - GP * 2;  // 511 = usable width inside group

        // Start content at top margin (standard Windows titlebar)
        int y = M;
        int x = M;  // x = left edge of group boxes
        int gy;  // Group content y start

        // ═══════════════════════════════════════════════════════════════
        // GROUP 1: COLOR
        // ═══════════════════════════════════════════════════════════════
        int g1y = y;
        gy = y + GTH + GP;  // Content starts after title badge + padding

        // Row 1: Preview + Hex + Pick + Theme toggle
        g_state.hPreview = CreateWindowW(L"ColorPreview", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            x + GP, gy, 70, 60, hWnd, (HMENU)ID_STATIC_PREVIEW, NULL, NULL);

        CreateModernLabel(g_str->hex, x + GP + 80, gy + 6, 32, BTN_H, hWnd);
        g_state.hEditHex = CreateWindowW(L"EDIT", L"#0022FF",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE | ES_CENTER,
            x + GP + 112, gy + 4, 76, BTN_H, hWnd, (HMENU)ID_EDIT_HEX, NULL, NULL);

        CreateModernButton(g_str->pick, WS_CHILD | WS_VISIBLE,
            x + GP + 196, gy + 4, 60, BTN_H, hWnd, ID_BTN_PICK_COLOR);

        // Right-aligned buttons with proper sizing
        int rightEdge = x + GP + ICW;  // right edge of content area
        CreateModernButton(g_str->theme, WS_CHILD | WS_VISIBLE,
            rightEdge - 106, gy + 4, 60, BTN_H, hWnd, ID_BTN_THEME);

        CreateModernButton(g_lang == LANG_EN ? L"DE" : L"EN", WS_CHILD | WS_VISIBLE,
            rightEdge - 42, gy + 4, 42, BTN_H, hWnd, ID_BTN_LANG);

        // Row 2: 7 Color presets - uniform sizing with proper gaps
        // Available width: ICW - 8 (margins) = 503px
        // 7 buttons + 6 gaps: 7*pw + 6*gap = 503 -> pw = (503 - 6*5) / 7 = 67.5
        int presetY = gy + 68;  // Push buttons further down
        int pw = 66;            // Button width (fits 7 buttons with gaps)
        int ph = 28;            // Button height
        int gap = 5;            // Gap between buttons
        int px = x + GP + 4;    // Left margin so border isn't clipped
        CreateModernButton(g_str->presetBlue, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_BLUE); px += pw + gap;
        CreateModernButton(g_str->presetRed, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_RED); px += pw + gap;
        CreateModernButton(g_str->presetGreen, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_GREEN); px += pw + gap;
        CreateModernButton(g_str->presetCyan, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_CYAN); px += pw + gap;
        CreateModernButton(g_str->presetPurple, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_PURPLE); px += pw + gap;
        CreateModernButton(g_str->presetWhite, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_WHITE); px += pw + gap;
        CreateModernButton(g_str->presetOff, WS_CHILD | WS_VISIBLE, px, presetY, pw, ph, hWnd, ID_BTN_PRESET_OFF);

        // Row 3-5: RGB Sliders - tighter vertical spacing
        int sliderX = x + GP + LABEL_W;
        int sliderW = ICW - LABEL_W;
        int sliderSpacing = SLIDER_H + 6;  // Less space between sliders
        int sy = presetY + ph + 20;  // Space after preset buttons

        CreateModernLabel(g_str->red, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd);
        g_state.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_R, NULL, NULL);
        SendMessage(g_state.hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        SetWindowTheme(g_state.hSliderR, L"", L"");  // Remove theme for custom drawing
        sy += sliderSpacing;

        CreateModernLabel(g_str->green, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd);
        g_state.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_G, NULL, NULL);
        SendMessage(g_state.hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        SetWindowTheme(g_state.hSliderG, L"", L"");  // Remove theme for custom drawing
        sy += sliderSpacing;

        CreateModernLabel(g_str->blue, x + GP, sy + 2, LABEL_W - 4, CTRL_H, hWnd);
        g_state.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, sy, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_B, NULL, NULL);
        SendMessage(g_state.hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
        SetWindowTheme(g_state.hSliderB, L"", L"");  // Remove theme for custom drawing

        // Initialize custom modern sliders (overlay on top of standard trackbars)
        int sliderHeight = 28;
        g_sliderR.slider = {{}, {sliderX, sy - 2*sliderSpacing, sliderX + sliderW, sy - 2*sliderSpacing + sliderHeight}, 0, 255, 'R', false, false, ID_SLIDER_R};
        g_sliderR.registered = true;
        g_sliderG.slider = {{}, {sliderX, sy - sliderSpacing, sliderX + sliderW, sy - sliderSpacing + sliderHeight}, 34, 255, 'G', false, false, ID_SLIDER_G};
        g_sliderG.registered = true;
        g_sliderB.slider = {{}, {sliderX, sy, sliderX + sliderW, sy + sliderHeight}, 255, 255, 'B', false, false, ID_SLIDER_B};
        g_sliderB.registered = true;

        // Color preview position
        g_colorPreview = {{x + GP, gy, x + GP + 70, gy + 70}, 0, 34, 255, false, 0};

        int g1h = sy + SLIDER_H + GP - g1y + 20;  // +20px extra group height, no bottom margin
        g_groups[0] = {x, g1y, CW, g1h, g_str->colorSelection};

        // Initialize modern card for this group
        g_cards[0] = {{x, g1y, x + CW, g1y + g1h}, L"", false, Gdiplus::Color(0,0,0,0)};
        wcscpy_s(g_cards[0].title, g_str->colorSelection);
        g_numCards = 1;
        y = g1y + g1h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 2: EFFECTS
        // ═══════════════════════════════════════════════════════════════
        int g2y = y;
        gy = y + GTH + GP;

        // Row 1: Keyboard + Edge mode
        CreateModernLabel(g_str->keyboardEffect, x + GP, gy + 2, 52, CTRL_H, hWnd);
        g_state.hComboKbMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            x + GP + 56, gy, 130, 200, hWnd, (HMENU)ID_COMBO_KB_MODE, NULL, NULL);
        SetWindowTheme(g_state.hComboKbMode, L"DarkMode_CFD", NULL);
        ApplyModernComboStyle(g_state.hComboKbMode);
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

        CreateModernLabel(g_str->edgeEffect, x + GP + 200, gy + 2, 36, CTRL_H, hWnd);
        g_state.hComboEdgeMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            x + GP + 240, gy, 120, 200, hWnd, (HMENU)ID_COMBO_EDGE_MODE, NULL, NULL);
        SetWindowTheme(g_state.hComboEdgeMode, L"DarkMode_CFD", NULL);
        ApplyModernComboStyle(g_state.hComboEdgeMode);
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Freeze");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Wave");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Spectrum");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Breath");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Static");
        SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)L"Off");
        SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, 4, 0);
        gy += CTRL_H + IS;

        // Row 2: Brightness + Speed
        CreateModernLabel(g_str->brightness, x + GP, gy + 2, 80, CTRL_H, hWnd);
        g_state.hSliderBrightness = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x + GP + 84, gy, 110, SLIDER_H, hWnd, (HMENU)ID_SLIDER_BRIGHTNESS, NULL, NULL);
        SendMessage(g_state.hSliderBrightness, TBM_SETRANGE, TRUE, MAKELONG(0, 4));
        SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, 4);
        SetWindowTheme(g_state.hSliderBrightness, L"", L"");

        CreateModernLabel(g_str->speed, x + GP + 210, gy + 2, 60, CTRL_H, hWnd);
        g_state.hSliderSpeed = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            x + GP + 274, gy, 110, SLIDER_H, hWnd, (HMENU)ID_SLIDER_SPEED, NULL, NULL);
        SendMessage(g_state.hSliderSpeed, TBM_SETRANGE, TRUE, MAKELONG(0, 5));
        SendMessage(g_state.hSliderSpeed, TBM_SETPOS, TRUE, 2);
        SetWindowTheme(g_state.hSliderSpeed, L"", L"");

        int g2h = gy + SLIDER_H + GP - g2y;
        g_groups[1] = {x, g2y, CW, g2h, g_str->effects};
        y = g2y + g2h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 3: DEVICES
        // ═══════════════════════════════════════════════════════════════
        int g3y = y;
        gy = y + GTH + GP;

        // Row 1: Device checkboxes (auto-width based on text)
        int cbW = 0, cbX = x + GP;
        int cbGap = 6;  // Gap between checkboxes

        g_state.hCheckAura = CreateModernCheckbox(L"ASUS Aura", cbX, gy, CTRL_H, hWnd, ID_CHECK_AURA, &cbW);
        SendMessage(g_state.hCheckAura, BM_SETCHECK, BST_CHECKED, 0);
        cbX += cbW + cbGap;

        g_state.hCheckMouse = CreateModernCheckbox(L"SteelSeries", cbX, gy, CTRL_H, hWnd, ID_CHECK_MOUSE, &cbW);
        SendMessage(g_state.hCheckMouse, BM_SETCHECK, BST_CHECKED, 0);
        cbX += cbW + cbGap;

        g_state.hCheckKeyboard = CreateModernCheckbox(L"Keyboard", cbX, gy, CTRL_H, hWnd, ID_CHECK_KEYBOARD, &cbW);
        SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, BST_CHECKED, 0);
        cbX += cbW + cbGap;

        g_state.hCheckEdge = CreateModernCheckbox(L"Edge", cbX, gy, CTRL_H, hWnd, ID_CHECK_EDGE, &cbW);
        SendMessage(g_state.hCheckEdge, BM_SETCHECK, BST_CHECKED, 0);
        cbX += cbW + cbGap;

        g_state.hCheckRAM = CreateModernCheckbox(L"RAM", cbX, gy, CTRL_H, hWnd, ID_CHECK_RAM, &cbW);
        SendMessage(g_state.hCheckRAM, BM_SETCHECK, BST_CHECKED, 0);

        // Row 2: Action buttons (right-aligned within group)
        gy += CTRL_H + IS;

        CreateModernButton(g_str->channelCorrection, WS_CHILD | WS_VISIBLE,
            x + GP, gy, 70, CTRL_H, hWnd, ID_BTN_CHANNEL_SETTINGS);

        CreateModernButton(L"ASUS", WS_CHILD | WS_VISIBLE,
            x + GP + 75, gy, 50, CTRL_H, hWnd, ID_BTN_ASUS_TEST);

        CreateModernButton(L"HID Reset", WS_CHILD | WS_VISIBLE,
            x + GP + 130, gy, 70, CTRL_H, hWnd, ID_BTN_HID_RESET);

        int g3h = gy + CTRL_H + GP - g3y;
        g_groups[2] = {x, g3y, CW, g3h, g_str->devices};
        y = g3y + g3h + GM;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 4: PROFILES
        // ═══════════════════════════════════════════════════════════════
        int g4y = y;
        gy = y + GTH + GP;

        // Row 1: Profile dropdown + Save/Load
        CreateModernLabel(g_str->profile, x + GP, gy + 2, 40, CTRL_H, hWnd);
        g_state.hComboProfiles = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
            x + GP + 44, gy, 110, 200, hWnd, (HMENU)ID_COMBO_PROFILES, NULL, NULL);
        SetWindowTheme(g_state.hComboProfiles, L"DarkMode_CFD", NULL);
        ApplyModernComboStyle(g_state.hComboProfiles);

        CreateModernButton(g_str->save, WS_CHILD | WS_VISIBLE,
            x + GP + 160, gy, 65, CTRL_H, hWnd, ID_BTN_SAVE_PROFILE);
        CreateModernButton(g_str->load, WS_CHILD | WS_VISIBLE,
            x + GP + 230, gy, 50, CTRL_H, hWnd, ID_BTN_LOAD_PROFILE);

        // Settings checkboxes (auto-width)
        int settingsCbX = x + GP + 285;
        g_state.hCheckAutostart = CreateModernCheckbox(g_str->autostart, settingsCbX, gy, CTRL_H, hWnd, ID_CHECK_AUTOSTART, &cbW);
        if (IsAutoStartEnabled()) {
            SendMessage(g_state.hCheckAutostart, BM_SETCHECK, BST_CHECKED, 0);
            g_state.autostart = true;
        }
        settingsCbX += cbW + cbGap;

        g_state.hCheckMinimizeTray = CreateModernCheckbox(g_str->tray, settingsCbX, gy, CTRL_H, hWnd, ID_CHECK_MINIMIZE_TRAY, &cbW);
        SendMessage(g_state.hCheckMinimizeTray, BM_SETCHECK, BST_CHECKED, 0);

        int g4h = gy + CTRL_H + GP - g4y;
        g_groups[3] = {x, g4y, CW, g4h, g_str->profilesSettings};
        g_numGroups = 4;
        y = g4y + g4h + GM;

        // ═══════════════════════════════════════════════════════════════
        // APPLY BUTTON + AUTO-APPLY CHECKBOX
        // ═══════════════════════════════════════════════════════════════
        int applyBtnW = 120;
        CreateModernButton(g_str->apply, WS_CHILD | WS_VISIBLE,
            x, y, applyBtnW, 32, hWnd, ID_BTN_APPLY);

        g_state.hCheckAutoApply = CreateModernCheckbox(g_str->autoApply, x + applyBtnW + 12, y + 4, CTRL_H, hWnd, ID_CHECK_AUTO_APPLY, NULL);
        SendMessage(g_state.hCheckAutoApply, BM_SETCHECK, g_state.autoApply ? BST_CHECKED : BST_UNCHECKED, 0);

        y += 36;

        // ═══════════════════════════════════════════════════════════════
        // GROUP 5: STATUS
        // ═══════════════════════════════════════════════════════════════
        int g5y = y;
        gy = y + GTH + GP;

        // Logo dimensions and spacing
        int logoSize = 64;
        int logoMargin = 12;  // Margin from edges
        int statusLogWidth = CW - GP * 2 - logoSize - logoMargin;  // Leave space for logo

        // Status log container (owner-draw for rounded background)
        int statusLogHeight = STATUS_H - GTH - GP * 2;
        int borderInset = 6;  // Inset for border visibility (enough for rounded corners)

        // Create container for rounded border effect (drawn first, behind edit)
        g_state.hStatusBorder = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            x + GP, gy, statusLogWidth, statusLogHeight, hWnd, (HMENU)9998, NULL, NULL);

        // Status log inside container (slightly inset for border)
        g_state.hStatus = CreateWindowW(L"EDIT", g_str->ready,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            x + GP + borderInset, gy + borderInset,
            statusLogWidth - borderInset * 2, statusLogHeight - borderInset * 2,
            hWnd, (HMENU)ID_STATIC_STATUS, NULL, NULL);
        // Apply dark theme to scrollbar
        SetWindowTheme(g_state.hStatus, L"DarkMode_Explorer", NULL);
        // Apply rounded corners to status log
        int editW = statusLogWidth - borderInset * 2;
        int editH = statusLogHeight - borderInset * 2;
        HRGN hRgn = CreateRoundRectRgn(0, 0, editW + 1, editH + 1, 6, 6);
        SetWindowRgn(g_state.hStatus, hRgn, TRUE);

        // Logo control (owner-draw static, positioned in bottom-right of status group)
        int logoX = x + CW - GP - logoSize;
        int logoY = gy + (STATUS_H - GTH - GP * 2) / 2 - logoSize / 2;  // Vertically centered in status area
        g_state.hLogo = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            logoX, logoY, logoSize, logoSize, hWnd, (HMENU)9999, NULL, NULL);

        int g5h = STATUS_H;
        g_groups[4] = {x, g5y, CW, g5h, g_str->statusTitle};
        g_numGroups = 5;

        // Initialize
        UpdateSliders();
        UpdateHexEdit();
        RefreshProfileList();
        CreateTrayIcon(hWnd);
        g_channels.Load();

        // Load last profile if exists
        if (!g_state.lastProfile.empty()) {
            // Check if profile exists in list
            for (const auto& p : g_state.profiles) {
                if (p == g_state.lastProfile) {
                    LoadProfile(g_state.lastProfile);
                    UpdateSliders();
                    UpdateHexEdit();
                    UpdatePreview();
                    // Select in combo
                    SetWindowTextW(g_state.hComboProfiles, g_state.lastProfile.c_str());
                    // Apply colors only on real startup, not after theme/language change
                    if (!g_skipApplyOnStart) {
                        ApplyColors();
                    }
                    break;
                }
            }
        }

        // Register global hotkeys (Ctrl+Alt+1-6)
        RegisterHotKey(hWnd, ID_HOTKEY_BLUE, MOD_CONTROL | MOD_ALT, '1');
        RegisterHotKey(hWnd, ID_HOTKEY_RED, MOD_CONTROL | MOD_ALT, '2');
        RegisterHotKey(hWnd, ID_HOTKEY_GREEN, MOD_CONTROL | MOD_ALT, '3');
        RegisterHotKey(hWnd, ID_HOTKEY_WHITE, MOD_CONTROL | MOD_ALT, '4');
        RegisterHotKey(hWnd, ID_HOTKEY_OFF, MOD_CONTROL | MOD_ALT, '0');
        RegisterHotKey(hWnd, ID_HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT, VK_SPACE);

        break;
    }

    case WM_HOTKEY: {
        static bool ledsOn = true;
        static uint8_t lastR = 0, lastG = 34, lastB = 255;

        switch (wParam) {
            case ID_HOTKEY_BLUE:
                SetPresetColor(0, 34, 255);
                ApplyColors();
                break;
            case ID_HOTKEY_RED:
                SetPresetColor(255, 0, 0);
                ApplyColors();
                break;
            case ID_HOTKEY_GREEN:
                SetPresetColor(0, 255, 0);
                ApplyColors();
                break;
            case ID_HOTKEY_WHITE:
                SetPresetColor(255, 255, 255);
                ApplyColors();
                break;
            case ID_HOTKEY_OFF:
                SetPresetColor(0, 0, 0);
                ApplyColors();
                break;
            case ID_HOTKEY_TOGGLE:
                if (ledsOn) {
                    lastR = g_state.red; lastG = g_state.green; lastB = g_state.blue;
                    SetPresetColor(0, 0, 0);
                } else {
                    SetPresetColor(lastR, lastG, lastB);
                }
                ledsOn = !ledsOn;
                ApplyColors();
                break;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        // Simple gradient background using GDI+
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

        // Diagonal gradient
        Gdiplus::LinearGradientBrush bgBrush(
            Gdiplus::Point(0, 0),
            Gdiplus::Point(clientRect.right, clientRect.bottom),
            Gdiplus::Color(255, 20, 22, 30),
            Gdiplus::Color(255, 35, 40, 55)
        );
        g.FillRectangle(&bgBrush, 0, 0, clientRect.right, clientRect.bottom);

        // Draw group boxes with rounded corners
        for (int i = 0; i < g_numGroups; i++) {
            DrawThemedGroupBox(hdc, g_groups[i]);
        }

        // Status log border is drawn via owner-draw control (WM_DRAWITEM, ID 9998)
        // Logo is drawn in its own owner-draw control (WM_DRAWITEM, ID 9999)

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND ctrl = (HWND)lParam;

        // Status log (readonly edit) - needs solid background to prevent ghosting
        if (ctrl == g_state.hStatus) {
            SetTextColor(hdc, RGB(200, 210, 220));
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(28, 32, 42));
            static HBRUSH hStatusBrush = CreateSolidBrush(RGB(28, 32, 42));
            return (LRESULT)hStatusBrush;
        }

        SetTextColor(hdc, RGB(220, 225, 235));
        SetBkMode(hdc, TRANSPARENT);

        // Check if it's a trackbar - use a nice dark blue-gray background
        if (ctrl == g_state.hSliderR || ctrl == g_state.hSliderG ||
            ctrl == g_state.hSliderB || ctrl == g_state.hSliderBrightness ||
            ctrl == g_state.hSliderSpeed) {
            static HBRUSH hSliderBrush = CreateSolidBrush(RGB(28, 32, 42));
            return (LRESULT)hSliderBrush;
        }

        // Use hollow brush for true transparency over gradient
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetTextColor(hdc, RGB(230, 235, 245));
        SetBkMode(hdc, OPAQUE);  // IMPORTANT: Use OPAQUE to prevent text ghosting
        SetBkColor(hdc, RGB(35, 40, 50));

        // Use a solid brush that matches the background
        static HBRUSH hEditBrush = NULL;
        if (!hEditBrush) {
            hEditBrush = CreateSolidBrush(RGB(35, 40, 50));
        }
        return (LRESULT)hEditBrush;
    }

    case WM_CTLCOLORLISTBOX: {
        // Style dropdown list (combobox popup)
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 230, 245));
        SetBkColor(hdc, RGB(35, 40, 55));
        static HBRUSH hListBrush = CreateSolidBrush(RGB(35, 40, 55));
        return (LRESULT)hListBrush;
    }

    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 225, 235));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_BUTTON) {
            wchar_t text[64] = {0};
            GetWindowTextW(dis->hwndItem, text, 64);

            bool isHovered = g_buttonHover[dis->hwndItem];
            bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
            bool isAccent = (dis->CtlID == ID_BTN_APPLY);

            DrawModernButton(dis->hDC, &dis->rcItem, text, isHovered, isPressed, isAccent);
            return TRUE;
        }
        // Status log border (rounded container)
        if (dis->CtlID == 9998) {
            Gdiplus::Graphics g(dis->hDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            int w = dis->rcItem.right - dis->rcItem.left;
            int h = dis->rcItem.bottom - dis->rcItem.top;
            float radius = 8.0f;

            // Create rounded rect path
            Gdiplus::GraphicsPath path;
            float d = radius * 2;
            path.AddArc(0.0f, 0.0f, d, d, 180, 90);
            path.AddArc((float)w - d, 0.0f, d, d, 270, 90);
            path.AddArc((float)w - d, (float)h - d, d, d, 0, 90);
            path.AddArc(0.0f, (float)h - d, d, d, 90, 90);
            path.CloseFigure();

            // Fill with slightly lighter background
            Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, 22, 26, 36));
            g.FillPath(&fillBrush, &path);

            // Draw border
            Gdiplus::Pen borderPen(Gdiplus::Color(255, 60, 70, 90), 1.5f);
            g.DrawPath(&borderPen, &path);

            return TRUE;
        }
        // Logo owner-draw static
        if (dis->CtlID == 9999 && g_pLogoImage && g_logoWidth > 0 && g_logoHeight > 0) {
            Gdiplus::Graphics g(dis->hDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

            // Fill background with group color
            Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 32, 36, 48));
            g.FillRectangle(&bgBrush, 0, 0, dis->rcItem.right, dis->rcItem.bottom);

            // Draw logo with slight transparency (watermark effect)
            Gdiplus::ColorMatrix colorMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.35f, 0.0f,  // 35% opacity
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            Gdiplus::ImageAttributes imgAttr;
            imgAttr.SetColorMatrix(&colorMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

            int w = dis->rcItem.right - dis->rcItem.left;
            int h = dis->rcItem.bottom - dis->rcItem.top;
            g.DrawImage(g_pLogoImage,
                Gdiplus::Rect(0, 0, w, h),
                0, 0, g_logoWidth, g_logoHeight,
                Gdiplus::UnitPixel, &imgAttr);
            return TRUE;
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1;  // Prevent flicker, WM_PAINT handles background

    case WM_GETMINMAXINFO: {
        // Set minimum window size (for potential future resize support)
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 500;
        break;  // Let DefWindowProc also handle this
    }

    case WM_HSCROLL: {
        HWND slider = (HWND)lParam;
        int pos = (int)SendMessage(slider, TBM_GETPOS, 0, 0);
        bool colorChanged = false;

        if (slider == g_state.hSliderR) {
            g_state.red = pos;
            UpdatePreview(); UpdateHexEdit(); colorChanged = true;
        }
        else if (slider == g_state.hSliderG) {
            g_state.green = pos;
            UpdatePreview(); UpdateHexEdit(); colorChanged = true;
        }
        else if (slider == g_state.hSliderB) {
            g_state.blue = pos;
            UpdatePreview(); UpdateHexEdit(); colorChanged = true;
        }
        else if (slider == g_state.hSliderBrightness) {
            g_state.brightness = pos;
            colorChanged = true;
        }
        else if (slider == g_state.hSliderSpeed) {
            g_state.speed = pos;
            colorChanged = true;
        }

        // Auto-apply with debouncing (150ms delay)
        if (colorChanged && g_state.autoApply) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            SetTimer(hWnd, ID_TIMER_DEBOUNCE, 150, NULL);
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_APPLY) {
            ApplyColors();  // Direct call, no thread
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
        else if (id == ID_CHECK_AUTO_APPLY) {
            g_state.autoApply = (SendMessage(g_state.hCheckAutoApply, BM_GETCHECK, 0, 0) == BST_CHECKED);
        }
        else if (id == ID_BTN_CHANNEL_SETTINGS) {
            // Open integrated Channel Settings dialog
            ShowChannelSettingsDialog(hWnd);
            g_channels.Load();  // Reload after dialog closes
            AppendStatus(L"Channel corrections updated");
        }
        else if (id == ID_BTN_ASUS_TEST) {
            // Open ASUS Test dialog
            ShowAsusTestDialog(hWnd);
            AppendStatus(L"ASUS channel settings updated");
        }
        else if (id == ID_BTN_HID_RESET) {
            // Manual HID reset and re-apply
            FullHIDReset();
            Sleep(500);
            AppendStatus(L"Re-applying colors...");
            ApplyColors();
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
            // Toggle theme and restart (only way to fully update trackbar backgrounds)
            g_theme = g_theme->isDark ? &g_lightTheme : &g_darkTheme;
            SaveAppSettings();

            // Restart application to apply theme fully (without re-applying colors)
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            RemoveTrayIcon();
            ShellExecuteW(NULL, L"open", exePath, L"--no-apply", NULL, SW_SHOW);
            PostQuitMessage(0);
        }
        else if (id == ID_BTN_LANG) {
            // Toggle language and restart app (without re-applying colors)
            g_lang = (g_lang == LANG_EN) ? LANG_DE : LANG_EN;
            g_str = (g_lang == LANG_EN) ? &g_strEN : &g_strDE;
            SaveAppSettings();

            // Restart application
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            RemoveTrayIcon();
            ShellExecuteW(NULL, L"open", exePath, L"--no-apply", NULL, SW_SHOW);
            PostQuitMessage(0);
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
        else if (id == ID_TRAY_STANDBY) { SystemStandby(); }
        else if (id == ID_TRAY_SHUTDOWN) {
            if (MessageBoxW(hWnd, L"Are you sure you want to shutdown?", L"Confirm Shutdown", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SystemShutdown();
            }
        }
        else if (id == ID_TRAY_RESTART) {
            if (MessageBoxW(hWnd, L"Are you sure you want to restart?", L"Confirm Restart", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SystemRestart();
            }
        }
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
        // Handle minimize button -> minimize to tray if enabled
        if (wParam == SC_MINIMIZE && g_state.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        // Handle close button (X) -> minimize to tray if enabled
        if (wParam == SC_CLOSE && g_state.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        // All other syscommands go to DefWindowProc
        return DefWindowProc(hWnd, msg, wParam, lParam);

    case WM_CLOSE:
        // This handles programmatic close (not from X button)
        if (g_state.minimizeToTray) {
            MinimizeToTray();
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);

    // Custom message from resume watcher thread
    // Resume detected by watchdog thread (time jump)
    case WM_USER + 100: {
        g_resumeDetected = false;
        KillTimer(hWnd, ID_TIMER_RESUME);
        SetTimer(hWnd, ID_TIMER_RESUME, 2000, NULL);
        return 0;
    }

    case WM_POWERBROADCAST: {
        // Handle SUSPEND - turn off all devices before sleep
        if (wParam == PBT_APMSUSPEND) {
            ClearStatus();
            AppendStatus(L"System entering standby...");
            // Turn off all RGB devices for clean state
            hid_init();
            if (g_state.enableAura) SetAsusAura(0, 0, 0);
            if (g_state.enableMouse) SetSteelSeries(0, 0, 0);
            if (g_state.enableKeyboard) SetEVisionKeyboard(0, 0, 0, 0, 0, 0);
            if (g_state.enableEdge) SetEVisionEdge(0, 0, 0, 0);
            if (g_state.enableRAM) SetGSkillRAM(0, 0, 0);
            hid_exit();
            AppendStatus(L"Devices off - ready for standby");
        }
        // Handle RESUME from sleep/hibernate
        else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            KillTimer(hWnd, ID_TIMER_RESUME);
            SetTimer(hWnd, ID_TIMER_RESUME, 3000, NULL);
        }
        // Handle display power state change (monitor on)
        else if (wParam == PBT_POWERSETTINGCHANGE) {
            POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)lParam;
            if (pbs && pbs->DataLength >= 4) {
                DWORD displayState = *((DWORD*)pbs->Data);
                if (displayState == 1) {
                    KillTimer(hWnd, ID_TIMER_RESUME);
                    SetTimer(hWnd, ID_TIMER_RESUME, 3000, NULL);
                }
            }
        }
        return TRUE;
    }

    // Session change (lock/unlock)
    case WM_WTSSESSION_CHANGE: {
        // WTS_SESSION_UNLOCK = 0x8
        if (wParam == 0x8) {
            KillTimer(hWnd, ID_TIMER_RESUME);
            SetTimer(hWnd, ID_TIMER_RESUME, 3000, NULL);
        }
        return TRUE;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_RESUME) {
            KillTimer(hWnd, ID_TIMER_RESUME);
            ClearStatus();
            AppendStatus(L"System resumed - resetting RGB...");
            FullHIDReset();
            ApplyColors();
        }
        else if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            ApplyColors();
        }
        break;

    case WM_DESTROY:
        // Save window position before exit
        SaveAppSettings();
        g_watcherRunning = false;  // Stop resume watcher thread
        // Unregister hotkeys
        UnregisterHotKey(hWnd, ID_HOTKEY_BLUE);
        UnregisterHotKey(hWnd, ID_HOTKEY_RED);
        UnregisterHotKey(hWnd, ID_HOTKEY_GREEN);
        UnregisterHotKey(hWnd, ID_HOTKEY_WHITE);
        UnregisterHotKey(hWnd, ID_HOTKEY_OFF);
        UnregisterHotKey(hWnd, ID_HOTKEY_TOGGLE);
        WTSUnRegisterSessionNotification(hWnd);
        RemoveTrayIcon();
        if (g_hBgBrush) DeleteObject(g_hBgBrush);
        if (g_hCtrlBrush) DeleteObject(g_hCtrlBrush);
        if (g_hBtnBrush) DeleteObject(g_hBtnBrush);
        if (g_hLogoBitmap) DeleteObject(g_hLogoBitmap);
        if (g_pLogoImage) { delete g_pLogoImage; g_pLogoImage = NULL; }
        if (g_hTransparentBrush) DeleteObject(g_hTransparentBrush);
        PostQuitMessage(0);
        break;

    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//=============================================================================
// MAIN
//=============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Check for command line flags
    bool startMinimized = (strstr(lpCmdLine, "--minimized") != nullptr);
    g_skipApplyOnStart = (strstr(lpCmdLine, "--no-apply") != nullptr);
    g_state.dryRun = (strstr(lpCmdLine, "--dry-run") != nullptr);

    // Initialize GDI+ for PNG loading
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Load saved settings (language, theme)
    LoadAppSettings();

    // Initialize ASUS hardware scan (checks for config changes)
    InitAsusHardware();

    // Full HID reset on startup to ensure proper device state after boot/restart
    if (!g_skipApplyOnStart) {
        FullHIDReset();
    }

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
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));  // Custom icon from resource
    RegisterClassW(&wc);

    // Standard Windows window - fixed size (no resize handles)
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    // Build window title with admin status and dry-run indicator
    bool isAdmin = IsRunningAsAdmin();
    swprintf_s(g_windowTitle, 256, g_str->windowTitle, isAdmin ? L"\x2705" : L"\x274C");
    if (g_state.dryRun) {
        wcscat_s(g_windowTitle, 256, L" [DRY RUN]");
    }

    // Calculate window size to get exact client area
    RECT rc = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT - TITLEBAR_H};  // Subtract custom titlebar height we're not using
    AdjustWindowRect(&rc, style, FALSE);

    // Create window with saved position
    g_state.hWnd = CreateWindowW(L"OneClickRGBClass", g_windowTitle,
        style,
        g_windowX, g_windowY, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);

    // Register for power setting notifications (resume from sleep)
    GUID GUID_CONSOLE_DISPLAY_STATE = {0x6fe69556, 0x704a, 0x47a0, {0x8f, 0x24, 0x8d, 0x93, 0x6f, 0xda, 0x47}};
    RegisterPowerSettingNotification(g_state.hWnd, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // Also register for session notifications (lock/unlock)
    WTSRegisterSessionNotification(g_state.hWnd, NOTIFY_FOR_THIS_SESSION);

    // Start resume watcher thread (detects time jumps from standby)
    std::thread(ResumeWatcherThread).detach();

    AppendStatus(L"Power notifications & resume watchdog started");

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

    // Cleanup GDI+
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return (int)msg.wParam;
}
