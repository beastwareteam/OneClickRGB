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

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0500
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif


#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <objidl.h>
#include <nlohmann/json.hpp>
#include <gdiplus.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include "hidapi.h"
#include "channel_config.h"
#include "themes.h"
#include "modern_ui.h"

using json = nlohmann::json;

#include <fstream>
inline void LogDebug(const char* msg) {
    std::ofstream f("debug.log", std::ios::app);
    f << msg << std::endl;
}

//=============================================================================
// SETTINGS-PERSISTENZ
//=============================================================================

struct AppSettings {
    int window_x = -1;
    int window_y = -1;
    int window_width = 800;
    int window_height = 600;
    bool autostart_enabled = false;
    bool minimize_to_tray = false;
    bool apply_on_startup = true;
    int last_brightness = 100;
    int last_color = 255;
    std::string last_profile = "";
    std::string startup_profile = "";
    bool show_notifications = true;
    int scan_interval_ms = 5000;
    bool start_minimized = false;
    // Erweiterte Settings:
    int red = 255;
    int green = 255;
    int blue = 255;
    int effect_keyboard = 0; // 0=Static, 1=Breathing, ...
    int effect_edge = 0;     // 0=Static, 1=Breathing, ...
    int speed = 50;
    bool live_preview = false;
    // Pro-Kanal-Werte (z.B. für 8 Kanäle)
    int channel_r[8] = {255,255,255,255,255,255,255,255};
    int channel_g[8] = {255,255,255,255,255,255,255,255};
    int channel_b[8] = {255,255,255,255,255,255,255,255};
    bool channel_active[8] = {true,true,true,true,true,true,true,true};
    // Weitere Felder nach Bedarf ...
};

// Definition of modern dark theme
ModernTheme g_modernDark = {
    // Backgrounds - deep dark with slight blue tint
    Gdiplus::Color(255, 18, 18, 24),      // bgPrimary
    Gdiplus::Color(255, 25, 25, 35),      // bgSecondary
    Gdiplus::Color(255, 35, 35, 50),      // bgTertiary
    Gdiplus::Color(200, 40, 40, 55),      // bgCard (semi-transparent)

    // Accent - electric cyan/blue
    Gdiplus::Color(255, 0, 200, 255),     // accent
    Gdiplus::Color(255, 50, 220, 255),    // accentHover
    Gdiplus::Color(100, 0, 200, 255),     // accentGlow

    // Text
    Gdiplus::Color(255, 240, 240, 245),   // textPrimary
    Gdiplus::Color(255, 180, 180, 190),   // textSecondary
    Gdiplus::Color(255, 100, 100, 120),   // textMuted

    // Border & effects
    Gdiplus::Color(255, 60, 60, 80),      // border
    Gdiplus::Color(80, 0, 0, 0),          // shadow
    Gdiplus::Color(60, 0, 200, 255),      // glow

    // State
    Gdiplus::Color(255, 0, 255, 136),     // success (neon green)
    Gdiplus::Color(255, 255, 200, 0),     // warning
    Gdiplus::Color(255, 255, 60, 100),    // error

    true
};

ModernTheme* g_mTheme = &g_modernDark;

// Unused legacy struct - kept for reference but deprecated
// AppSettings g_settings; 

void SetTheme(int themeId); // Forward decl

// Forward declarations for settings persistence
void LoadSettings();
void SaveSettings();
std::wstring GetSettingsPath();

// Implementation of LoadSettings and SaveSettings moved below to satisfy scope requirements


#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")

// Forward declarations for Modern UI subclass procedures
LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK BtnCheckboxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK ComboSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK StaticSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK ColorPreviewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK EditBorderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

//=============================================================================
// CONSTANTS & LAYOUT
//=============================================================================

#define APP_NAME L"OneClickRGB"
#define APP_VERSION L"3.5.1"
#define APP_VERSION_A "3.5.1"  // ANSI version for resources

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
#define CHECKBOX_W 105
#define SMALL_BTN_W 85
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

// Removed struct Theme, g_darkTheme, g_lightTheme, and g_theme as part of theme consolidation
HBRUSH g_hBgBrush = NULL;
HBRUSH g_hCtrlBrush = NULL;
HBRUSH g_hBtnBrush = NULL;

// Trackbar background brush matching the dark theme
HBRUSH g_hTrackbarBrush = NULL;
ULONG_PTR g_gdiplusToken = 0;

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
    // Tooltips
    const wchar_t* tipSliderR;
    const wchar_t* tipSliderG;
    const wchar_t* tipSliderB;
    const wchar_t* tipColorPreview;
    const wchar_t* tipHexInput;
    const wchar_t* tipPickColor;
    const wchar_t* tipPresetBlue;
    const wchar_t* tipPresetRed;
    const wchar_t* tipPresetGreen;
    const wchar_t* tipPresetCyan;
    const wchar_t* tipPresetPurple;
    const wchar_t* tipPresetWhite;
    const wchar_t* tipPresetOff;
    const wchar_t* tipKeyboardMode;
    const wchar_t* tipEdgeMode;
    const wchar_t* tipBrightness;
    const wchar_t* tipSpeed;
    const wchar_t* tipChannels;
    const wchar_t* tipProfile;
    const wchar_t* tipSave;
    const wchar_t* tipLoad;
    const wchar_t* tipAutostart;
    const wchar_t* tipTray;
    const wchar_t* tipLive;
    const wchar_t* tipApply;
    const wchar_t* tipTheme;
    const wchar_t* tipLang;
    const wchar_t* tipStatus;
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
    // Window title (version inserted at runtime via APP_VERSION)
    L"Complete RGB Control [Admin: %s]",
    // Color presets
    L"Blue", L"Red", L"Green", L"Cyan", L"Purple", L"White", L"Off",
    // Keyboard modes
    L"Static", L"Breathing", L"Wave", L"Reactive", L"Rainbow",
    // Edge modes
    L"Static", L"Breathing", L"Wave", L"Spectrum", L"Off",
    // Channel settings dialog
    L"Channel Color Correction", L"Save && Close", L"Reset All",
    L"100% = no change. Adjust to correct color deviation.",
    // Tooltips
    L"Red channel (0-255)\nAdjust the red color intensity",
    L"Green channel (0-255)\nAdjust the green color intensity",
    L"Blue channel (0-255)\nAdjust the blue color intensity",
    L"Color preview\nShows the current selected color",
    L"Hex color input\nEnter color as #RRGGBB (e.g. #FF0000 for red)",
    L"Open color picker dialog\nSelect any color visually",
    L"Quick preset: Blue\nASUS Aura standard color",
    L"Quick preset: Red\nIntense red color",
    L"Quick preset: Green\nIntense green color",
    L"Quick preset: Cyan\nTurquoise/Aqua color",
    L"Quick preset: Purple\nMagenta/Violet color",
    L"Quick preset: White\nAll channels at maximum",
    L"Turn off all LEDs\nSets color to black (0,0,0)",
    L"Keyboard lighting effect\nStatic, Breathing, Wave, Reactive, Rainbow",
    L"Edge LED effect (laptop keyboard edges)\nControls the side lighting strips",
    L"Overall brightness (0-100%)\nAffects all connected devices",
    L"Animation speed\nControls breathing/wave effect timing",
    L"Per-channel color correction\nFine-tune individual device colors",
    L"Select saved profile\nQuickly switch between color configurations",
    L"Save current settings\nStore color, effects and device settings",
    L"Load selected profile\nRestore previously saved settings",
    L"Start with Windows\nLaunch minimized when Windows starts",
    L"Minimize to system tray\nHide window but keep running",
    L"Live preview\nApply changes automatically while adjusting",
    L"Apply settings to all devices\nSend current color to all RGB hardware",
    L"Switch color theme\nDark / Light / Colorblind modes",
    L"Switch language\nEnglish / Deutsch",
    L"Application log\nShows device status and applied settings"
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
    // Window title (version inserted at runtime via APP_VERSION)
    L"Komplette RGB-Steuerung [Admin: %s]",
    // Color presets
    L"Blau", L"Rot", L"Gr\x00FCn", L"Cyan", L"Lila", L"Wei\x00DF", L"Aus",
    // Keyboard modes
    L"Statisch", L"Atmend", L"Welle", L"Reaktiv", L"Regenbogen",
    // Edge modes
    L"Statisch", L"Atmend", L"Welle", L"Spektrum", L"Aus",
    // Channel settings dialog
    L"Kanal-Farbkorrektur", L"Speichern && Schlie\x00DFen", L"Alle zur\x00FCcksetzen",
    L"100% = keine \x00C4nderung. Anpassen um Farbabweichungen zu korrigieren.",
    // Tooltips
    L"Rotkanal (0-255)\nRote Farbintensit\x00E4t einstellen",
    L"Gr\x00FCnkanal (0-255)\nGr\x00FCne Farbintensit\x00E4t einstellen",
    L"Blaukanal (0-255)\nBlaue Farbintensit\x00E4t einstellen",
    L"Farbvorschau\nZeigt die aktuell gew\x00E4hlte Farbe",
    L"Hex-Farbeingabe\nFarbe als #RRGGBB eingeben (z.B. #FF0000 f\x00FCr Rot)",
    L"Farbauswahl \x00F6ffnen\nBelibige Farbe visuell ausw\x00E4hlen",
    L"Schnellauswahl: Blau\nASUS Aura Standardfarbe",
    L"Schnellauswahl: Rot\nIntensives Rot",
    L"Schnellauswahl: Gr\x00FCn\nIntensives Gr\x00FCn",
    L"Schnellauswahl: Cyan\nT\x00FCrkis/Aqua-Farbe",
    L"Schnellauswahl: Lila\nMagenta/Violett-Farbe",
    L"Schnellauswahl: Wei\x00DF\nAlle Kan\x00E4le auf Maximum",
    L"Alle LEDs ausschalten\nSetzt Farbe auf Schwarz (0,0,0)",
    L"Tastatur-Lichteffekt\nStatisch, Atmend, Welle, Reaktiv, Regenbogen",
    L"Rand-LED Effekt (Laptop-Tastaturr\x00E4nder)\nSteuert die seitlichen Lichtleisten",
    L"Gesamthelligkeit (0-100%)\nBeeinflusst alle verbundenen Ger\x00E4te",
    L"Animationsgeschwindigkeit\nSteuert Atmen/Wellen-Effekt Timing",
    L"Kanal-Farbkorrektur\nEinzelne Ger\x00E4tefarben fein abstimmen",
    L"Gespeichertes Profil ausw\x00E4hlen\nSchnell zwischen Farbkonfigurationen wechseln",
    L"Aktuelle Einstellungen speichern\nFarbe, Effekte und Ger\x00E4teeinstellungen sichern",
    L"Ausgew\x00E4hltes Profil laden\nGespeicherte Einstellungen wiederherstellen",
    L"Mit Windows starten\nMinimiert starten wenn Windows hochf\x00E4hrt",
    L"In System-Tray minimieren\nFenster verstecken aber weiterlaufen",
    L"Live-Vorschau\n\x00C4nderungen automatisch beim Anpassen anwenden",
    L"Einstellungen auf alle Ger\x00E4te anwenden\nAktuelle Farbe an alle RGB-Hardware senden",
    L"Farbschema wechseln\nDunkel / Hell / Farbenblind Modi",
    L"Sprache wechseln\nEnglish / Deutsch",
    L"Anwendungsprotokoll\nZeigt Ger\x00E4testatus und angewandte Einstellungen"
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
    HWND hLabelRVal = NULL, hLabelGVal = NULL, hLabelBVal = NULL;  // Live value labels
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

    // Tooltip
    HWND hTooltip = NULL;

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

// SaveAppSettings and LoadAppSettings removed in favor of unified LoadSettings/SaveSettings (JSON)
// and single g_state source of truth.

std::wstring GetSettingsPath() {
    wchar_t* appdata = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appdata) != S_OK) {
        return L"settings.json";
    }
    std::wstring path = appdata;
    CoTaskMemFree(appdata);
    path += L"\\OneClickRGB\\settings.json";
    return path;
}

void SaveSettings() {
    std::wstring path = GetSettingsPath();
    // Create directory if not exists
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring dir = path.substr(0, pos);
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }
    json j;
    
    // Window position
    if (g_state.hWnd) {
        RECT rc;
        GetWindowRect(g_state.hWnd, &rc);
        j["window"] = { {"x", rc.left}, {"y", rc.top} };
    } else {
        j["window"] = { {"x", g_windowX}, {"y", g_windowY} };
    }

    // App Settings
    j["autostart"] = g_state.autostart;
    j["minimize_to_tray"] = g_state.minimizeToTray;
    j["auto_apply"] = g_state.autoApply;
    j["lang"] = (g_lang == LANG_DE ? "de" : "en");
    j["theme"] = GetThemeId();

    // RGB State
    j["red"] = (int)g_state.red;
    j["green"] = (int)g_state.green;
    j["blue"] = (int)g_state.blue;
    j["brightness"] = (int)g_state.brightness;
    j["speed"] = (int)g_state.speed;
    j["kbMode"] = (int)g_state.kbMode;
    j["edgeMode"] = (int)g_state.edgeMode;

    // Device Enables
    j["enableAura"] = g_state.enableAura;
    j["enableMouse"] = g_state.enableMouse;
    j["enableKeyboard"] = g_state.enableKeyboard;
    j["enableRAM"] = g_state.enableRAM;
    j["enableEdge"] = g_state.enableEdge;

    if (!g_state.lastProfile.empty()) {
        std::string profileName;
        for (wchar_t wc : g_state.lastProfile) profileName += static_cast<char>(wc);
        j["last_profile"] = profileName;
    }

    std::ofstream f(path);
    if (f) f << std::setw(4) << j;
}

void LoadSettings() {
    std::wstring path = GetSettingsPath();
    std::ifstream f(path);
    if (!f) return;
    try {
        json j;
        f >> j;
        
        if (j.contains("window")) {
            g_windowX = j["window"].value("x", CW_USEDEFAULT);
            g_windowY = j["window"].value("y", CW_USEDEFAULT);
        }

        g_state.autostart = j.value("autostart", false);
        g_state.minimizeToTray = j.value("minimize_to_tray", true);
        g_state.autoApply = j.value("auto_apply", true);

        if (j.value("lang", "en") == "de") {
            g_lang = LANG_DE;
            g_str = &g_strDE;
        }

        if (j.contains("theme")) SetTheme(j["theme"]);

        g_state.red = j.value("red", 0);
        g_state.green = j.value("green", 34);
        g_state.blue = j.value("blue", 255);
        g_state.brightness = j.value("brightness", 4);
        g_state.speed = j.value("speed", 2);
        g_state.kbMode = j.value("kbMode", (int)KB_MODE_STATIC);
        g_state.edgeMode = j.value("edgeMode", (int)EDGE_MODE_STATIC);

        g_state.enableAura = j.value("enableAura", true);
        g_state.enableMouse = j.value("enableMouse", true);
        g_state.enableKeyboard = j.value("enableKeyboard", true);
        g_state.enableRAM = j.value("enableRAM", true);
        g_state.enableEdge = j.value("enableEdge", true);

        if (j.contains("last_profile")) {
            std::string pn = j["last_profile"];
            g_state.lastProfile = std::wstring(pn.begin(), pn.end());
        }
    } catch (...) {}
}

void AppendStatus(const wchar_t* text) {
    std::wstring currentText;
    {
        std::lock_guard<std::mutex> lock(g_state.statusMutex);
        g_state.statusLog += text;
        g_state.statusLog += L"\r\n";
        currentText = g_state.statusLog;
    }
    
    if (g_state.hStatus) {
        // Call UI functions without holding the lock to prevent deadlock
        // when worker threads update the log while UI thread is busy.
        SetWindowTextW(g_state.hStatus, currentText.c_str());
        SendMessage(g_state.hStatus, EM_SETSEL, currentText.length(), currentText.length());
        SendMessage(g_state.hStatus, EM_SCROLLCARET, 0, 0);
    }
}

void ClearStatus() {
    {
        std::lock_guard<std::mutex> lock(g_state.statusMutex);
        g_state.statusLog.clear();
    }
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
                SaveSettings();  // Remember last profile
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
    Channel channels[16];
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

    // Standard RGB headers - use IDs 0x02, 0x03...
    for (int i = 0; i < cfg.numRGBHeaders && cfg.numChannels < 16; i++) {
        cfg.channels[cfg.numChannels].present = true;
        cfg.channels[cfg.numChannels].ledCount = 1;  // Standard headers set as single zone
        cfg.channels[cfg.numChannels].addressable = false;
        cfg.channels[cfg.numChannels].directChannel = 0x02 + i;
        cfg.channels[cfg.numChannels].colorR = 0;
        cfg.channels[cfg.numChannels].colorG = 34;
        cfg.channels[cfg.numChannels].colorB = 255;
        cfg.channels[cfg.numChannels].enabled = true;
        sprintf(cfg.channels[cfg.numChannels].name, "RGB Header %d", i + 1);
        cfg.numChannels++;
    }

    // Diagnostic/Internal zones (Force scan if Mainboard was detected)
    if (cfg.numMainboardLEDs > 0 && cfg.numChannels < 16) {
        // Some PCH/IO zones are at 0x0B, 0x0C
        int extraZones[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C };
        for (int zoneId : extraZones) {
            bool alreadyExists = false;
            for (int j = 0; j < cfg.numChannels; j++) {
                if (cfg.channels[j].directChannel == zoneId) alreadyExists = true;
            }
            if (!alreadyExists && cfg.numChannels < 16) {
                cfg.channels[cfg.numChannels].present = true;
                cfg.channels[cfg.numChannels].ledCount = 30; // Assume 30 for scan
                cfg.channels[cfg.numChannels].directChannel = zoneId;
                cfg.channels[cfg.numChannels].enabled = true;
                sprintf(cfg.channels[cfg.numChannels].name, "Zone (ID 0x%02X)", zoneId);
                cfg.numChannels++;
            }
        }
    }

    // Addressable headers - use their index as direct_channel
    for (int i = 0; i < cfg.numAddressableHeaders && cfg.numChannels < 16; i++) {
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

    // Force Direct Mode for this channel (Command 0x43)
    // This is the "Bit" required to take control of internal motherboard LEDs
    uint8_t modeBuf[65];
    memset(modeBuf, 0, sizeof(modeBuf));
    modeBuf[0x00] = 0xEC;
    modeBuf[0x01] = 0x43;
    modeBuf[0x02] = channel;
    modeBuf[0x03] = 0xFF; // Mode "Direct/Software" (some boards use 0x01 Static)
    hid_write(dev, modeBuf, 65);
    Sleep(2);

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
            {0x00, 60}, {0x01, 120}, {0x02, 120}, {0x03, 60}, {0x04, 60}, {0x0B, 60}, {0x0C, 60}
        };
        for (int i = 0; i < 7; i++) {
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
    
    // We try multiple known offsets for Side LEDs
    uint16_t offsets[] = { 
        (uint16_t)(profile * 0x40 + 0x01 + 0x1a), // Standard Thyrus
        (uint16_t)(profile * 0x40 + 0x01 + 0x15), // Some Omnis variants
        (uint16_t)(0x1E)                          // Direct Edge ID
    };

    for (uint16_t off : offsets) {
        uint8_t edgeData[10] = {mode, 0x04, 0x02, 0x00, 0x00, cr, cg, cb, 0x00, 0x01};
        EVisionQuery(dev, 0x06, off, edgeData, 10, nullptr);
    }

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

// Forward declaration
bool SetGSkillRAM(uint8_t r, uint8_t g, uint8_t b);

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
// UI HELPER FUNCTIONS (moved from top of file)
//=============================================================================

// Oeffnet einen Farbauswahldialog und uebernimmt die gewaehlte Farbe in die UI
void PickColor() {
    CHOOSECOLORW cc = {0};
    static COLORREF customColors[16] = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = g_state.hWnd;
    COLORREF rgb = RGB(g_state.red, g_state.green, g_state.blue);
    cc.rgbResult = rgb;
    cc.lpCustColors = customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColorW(&cc)) {
        g_state.red = GetRValue(cc.rgbResult);
        g_state.green = GetGValue(cc.rgbResult);
        g_state.blue = GetBValue(cc.rgbResult);
        UpdateSliders();
        UpdatePreview();
        if (g_state.hEditHex) {
            wchar_t hex[10];
            swprintf(hex, 10, L"#%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
            SetWindowTextW(g_state.hEditHex, hex);
        }
    }
}

// Parst einen Hex-String wie #RRGGBB und setzt die Farbe
void ParseHexColor(const wchar_t* hex) {
    if (!hex) return;
    int r=0, g=0, b=0;
    if (wcslen(hex) == 7 && hex[0] == L'#') {
        swscanf(hex+1, L"%02x%02x%02x", &r, &g, &b);
    } else if (wcslen(hex) == 6) {
        swscanf(hex, L"%02x%02x%02x", &r, &g, &b);
    } else {
        return;
    }
    g_state.red = (uint8_t)r;
    g_state.green = (uint8_t)g;
    g_state.blue = (uint8_t)b;
    UpdateSliders();
    UpdatePreview();
}

// Aktualisiert die Farbvorschau in der UI
void UpdatePreview() {
    if (g_state.hPreview) {
        InvalidateRect(g_state.hPreview, NULL, TRUE);
    }
    // Update modern color preview
    g_colorPreview.r = g_state.red;
    g_colorPreview.g = g_state.green;
    g_colorPreview.b = g_state.blue;
}

// Setzt die Slider-Positionen auf die aktuellen RGB-Werte
void UpdateSliders() {
    if (g_state.hSliderR) SendMessage(g_state.hSliderR, TBM_SETPOS, TRUE, g_state.red);
    if (g_state.hSliderG) SendMessage(g_state.hSliderG, TBM_SETPOS, TRUE, g_state.green);
    if (g_state.hSliderB) SendMessage(g_state.hSliderB, TBM_SETPOS, TRUE, g_state.blue);
    if (g_state.hLabelRVal) {
        wchar_t buf[8]; swprintf(buf, 8, L"%d", g_state.red);
        SetWindowTextW(g_state.hLabelRVal, buf);
    }
    if (g_state.hLabelGVal) {
        wchar_t buf[8]; swprintf(buf, 8, L"%d", g_state.green);
        SetWindowTextW(g_state.hLabelGVal, buf);
    }
    if (g_state.hLabelBVal) {
        wchar_t buf[8]; swprintf(buf, 8, L"%d", g_state.blue);
        SetWindowTextW(g_state.hLabelBVal, buf);
    }
}

//=============================================================================
// PRESET & UI UPDATE FUNCTIONS
//=============================================================================

void SetPresetColor(int r, int g, int b) {
    g_state.red = (uint8_t)r;
    g_state.green = (uint8_t)g;
    g_state.blue = (uint8_t)b;
    UpdateSliders();
    UpdatePreview();
    if (g_state.hEditHex) {
        wchar_t hex[10];
        swprintf(hex, 10, L"#%02X%02X%02X", r, g, b);
        SetWindowTextW(g_state.hEditHex, hex);
    }
    // Save to settings
    SaveSettings();
    if (g_state.autoApply) {
        std::thread(ApplyColors).detach();
    }
}

void UpdateAllControls() {
    UpdateSliders();
    UpdatePreview();
    if (g_state.hEditHex) {
        wchar_t hex[10];
        swprintf(hex, 10, L"#%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
        SetWindowTextW(g_state.hEditHex, hex);
    }
    if (g_state.hCheckAura) SendMessage(g_state.hCheckAura, BM_SETCHECK, g_state.enableAura ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckMouse) SendMessage(g_state.hCheckMouse, BM_SETCHECK, g_state.enableMouse ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckKeyboard) SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, g_state.enableKeyboard ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckRAM) SendMessage(g_state.hCheckRAM, BM_SETCHECK, g_state.enableRAM ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckEdge) SendMessage(g_state.hCheckEdge, BM_SETCHECK, g_state.enableEdge ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckAutostart) SendMessage(g_state.hCheckAutostart, BM_SETCHECK, g_state.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckMinimizeTray) SendMessage(g_state.hCheckMinimizeTray, BM_SETCHECK, g_state.minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hCheckAutoApply) SendMessage(g_state.hCheckAutoApply, BM_SETCHECK, g_state.autoApply ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_state.hSliderBrightness) SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, g_state.brightness);
    if (g_state.hSliderSpeed) SendMessage(g_state.hSliderSpeed, TBM_SETPOS, TRUE, g_state.speed);
    
    // Sync Combos
    if (g_state.hComboKbMode) SendMessage(g_state.hComboKbMode, CB_SETCURSEL, -1, 0); // Reset
    if (g_state.hComboKbMode) {
        // Map mode to index if needed, for now assume index matches enum or set directly
        SendMessage(g_state.hComboKbMode, CB_SETCURSEL, g_state.kbMode, 0);
    }
    if (g_state.hComboEdgeMode) SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, g_state.edgeMode, 0);
    
    // Sync Profile Combo selection
    if (g_state.hComboProfiles && !g_state.currentProfile.empty()) {
        int idx = (int)SendMessageW(g_state.hComboProfiles, CB_FINDSTRINGEXACT, -1, (LPARAM)g_state.currentProfile.c_str());
        if (idx != CB_ERR) {
            SendMessage(g_state.hComboProfiles, CB_SETCURSEL, idx, 0);
        } else {
            SetWindowTextW(g_state.hComboProfiles, g_state.currentProfile.c_str());
        }
    }
}

//=============================================================================
// TRAY ICON FUNCTIONS
//=============================================================================

void MinimizeToTray() {
    NOTIFYICONDATAW& nid = g_state.nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_state.hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
    wcscpy_s(nid.szTip, APP_NAME);
    Shell_NotifyIconW(NIM_ADD, &nid);
    ShowWindow(g_state.hWnd, SW_HIDE);
    g_state.minimizedToTray = true;
}

void RestoreFromTray() {
    ShowWindow(g_state.hWnd, SW_SHOW);
    SetForegroundWindow(g_state.hWnd);
    g_state.minimizedToTray = false;
}

void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_state.nid);
}

void ShowTrayMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Show");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_BLUE, L"Blue");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RED, L"Red");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_GREEN, L"Green");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_WHITE, L"White");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OFF, L"Off");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_STANDBY, L"Standby");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHUTDOWN, L"Shutdown");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTART, L"Restart");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

//=============================================================================
// CHANNEL SETTINGS DIALOG
//=============================================================================

INT_PTR CALLBACK ChannelSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowTextW(hDlg, g_str->csTitle);
        // Create channel correction controls
        int y = 30;
        for (int i = 0; i < 8; i++) {
            wchar_t label[64];
            swprintf(label, 64, L"ASUS Ch %d", i);
            CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, 10, y, 100, 20, hDlg, NULL, NULL, NULL);

            CreateWindowW(L"STATIC", L"R:", WS_CHILD | WS_VISIBLE, 115, y, 15, 20, hDlg, NULL, NULL, NULL);
            HWND hR = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                130, y-2, 80, 25, hDlg, (HMENU)(INT_PTR)(7000 + i*3), NULL, NULL);
            SendMessage(hR, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(hR, TBM_SETPOS, TRUE, g_channels.aura_channels[i].red_adjust);

            CreateWindowW(L"STATIC", L"G:", WS_CHILD | WS_VISIBLE, 215, y, 15, 20, hDlg, NULL, NULL, NULL);
            HWND hG = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                230, y-2, 80, 25, hDlg, (HMENU)(INT_PTR)(7001 + i*3), NULL, NULL);
            SendMessage(hG, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(hG, TBM_SETPOS, TRUE, g_channels.aura_channels[i].green_adjust);

            CreateWindowW(L"STATIC", L"B:", WS_CHILD | WS_VISIBLE, 315, y, 15, 20, hDlg, NULL, NULL, NULL);
            HWND hB = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                330, y-2, 80, 25, hDlg, (HMENU)(INT_PTR)(7002 + i*3), NULL, NULL);
            SendMessage(hB, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(hB, TBM_SETPOS, TRUE, g_channels.aura_channels[i].blue_adjust);
            y += 30;
        }
        CreateWindowW(L"STATIC", g_str->csHint, WS_CHILD | WS_VISIBLE, 10, y+5, 400, 20, hDlg, NULL, NULL, NULL);
        CreateWindowW(L"BUTTON", g_str->csSaveClose, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, y+30, 120, 28, hDlg, (HMENU)IDOK, NULL, NULL);
        CreateWindowW(L"BUTTON", g_str->csResetAll, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, y+30, 100, 28, hDlg, (HMENU)IDRETRY, NULL, NULL);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            for (int i = 0; i < 8; i++) {
                g_channels.aura_channels[i].red_adjust = (int)SendDlgItemMessage(hDlg, 7000+i*3, TBM_GETPOS, 0, 0);
                g_channels.aura_channels[i].green_adjust = (int)SendDlgItemMessage(hDlg, 7001+i*3, TBM_GETPOS, 0, 0);
                g_channels.aura_channels[i].blue_adjust = (int)SendDlgItemMessage(hDlg, 7002+i*3, TBM_GETPOS, 0, 0);
            }
            g_channels.Save();
            EndDialog(hDlg, IDOK);
        } else if (LOWORD(wParam) == IDRETRY) {
            for (int i = 0; i < 8; i++) {
                g_channels.aura_channels[i].red_adjust = 100;
                g_channels.aura_channels[i].green_adjust = 100;
                g_channels.aura_channels[i].blue_adjust = 100;
                SendDlgItemMessage(hDlg, 7000+i*3, TBM_SETPOS, TRUE, 100);
                SendDlgItemMessage(hDlg, 7001+i*3, TBM_SETPOS, TRUE, 100);
                SendDlgItemMessage(hDlg, 7002+i*3, TBM_SETPOS, TRUE, 100);
            }
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        break;
    }
    return FALSE;
}

void ShowChannelSettingsDialog(HWND hWnd) {
    // Create a dialog template in memory
    BYTE dlgTemplate[512] = {0};
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)dlgTemplate;
    pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    pDlg->cx = 230; pDlg->cy = 200;
    DialogBoxIndirectW(GetModuleHandle(NULL), pDlg, hWnd, ChannelSettingsDlgProc);
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
        // Handle channel checkboxes
        for (int i = 0; i < g_asusTest->numChannels; i++) {
            if (id == ID_ASUS_CH_CHECK_BASE + i) {
                g_asusTest->channelActive[i] = (SendMessage(g_asusTest->hCheckBox[i], BM_GETCHECK, 0, 0) == BST_CHECKED);
                break;
            }
        }
        if (id == ID_ASUS_TEST_ALL) {
            for (int i = 0; i < g_asusTest->numChannels; i++) {
                if (g_asusTest->channelActive[i]) {
                    TestAsusChannel(i, g_asusTest->channelR[i], g_asusTest->channelG[i], g_asusTest->channelB[i]);
                }
            }
            SetWindowTextW(g_asusTest->hStatus, L"Alle Kanaele angewendet");
        }
        else if (id == ID_ASUS_RESET) {
            for (int i = 0; i < g_asusTest->numChannels; i++) {
                TestAsusChannel(i, 0, 0, 0);
            }
            SetWindowTextW(g_asusTest->hStatus, L"Alle Kanaele ausgeschaltet");
        }
        else if (id == ID_ASUS_CLOSE) {
            // Save colors back to hardware config
            if (g_asusHwConfig.valid) {
                for (int i = 0; i < g_asusTest->numChannels && i < g_asusHwConfig.numChannels; i++) {
                    g_asusHwConfig.channels[i].colorR = g_asusTest->channelR[i];
                    g_asusHwConfig.channels[i].colorG = g_asusTest->channelG[i];
                    g_asusHwConfig.channels[i].colorB = g_asusTest->channelB[i];
                    g_asusHwConfig.channels[i].enabled = g_asusTest->channelActive[i];
                }
                SaveAsusHardwareConfig();
            }
            EndDialog(hWnd, IDOK);
        }
        break;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            // Apply the pending channel
            static int s_lastChannel = -1;
            for (int ch = 0; ch < g_asusTest->numChannels; ch++) {
                if (g_asusTest->channelActive[ch]) {
                    TestAsusChannel(ch, g_asusTest->channelR[ch], g_asusTest->channelG[ch], g_asusTest->channelB[ch]);
                }
            }
        }
        break;

    case WM_CLOSE:
        EndDialog(hWnd, IDCANCEL);
        break;

    case WM_DESTROY:
        if (g_asusTest) { delete g_asusTest; g_asusTest = nullptr; }
        break;
    }
    return FALSE;
}

void ShowAsusTestDialog(HWND hWnd) {
    // Calculate dialog size based on number of channels
    int numCh = g_asusHwConfig.valid ? g_asusHwConfig.numChannels : 3;
    if (numCh > 8) numCh = 8;
    if (numCh < 1) numCh = 1;
    int dlgHeight = 35 + numCh * 105 + 80;

    // Create dialog template in memory
    BYTE dlgTemplate[512] = {0};
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)dlgTemplate;
    pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    pDlg->cx = 260; pDlg->cy = (short)(dlgHeight * 8 / 13);
    DialogBoxIndirectW(GetModuleHandle(NULL), pDlg, hWnd, AsusTestDlgProc);
}

//=============================================================================
// MAIN WINDOW PROCEDURE
//=============================================================================

// Helper: Create a tooltip for a control
void AddTooltip(HWND hTip, HWND hCtrl, const wchar_t* text) {
    if (!hTip || !hCtrl || !text) return;
    TOOLINFOW ti = {sizeof(ti)};
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = GetParent(hCtrl);
    ti.uId = (UINT_PTR)hCtrl;
    ti.lpszText = (LPWSTR)text;
    SendMessage(hTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE: {
        LogDebug("WM_CREATE started");
        HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
        HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        // Create tooltip control
        g_state.hTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hWnd, NULL, hInst, NULL);
        SendMessage(g_state.hTooltip, TTM_SETMAXTIPWIDTH, 0, 300);
        SendMessage(g_state.hTooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);

        int clientW = WINDOW_WIDTH - 2 * MARGIN;
        int groupW = clientW;
        int innerW = groupW - 2 * GROUP_PADDING;
        int curY = MARGIN;
        LogDebug("WM_CREATE variables initialized");

        // ============= COLOR SELECTION GROUP =============
        g_cards[0].rect = {MARGIN, curY, MARGIN + groupW, curY + GROUP_TITLE_H + 240};
        wcscpy_s(g_cards[0].title, g_str->colorSelection);
        g_numCards = 1;

        int gx = MARGIN + GROUP_PADDING;
        int gy = curY + GROUP_TITLE_H;
        int innerAvailableW = groupW - 2 * GROUP_PADDING;

        // 5 rows evenly dividing the 240px card body height
        int rowsTotalH = 240;
        int rowH = 48;

        int row1Y = gy + (rowH * 0) + (rowH - 20) / 2;
        int row2Y = gy + (rowH * 1) + (rowH - 20) / 2;
        int row3Y = gy + (rowH * 2) + (rowH - 20) / 2;

        // Color Preview (Left side, repositioned with equal gaps)
        int previewSize = 100;
        int previewGap = 20;
        int previewX = gx + previewGap;
        int previewY = gy + previewGap;
        
        g_state.hPreview = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
            previewX, previewY, previewSize, previewSize, hWnd, (HMENU)ID_STATIC_PREVIEW, hInst, NULL);
        g_colorPreview.rect = {previewX, previewY, previewX + previewSize, previewY + previewSize};
        SetWindowSubclass(g_state.hPreview, ColorPreviewSubclassProc, 1, 0);
        AddTooltip(g_state.hTooltip, g_state.hPreview, g_str->tipColorPreview);

        // Hex input (Directly under preview)
        int hexInputY = previewY + previewSize + 15;
        int hexLblW = 35;
        int hexInputW = 80;
        int hexStartX = previewX + (previewSize - (hexLblW + hexInputW + 2)) / 2;
        
        CreateWindowW(L"STATIC", g_str->hex, WS_CHILD | WS_VISIBLE, hexStartX, hexInputY + 4, hexLblW, 18, hWnd, NULL, hInst, NULL);
        g_state.hEditHex = CreateWindowExW(0, L"EDIT", L"#0022FF",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_UPPERCASE, 
            hexStartX + hexLblW + 2, hexInputY, hexInputW, 24, hWnd, (HMENU)ID_EDIT_HEX, hInst, NULL);
        SetWindowSubclass(g_state.hEditHex, EditBorderSubclassProc, 1, 0);
        AddTooltip(g_state.hTooltip, g_state.hEditHex, g_str->tipHexInput);

        // Sliders (Right side, adjusted to avoid preview)
        int valueLabelW = 35;
        int maxSliderW = 200;
        int labelW = 40;
        int rightEdge = gx + innerAvailableW;
        
        // Dynamic slider width
        int sliderStartX = previewX + previewSize + 40; 
        int sliderW = rightEdge - sliderStartX - labelW - valueLabelW - 10;
        if (sliderW > maxSliderW) sliderW = maxSliderW;

        int valueLabelX = rightEdge - valueLabelW;
        int sliderX = valueLabelX - sliderW - 5;
        int labelX = sliderX - labelW - 5;

        // R slider row
        CreateWindowW(L"STATIC", g_str->red, WS_CHILD | WS_VISIBLE | SS_RIGHT, labelX, row1Y+2, labelW, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, row1Y, sliderW, 20, hWnd, (HMENU)ID_SLIDER_R, hInst, NULL);
        SendMessage(g_state.hSliderR, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(g_state.hSliderR, TBM_SETPOS, TRUE, g_state.red);
        SetWindowSubclass(g_state.hSliderR, SliderSubclassProc, 1, (DWORD_PTR)&g_sliderR);
        g_sliderR.slider.hWnd = g_state.hSliderR;
        g_sliderR.slider.channel = 'R';
        g_sliderR.slider.maxValue = 255;
        g_state.hLabelRVal = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE | SS_CENTER,
            valueLabelX, row1Y+2, valueLabelW, 18, hWnd, NULL, hInst, NULL);
        AddTooltip(g_state.hTooltip, g_state.hSliderR, g_str->tipSliderR);

        // G slider row
        CreateWindowW(L"STATIC", g_str->green, WS_CHILD | WS_VISIBLE | SS_RIGHT, labelX, row2Y+2, labelW, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, row2Y, sliderW, 20, hWnd, (HMENU)ID_SLIDER_G, hInst, NULL);
        SendMessage(g_state.hSliderG, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(g_state.hSliderG, TBM_SETPOS, TRUE, g_state.green);
        SetWindowSubclass(g_state.hSliderG, SliderSubclassProc, 2, (DWORD_PTR)&g_sliderG);
        g_sliderG.slider.hWnd = g_state.hSliderG;
        g_sliderG.slider.channel = 'G';
        g_sliderG.slider.maxValue = 255;
        g_state.hLabelGVal = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE | SS_CENTER,
            valueLabelX, row2Y+2, valueLabelW, 18, hWnd, NULL, hInst, NULL);
        AddTooltip(g_state.hTooltip, g_state.hSliderG, g_str->tipSliderG);

        // B slider row
        CreateWindowW(L"STATIC", g_str->blue, WS_CHILD | WS_VISIBLE | SS_RIGHT, labelX, row3Y+2, labelW, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            sliderX, row3Y, sliderW, 20, hWnd, (HMENU)ID_SLIDER_B, hInst, NULL);
        SendMessage(g_state.hSliderB, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(g_state.hSliderB, TBM_SETPOS, TRUE, g_state.blue);
        SetWindowSubclass(g_state.hSliderB, SliderSubclassProc, 3, (DWORD_PTR)&g_sliderB);
        g_sliderB.slider.hWnd = g_state.hSliderB;
        g_sliderB.slider.channel = 'B';
        g_sliderB.slider.maxValue = 255;
        g_state.hLabelBVal = CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE | SS_CENTER,
            valueLabelX, row3Y+2, valueLabelW, 18, hWnd, NULL, hInst, NULL);
        AddTooltip(g_state.hTooltip, g_state.hSliderB, g_str->tipSliderB);

        // Color preset buttons
        int row5Y = gy + (rowH * 4) + (rowH - BTN_H) / 2;
        int px = gx;
        int pbwTotalAvailable = innerAvailableW - (6 * BTN_GAP);
        int pbw = pbwTotalAvailable / 7;
        int rem = pbwTotalAvailable % 7;
        
        struct { int id; const wchar_t* label; } presets[] = {
            {ID_BTN_PRESET_BLUE, g_str->presetBlue}, {ID_BTN_PRESET_RED, g_str->presetRed},
            {ID_BTN_PRESET_GREEN, g_str->presetGreen}, {ID_BTN_PRESET_CYAN, g_str->presetCyan},
            {ID_BTN_PRESET_PURPLE, g_str->presetPurple}, {ID_BTN_PRESET_WHITE, g_str->presetWhite},
            {ID_BTN_PRESET_OFF, g_str->presetOff}
        };
        for (int i = 0; i < 7; i++) {
            int currentPbw = pbw + (i < rem ? 1 : 0);
            CreateWindowW(L"BUTTON", presets[i].label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                px, row5Y, currentPbw, BTN_H, hWnd, (HMENU)(INT_PTR)presets[i].id, hInst, NULL);
            px += currentPbw + BTN_GAP;
        }

        curY = g_cards[0].rect.bottom + GROUP_MARGIN;
        // ============= EFFECTS GROUP =============
        g_cards[1].rect = {MARGIN, curY, MARGIN + groupW, curY + GROUP_TITLE_H + 85};
        wcscpy_s(g_cards[1].title, g_str->effects);
        g_numCards = 2;
        
        gx = MARGIN + GROUP_PADDING;
        gy = curY + GROUP_TITLE_H;

        // Keyboard mode combo
        CreateWindowW(L"STATIC", g_str->keyboardEffect, WS_CHILD | WS_VISIBLE, gx, gy+3, 70, 18, hWnd, NULL, hInst, NULL);
        g_state.hComboKbMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            gx+75, gy, MAX_COMBO_W, 200, hWnd, (HMENU)ID_COMBO_KB_MODE, hInst, NULL);
        const wchar_t* kbModes[] = {g_str->modeStatic, g_str->modeBreathing, L"Spectrum", L"Wave Short",
            L"Wave Long", L"Color Wheel", g_str->modeReactive, L"Ripple", L"Starlight", g_str->modeRainbow, L"Hurricane"};
        for (int i = 0; i < 11; i++) SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)kbModes[i]);
        SendMessage(g_state.hComboKbMode, CB_SETCURSEL, 0, 0);
        AddTooltip(g_state.hTooltip, g_state.hComboKbMode, g_str->tipKeyboardMode);

        // Edge mode combo
        int edgeX = gx + 75 + MAX_COMBO_W + 20;
        CreateWindowW(L"STATIC", g_str->edgeEffect, WS_CHILD | WS_VISIBLE, edgeX, gy+3, 50, 18, hWnd, NULL, hInst, NULL);
        g_state.hComboEdgeMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            edgeX+55, gy, MAX_COMBO_W, 200, hWnd, (HMENU)ID_COMBO_EDGE_MODE, hInst, NULL);
        const wchar_t* edgeModes[] = {g_str->edgeStatic, g_str->edgeBreathing, g_str->edgeWave, g_str->edgeSpectrum, g_str->edgeOff};
        for (int i = 0; i < 5; i++) SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)edgeModes[i]);
        SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, 0, 0);
        AddTooltip(g_state.hTooltip, g_state.hComboEdgeMode, g_str->tipEdgeMode);

        // Brightness slider
        int effY2 = gy + CTRL_H + ITEM_SPACING + 15;
        CreateWindowW(L"STATIC", g_str->brightness, WS_CHILD | WS_VISIBLE, gx, effY2+2, 70, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderBrightness = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            gx+75, effY2, 150, 20, hWnd, (HMENU)ID_SLIDER_BRIGHTNESS, hInst, NULL);
        SendMessage(g_state.hSliderBrightness, TBM_SETRANGE, TRUE, MAKELPARAM(0, 4));
        SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, g_state.brightness);
        SetWindowSubclass(g_state.hSliderBrightness, SliderSubclassProc, 4, (DWORD_PTR)&g_sliderBrightness);
        g_sliderBrightness.slider.hWnd = g_state.hSliderBrightness;
        g_sliderBrightness.slider.channel = 'X';
        g_sliderBrightness.slider.maxValue = 4;

        // Speed slider
        CreateWindowW(L"STATIC", g_str->speed, WS_CHILD | WS_VISIBLE, edgeX, effY2+2, 50, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderSpeed = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
            edgeX+55, effY2, 150, 20, hWnd, (HMENU)ID_SLIDER_SPEED, hInst, NULL);
        SendMessage(g_state.hSliderSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(0, 5));
        SendMessage(g_state.hSliderSpeed, TBM_SETPOS, TRUE, g_state.speed);
        SetWindowSubclass(g_state.hSliderSpeed, SliderSubclassProc, 5, (DWORD_PTR)&g_sliderSpeed);
        g_sliderSpeed.slider.hWnd = g_state.hSliderSpeed;
        g_sliderSpeed.slider.channel = 'X';
        g_sliderSpeed.slider.maxValue = 5;
        AddTooltip(g_state.hTooltip, g_state.hSliderSpeed, g_str->tipSpeed);
        curY = g_cards[1].rect.bottom + GROUP_MARGIN;

        // ============= DEVICES GROUP =============
        g_cards[2].rect = {MARGIN, curY, MARGIN + groupW, curY + GROUP_TITLE_H + 65};
        wcscpy_s(g_cards[2].title, g_str->devices);
        g_numCards = 3;
        
        gx = MARGIN + GROUP_PADDING;
        gy = curY + GROUP_TITLE_H;
        // Device checkboxes (first row)
        // Set the positioning step wider than the control width to prevent 
        // expanded client areas (from Modern UI glow) from overlapping.
        int ck_width = 105;
        int ck_step = 120;
        g_state.hCheckAura = CreateWindowW(L"BUTTON", L"Aura", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_AURA, hInst, NULL);
        g_state.hCheckMouse = CreateWindowW(L"BUTTON", L"Mouse", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_MOUSE, hInst, NULL);
        g_state.hCheckKeyboard = CreateWindowW(L"BUTTON", L"Keyboard", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step*2, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_KEYBOARD, hInst, NULL);
        g_state.hCheckRAM = CreateWindowW(L"BUTTON", L"RAM", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step*3, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_RAM, hInst, NULL);
        g_state.hCheckEdge = CreateWindowW(L"BUTTON", L"Edge", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step*4, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_EDGE, hInst, NULL);
        SendMessage(g_state.hCheckAura, BM_SETCHECK, g_state.enableAura ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckMouse, BM_SETCHECK, g_state.enableMouse ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, g_state.enableKeyboard ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckRAM, BM_SETCHECK, g_state.enableRAM ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckEdge, BM_SETCHECK, g_state.enableEdge ? BST_CHECKED : BST_UNCHECKED, 0);

        // Utility buttons (second row)
        int btnY2 = gy + 24;
        CreateWindowW(L"BUTTON", g_str->channelCorrection, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            gx, btnY2, 100, BTN_H, hWnd, (HMENU)ID_BTN_CHANNEL_SETTINGS, hInst, NULL);
        CreateWindowW(L"BUTTON", L"ASUS Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            gx+106, btnY2, 80, BTN_H, hWnd, (HMENU)ID_BTN_ASUS_TEST, hInst, NULL);
        CreateWindowW(L"BUTTON", L"HID Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            gx+192, btnY2, 80, BTN_H, hWnd, (HMENU)ID_BTN_HID_RESET, hInst, NULL);
        curY = g_cards[2].rect.bottom + GROUP_MARGIN;

        // ============= PROFILES & SETTINGS GROUP =============
        g_cards[3].rect = {MARGIN, curY, MARGIN + groupW, curY + GROUP_TITLE_H + 65};
        wcscpy_s(g_cards[3].title, g_str->profilesSettings);
        g_numCards = 4;
        gx = MARGIN + GROUP_PADDING;
        gy = curY + GROUP_TITLE_H;

        // Profile row
        // Account for +5px glow expansion on each side of buttons/combos:
        // leave 15px gap between elements (5px own expansion + 5px neighbor + 5px margin)
        int profLabelW = 50;
        int profComboW = 140;
        int profBtnW = 75;
        int profGap = 15;  // gap between expanded controls
        int profX = gx;
        CreateWindowW(L"STATIC", g_str->profile, WS_CHILD | WS_VISIBLE, profX, gy+3, profLabelW, 18, hWnd, NULL, hInst, NULL);
        profX += profLabelW + 5;
        g_state.hComboProfiles = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
            profX, gy, profComboW, 200, hWnd, (HMENU)ID_COMBO_PROFILES, hInst, NULL);
        profX += profComboW + profGap;
        CreateWindowW(L"BUTTON", g_str->save, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            profX, gy, profBtnW, BTN_H, hWnd, (HMENU)ID_BTN_SAVE_PROFILE, hInst, NULL);
        profX += profBtnW + profGap;
        CreateWindowW(L"BUTTON", g_str->load, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            profX, gy, profBtnW, BTN_H, hWnd, (HMENU)ID_BTN_LOAD_PROFILE, hInst, NULL);

        // Settings checkboxes row
        int setY = gy + CTRL_H + ITEM_SPACING;
        g_state.hCheckAutostart = CreateWindowW(L"BUTTON", g_str->autostart, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_AUTOSTART, hInst, NULL);
        g_state.hCheckMinimizeTray = CreateWindowW(L"BUTTON", g_str->tray, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_MINIMIZE_TRAY, hInst, NULL);
        g_state.hCheckAutoApply = CreateWindowW(L"BUTTON", g_str->autoApply, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            gx+ck_step*2, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_AUTO_APPLY, hInst, NULL);
        SendMessage(g_state.hCheckAutostart, BM_SETCHECK, g_state.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckMinimizeTray, BM_SETCHECK, g_state.minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckAutoApply, BM_SETCHECK, g_state.autoApply ? BST_CHECKED : BST_UNCHECKED, 0);
        curY = g_cards[3].rect.bottom + GROUP_MARGIN;

        // ============= ACTION BUTTONS =============
        CreateWindowW(L"BUTTON", g_str->apply, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            MARGIN, curY, MAX_BUTTON_W + 20, BTN_H + 4, hWnd, (HMENU)ID_BTN_APPLY, hInst, NULL);
        CreateWindowW(L"BUTTON", g_str->theme, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            MARGIN + MAX_BUTTON_W + 30, curY, 65, BTN_H, hWnd, (HMENU)ID_BTN_THEME, hInst, NULL);
        CreateWindowW(L"BUTTON", L"DE/EN", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            MARGIN + MAX_BUTTON_W + 100, curY, 55, BTN_H, hWnd, (HMENU)ID_BTN_LANG, hInst, NULL);
        curY += BTN_H + 8;

        // ============= STATUS LOG =============
        g_state.hStatus = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            MARGIN, curY, groupW, STATUS_H, hWnd, (HMENU)ID_STATIC_STATUS, hInst, NULL);
        SetWindowSubclass(g_state.hStatus, EditBorderSubclassProc, 1, 0);
        // Apply Explorer theme for better scrollbars
        SetWindowTheme(g_state.hStatus, L"Explorer", NULL);

        // Apply font to all child windows
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            
            // Auto-subclass all buttons and checkboxes for Modern UI
            wchar_t className[256];
            GetClassNameW(hChild, className, 256);
            bool expand = false;
            
            if (wcscmp(className, L"Button") == 0 || wcscmp(className, L"BUTTON") == 0) {
                SetWindowSubclass(hChild, BtnCheckboxSubclassProc, 1, 0);
                expand = true;
            } else if (wcscmp(className, L"ComboBox") == 0 || wcscmp(className, L"COMBOBOX") == 0) {
                SetWindowSubclass(hChild, ComboSubclassProc, 1, 0);
                // Do NOT expand comboboxes - glow expansion breaks native dropdown functionality
                expand = false;
                
                // Find and fix the internal Edit control (inside CBS_DROPDOWN)
                HWND hEdit = FindWindowExW(hChild, NULL, L"EDIT", NULL);
                if (hEdit) {
                    SetWindowSubclass(hEdit, EditSubclassProc, 1, 0);
                    // Remove native border from internal Edit
                    SetWindowLong(hEdit, GWL_EXSTYLE, GetWindowLong(hEdit, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                    // Position Edit so it doesn't overlap our custom arrow (which is at end - 30)
                    RECT rc; GetClientRect(hChild, &rc);
                    SetWindowPos(hEdit, NULL, 10, 4, rc.right - 40, rc.bottom - 8, SWP_NOZORDER | SWP_FRAMECHANGED);
                }
            } else if (wcscmp(className, L"Static") == 0 || wcscmp(className, L"STATIC") == 0) {
                LONG style = GetWindowLong(hChild, GWL_STYLE);
                if (!(style & SS_OWNERDRAW)) { // Don't override the Color Preview box
                    SetWindowSubclass(hChild, StaticSubclassProc, 1, 0);
                }
            } else if (wcscmp(className, L"msctls_trackbar32") == 0 || wcscmp(className, L"Trackbar") == 0) {
                // Slider expansion handled exclusively to 10px instead of 5px
                RECT rcC; GetWindowRect(hChild, &rcC);
                MapWindowPoints(HWND_DESKTOP, GetParent(hChild), (LPPOINT)&rcC, 2);
                SetWindowPos(hChild, NULL, rcC.left - 10, rcC.top - 10, (rcC.right - rcC.left) + 20, (rcC.bottom - rcC.top) + 20, SWP_NOZORDER);
                expand = false;
            }
            
            if (expand) {
                // Expand window by 5px on all sides to allow rendering glow effects naturally
                RECT rcC; GetWindowRect(hChild, &rcC);
                MapWindowPoints(HWND_DESKTOP, GetParent(hChild), (LPPOINT)&rcC, 2);
                SetWindowPos(hChild, NULL, rcC.left - 5, rcC.top - 5, (rcC.right - rcC.left) + 10, (rcC.bottom - rcC.top) + 10, SWP_NOZORDER);
            }
            
            return TRUE;
        }, (LPARAM)hFont);

        // Load saved settings into controls
        g_channels.Load();
        RefreshProfileList();
        UpdateSliders();
        UpdatePreview();

        // Load saved profile if any
        if (!g_state.lastProfile.empty()) {
            LoadProfile(g_state.lastProfile);
        }

        // Register global hotkeys
        RegisterHotKey(hWnd, ID_HOTKEY_BLUE, MOD_CONTROL | MOD_ALT, 'B');
        RegisterHotKey(hWnd, ID_HOTKEY_RED, MOD_CONTROL | MOD_ALT, 'R');
        RegisterHotKey(hWnd, ID_HOTKEY_GREEN, MOD_CONTROL | MOD_ALT, 'G');
        RegisterHotKey(hWnd, ID_HOTKEY_WHITE, MOD_CONTROL | MOD_ALT, 'W');
        RegisterHotKey(hWnd, ID_HOTKEY_OFF, MOD_CONTROL | MOD_ALT, '0');

        // Final sync of all UI elements to the loaded config
        UpdateAllControls();

        // Apply colors on startup (unless --no-apply)
        if (!g_skipApplyOnStart) {
            std::thread(ApplyColors).detach();
        }
        LogDebug("WM_CREATE finished");
        AppendStatus(L"OneClickRGB started");
        break;
    }
    case WM_ERASEBKGND:
        return 1; // Handled, prevent flicker

    case WM_PRINTCLIENT:
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = (msg == WM_PRINTCLIENT) ? (HDC)wParam : BeginPaint(hWnd, &ps);

        // Double buffering to prevent flicker
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        // Fill background
        HBRUSH bgBrush = CreateSolidBrush(g_currentTheme->bgWindowTop);
        FillRect(hdcMem, &rcClient, bgBrush);
        DeleteObject(bgBrush);

        // Draw modern cards globally
        for (int i = 0; i < g_numCards; i++) {
            g_cards[i].Draw(hdcMem);
        }

        // Blit and cleanup
        BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        if (msg == WM_PAINT) EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_HSCROLL: {
        HWND hSlider = (HWND)lParam;
        if (hSlider == g_state.hSliderR) {
            g_state.red = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        } else if (hSlider == g_state.hSliderG) {
            g_state.green = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        } else if (hSlider == g_state.hSliderB) {
            g_state.blue = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        } else if (hSlider == g_state.hSliderBrightness) {
            g_state.brightness = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        } else if (hSlider == g_state.hSliderSpeed) {
            g_state.speed = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        }
        UpdateSliders();
        UpdatePreview();
        // Update hex display
        if (g_state.hEditHex) {
            wchar_t hex[10];
            swprintf(hex, 10, L"#%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
            SetWindowTextW(g_state.hEditHex, hex);
        }
        // Live preview if enabled
        if (g_state.autoApply) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            SetTimer(hWnd, ID_TIMER_DEBOUNCE, 300, NULL);
        }
        break;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == ID_STATIC_PREVIEW) {
            Gdiplus::Graphics g(dis->hDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
            Gdiplus::RectF r(0, 0, (float)(dis->rcItem.right - dis->rcItem.left), (float)(dis->rcItem.bottom - dis->rcItem.top));
            
            float radius = 12.0f;
            Gdiplus::Color color((BYTE)g_state.red, (BYTE)g_state.green, (BYTE)g_state.blue);
            
            // Draw background fill with rounded corners
            DrawRoundedRect(g, r, radius, color, g_mTheme->border, 1.5f);
            
            // Add a subtle inner shadow/depth
            Gdiplus::Color shadowCol(40, 0, 0, 0);
            Gdiplus::Pen shadowPen(shadowCol, 2.0f);
            Gdiplus::GraphicsPath path;
            float d = radius * 2;
            path.AddArc(r.X + 1, r.Y + 1, d, d, 180, 90);
            path.AddLine(r.X + radius + 1, r.Y + 1, r.X + r.Width - radius - 1, r.Y + 1);
            path.AddArc(r.X + r.Width - d - 1, r.Y + 1, d, d, 270, 90);
            g.DrawPath(&shadowPen, &path);
            
            return TRUE;
        }
        break;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, g_currentTheme->textPrimary);
        SetBkMode(hdcCtrl, TRANSPARENT);
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, g_currentTheme->textPrimary);
        SetBkMode(hdcCtrl, OPAQUE);
        SetBkColor(hdcCtrl, g_currentTheme->bgControl);
        if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(g_currentTheme->bgControl);
        return (LRESULT)g_hBgBrush;
    }

    case WM_HOTKEY:
        switch (wParam) {
        case ID_HOTKEY_BLUE: SetPresetColor(0, 34, 255); std::thread(ApplyColors).detach(); break;
        case ID_HOTKEY_RED: SetPresetColor(255, 0, 0); std::thread(ApplyColors).detach(); break;
        case ID_HOTKEY_GREEN: SetPresetColor(0, 255, 0); std::thread(ApplyColors).detach(); break;
        case ID_HOTKEY_WHITE: SetPresetColor(255, 255, 255); std::thread(ApplyColors).detach(); break;
        case ID_HOTKEY_OFF: SetPresetColor(0, 0, 0); std::thread(ApplyColors).detach(); break;
        }
        break;

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
            SaveSettings();
        }
        else if (id == ID_COMBO_KB_MODE && code == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_state.hComboKbMode, CB_GETCURSEL, 0, 0);
            uint8_t modes[] = {KB_MODE_STATIC, KB_MODE_BREATHING, KB_MODE_SPECTRUM, KB_MODE_WAVE_SHORT,
                               KB_MODE_WAVE_LONG, KB_MODE_COLOR_WHEEL, KB_MODE_REACTIVE, KB_MODE_RIPPLE,
                               KB_MODE_STARLIGHT, KB_MODE_RAINBOW, KB_MODE_HURRICANE};
            if (sel >= 0 && sel < 11) {
                g_state.kbMode = modes[sel];
                SaveSettings();
            }
        }
        else if (id == ID_COMBO_EDGE_MODE && code == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_state.hComboEdgeMode, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel <= 5) {
                g_state.edgeMode = sel;
                SaveSettings();
            }
        }
        else if (id == ID_CHECK_AURA) { g_state.enableAura = (SendMessage(g_state.hCheckAura, BM_GETCHECK, 0, 0) == BST_CHECKED); SaveSettings(); }
        else if (id == ID_CHECK_MOUSE) { g_state.enableMouse = (SendMessage(g_state.hCheckMouse, BM_GETCHECK, 0, 0) == BST_CHECKED); SaveSettings(); }
        else if (id == ID_CHECK_KEYBOARD) { g_state.enableKeyboard = (SendMessage(g_state.hCheckKeyboard, BM_GETCHECK, 0, 0) == BST_CHECKED); SaveSettings(); }
        else if (id == ID_CHECK_RAM) { g_state.enableRAM = (SendMessage(g_state.hCheckRAM, BM_GETCHECK, 0, 0) == BST_CHECKED); SaveSettings(); }
        else if (id == ID_CHECK_EDGE) { g_state.enableEdge = (SendMessage(g_state.hCheckEdge, BM_GETCHECK, 0, 0) == BST_CHECKED); SaveSettings(); }
        else if (id == ID_CHECK_AUTOSTART) {
            g_state.autostart = (SendMessage(g_state.hCheckAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetAutoStart(g_state.autostart);
            SaveSettings();
        }
        else if (id == ID_CHECK_MINIMIZE_TRAY) {
            g_state.minimizeToTray = (SendMessage(g_state.hCheckMinimizeTray, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveSettings();
        }
        else if (id == ID_CHECK_AUTO_APPLY) {
            g_state.autoApply = (SendMessage(g_state.hCheckAutoApply, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SaveSettings();
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
            // Cycle through themes: Dark -> Light -> Colorblind -> Dark
            int nextTheme = (GetThemeId() + 1) % 3;
            SetTheme(nextTheme);
                    SaveSettings();

            // Restart application with focus using CreateProcess
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            wchar_t cmdLine[MAX_PATH + 64];
            swprintf(cmdLine, MAX_PATH + 64, L"\"%s\" --no-apply --foreground", exePath);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;
            PROCESS_INFORMATION pi;

            RemoveTrayIcon();
            CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            PostQuitMessage(0);
        }
        else if (id == ID_BTN_LANG) {
            // Toggle language and restart app (without re-applying colors)
            g_lang = (g_lang == LANG_EN) ? LANG_DE : LANG_EN;
            g_str = (g_lang == LANG_EN) ? &g_strEN : &g_strDE;
                    SaveSettings();

            // Restart application with focus using CreateProcess
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            wchar_t cmdLine[MAX_PATH + 64];
            swprintf(cmdLine, MAX_PATH + 64, L"\"%s\" --no-apply --foreground", exePath);

            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;
            PROCESS_INFORMATION pi;

            RemoveTrayIcon();
            CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
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
                SaveSettings();
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

// Forward declarations already at top: StaticSubclassProc
LRESULT CALLBACK StaticSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_ERASEBKGND) return 1;
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        POINT pt = {0, 0};
        MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);
        SetWindowOrgEx(hdcMem, pt.x, pt.y, NULL);
        SendMessage(GetParent(hWnd), WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
        SetWindowOrgEx(hdcMem, 0, 0, NULL);
        
        wchar_t text[256];
        GetWindowTextW(hWnd, text, 256);
        
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, g_currentTheme->textPrimary);
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
        
        DrawTextW(hdcMem, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        
        SelectObject(hdcMem, hOldFont);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    CustomSliderData* data = (CustomSliderData*)dwRefData;
    ModernSlider& mslider = data->slider;
    
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);
            
            // Ask parent to draw background exactly where we are
            POINT pt = {0, 0};
            MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);
            SetWindowOrgEx(hdcMem, pt.x, pt.y, NULL);
            SendMessage(GetParent(hWnd), WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
            SetWindowOrgEx(hdcMem, 0, 0, NULL);
            
            // Sync value
            mslider.value = (int)SendMessage(hWnd, TBM_GETPOS, 0, 0);
            mslider.rect = {10, 10, rc.right - 10, rc.bottom - 10}; // 10px inset for massive slider glow
            mslider.Draw(hdcMem);
            
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hOldBm);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (!mslider.isHovered) {
                mslider.isHovered = true;
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            mslider.isHovered = false;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case WM_LBUTTONDOWN:
            mslider.isDragging = true;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case WM_LBUTTONUP:
            mslider.isDragging = false;
            InvalidateRect(hWnd, NULL, FALSE);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK BtnCheckboxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);
            
            // Ask parent to draw background exactly where we are
            POINT pt = {0, 0};
            MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);
            SetWindowOrgEx(hdcMem, pt.x, pt.y, NULL);
            SendMessage(GetParent(hWnd), WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
            SetWindowOrgEx(hdcMem, 0, 0, NULL);
            
            LONG style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;
            bool isHovered = (bool)GetPropW(hWnd, L"hover");
            
            if (style == BS_AUTOCHECKBOX || style == BS_CHECKBOX) {
                ModernCheckbox cb;
                cb.rect = {5, 5, rc.right - 5, rc.bottom - 5}; // 5px inset for glow
                GetWindowTextW(hWnd, cb.text, 64);
                cb.isChecked = (SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
                cb.isEnabled = IsWindowEnabled(hWnd);
                cb.isHovered = isHovered;
                cb.Draw(hdcMem);
            } else {
                ModernButton btn;
                btn.rect = {5, 5, rc.right - 5, rc.bottom - 5}; // 5px inset for glow
                GetWindowTextW(hWnd, btn.text, 64);
                btn.isPressed = (SendMessage(hWnd, BM_GETSTATE, 0, 0) & BST_PUSHED);
                btn.isEnabled = IsWindowEnabled(hWnd);
                btn.isHovered = isHovered;
                btn.isAccent = (GetWindowLong(hWnd, GWLP_ID) == ID_BTN_APPLY);

                // Handle custom glows for preset buttons
                int btnId = GetWindowLong(hWnd, GWLP_ID);
                btn.hasCustomGlow = false;
                if (btnId >= ID_BTN_PRESET_BLUE && btnId <= ID_BTN_PRESET_CYAN) {
                    btn.hasCustomGlow = true;
                    if (btnId == ID_BTN_PRESET_BLUE) btn.customGlowColor = Gdiplus::Color(255, 0, 34, 255);
                    else if (btnId == ID_BTN_PRESET_RED) btn.customGlowColor = Gdiplus::Color(255, 255, 0, 0);
                    else if (btnId == ID_BTN_PRESET_GREEN) btn.customGlowColor = Gdiplus::Color(255, 0, 255, 0);
                    else if (btnId == ID_BTN_PRESET_CYAN) btn.customGlowColor = Gdiplus::Color(255, 0, 255, 255);
                    else if (btnId == ID_BTN_PRESET_PURPLE) btn.customGlowColor = Gdiplus::Color(255, 128, 0, 255);
                    else if (btnId == ID_BTN_PRESET_WHITE) btn.customGlowColor = Gdiplus::Color(255, 255, 255, 255);
                    else if (btnId == ID_BTN_PRESET_OFF) btn.customGlowColor = Gdiplus::Color(255, 100, 100, 100);
                }

                btn.Draw(hdcMem);
            }
            
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hOldBm);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (!GetPropW(hWnd, L"hover")) {
                SetPropW(hWnd, L"hover", (HANDLE)1);
                TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            RemovePropW(hWnd, L"hover");
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        // Native button control handles drawing its pushed state via WM_PAINT internally?
        // We need to invalidate when state changes (e.g. mouse down)
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
            InvalidateRect(hWnd, NULL, FALSE);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ComboSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);
        
        // Ask parent to draw background exactly where we are
        POINT pt = {0, 0};
        MapWindowPoints(hWnd, GetParent(hWnd), &pt, 1);
        SetWindowOrgEx(hdcMem, pt.x, pt.y, NULL);
        SendMessage(GetParent(hWnd), WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
        SetWindowOrgEx(hdcMem, 0, 0, NULL);
        
        ModernCombo combo;
        combo.rect = {0, 0, rc.right, rc.bottom}; // No inset - combo is not glow-expanded
        combo.isHovered = (bool)GetPropW(hWnd, L"hover");
        GetWindowTextW(hWnd, combo.selectedText, 128);
        combo.Draw(hdcMem);
        
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hWnd, &ps);
        return 0; // Skip native rendering wrapper
    } 
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(g_mTheme->textPrimary.GetR(), g_mTheme->textPrimary.GetG(), g_mTheme->textPrimary.GetB()));
        SetBkMode(hdc, TRANSPARENT);
        // Use bgControl brush (g_hBgBrush is initialized in WndProc::WM_CTLCOLOREDIT)
        if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(g_currentTheme->bgControl);
        return (LRESULT)g_hBgBrush;
    }
    case WM_MOUSEMOVE:
        if (!GetPropW(hWnd, L"hover")) {
            SetPropW(hWnd, L"hover", (HANDLE)1);
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        RECT rc; GetClientRect(hWnd, &rc);
        // If click is in the arrow area (right side), toggle dropdown
        if (x > rc.right - 35) {
            BOOL dropped = (BOOL)SendMessage(hWnd, CB_GETDROPPEDSTATE, 0, 0);
            SendMessage(hWnd, CB_SHOWDROPDOWN, !dropped, 0);
            return 0;
        }
        break;
    }
    case WM_MOUSELEAVE:
        RemovePropW(hWnd, L"hover");
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_ERASEBKGND) {
        // Transparent edit requires parent to draw background
        return 1;
    }
    if (uMsg == WM_PAINT) {
        // Standard edit controls don't support true transparency easily without help.
        // But since we handle WM_CTLCOLOREDIT in the parent, native painting works.
        // We just need to make sure we don't accidentally draw native boundaries.
    }
    // No parent invalidation here - it causes full UI flicker on focus changes.
    // Parent painting is already handled via WM_CTLCOLOREDIT.
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//=============================================================================
// CUSTOM UI SUBCLASSES
//=============================================================================

LRESULT CALLBACK ColorPreviewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return TRUE;
    case WM_LBUTTONDOWN:
        PickColor();
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK EditBorderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_NCPAINT: {
        // Draw modern rounded border
        HDC hdc = GetWindowDC(hWnd);
        RECT rc; GetWindowRect(hWnd, &rc);
        OffsetRect(&rc, -rc.left, -rc.top);
        
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        Gdiplus::RectF r((float)rc.left, (float)rc.top, (float)rc.right, (float)rc.bottom);
        
        // Clear background for rounded corners to work
        Gdiplus::SolidBrush bgBrush(g_mTheme->bgPrimary);
        g.FillRectangle(&bgBrush, r);
        
        // Draw the themed border
        DrawRoundedRect(g, r, 6.0f, Gdiplus::Color(0,0,0,0), g_mTheme->border, 1.0f);
        
        ReleaseDC(hWnd, hdc);
        return 0;
    }
    case WM_NCCALCSIZE:
        if (wParam) {
            NCCALCSIZE_PARAMS* pnc = (NCCALCSIZE_PARAMS*)lParam;
            pnc->rgrc[0].left += 4;
            pnc->rgrc[0].top += 4;
            pnc->rgrc[0].right -= 4;
            pnc->rgrc[0].bottom -= 4;
            return 0;
        }
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//=============================================================================
// MAIN
//=============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    LogDebug("WinMain started");
    // Check for command line flags
    bool startMinimized = (strstr(lpCmdLine, "--minimized") != nullptr);
    g_skipApplyOnStart = (strstr(lpCmdLine, "--no-apply") != nullptr);
    g_state.dryRun = (strstr(lpCmdLine, "--dry-run") != nullptr);
    bool forceForeground = (strstr(lpCmdLine, "--foreground") != nullptr);

    // Initialize GDI+ for PNG loading
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    LogDebug("GDI+ started");

    // Load saved settings (language, theme, colors, enables)
    LoadSettings();
    LogDebug("Settings loaded");
    SyncModernTheme();
    LogDebug("Modern theme synced");

    // Initialize ASUS hardware scan (checks for config changes)
    InitAsusHardware();

    // Full HID reset on startup to ensure proper device state after boot/restart
    if (!g_skipApplyOnStart) {
        FullHIDReset();
    }

    // Init common controls (including tooltips)
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
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

    // Build window title with version, admin status and dry-run indicator
    // Format: "OneClickRGB v3.5.1 - Complete RGB Control [Admin: ✅]"
    bool isAdmin = IsRunningAsAdmin();
    wchar_t titleSuffix[128];
    swprintf_s(titleSuffix, 128, g_str->windowTitle, isAdmin ? L"\x2705" : L"\x274C");
    swprintf_s(g_windowTitle, 256, L"%s v%s - %s", APP_NAME, APP_VERSION, titleSuffix);
    if (g_state.dryRun) {
        wcscat_s(g_windowTitle, 256, L" [DRY RUN]");
    }

    // Calculate window size to get exact client area
    RECT rc = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT - TITLEBAR_H};  // Subtract custom titlebar height we're not using
    AdjustWindowRect(&rc, style, FALSE);

    // Create window with saved position
    LogDebug("Creating Window...");
    g_state.hWnd = CreateWindowW(L"OneClickRGBClass", g_windowTitle,
        style,
        g_windowX, g_windowY, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);
    LogDebug("Window Created");

    if (!g_state.hWnd) {
        wchar_t err[256];
        swprintf_s(err, 256, L"CreateWindowW failed! Error: %lu", GetLastError());
        MessageBoxW(NULL, err, L"Error", MB_OK);
        return 1;
    }

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
        LogDebug("Window shown");

        // Force foreground when restarting from theme/language change
        if (forceForeground) {
            // Allow SetForegroundWindow to work by attaching to foreground thread
            DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
            DWORD appThread = GetCurrentThreadId();
            if (foreThread != appThread) {
                AttachThreadInput(foreThread, appThread, TRUE);
                BringWindowToTop(g_state.hWnd);
                SetForegroundWindow(g_state.hWnd);
                AttachThreadInput(foreThread, appThread, FALSE);
            } else {
                BringWindowToTop(g_state.hWnd);
                SetForegroundWindow(g_state.hWnd);
            }
        }
    }
    UpdateWindow(g_state.hWnd);

    LogDebug("Entering Message Loop");
    // Message loop with keyboard navigation support
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Enable Tab/Arrow key navigation and Enter to activate buttons
        if (!IsDialogMessage(g_state.hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Cleanup GDI+
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return (int)msg.wParam;
}