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
#include <condition_variable>
#include <objidl.h>
#include <nlohmann/json.hpp>
#include <gdiplus.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include "hidapi.h"
#include "channel_config.h"
#include "app_config.h"
#include "themes.h"
#include "modern_ui.h"

using json = nlohmann::json;

#include <fstream>
inline void LogDebug(const char* msg) {
    std::ofstream f("debug.log", std::ios::app);
    f << msg << std::endl;
}

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

//=============================================================================
// SETTINGS STUBS
// Forward declarations - full implementations follow after AppState is defined
//=============================================================================

void SaveAppSettings();
void LoadAppSettings();

// SaveSettings/LoadSettings are kept for call-site compatibility;
// they delegate to the unified SaveAppSettings/LoadAppSettings.
void SaveSettings() { SaveAppSettings(); }
void LoadSettings() { LoadAppSettings(); }

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

// Shared helper: paint control background.
// For popup dialogs (ChanSettingsDlg, AsusTestDlg) use solid dark fill.
// For the main window use WM_PRINTCLIENT to get the gradient background.
static inline void FillCtrlBackground(HDC hdcMem, HWND hCtrl, const RECT& rc) {
    wchar_t cls[64] = {};
    GetClassNameW(GetParent(hCtrl), cls, 64);
    bool isPopup = (wcscmp(cls, L"ChanSettingsDlg") == 0 ||
                    wcscmp(cls, L"AsusTestDlg")     == 0 ||
                    wcscmp(cls, L"#32770")           == 0);
    if (isPopup) {
        RECT rf = {0, 0, rc.right - rc.left, rc.bottom - rc.top};
        HBRUSH hBr = CreateSolidBrush(RGB(30, 34, 46));
        FillRect(hdcMem, &rf, hBr);
        DeleteObject(hBr);
    } else {
        POINT pt = {0, 0};
        MapWindowPoints(hCtrl, GetParent(hCtrl), &pt, 1);
        SetWindowOrgEx(hdcMem, pt.x, pt.y, NULL);
        SendMessage(GetParent(hCtrl), WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
        SetWindowOrgEx(hdcMem, 0, 0, NULL);
    }
}

//=============================================================================
// CONSTANTS & LAYOUT
//=============================================================================

#define APP_NAME L"OneClickRGB"
#define APP_VERSION L"3.5.1"
#define APP_VERSION_A "3.5.1"  // ANSI version for resources

// Layout constants (responsive)
#define WINDOW_WIDTH 640
// Tall enough for every group plus the full status log and a bottom margin.
// Client height = WINDOW_HEIGHT - TITLEBAR_H; the layout runs to
// 663 (end of the action buttons) + STATUS_H + MARGIN, so anything less clips
// the bottom of the log.
#define WINDOW_HEIGHT 870
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
// Trackbar height. ModernSlider draws a knob of radius 10 (20px across), plus a
// drop shadow offset 2px downwards and a 1.5px border straddling the edge. At
// the old 20px the control was exactly one knob tall, so the shadow and the
// lower half of the border were clipped away - the sliders looked cut off along
// the bottom. 28 leaves room for knob + shadow + border and matches BTN_H.
#define SLIDER_H 28
#define STATUS_H 160        // Status log height - must fit the apply output

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
#define APPLY_DEBOUNCE_MS 180
#define WM_APP_STATUS_APPEND (WM_APP + 10)
#define WM_APP_STATUS_CLEAR (WM_APP + 11)

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
HBRUSH g_hWndBgBrush = NULL; // Window-class background brush (dunkles Theme, verhindert weißen Blitz)

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
    L"Channel Color Correction", L"Save", L"Reset All",
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
    // Group titles (ä=\u00E4, ö=\u00F6, ü=\u00FC, ß=\u00DF, Ä=\u00C4, Ö=\u00D6, Ü=\u00DC)
    L"Farbe", L"Effekte", L"Ger\u00E4te", L"Profile",
    // Color section
    L"Rot", L"Gr\u00FCn", L"Blau", L"W\u00E4hlen", L"Hex:",
    // Effects section
    L"Tastatur", L"Rand-LEDs", L"Helligkeit", L"Tempo",
    // Devices section
    L"Kan\u00E4le...",
    // Profiles section
    L"Profil", L"Speichern", L"Laden", L"Autostart", L"Tray", L"Live",
    // Buttons
    L"ANWENDEN", L"Design",
    // Status
    L"Status", L"Bereit - Farbe w\u00E4hlen und Anwenden klicken",
    // Window title (version inserted at runtime via APP_VERSION)
    L"Komplette RGB-Steuerung [Admin: %s]",
    // Color presets
    L"Blau", L"Rot", L"Gr\u00FCn", L"Cyan", L"Lila", L"Wei\u00DF", L"Aus",
    // Keyboard modes
    L"Statisch", L"Atmend", L"Welle", L"Reaktiv", L"Regenbogen",
    // Edge modes
    L"Statisch", L"Atmend", L"Welle", L"Spektrum", L"Aus",
    // Channel settings dialog
    L"Kanal-Farbkorrektur", L"Speichern", L"Zur\u00FCcksetzen",
    L"100% = keine \u00C4nderung. Anpassen um Farbabweichungen zu korrigieren.",
    // Tooltips
    L"Rotkanal (0-255)\nRote Farbintensit\u00E4t einstellen",
    L"Gr\u00FCnkanal (0-255)\nGr\u00FCne Farbintensit\u00E4t einstellen",
    L"Blaukanal (0-255)\nBlaue Farbintensit\u00E4t einstellen",
    L"Farbvorschau\nZeigt die aktuell gew\u00E4hlte Farbe",
    L"Hex-Farbeingabe\nFarbe als #RRGGBB eingeben (z.B. #FF0000 f\u00FCr Rot)",
    L"Farbauswahl \u00F6ffnen\nBelibige Farbe visuell ausw\u00E4hlen",
    L"Schnellauswahl: Blau\nASUS Aura Standardfarbe",
    L"Schnellauswahl: Rot\nIntensives Rot",
    L"Schnellauswahl: Gr\u00FCn\nIntensives Gr\u00FCn",
    L"Schnellauswahl: Cyan\nT\u00FCrkis/Aqua-Farbe",
    L"Schnellauswahl: Lila\nMagenta/Violett-Farbe",
    L"Schnellauswahl: Wei\u00DF\nAlle Kan\u00E4le auf Maximum",
    L"Alle LEDs ausschalten\nSetzt Farbe auf Schwarz (0,0,0)",
    L"Tastatur-Lichteffekt\nStatisch, Atmend, Welle, Reaktiv, Regenbogen",
    L"Rand-LED Effekt (Laptop-Tastaturr\u00E4nder)\nSteuert die seitlichen Lichtleisten",
    L"Gesamthelligkeit (0-100%)\nBeeinflusst alle verbundenen Ger\u00E4te",
    L"Animationsgeschwindigkeit\nSteuert Atmen/Wellen-Effekt Timing",
    L"Kanal-Farbkorrektur\nEinzelne Ger\u00E4tefarben fein abstimmen",
    L"Gespeichertes Profil ausw\u00E4hlen\nSchnell zwischen Farbkonfigurationen wechseln",
    L"Aktuelle Einstellungen speichern\nFarbe, Effekte und Ger\u00E4teeinstellungen sichern",
    L"Ausgew\u00E4hltes Profil laden\nGespeicherte Einstellungen wiederherstellen",
    L"Mit Windows starten\nMinimiert starten wenn Windows hochf\u00E4hrt",
    L"In System-Tray minimieren\nFenster verstecken aber weiterlaufen",
    L"Live-Vorschau\n\u00C4nderungen automatisch beim Anpassen anwenden",
    L"Einstellungen auf alle Ger\u00E4te anwenden\nAktuelle Farbe an alle RGB-Hardware senden",
    L"Farbschema wechseln\nDunkel / Hell / Farbenblind Modi",
    L"Sprache wechseln\nEnglish / Deutsch",
    L"Anwendungsprotokoll\nZeigt Ger\u00E4testatus und angewandte Einstellungen"
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

// Maps KB_MODE_* enum values to ComboBox indices (order must match WM_CREATE combo population)
static const uint8_t KB_MODE_TABLE[] = {
    KB_MODE_STATIC, KB_MODE_BREATHING, KB_MODE_SPECTRUM, KB_MODE_WAVE_SHORT,
    KB_MODE_WAVE_LONG, KB_MODE_COLOR_WHEEL, KB_MODE_REACTIVE, KB_MODE_RIPPLE,
    KB_MODE_STARLIGHT, KB_MODE_RAINBOW, KB_MODE_HURRICANE
};
static const int KB_MODE_COUNT = 11;

// Returns ComboBox index for a given kbMode byte value (-1 if not found)
inline int KbModeToIndex(uint8_t mode) {
    for (int i = 0; i < KB_MODE_COUNT; i++)
        if (KB_MODE_TABLE[i] == mode) return i;
    return 0; // fallback: Static
}

// Returns kbMode byte value for a given ComboBox index
inline uint8_t IndexToKbMode(int idx) {
    if (idx >= 0 && idx < KB_MODE_COUNT) return KB_MODE_TABLE[idx];
    return KB_MODE_STATIC;
}

// Edge modes (Endorfy)
enum EdgeMode {
    EDGE_MODE_FREEZE = 0x00,
    EDGE_MODE_WAVE = 0x01,
    EDGE_MODE_SPECTRUM = 0x02,
    EDGE_MODE_BREATHING = 0x03,
    // On Endorfy EVision edge strips, solid color is encoded as 0x00.
    // Keep the STATIC symbol for UI semantics, but map it to FREEZE.
    EDGE_MODE_STATIC = EDGE_MODE_FREEZE,
    EDGE_MODE_OFF = 0x05
};

// Maps ComboBox edge indices to hardware byte values
// ComboBox order: Static(0), Breathing(1), Wave(2), Spectrum(3), Off(4)
static const uint8_t EDGE_MODE_TABLE[] = {
    EDGE_MODE_STATIC, EDGE_MODE_BREATHING, EDGE_MODE_WAVE, EDGE_MODE_SPECTRUM, EDGE_MODE_OFF
};
static const int EDGE_MODE_COUNT = 5;

inline int EdgeModeToIndex(uint8_t mode) {
    for (int i = 0; i < EDGE_MODE_COUNT; i++)
        if (EDGE_MODE_TABLE[i] == mode) return i;
    return 0; // fallback: Static
}

inline uint8_t IndexToEdgeMode(int idx) {
    if (idx >= 0 && idx < EDGE_MODE_COUNT) return EDGE_MODE_TABLE[idx];
    return EDGE_MODE_STATIC;
}

inline uint8_t NormalizeEdgeMode(uint8_t mode) {
    // Legacy configs used 0x04 for "static" and often produced rainbow-only behavior.
    if (mode == 0x04) return EDGE_MODE_STATIC;
    if (mode == EDGE_MODE_OFF ||
        mode == EDGE_MODE_STATIC ||
        mode == EDGE_MODE_BREATHING ||
        mode == EDGE_MODE_WAVE ||
        mode == EDGE_MODE_SPECTRUM) {
        return mode;
    }
    return EDGE_MODE_STATIC;
}

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
    std::wstring lastStatusLine;
    ULONGLONG lastStatusTick = 0;
    std::atomic<bool> applying{false};
    std::mutex statusMutex;

    // Serialize all hardware adapter I/O across apply/reset/power/test paths
    std::mutex deviceIoMutex;

    // Apply queue (single worker)
    std::mutex applyMutex;
    std::condition_variable applyCv;
    bool applyWorkerRunning = false;
    bool applyRequested = false;
    std::thread applyWorker;

    // Profiles
    std::vector<std::wstring> profiles;
    std::wstring currentProfile;
    std::wstring lastProfile;
} g_state;

// Channels are now owned by g_config (app_config.h)

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
    // Sync runtime state → unified config, then persist
    g_config.red        = g_state.red;
    g_config.green      = g_state.green;
    g_config.blue       = g_state.blue;
    g_config.brightness = g_state.brightness;
    g_config.speed      = g_state.speed;
    g_config.kbMode     = g_state.kbMode;
    g_config.edgeMode   = g_state.edgeMode;
    g_config.enableAura     = g_state.enableAura;
    g_config.enableMouse    = g_state.enableMouse;
    g_config.enableKeyboard = g_state.enableKeyboard;
    g_config.enableRAM      = g_state.enableRAM;
    g_config.enableEdge     = g_state.enableEdge;
    g_config.autostart      = g_state.autostart;
    g_config.minimizeToTray = g_state.minimizeToTray;
    g_config.autoApply      = g_state.autoApply;
    g_config.themeId        = GetThemeId();
    g_config.langId         = (g_lang == LANG_DE) ? 1 : 0;
    // Mirror lastProfile unconditionally - the old "only if non-empty" guard
    // meant a cleared selection could never be written back, so a deleted or
    // renamed profile stayed in the config forever. Convert through UTF-8
    // instead of truncating each wchar_t to a char, which mangled every
    // profile name containing umlauts.
    g_config.lastProfile.clear();
    if (!g_state.lastProfile.empty()) {
        int n = WideCharToMultiByte(CP_UTF8, 0, g_state.lastProfile.c_str(), -1,
                                    nullptr, 0, nullptr, nullptr);
        if (n > 1) {
            std::string tmp(n - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, g_state.lastProfile.c_str(), -1,
                                &tmp[0], n, nullptr, nullptr);
            g_config.lastProfile = tmp;
        }
    }
    if (g_state.hWnd) {
        RECT rc;
        GetWindowRect(g_state.hWnd, &rc);
        g_config.windowX = rc.left;
        g_config.windowY = rc.top;
    }
    g_config.Save();
}

void LoadAppSettings() {
    g_config.Load();
    // Sync unified config → runtime state
    g_state.red        = g_config.red;
    g_state.green      = g_config.green;
    g_state.blue       = g_config.blue;
    g_state.brightness = g_config.brightness;
    g_state.speed      = g_config.speed;
    g_state.kbMode     = g_config.kbMode;
    g_state.edgeMode   = NormalizeEdgeMode(g_config.edgeMode);
    g_config.edgeMode  = g_state.edgeMode;
    g_state.enableAura     = g_config.enableAura;
    g_state.enableMouse    = g_config.enableMouse;
    g_state.enableKeyboard = g_config.enableKeyboard;
    g_state.enableRAM      = g_config.enableRAM;
    g_state.enableEdge     = g_config.enableEdge;
    g_state.autostart      = g_config.autostart;
    g_state.minimizeToTray = g_config.minimizeToTray;
    g_state.autoApply      = g_config.autoApply;
    // Counterpart to the UTF-8 encode in SaveAppSettings; the old byte-wise
    // widening produced mojibake for any non-ASCII profile name.
    g_state.lastProfile.clear();
    if (!g_config.lastProfile.empty()) {
        int n = MultiByteToWideChar(CP_UTF8, 0, g_config.lastProfile.c_str(), -1, nullptr, 0);
        if (n > 1) {
            std::wstring tmp(n - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, g_config.lastProfile.c_str(), -1, &tmp[0], n);
            g_state.lastProfile = tmp;
        }
    }
    g_windowX = (g_config.windowX != -1) ? g_config.windowX : CW_USEDEFAULT;
    g_windowY = (g_config.windowY != -1) ? g_config.windowY : CW_USEDEFAULT;
    SetTheme(g_config.themeId);
    if (g_config.langId == 1) { g_lang = LANG_DE; g_str = &g_strDE; }
}

void AppendStatus(const wchar_t* text) {
    DWORD uiThreadId = 0;
    if (g_state.hWnd) {
        uiThreadId = GetWindowThreadProcessId(g_state.hWnd, NULL);
    }

    if (g_state.hWnd && uiThreadId != 0 && GetCurrentThreadId() != uiThreadId) {
        std::wstring* msg = new std::wstring(text ? text : L"");
        if (!PostMessage(g_state.hWnd, WM_APP_STATUS_APPEND, 0, (LPARAM)msg)) {
            delete msg;
        }
        return;
    }

    std::wstring currentText;
    {
        std::lock_guard<std::mutex> lock(g_state.statusMutex);
        const wchar_t* safeText = text ? text : L"";
        ULONGLONG now = GetTickCount64();
        if (g_state.lastStatusLine == safeText && (now - g_state.lastStatusTick) < 150) {
            return;
        }
        g_state.lastStatusLine = safeText;
        g_state.lastStatusTick = now;

        g_state.statusLog += safeText;
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
    DWORD uiThreadId = 0;
    if (g_state.hWnd) {
        uiThreadId = GetWindowThreadProcessId(g_state.hWnd, NULL);
    }

    if (g_state.hWnd && uiThreadId != 0 && GetCurrentThreadId() != uiThreadId) {
        PostMessage(g_state.hWnd, WM_APP_STATUS_CLEAR, 0, 0);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_state.statusMutex);
        g_state.statusLog.clear();
        g_state.lastStatusLine.clear();
        g_state.lastStatusTick = 0;
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
        // The profile just written becomes the active one. Without this,
        // lastProfile kept pointing at whatever was loaded before, so the next
        // Laden/Speichern - and any later restore - silently targeted the old
        // profile instead of the one the user just saved.
        g_state.currentProfile = name;
        g_state.lastProfile    = name;
        AppendStatus((L"Profile saved: " + name).c_str());
    }
}

bool LoadProfile(const std::wstring& name) {
    std::wstring path = GetAppDataPath() + L"\\profiles\\" + name + L".rgb";
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    uint8_t red = g_state.red;
    uint8_t green = g_state.green;
    uint8_t blue = g_state.blue;
    uint8_t brightness = g_state.brightness;
    uint8_t speed = g_state.speed;
    uint8_t kbMode = g_state.kbMode;
    uint8_t edgeMode = g_state.edgeMode;
    bool enableAura = g_state.enableAura;
    bool enableMouse = g_state.enableMouse;
    bool enableKeyboard = g_state.enableKeyboard;
    bool enableRAM = g_state.enableRAM;
    bool enableEdge = g_state.enableEdge;

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            int val = 0;
            try {
                val = std::stoi(value);
            } catch (...) {
                continue;
            }

            if (key == "red") red = (uint8_t)val;
            else if (key == "green") green = (uint8_t)val;
            else if (key == "blue") blue = (uint8_t)val;
            else if (key == "brightness") brightness = (uint8_t)val;
            else if (key == "speed") speed = (uint8_t)val;
            else if (key == "kbMode") kbMode = (uint8_t)val;
            else if (key == "edgeMode") edgeMode = (uint8_t)val;
            else if (key == "enableAura") enableAura = (val != 0);
            else if (key == "enableMouse") enableMouse = (val != 0);
            else if (key == "enableKeyboard") enableKeyboard = (val != 0);
            else if (key == "enableRAM") enableRAM = (val != 0);
            else if (key == "enableEdge") enableEdge = (val != 0);
        }
    }

    file.close();

    // Atomic commit: apply loaded snapshot in one state update
    g_state.red = red;
    g_state.green = green;
    g_state.blue = blue;
    g_state.brightness = brightness;
    g_state.speed = speed;
    g_state.kbMode = kbMode;
    g_state.edgeMode = NormalizeEdgeMode(edgeMode);
    g_state.enableAura = enableAura;
    g_state.enableMouse = enableMouse;
    g_state.enableKeyboard = enableKeyboard;
    g_state.enableRAM = enableRAM;
    g_state.enableEdge = enableEdge;
    g_state.currentProfile = name;
    g_state.lastProfile = name;

    AppendStatus((L"Profile loaded: " + name).c_str());
    return true;
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
// DRY-RUN GUARD
//
// --dry-run has to reach every writing path, not just ApplyColors. It used to
// be parsed only at line ~4794, i.e. *after* --probe, --kbdump, --kbtest,
// --kbmode* and --mouse-zones-test had already run and returned, so
// "--dry-run --kbmode-sweep" stamped 21 mode bytes into the keyboard flash.
//
// The flag is now the first thing WinMain reads, and every public device
// setter asks this guard before it opens a handle - so a path added later
// cannot quietly bypass it.
//
// Returns false ("nothing was verified"), never true: a dry run proves
// nothing about the hardware, and claiming success for a write that never
// left the process is exactly the blind success CLAUDE.md rule 1 forbids.
//=============================================================================

static bool DryRunSkip(const wchar_t* what) {
    if (!g_state.dryRun) return false;
    wchar_t buf[160];
    swprintf(buf, 160, L"[DRY] %s: kein Write gesendet", what);
    AppendStatus(buf);
    return true;
}

//=============================================================================
// DEVICE CONTROL - ASUS AURA
// Direct HID control with 65-byte buffer (Report ID 0xEC)
//=============================================================================

#define ASUS_LEDS_PER_PACKET 20
// Only used when the 0xB0 config table gives no usable per-header maximum.
// The real value is read from the table - see ParseAsusConfig().
#define ASUS_ADDRESSABLE_FALLBACK_LEDS 120
// Sanity bound for a per-header LED maximum read out of the config table.
#define ASUS_ADDRESSABLE_SANE_MAX 240
#define AURA_REQUEST_FIRMWARE_VERSION 0x82
#define AURA_REQUEST_CONFIG_TABLE 0xB0
#define AURA_CONFIG_CHANNELS 8

// Fallback channel map - used only when the 0xB0 config table could not be
// parsed (g_asusHwConfig.valid == false).
//
// Two contradictory tables used to live here: SetAsusAura drove
// {0,1,2,3,4,0x0B,0x0C} while SetAsusAuraQuick drove {0..7}, so the live
// preview and the actual apply addressed different hardware on the same
// board. This is the set both of them agreed on, with the identity
// index->channel mapping the rest of the code already assumes (see
// ShowAsusTestDialog: "Default: use index as channel") and which matches how
// ParseAsusConfig() numbers the addressable headers.
//
// OPEN (Sanierungsplan 4.3): which channels this board really exposes is
// unverified. Neither tail (0x0B/0x0C nor 5/6/7) was ever confirmed against a
// device, and there is no read-back for Aura direct-mode colours to settle it
// from software. Extend this table from a live scan only - not from a guess.
struct AuraFallbackChannel { int channel; int leds; };
static const AuraFallbackChannel AURA_FALLBACK_CHANNELS[] = {
    {0x00, 60}, {0x01, 120}, {0x02, 120}, {0x03, 60}, {0x04, 60}
};
static const int AURA_FALLBACK_COUNT =
    (int)(sizeof(AURA_FALLBACK_CHANNELS) / sizeof(AURA_FALLBACK_CHANNELS[0]));

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

    // OpenRGB-like: only onboard zone + addressable headers as direct channels.
    // (Legacy RGB/diagnostic zone scan removed: it caused channel drift and config mismatch.)
    for (int i = 0; i < cfg.numAddressableHeaders && cfg.numChannels < 16; i++) {
        cfg.channels[cfg.numChannels].present = true;

        // Per-header LED maximum, read from the 0xB0 table rather than assumed.
        //
        // This used to be hardcoded to 1 ("one logical LED target"), which made
        // SetAsusChannel emit colour data for the first pixel only - everything
        // further along an attached ARGB strip stayed dark.
        //
        // Layout of the table (derived from a real dump, board AULA3-AR32-0222):
        //   [0x02]            = number of addressable headers
        //   [0x03 + i*6 .. +5]= one 6-byte record per header
        //   record byte +3    = maximum LED count this header drives
        // The dump reads 03 | 01 00 00 78 3C 00 | 01 00 00 78 3C 00 | 01 00 00 78 3C 00,
        // i.e. three identical records, each reporting 0x78 = 120.
        //
        // Reading it keeps boards with a different capacity correct instead of
        // baking one machine's number into the binary. The value is only a
        // ceiling - Aura cannot know the physical strip length, so a shorter
        // strip simply ignores the surplus pixel data.
        int rec     = 0x03 + i * 6;
        int maxLeds = (rec + 3 < 60) ? (int)cfg.configTable[rec + 3] : 0;
        if (maxLeds < 1 || maxLeds > ASUS_ADDRESSABLE_SANE_MAX)
            maxLeds = ASUS_ADDRESSABLE_FALLBACK_LEDS;
        cfg.channels[cfg.numChannels].ledCount = maxLeds;
        cfg.channels[cfg.numChannels].addressable = true;
        cfg.channels[cfg.numChannels].directChannel = i;  // Addressable uses 0, 1, 2...
        cfg.channels[cfg.numChannels].colorR = 0;
        cfg.channels[cfg.numChannels].colorG = 34;
        cfg.channels[cfg.numChannels].colorB = 255;
        cfg.channels[cfg.numChannels].enabled = true;
        sprintf(cfg.channels[cfg.numChannels].name, "Addressable %d (%d LEDs)", i + 1, maxLeds);
        cfg.numChannels++;
    }

    cfg.valid = (cfg.numChannels > 0);
}

// Full hardware scan
bool ScanAsusHardware() {
    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

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
            if (read != 1) return false;
            // Re-derive the channel list from the cached raw 0xB0 table instead
            // of trusting the parsed channels in the file. The cache stores the
            // *result* of ParseAsusConfig, so any later change to how channels
            // or LED counts are derived would otherwise stay masked behind a
            // stale file until the hardware itself changed.
            ParseAsusConfig(g_asusHwConfig);
            return g_asusHwConfig.valid;
        }
    }
    return false;
}

// Check if hardware config has changed
bool HasAsusHardwareChanged() {
    AsusHardwareConfig current;

    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

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

    if (numLEDs < 1) numLEDs = 1;

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

static void ApplyAsusChannelColor(hid_device* dev, int auraIndex, int directChannel, int ledCount,
                                  uint8_t baseR, uint8_t baseG, uint8_t baseB, bool applyCorrection,
                                  bool respectGlobalEnable,
                                  int& setCount) {
    if (!dev) return;
    if (auraIndex < 0 || auraIndex >= AURA_CONFIG_CHANNELS) return;
    if (respectGlobalEnable && !g_config.aura[auraIndex].enabled) return;

    uint8_t cr = baseR, cg = baseG, cb = baseB;
    if (applyCorrection) {
        g_config.aura[auraIndex].ApplyCorrection(cr, cg, cb);
    }

    SetAsusChannel(dev, directChannel, ledCount, cr, cg, cb);
    setCount++;
}

bool SetAsusAura(uint8_t r, uint8_t g, uint8_t b) {
    if (DryRunSkip(L"ASUS Aura")) return false;

    hid_device* dev = OpenAsusAura();
    if (!dev) {
        AppendStatus(L"[ASUS Aura] Not found");
        return false;
    }

    int setCount = 0;

    // Use hardware config if available
    if (g_asusHwConfig.valid) {
        int maxAuraChannels = g_asusHwConfig.numChannels;
        if (maxAuraChannels > AURA_CONFIG_CHANNELS) maxAuraChannels = AURA_CONFIG_CHANNELS;
        for (int i = 0; i < maxAuraChannels; i++) {
            ApplyAsusChannelColor(dev, i,
                g_asusHwConfig.channels[i].directChannel,
                g_asusHwConfig.channels[i].ledCount,
                r, g, b,
                true,
                true,
                setCount);
        }
    } else {
        for (int i = 0; i < AURA_FALLBACK_COUNT; i++) {
            ApplyAsusChannelColor(dev, i, AURA_FALLBACK_CHANNELS[i].channel,
                                  AURA_FALLBACK_CHANNELS[i].leds,
                                  r, g, b, true, true, setCount);
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
    if (DryRunSkip(L"ASUS Aura (Vorschau)")) return;

    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

    hid_init();
    hid_device* dev = OpenAsusAura();
    if (!dev) {
        hid_exit();
        return;
    }

    // Use hardware config if available
    if (g_asusHwConfig.valid) {
        int maxAuraChannels = g_asusHwConfig.numChannels;
        if (maxAuraChannels > AURA_CONFIG_CHANNELS) maxAuraChannels = AURA_CONFIG_CHANNELS;
        for (int i = 0; i < maxAuraChannels; i++) {
            int ignoredCount = 0;
            ApplyAsusChannelColor(dev, i,
                g_asusHwConfig.channels[i].directChannel,
                g_asusHwConfig.channels[i].ledCount,
                r, g, b,
                true,
                true,
                ignoredCount);
        }
    } else {
        for (int i = 0; i < AURA_FALLBACK_COUNT; i++) {
            int ignoredCount = 0;
            ApplyAsusChannelColor(dev, i, AURA_FALLBACK_CHANNELS[i].channel,
                                  AURA_FALLBACK_CHANNELS[i].leds,
                                  r, g, b, true, true, ignoredCount);
        }
    }

    hid_close(dev);
    hid_exit();
}

//=============================================================================
// DEVICE CONTROL - STEELSERIES
//=============================================================================

bool SetSteelSeries(uint8_t r, uint8_t g, uint8_t b) {
    if (DryRunSkip(L"SteelSeries")) return false;

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
    g_config.steelseries.ApplyCorrection(cr, cg, cb);

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

// ---------------------------------------------------------------------------
// Per-zone SteelSeries control (Rival 600, 8 zones from g_config.mouseZones)
// Zone identity/layout comes from the SteelSeries GG DB; see docs/Mouse_Protocol.md.
// Same wire format as SetSteelSeries: per-zone {0x1C,0x27,0x00,zoneBit,r,g,b} + save 0x09.
// ---------------------------------------------------------------------------
static hid_device* OpenRival600() {
    hid_device* dev = nullptr;
    struct hid_device_info* devs = hid_enumerate(Devices::STEELSERIES_VID, Devices::RIVAL_600_PID);
    for (auto* cur = devs; cur; cur = cur->next) {
        if (cur->interface_number == 0) { dev = hid_open_path(cur->path); break; }
    }
    hid_free_enumeration(devs);
    return dev;
}

static void SSWriteZone(hid_device* dev, uint8_t zoneBit, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t pkt[8] = {0x1C, 0x27, 0x00, zoneBit, r, g, b, 0};
    hid_write(dev, pkt, 7);
    Sleep(10);
}
static void SSSave(hid_device* dev) { uint8_t save[10] = {0x09}; hid_write(dev, save, 9); }

// Write each mouse zone its own (corrected) colour.
bool SetSteelSeriesZones() {
    if (DryRunSkip(L"SteelSeries Zonen")) return false;

    hid_device* dev = OpenRival600();
    if (!dev) { AppendStatus(L"[SteelSeries] Not found"); return false; }
    int n = 0;
    for (const auto& z : g_config.mouseZones) {
        uint8_t cr = z.color.r, cg = z.color.g, cb = z.color.b;
        g_config.steelseries.ApplyCorrection(cr, cg, cb);  // global mouse enable/correction
        if (!z.enabled) { cr = cg = cb = 0; }
        SSWriteZone(dev, (uint8_t)z.hwIndex, cr, cg, cb);
        n++;
    }
    SSSave(dev);
    hid_close(dev);
    wchar_t buf[64]; swprintf(buf, 64, L"[SteelSeries] %d zones set", n); AppendStatus(buf);
    return true;
}

// Identify: blink exactly one mouse zone white<->off for ~durationMs, all others
// off, then restore every zone's real colour. Used to verify the zoneBit ->
// physical-zone mapping against the SteelSeries DB names. Self-contained
// (own hid_init/exit + deviceIoMutex); do not call while holding that mutex.
bool IdentifyMouseZone(int idx, int durationMs = 3000) {
    if (idx < 0 || idx >= (int)g_config.mouseZones.size()) return false;
    if (DryRunSkip(L"SteelSeries Identify")) return false;
    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);
    hid_init();
    hid_device* dev = OpenRival600();
    if (!dev) { hid_exit(); AppendStatus(L"[SteelSeries] Identify: device not found"); return false; }

    uint8_t bit = (uint8_t)g_config.mouseZones[idx].hwIndex;
    int elapsed = 0; bool on = true;
    while (elapsed < durationMs) {
        for (const auto& z : g_config.mouseZones) SSWriteZone(dev, (uint8_t)z.hwIndex, 0, 0, 0);
        if (on) SSWriteZone(dev, bit, 255, 255, 255);
        SSSave(dev);
        Sleep(350); elapsed += 350; on = !on;
    }
    // restore real colours
    for (const auto& z : g_config.mouseZones) {
        uint8_t cr = z.color.r, cg = z.color.g, cb = z.color.b;
        g_config.steelseries.ApplyCorrection(cr, cg, cb);
        if (!z.enabled) { cr = cg = cb = 0; }
        SSWriteZone(dev, (uint8_t)z.hwIndex, cr, cg, cb);
    }
    SSSave(dev);
    hid_close(dev);
    hid_exit();
    AppendStatus(L"[SteelSeries] Identify done");
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

// Result of the last SetEVisionKeyboard read-back. Filled on every call so the
// headless --kbmode probes can report what the firmware really stored without
// duplicating the write path (see CLAUDE.md rule 1: tests read live values).
struct KbVerifyResult {
    bool    valid    = false;   // read-back succeeded
    int     writeRes = 0;
    int     readRes  = 0;
    uint8_t want[9]  = {0};     // mode,bright,speed,dir,rand,R,G,B,coloff
    uint8_t got[18]  = {0};     // what the profile block actually holds now
};
static KbVerifyResult g_lastKbVerify;

bool SetEVisionKeyboard(uint8_t r, uint8_t g, uint8_t b, uint8_t mode, uint8_t brightness, uint8_t speed) {
    // Invalidate the read-back record FIRST, before anything can return early.
    // It used to be reset only just before the read-back itself, i.e. behind
    // the "keyboard not found" bail-out - so if the keyboard dropped off the
    // bus mid --kbmode-sweep, every following row in kbmode_probe.txt printed
    // the last successful read-back as though it were fresh. An old dump
    // reported as a live value, in the very probe built to prevent that.
    g_lastKbVerify = KbVerifyResult{};

    if (DryRunSkip(L"EVision Keyboard")) return false;

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
    g_config.keyboard.ApplyCorrection(cr, cg, cb);

    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t profile_offset = profile * 0x40 + 0x01;

    // Read existing keyboard profile first, then patch only known fields.
    // This preserves unknown bytes that may store vendor-specific layout/profile state.
    uint8_t config[18] = {0};
    int readRes = EVisionQuery(dev, 0x05, profile_offset, nullptr, 18, config);
    if (readRes < 0) {
        memset(config, 0, sizeof(config));
    }

    // Update known keyboard fields.
    //
    // The keyboard block uses the same 10-byte layout as the edge payload at
    // +0x1E (docs/Keyboard_Protocol.md 3.1):
    //   [mode, brightness, speed, direction, random, R, G, B, colorOffset, save]
    // Only the first nine were ever written here, so the trailing commit/save
    // byte at profile_base+0x0A stayed whatever it was - a live read-back after
    // selecting Breathing showed the block as "05 04 04 00 00 00 13 FF 00 [00]".
    // The mode byte reaches the flash but the firmware never applies it, which
    // is exactly the reported symptom: the colour changes, the effect does not.
    config[0] = mode;           // +0x01 Mode
    config[1] = brightness;     // +0x02 Brightness (0-4)
    config[2] = speed;          // +0x03 Speed (0-5, inverted)
    config[3] = 0;              // +0x04 Direction
    config[4] = 0;              // +0x05 Random color off
    config[5] = cr;             // +0x06 Red (corrected)
    config[6] = cg;             // +0x07 Green (corrected)
    config[7] = cb;             // +0x08 Blue (corrected)
    config[8] = 0;              // +0x09 Color offset
    config[9] = 0x01;           // +0x0A Commit/save - same flag the edge path sets

    int writeRes = EVisionQuery(dev, 0x06, profile_offset, config, 18, nullptr);
    Sleep(10);

    // Read back what the firmware actually stored. The write result alone is no
    // proof: the device ACKs writes whose payload it silently discards, so a
    // mode byte it does not implement produced a cheerful "Keyboard set" while
    // the lighting never changed. Only the read-back is reported below.
    g_lastKbVerify.writeRes = writeRes;
    memcpy(g_lastKbVerify.want, config, 9);
    g_lastKbVerify.readRes = EVisionQuery(dev, 0x05, profile_offset, nullptr, 18,
                                          g_lastKbVerify.got);
    g_lastKbVerify.valid = (g_lastKbVerify.readRes >= 0);

    const uint8_t* got = g_lastKbVerify.got;
    bool verified = g_lastKbVerify.valid &&
                    got[0] == mode && got[1] == brightness && got[2] == speed &&
                    got[5] == cr && got[6] == cg && got[7] == cb;

    {
        char dbg[224];
        snprintf(dbg, sizeof(dbg),
                 "[EVision] KB profile=%d off=0x%02X writeRes=%d readRes=%d "
                 "want[mode=%02X br=%02X sp=%02X rgb=%02X%02X%02X] "
                 "got[mode=%02X br=%02X sp=%02X rgb=%02X%02X%02X] verified=%d",
                 (int)profile, (unsigned)profile_offset, writeRes,
                 g_lastKbVerify.readRes,
                 mode, brightness, speed, cr, cg, cb,
                 got[0], got[1], got[2], got[5], got[6], got[7], (int)verified);
        LogDebug(dbg);
    }

    // Unlock the Windows key: clear the two bytes at profile_base+0x14.
    //
    // This call existed until commit f1c6aaa removed it from both device paths
    // (keeping it only in the --switch-test=edge-diagnose branch). Removing it
    // is what made the Win-key lock permanent: the old 15-offset brute-force in
    // SetEVisionEdge wrote at 0x13 and 0x16, which straddle 0x14/0x15, so every
    // apply stamped payload bytes (brightness 0x04, speed 0x02) into the
    // Win-Lock field, and this unlock was the thing that cleared them again.
    // A live dump with the key locked reads exactly "04 02" there.
    //
    // The brute-force is gone now, so nothing corrupts the field anymore, but
    // the write is kept because it also clears a lock the user toggled via
    // Fn+Win - and it is addressed per active profile rather than hardcoded to
    // profile 0 as the original was.
    uint8_t unlock[2] = {0x00, 0x00};
    EVisionQuery(dev, 0x06, (uint16_t)(profile * 0x40 + 0x14), unlock, 2, nullptr);
    Sleep(10);

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);  // End configure
    hid_close(dev);

    wchar_t buf[192];
    if (!g_lastKbVerify.valid) {
        swprintf(buf, 192, L"[EVision] Keyboard mode 0x%02X written, read-back failed (%d)",
                 mode, g_lastKbVerify.readRes);
    } else if (verified) {
        swprintf(buf, 192, L"[EVision] Keyboard verified: mode 0x%02X, brightness %d, speed %d",
                 mode, brightness, speed);
    } else {
        // Report the mismatch instead of a blanket success - this is how a mode
        // the firmware refuses becomes visible instead of silently doing nothing.
        swprintf(buf, 192,
                 L"[EVision] Keyboard MISMATCH - mode 0x%02X->0x%02X, brightness %d->%d, speed %d->%d",
                 mode, got[0], brightness, got[1], speed, got[2]);
    }
    AppendStatus(buf);
    return verified;
}

// ---------------------------------------------------------------------------
// EVision Edge LED helper – enumerates the HID device and logs the path
// ---------------------------------------------------------------------------
static hid_device* OpenEVisionEdgeDev(char* pathOut, int pathMax) {
    // Try primary usage page 0xFF1C, then optional fallback 0xFF00
    const uint16_t tryPages[] = {
        Devices::EVISION_USAGE_PAGE, // 0xFF1C
        0xFF00,                      // fallback for some firmware variants
    };
    for (uint16_t page : tryPages) {
        struct hid_device_info* devs = hid_enumerate(
            Devices::EVISION_VID, Devices::EVISION_PID);
        for (auto* cur = devs; cur; cur = cur->next) {
            if (cur->usage_page == page) {
                hid_device* d = hid_open_path(cur->path);
                if (d) {
                    if (pathOut && pathMax > 0)
                        snprintf(pathOut, pathMax, "%s", cur->path);
                    hid_free_enumeration(devs);
                    return d;
                }
            }
        }
        hid_free_enumeration(devs);
    }
    return nullptr;
}

// brightness 0-4, speed 0-5 - both come from the Effects group sliders. They
// used to be hardcoded here (brightness 4, speed 2) and were not even function
// parameters, which is why the Tempo slider had no effect on the edge strips
// and every effect always animated at the same rate.
bool SetEVisionEdge(uint8_t r, uint8_t g, uint8_t b, uint8_t mode,
                    uint8_t brightness, uint8_t speed) {
    if (DryRunSkip(L"EVision Edge")) return false;

    char devPath[256] = "<not opened>";
    hid_device* dev = OpenEVisionEdgeDev(devPath, sizeof(devPath));

    if (!dev) {
        AppendStatus(L"[EVision] Edge: device not found (VID=0x3299 PID=0x4E9F)");
        LogDebug("[EVision] Edge: device not found – check USB connection");
        return false;
    }

    char dbg[256];
    snprintf(dbg, sizeof(dbg), "[EVision] Edge: opened path=%s mode=0x%02X rgb=%d,%d,%d",
             devPath, mode, r, g, b);
    LogDebug(dbg);

    // ------------------------------------------------------------------
    // Color correction – skip for OFF (force all zeros for certainty)
    // ------------------------------------------------------------------
    uint8_t cr = r, cg = g, cb = b;
    if (mode == EDGE_MODE_OFF) {
        cr = cg = cb = 0;
    } else {
        g_config.edge.ApplyCorrection(cr, cg, cb);
    }

    // ------------------------------------------------------------------
    // Begin configure session
    // ------------------------------------------------------------------
    int wakeRes = EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(25);
    snprintf(dbg, sizeof(dbg), "[EVision] Edge: wake result=%d", wakeRes);
    LogDebug(dbg);

    // Read active profile (0-2); clamp to valid range
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    snprintf(dbg, sizeof(dbg), "[EVision] Edge: active profile=%d", (int)profile);
    LogDebug(dbg);

    // ------------------------------------------------------------------
    // Build 10-byte edge payload
    // Layout: [mode, brightness, speed, direction, random, R, G, B, colorOff, save]
    // ------------------------------------------------------------------
    uint8_t bright = (mode == EDGE_MODE_OFF) ? 0 : (brightness > 4 ? 4 : brightness);
    uint8_t spd    = (speed > 5) ? 5 : speed;
    uint8_t edgeData[10] = {
        mode,        // [0] effect mode
        bright,      // [1] brightness (0-4, from the Helligkeit slider)
        spd,         // [2] speed      (0-5, from the Tempo slider)
        0x00,        // [3] direction
        0x00,        // [4] random color = off (use given color)
        cr, cg, cb,  // [5][6][7] RGB
        0x00,        // [8] color offset
        0x01         // [9] commit/save to flash
    };

    // ------------------------------------------------------------------
    // Single targeted write to the edge payload slot of the ACTIVE profile.
    //
    // This used to brute-force the same 10 bytes onto 15 offsets (0x13/0x16/
    // 0x19/0x1B/0x1E in every one of the three profile blocks). Only +0x1E is
    // the edge slot - see docs/Keyboard_Protocol.md section 3.1, where the live
    // dump reads exactly [mode,bright,speed,dir,rand,R,G,B,coloff,save] there.
    // The other four offsets land 3-11 bytes earlier and therefore scribble the
    // payload straight through +0x14..0x1D, the still-undecoded per-zone tuple
    // region, five times over at overlapping positions. The captured config
    // dump shows the damage plainly: "04 02 00 04 02 00 04 00 04 02 00 04 02"
    // is nothing but those overlapping brightness/speed bytes.
    //
    // That stray writing is what corrupted the keyboard's on-board config and
    // produced the unwanted Windows-key lock. It also hit profiles 1 and 2,
    // which the user may not even be using. One profile, one offset, one write.
    // ------------------------------------------------------------------
    uint16_t edgeOff = (uint16_t)(profile * 0x40 + 0x1E);
    int res = EVisionQuery(dev, 0x06, edgeOff, edgeData, 10, nullptr);
    Sleep(10);

    // Read back rather than trusting the write result (CLAUDE.md rule 1). The
    // device ACKs writes it discards, so res>=0 on its own is not evidence that
    // the strip changed.
    uint8_t edgeBack[10] = {0};
    int edgeRead = EVisionQuery(dev, 0x05, edgeOff, nullptr, 10, edgeBack);
    bool edgeVerified = (edgeRead >= 0) &&
                        edgeBack[0] == mode   && edgeBack[1] == bright &&
                        edgeBack[2] == spd    && edgeBack[5] == cr &&
                        edgeBack[6] == cg     && edgeBack[7] == cb;
    int ok = edgeVerified ? 1 : 0;

    snprintf(dbg, sizeof(dbg),
             "[EVision] Edge write off=0x%02X (P%d+0x1E) res=%d readRes=%d "
             "want[%02X %02X %02X %02X%02X%02X] got[%02X %02X %02X %02X%02X%02X] verified=%d",
             (unsigned)edgeOff, (int)profile, res, edgeRead,
             mode, bright, spd, cr, cg, cb,
             edgeBack[0], edgeBack[1], edgeBack[2],
             edgeBack[5], edgeBack[6], edgeBack[7], (int)edgeVerified);
    LogDebug(dbg);
    Sleep(3);

    // ------------------------------------------------------------------
    // End configure session
    // ------------------------------------------------------------------
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);

    hid_close(dev);

    snprintf(dbg, sizeof(dbg),
             "[EVision] Edge done: profile=%d off=0x%02X mode=0x%02X rgb=%d,%d,%d ok=%d",
             (int)profile, (unsigned)edgeOff, (unsigned)mode,
             (unsigned)cr, (unsigned)cg, (unsigned)cb, ok);
    LogDebug(dbg);

    wchar_t wbuf[192];
    if (edgeRead < 0) {
        swprintf(wbuf, 192,
                 L"[EVision] Edge: P%d+0x1E mode=0x%02X geschrieben, Rücklesen fehlgeschlagen (%d)",
                 (int)profile, mode, edgeRead);
    } else if (edgeVerified) {
        swprintf(wbuf, 192,
                 L"[EVision] Edge: P%d+0x1E mode=0x%02X rgb=%d,%d,%d verifiziert",
                 (int)profile, mode, cr, cg, cb);
    } else {
        swprintf(wbuf, 192,
                 L"[EVision] Edge ABWEICHUNG - mode 0x%02X->0x%02X, Helligkeit %d->%d, Tempo %d->%d",
                 mode, edgeBack[0], bright, edgeBack[1], spd, edgeBack[2]);
    }
    AppendStatus(wbuf);

    return ok > 0;
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
    if (DryRunSkip(L"G.Skill RAM")) return false;

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
                g_config.ram[slot].ApplyCorrection(cr, cg, cb);
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

// --- Resume gating -----------------------------------------------------------
// A post-resume reset is expensive: FullHIDReset() tears down and re-enumerates
// the whole HID stack and sleeps ~800ms, then every device is written again.
// Four separate notifications can request it (watchdog time jump, APM resume,
// display-on, session unlock) and two of them - display-on and unlock - also
// fire during ordinary AFK idle, when no standby happened at all. Without
// gating, an idle machine whose monitor keeps cycling off/on re-initialises the
// devices forever; the re-apply writes to keyboard and mouse, which can itself
// wake the display and sustain the cycle.
//
// Rules: only reset when a real suspend was observed, never more than one reset
// at a time, and at most one per cooldown window.
#define RESUME_COOLDOWN_MS 15000
std::atomic<bool> g_suspendSeen{false};        // genuine standby observed
std::atomic<bool> g_resetInFlight{false};      // reset currently running
std::atomic<bool> g_resetArmed{false};         // timer already scheduled
std::atomic<ULONGLONG> g_lastResetTick{0};     // completion time of last reset

// Arm the deferred post-resume reset. Silently ignores triggers that are not
// backed by an actual suspend, and those inside the cooldown window.
static void ScheduleResumeReset(HWND hWnd) {
    if (!g_suspendSeen.load()) return;   // display-on / unlock while awake
    if (g_resetInFlight.load()) return;
    // Already scheduled: leave the pending timer alone. Re-arming on every
    // event would push the deadline out indefinitely while the display keeps
    // cycling, so the reset would never actually run.
    if (g_resetArmed.load()) return;
    ULONGLONG last = g_lastResetTick.load();
    if (last && (GetTickCount64() - last) < RESUME_COOLDOWN_MS) return;
    g_resetArmed = true;
    SetTimer(hWnd, ID_TIMER_RESUME, 3000, NULL);
}

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
    if (DryRunSkip(L"HID-Reset")) return;

    AppendStatus(L"Resetting all RGB devices...");

    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

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

    {
        std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

        hid_init();

        // Deterministic adapter order
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
            SetEVisionEdge(r, g, b, edgeMode, (uint8_t)brightness, (uint8_t)speed);
        }
        if (doRAM) {
            SetGSkillRAM(r, g, b);
        }

        hid_exit();
    }

    AppendStatus(L"=== Done! ===");

    g_state.applying = false;
}

void ApplyWorkerLoop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(g_state.applyMutex);
            g_state.applyCv.wait(lock, [] {
                return !g_state.applyWorkerRunning || g_state.applyRequested;
            });
            if (!g_state.applyWorkerRunning) {
                break;
            }
            g_state.applyRequested = false;
        }
        ApplyColors();
    }
}

void StartApplyWorker() {
    std::lock_guard<std::mutex> lock(g_state.applyMutex);
    if (g_state.applyWorkerRunning) return;
    g_state.applyWorkerRunning = true;
    g_state.applyRequested = false;
    g_state.applyWorker = std::thread(ApplyWorkerLoop);
}

void StopApplyWorker() {
    {
        std::lock_guard<std::mutex> lock(g_state.applyMutex);
        g_state.applyWorkerRunning = false;
        g_state.applyRequested = false;
    }
    g_state.applyCv.notify_all();
    if (g_state.applyWorker.joinable()) {
        g_state.applyWorker.join();
    }
}

void RequestApplyColors(bool force) {
    (void)force;
    {
        std::lock_guard<std::mutex> lock(g_state.applyMutex);
        g_state.applyRequested = true;
    }
    g_state.applyCv.notify_one();
}

void CommitStateAndApply(bool forceApply) {
    SaveSettings();
    RequestApplyColors(forceApply);
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
    CommitStateAndApply(false);
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
    if (g_state.hComboKbMode) {
        SendMessage(g_state.hComboKbMode, CB_SETCURSEL, KbModeToIndex(g_state.kbMode), 0);
    }
    if (g_state.hComboEdgeMode) SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, EdgeModeToIndex(g_state.edgeMode), 0);
    
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
// CHANNEL SETTINGS DIALOG - Proper WndProc-based popup (not DialogBoxIndirect)
//=============================================================================

// Per-channel slider handles stored as window user data
struct ChanDlgData {
    HWND hSliderR[8];
    HWND hSliderG[8];
    HWND hSliderB[8];
    HWND hValR[8];
    HWND hValG[8];
    HWND hValB[8];
    HWND hParent;
};

static LRESULT CALLBACK ChanSettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ChanDlgData* d = (ChanDlgData*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        d = new ChanDlgData();
        memset(d, 0, sizeof(ChanDlgData));
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)d);

        HINSTANCE hInst = GetModuleHandleW(NULL);
        // Column layout: label(90) | R-lbl(18)+slider(110)+val(32) | G | B
        int colLabel = 10;
        int colR = 104, colG = 268, colB = 432;
        int sliderW = 110;
        int y = 36;

        // Header labels
        CreateWindowW(L"STATIC", L"Channel",  WS_CHILD|WS_VISIBLE, colLabel, 10, 90,  18, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"STATIC", L"Red (0-200)",   WS_CHILD|WS_VISIBLE, colR,   10, 140, 18, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"STATIC", L"Green (0-200)", WS_CHILD|WS_VISIBLE, colG,   10, 140, 18, hWnd, NULL, hInst, NULL);
        CreateWindowW(L"STATIC", L"Blue (0-200)",  WS_CHILD|WS_VISIBLE, colB,   10, 140, 18, hWnd, NULL, hInst, NULL);

        for (int i = 0; i < 8; i++) {
            wchar_t label[32];
            swprintf(label, 32, L"ASUS Ch %d", i);
            CreateWindowW(L"STATIC", label, WS_CHILD|WS_VISIBLE, colLabel, y+3, 90, 20, hWnd, NULL, hInst, NULL);

            // R
            d->hSliderR[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,
                colR, y-1, sliderW, 24, hWnd, (HMENU)(INT_PTR)(7000+i*3), hInst, NULL);
            SendMessage(d->hSliderR[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(d->hSliderR[i], TBM_SETPOS,   TRUE, g_config.aura[i].red_adjust);
            wchar_t v[8]; swprintf(v, 8, L"%d", g_config.aura[i].red_adjust);
            d->hValR[i] = CreateWindowW(L"STATIC", v, WS_CHILD|WS_VISIBLE|SS_CENTER,
                colR+sliderW+2, y+3, 30, 18, hWnd, NULL, hInst, NULL);

            // G
            d->hSliderG[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,
                colG, y-1, sliderW, 24, hWnd, (HMENU)(INT_PTR)(7001+i*3), hInst, NULL);
            SendMessage(d->hSliderG[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(d->hSliderG[i], TBM_SETPOS,   TRUE, g_config.aura[i].green_adjust);
            swprintf(v, 8, L"%d", g_config.aura[i].green_adjust);
            d->hValG[i] = CreateWindowW(L"STATIC", v, WS_CHILD|WS_VISIBLE|SS_CENTER,
                colG+sliderW+2, y+3, 30, 18, hWnd, NULL, hInst, NULL);

            // B
            d->hSliderB[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,
                colB, y-1, sliderW, 24, hWnd, (HMENU)(INT_PTR)(7002+i*3), hInst, NULL);
            SendMessage(d->hSliderB[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
            SendMessage(d->hSliderB[i], TBM_SETPOS,   TRUE, g_config.aura[i].blue_adjust);
            swprintf(v, 8, L"%d", g_config.aura[i].blue_adjust);
            d->hValB[i] = CreateWindowW(L"STATIC", v, WS_CHILD|WS_VISIBLE|SS_CENTER,
                colB+sliderW+2, y+3, 30, 18, hWnd, NULL, hInst, NULL);

            y += 30;
        }

        // Hint + buttons
        int btnY = y + 8;
        CreateWindowW(L"STATIC", g_str->csHint, WS_CHILD|WS_VISIBLE,
            colLabel, btnY, 420, 20, hWnd, NULL, hInst, NULL);
        HWND hBtnOk = CreateWindowW(L"BUTTON", g_str->csSaveClose,
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            colLabel, btnY+28, 100, 30, hWnd, (HMENU)IDOK, hInst, NULL);
        HWND hBtnReset = CreateWindowW(L"BUTTON", g_str->csResetAll,
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            colLabel+110, btnY+28, 120, 30, hWnd, (HMENU)IDRETRY, hInst, NULL);
        HWND hBtnCancel = CreateWindowW(L"BUTTON", L"Abbrechen",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
            colLabel+240, btnY+28, 100, 30, hWnd, (HMENU)IDCANCEL, hInst, NULL);

        // Apply dark-theme font and subclassing to all child controls
        HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessage(hChild, WM_SETFONT, lParam, TRUE);
            wchar_t cls[64];
            GetClassNameW(hChild, cls, 64);
            if (wcscmp(cls, L"Button") == 0 || wcscmp(cls, L"BUTTON") == 0) {
                SetWindowSubclass(hChild, BtnCheckboxSubclassProc, 1, 0);
                // Remove native theme so our custom WM_PAINT takes full control
                SetWindowTheme(hChild, L"", L"");
                InvalidateRect(hChild, NULL, TRUE);
            } else if (wcscmp(cls, L"Static") == 0 || wcscmp(cls, L"STATIC") == 0) {
                SetWindowSubclass(hChild, StaticSubclassProc, 1, 0);
                InvalidateRect(hChild, NULL, TRUE);
            } else if (wcscmp(cls, L"msctls_trackbar32") == 0) {
                SetWindowTheme(hChild, L"DarkMode_Explorer", NULL);
            }
            return TRUE;
        }, (LPARAM)hFont);

        (void)hBtnOk; (void)hBtnReset; (void)hBtnCancel;
        return 0;
    }

    case WM_HSCROLL: {
        if (!d) break;
        HWND hSlider = (HWND)lParam;
        for (int i = 0; i < 8; i++) {
            if (hSlider == d->hSliderR[i]) {
                int val = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                g_config.aura[i].red_adjust = val;
                wchar_t v[8]; swprintf(v, 8, L"%d", val);
                SetWindowTextW(d->hValR[i], v);
            } else if (hSlider == d->hSliderG[i]) {
                int val = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                g_config.aura[i].green_adjust = val;
                wchar_t v[8]; swprintf(v, 8, L"%d", val);
                SetWindowTextW(d->hValG[i], v);
            } else if (hSlider == d->hSliderB[i]) {
                int val = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
                g_config.aura[i].blue_adjust = val;
                wchar_t v[8]; swprintf(v, 8, L"%d", val);
                SetWindowTextW(d->hValB[i], v);
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            // Values already updated live via WM_HSCROLL; just save & close
            SaveSettings();
            HWND hPar = d ? d->hParent : GetWindow(hWnd, GW_OWNER);
            if (d) { delete d; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); d = nullptr; }
            if (hPar) { EnableWindow(hPar, TRUE); SetForegroundWindow(hPar); }
            DestroyWindow(hWnd);
        } else if (id == IDRETRY) {
            if (!d) break;
            for (int i = 0; i < 8; i++) {
                g_config.aura[i].red_adjust = 100;
                g_config.aura[i].green_adjust = 100;
                g_config.aura[i].blue_adjust = 100;
                SendMessage(d->hSliderR[i], TBM_SETPOS, TRUE, 100);
                SendMessage(d->hSliderG[i], TBM_SETPOS, TRUE, 100);
                SendMessage(d->hSliderB[i], TBM_SETPOS, TRUE, 100);
                SetWindowTextW(d->hValR[i], L"100");
                SetWindowTextW(d->hValG[i], L"100");
                SetWindowTextW(d->hValB[i], L"100");
            }
        } else if (id == IDCANCEL) {
            HWND hPar = d ? d->hParent : GetWindow(hWnd, GW_OWNER);
            if (d) { delete d; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); d = nullptr; }
            if (hPar) { EnableWindow(hPar, TRUE); SetForegroundWindow(hPar); }
            DestroyWindow(hWnd);
        }
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 225, 235));
        SetBkMode(hdc, TRANSPARENT);
        static HBRUSH hStaticBrush = CreateSolidBrush(RGB(30, 34, 46));
        return (LRESULT)hStaticBrush;
    }

    // WM_CTLCOLORBTN: return transparent brush so BtnCheckboxSubclassProc
    // can draw the full button background via WM_ERASEBKGND / WM_PAINT.
    // Native buttons only respect this if they are owner-draw; our subclass
    // handles painting itself, so we just supply the background brush.
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        static HBRUSH hBtnBrush = CreateSolidBrush(RGB(30, 34, 46));
        return (LRESULT)hBtnBrush;
    }

    case WM_PRINTCLIENT: {
        // BtnCheckboxSubclassProc calls WM_PRINTCLIENT on parent to get bg
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH hBr = CreateSolidBrush(RGB(30, 34, 46));
        FillRect(hdc, &rc, hBr);
        DeleteObject(hBr);
        return 1;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH hBr = CreateSolidBrush(RGB(30, 34, 46));
        FillRect(hdc, &rc, hBr);
        DeleteObject(hBr);
        return 1;
    }

    case WM_DESTROY: {
        HWND hParent = GetWindow(hWnd, GW_OWNER);
        if (d) { delete d; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); }
        if (hParent) { EnableWindow(hParent, TRUE); SetForegroundWindow(hParent); }
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            HWND hParent = GetWindow(hWnd, GW_OWNER);
            if (d) { delete d; SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0); }
            if (hParent) { EnableWindow(hParent, TRUE); SetForegroundWindow(hParent); }
            DestroyWindow(hWnd);
        }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void ShowChannelSettingsDialog(HWND hParent) {
    static bool s_classRegistered = false;
    if (!s_classRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = ChanSettingsWndProc;
        wc.hInstance     = GetModuleHandleW(NULL);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszClassName = L"ChanSettingsDlg";
        RegisterClassW(&wc);
        s_classRegistered = true;
    }

    // Window size: 8 rows × 30px + header(36) + buttons(70) + padding(20) = ~436 tall
    // Width: colB(432) + slider(110) + val(32) + margin(16) = ~590
    int dlgW = 596, dlgH = 450;

    // Center over parent
    RECT pr; GetWindowRect(hParent, &pr);
    int cx = pr.left + (pr.right - pr.left - dlgW) / 2;
    int cy = pr.top  + (pr.bottom - pr.top - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW,
        L"ChanSettingsDlg",
        g_str->csTitle,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, dlgW, dlgH,
        hParent, NULL, GetModuleHandleW(NULL), NULL);

    if (!hDlg) return;

    // Store parent ref in dialog data after WM_CREATE sets GWLP_USERDATA
    ChanDlgData* d = (ChanDlgData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);
    if (d) d->hParent = hParent;

    // Modal: disable parent, run message loop
    EnableWindow(hParent, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);
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
    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);

    if (channel < 0 || channel >= AURA_CONFIG_CHANNELS) {
        return false;
    }

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

    int setCount = 0;
    ApplyAsusChannelColor(dev, channel, directChannel, ledCount, r, g, b, true, false, setCount);

    hid_close(dev);
    hid_exit();
    return (setCount > 0);
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
            swprintf(title, 128, L"ASUS Aura - %s (%d Kan\u00E4le)", fw, numCh);
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

        int row1Y = gy + (rowH * 0) + (rowH - SLIDER_H) / 2;
        int row2Y = gy + (rowH * 1) + (rowH - SLIDER_H) / 2;
        int row3Y = gy + (rowH * 2) + (rowH - SLIDER_H) / 2;

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
        g_state.hSliderR = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            sliderX, row1Y, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_R, hInst, NULL);
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
        g_state.hSliderG = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            sliderX, row2Y, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_G, hInst, NULL);
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
        g_state.hSliderB = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            sliderX, row3Y, sliderW, SLIDER_H, hWnd, (HMENU)ID_SLIDER_B, hInst, NULL);
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
            CreateWindowW(L"BUTTON", presets[i].label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
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
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            gx+75, gy, MAX_COMBO_W, 200, hWnd, (HMENU)ID_COMBO_KB_MODE, hInst, NULL);
        const wchar_t* kbModes[] = {g_str->modeStatic, g_str->modeBreathing, L"Spectrum", L"Wave Short",
            L"Wave Long", L"Color Wheel", g_str->modeReactive, L"Ripple", L"Starlight", g_str->modeRainbow, L"Hurricane"};
        for (int i = 0; i < 11; i++) SendMessageW(g_state.hComboKbMode, CB_ADDSTRING, 0, (LPARAM)kbModes[i]);
        SendMessage(g_state.hComboKbMode, CB_SETCURSEL, KbModeToIndex(g_state.kbMode), 0);
        AddTooltip(g_state.hTooltip, g_state.hComboKbMode, g_str->tipKeyboardMode);

        // Edge mode combo
        int edgeX = gx + 75 + MAX_COMBO_W + 20;
        CreateWindowW(L"STATIC", g_str->edgeEffect, WS_CHILD | WS_VISIBLE, edgeX, gy+3, 50, 18, hWnd, NULL, hInst, NULL);
        g_state.hComboEdgeMode = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            edgeX+55, gy, MAX_COMBO_W, 200, hWnd, (HMENU)ID_COMBO_EDGE_MODE, hInst, NULL);
        const wchar_t* edgeModes[] = {g_str->edgeStatic, g_str->edgeBreathing, g_str->edgeWave, g_str->edgeSpectrum, g_str->edgeOff};
        for (int i = 0; i < 5; i++) SendMessageW(g_state.hComboEdgeMode, CB_ADDSTRING, 0, (LPARAM)edgeModes[i]);
        SendMessage(g_state.hComboEdgeMode, CB_SETCURSEL, EdgeModeToIndex(g_state.edgeMode), 0);
        AddTooltip(g_state.hTooltip, g_state.hComboEdgeMode, g_str->tipEdgeMode);

        // Brightness slider
        int effY2 = gy + CTRL_H + ITEM_SPACING + 15;
        CreateWindowW(L"STATIC", g_str->brightness, WS_CHILD | WS_VISIBLE, gx, effY2+2, 70, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderBrightness = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            gx+75, effY2, 150, SLIDER_H, hWnd, (HMENU)ID_SLIDER_BRIGHTNESS, hInst, NULL);
        SendMessage(g_state.hSliderBrightness, TBM_SETRANGE, TRUE, MAKELPARAM(0, 4));
        SendMessage(g_state.hSliderBrightness, TBM_SETPOS, TRUE, g_state.brightness);
        SetWindowSubclass(g_state.hSliderBrightness, SliderSubclassProc, 4, (DWORD_PTR)&g_sliderBrightness);
        g_sliderBrightness.slider.hWnd = g_state.hSliderBrightness;
        g_sliderBrightness.slider.channel = 'X';
        g_sliderBrightness.slider.maxValue = 4;

        // Speed slider
        CreateWindowW(L"STATIC", g_str->speed, WS_CHILD | WS_VISIBLE, edgeX, effY2+2, 50, 18, hWnd, NULL, hInst, NULL);
        g_state.hSliderSpeed = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            edgeX+55, effY2, 150, SLIDER_H, hWnd, (HMENU)ID_SLIDER_SPEED, hInst, NULL);
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
        g_state.hCheckAura = CreateWindowW(L"BUTTON", L"Aura", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_AURA, hInst, NULL);
        g_state.hCheckMouse = CreateWindowW(L"BUTTON", L"Mouse", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_MOUSE, hInst, NULL);
        g_state.hCheckKeyboard = CreateWindowW(L"BUTTON", L"Keyboard", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step*2, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_KEYBOARD, hInst, NULL);
        g_state.hCheckRAM = CreateWindowW(L"BUTTON", L"RAM", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step*3, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_RAM, hInst, NULL);
        g_state.hCheckEdge = CreateWindowW(L"BUTTON", L"Edge", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step*4, gy, ck_width, 20, hWnd, (HMENU)ID_CHECK_EDGE, hInst, NULL);
        SendMessage(g_state.hCheckAura, BM_SETCHECK, g_state.enableAura ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckMouse, BM_SETCHECK, g_state.enableMouse ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckKeyboard, BM_SETCHECK, g_state.enableKeyboard ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckRAM, BM_SETCHECK, g_state.enableRAM ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckEdge, BM_SETCHECK, g_state.enableEdge ? BST_CHECKED : BST_UNCHECKED, 0);

        // Utility buttons (second row)
        int btnY2 = gy + 24;
        CreateWindowW(L"BUTTON", g_str->channelCorrection, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            gx, btnY2, 100, BTN_H, hWnd, (HMENU)ID_BTN_CHANNEL_SETTINGS, hInst, NULL);
        CreateWindowW(L"BUTTON", L"ASUS Test", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            gx+106, btnY2, 80, BTN_H, hWnd, (HMENU)ID_BTN_ASUS_TEST, hInst, NULL);
        CreateWindowW(L"BUTTON", L"HID Reset", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
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
        // One uniform gap for the whole row. The old code used 15 between the
        // combo and the buttons but only 5 after the label, which is what made
        // the spacing look wrong. The 15 was originally chosen to leave room for
        // a +5px "glow" expansion, but comboboxes and buttons are both created
        // with expand=false further down, so nothing expands and the extra
        // allowance is dead - hence a single, honest gap value.
        int profGap = 10;
        int profX = gx;
        // Centre the label against the control height instead of a fixed +3.
        CreateWindowW(L"STATIC", g_str->profile, WS_CHILD | WS_VISIBLE,
            profX, gy + (BTN_H - 18) / 2, profLabelW, 18, hWnd, NULL, hInst, NULL);
        profX += profLabelW + profGap;
        g_state.hComboProfiles = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL,
            profX, gy, profComboW, 200, hWnd, (HMENU)ID_COMBO_PROFILES, hInst, NULL);
        // Match the closed combo height to the buttons next to it, otherwise the
        // row sits on three different baselines. -1 addresses the edit field.
        SendMessage(g_state.hComboProfiles, CB_SETITEMHEIGHT, (WPARAM)-1, BTN_H - 8);
        profX += profComboW + profGap;
        CreateWindowW(L"BUTTON", g_str->save, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            profX, gy, profBtnW, BTN_H, hWnd, (HMENU)ID_BTN_SAVE_PROFILE, hInst, NULL);
        profX += profBtnW + profGap;
        CreateWindowW(L"BUTTON", g_str->load, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            profX, gy, profBtnW, BTN_H, hWnd, (HMENU)ID_BTN_LOAD_PROFILE, hInst, NULL);

        // Settings checkboxes row
        int setY = gy + CTRL_H + ITEM_SPACING;
        g_state.hCheckAutostart = CreateWindowW(L"BUTTON", g_str->autostart, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_AUTOSTART, hInst, NULL);
        g_state.hCheckMinimizeTray = CreateWindowW(L"BUTTON", g_str->tray, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_MINIMIZE_TRAY, hInst, NULL);
        g_state.hCheckAutoApply = CreateWindowW(L"BUTTON", g_str->autoApply, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            gx+ck_step*2, setY, ck_width, 20, hWnd, (HMENU)ID_CHECK_AUTO_APPLY, hInst, NULL);
        SendMessage(g_state.hCheckAutostart, BM_SETCHECK, g_state.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckMinimizeTray, BM_SETCHECK, g_state.minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(g_state.hCheckAutoApply, BM_SETCHECK, g_state.autoApply ? BST_CHECKED : BST_UNCHECKED, 0);
        curY = g_cards[3].rect.bottom + GROUP_MARGIN;

        // ============= ACTION BUTTONS =============
        CreateWindowW(L"BUTTON", g_str->apply, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            MARGIN, curY, MAX_BUTTON_W + 20, BTN_H + 4, hWnd, (HMENU)ID_BTN_APPLY, hInst, NULL);
        CreateWindowW(L"BUTTON", g_str->theme, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            MARGIN + MAX_BUTTON_W + 30, curY, 65, BTN_H, hWnd, (HMENU)ID_BTN_THEME, hInst, NULL);
        CreateWindowW(L"BUTTON", L"DE/EN", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
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
                LONG style = GetWindowLong(hChild, GWL_STYLE);
                if ((style & BS_TYPEMASK) == BS_AUTOCHECKBOX) {
                    SetWindowLong(hChild, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_CHECKBOX);
                    SetWindowPos(hChild, NULL, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                }
                SetWindowSubclass(hChild, BtnCheckboxSubclassProc, 1, 0);
                expand = false;
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
                // Keep trackbar geometry unchanged to avoid overlap/input blocking
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

        // Settings are already loaded via LoadAppSettings() in WinMain
        RefreshProfileList();
        UpdateSliders();
        UpdatePreview();

        // Deliberately NOT auto-loading lastProfile here. The colours the user
        // last had are already in g_state from LoadAppSettings(); applying the
        // profile on top would silently discard every manual change made since
        // that profile was last used - the classic "I changed the colour, went
        // away, came back and the old profile is on again". The name is only
        // preselected in the combo so that Laden/Speichern target it with one
        // click. A profile is applied when the user asks for it, never on its own.
        g_state.currentProfile = g_state.lastProfile;

        // Register global hotkeys
        RegisterHotKey(hWnd, ID_HOTKEY_BLUE, MOD_CONTROL | MOD_ALT, 'B');
        RegisterHotKey(hWnd, ID_HOTKEY_RED, MOD_CONTROL | MOD_ALT, 'R');
        RegisterHotKey(hWnd, ID_HOTKEY_GREEN, MOD_CONTROL | MOD_ALT, 'G');
        RegisterHotKey(hWnd, ID_HOTKEY_WHITE, MOD_CONTROL | MOD_ALT, 'W');
        RegisterHotKey(hWnd, ID_HOTKEY_OFF, MOD_CONTROL | MOD_ALT, '0');

        // Final sync of all UI elements to the loaded config
        UpdateAllControls();

        // Start centralized apply worker queue
        StartApplyWorker();

        // Apply colors on startup (unless --no-apply)
        if (!g_skipApplyOnStart) {
            RequestApplyColors(true);
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
        UINT scrollCode = LOWORD(wParam);
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
        UpdatePreview();
        // Update hex display
        if (g_state.hEditHex) {
            wchar_t hex[10];
            swprintf(hex, 10, L"#%02X%02X%02X", g_state.red, g_state.green, g_state.blue);
            SetWindowTextW(g_state.hEditHex, hex);
        }
        // Persist immediately when drag ends or auto-apply is disabled
        if (!g_state.autoApply || scrollCode == TB_ENDTRACK || scrollCode == TB_THUMBPOSITION || scrollCode == SB_ENDSCROLL) {
            SaveSettings();
        }
        // Live preview if enabled
        if (g_state.autoApply) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            SetTimer(hWnd, ID_TIMER_DEBOUNCE, APPLY_DEBOUNCE_MS, NULL);
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
        // A read-only EDIT is coloured via WM_CTLCOLORSTATIC, not
        // WM_CTLCOLOREDIT. The status log is exactly such a control: with a
        // hollow brush its background is never erased, so every append - and
        // every EM_SCROLLCARET scroll - leaves the previous text standing and
        // the lines pile up on top of each other. Give it the same opaque
        // brush the editable fields get; real STATIC labels keep the
        // transparent treatment so the gradient shows through.
        if ((HWND)lParam == g_state.hStatus) {
            SetBkMode(hdcCtrl, OPAQUE);
            SetBkColor(hdcCtrl, g_currentTheme->bgControl);
            if (!g_hBgBrush) g_hBgBrush = CreateSolidBrush(g_currentTheme->bgControl);
            return (LRESULT)g_hBgBrush;
        }
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
        case ID_HOTKEY_BLUE: SetPresetColor(0, 34, 255); RequestApplyColors(true); break;
        case ID_HOTKEY_RED: SetPresetColor(255, 0, 0); RequestApplyColors(true); break;
        case ID_HOTKEY_GREEN: SetPresetColor(0, 255, 0); RequestApplyColors(true); break;
        case ID_HOTKEY_WHITE: SetPresetColor(255, 255, 255); RequestApplyColors(true); break;
        case ID_HOTKEY_OFF: SetPresetColor(0, 0, 0); RequestApplyColors(true); break;
        }
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_APPLY) {
            CommitStateAndApply(true);
        }
        else if (id == ID_BTN_PICK_COLOR) {
            PickColor();
            CommitStateAndApply(false);
        }
        else if (id == ID_EDIT_HEX && code == EN_KILLFOCUS) {
            wchar_t hex[16];
            GetWindowTextW(g_state.hEditHex, hex, 16);
            ParseHexColor(hex);
            UpdatePreview();
            UpdateSliders();
            // Hex-Änderung speichern
            CommitStateAndApply(false);
        }
        else if (id == ID_COMBO_KB_MODE && code == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_state.hComboKbMode, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < KB_MODE_COUNT) {
                g_state.kbMode = IndexToKbMode(sel);
                CommitStateAndApply(false);
            }
        }
        else if (id == ID_COMBO_EDGE_MODE && code == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_state.hComboEdgeMode, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < EDGE_MODE_COUNT) {
                g_state.edgeMode = IndexToEdgeMode(sel);
                CommitStateAndApply(false);
            }
        }
        else if (id == ID_CHECK_AURA) { g_state.enableAura = (SendMessage(g_state.hCheckAura, BM_GETCHECK, 0, 0) == BST_CHECKED); CommitStateAndApply(false); }
        else if (id == ID_CHECK_MOUSE) { g_state.enableMouse = (SendMessage(g_state.hCheckMouse, BM_GETCHECK, 0, 0) == BST_CHECKED); CommitStateAndApply(false); }
        else if (id == ID_CHECK_KEYBOARD) { g_state.enableKeyboard = (SendMessage(g_state.hCheckKeyboard, BM_GETCHECK, 0, 0) == BST_CHECKED); CommitStateAndApply(false); }
        else if (id == ID_CHECK_RAM) { g_state.enableRAM = (SendMessage(g_state.hCheckRAM, BM_GETCHECK, 0, 0) == BST_CHECKED); CommitStateAndApply(false); }
        else if (id == ID_CHECK_EDGE) { g_state.enableEdge = (SendMessage(g_state.hCheckEdge, BM_GETCHECK, 0, 0) == BST_CHECKED); CommitStateAndApply(false); }
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
            // Dialog commits through SaveSettings() on OK
            ShowChannelSettingsDialog(hWnd);
            AppendStatus(L"Channel corrections updated");
            CommitStateAndApply(false);
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
            CommitStateAndApply(true);
        }
        else if (id == ID_BTN_SAVE_PROFILE) {
            wchar_t name[64];
            GetWindowTextW(g_state.hComboProfiles, name, 64);
            if (wcslen(name) > 0) {
                SaveProfile(name);      // also makes it the active profile
                RefreshProfileList();
                SaveSettings();         // persist the new lastProfile right away
                UpdateAllControls();    // reselect it in the combo
            }
        }
        else if (id == ID_BTN_LOAD_PROFILE) {
            wchar_t name[64];
            GetWindowTextW(g_state.hComboProfiles, name, 64);
            if (wcslen(name) > 0) {
                if (LoadProfile(name)) {
                    UpdateAllControls();
                    CommitStateAndApply(true);
                } else {
                    AppendStatus(L"Profile load failed");
                }
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
            SaveAppSettings();

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
            SaveAppSettings();

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
        else if (id == ID_TRAY_BLUE) { SetPresetColor(0, 34, 255); RequestApplyColors(true); }
        else if (id == ID_TRAY_RED) { SetPresetColor(255, 0, 0); RequestApplyColors(true); }
        else if (id == ID_TRAY_GREEN) { SetPresetColor(0, 255, 0); RequestApplyColors(true); }
        else if (id == ID_TRAY_WHITE) { SetPresetColor(255, 255, 255); RequestApplyColors(true); }
        else if (id == ID_TRAY_OFF) { SetPresetColor(0, 0, 0); RequestApplyColors(true); }
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
        // A >5s time jump means the machine really was out, even if we never
        // saw PBT_APMSUSPEND (it is not delivered for every sleep path).
        g_suspendSeen = true;
        ScheduleResumeReset(hWnd);
        return 0;
    }

    case WM_APP_STATUS_APPEND: {
        std::wstring* msg = (std::wstring*)lParam;
        if (msg) {
            AppendStatus(msg->c_str());
            delete msg;
        }
        return 0;
    }

    case WM_APP_STATUS_CLEAR:
        ClearStatus();
        return 0;

    case WM_POWERBROADCAST: {
        // Handle SUSPEND - turn off all devices before sleep
        if (wParam == PBT_APMSUSPEND) {
            g_suspendSeen = true;
            ClearStatus();
            AppendStatus(L"System entering standby...");
            // Turn off all RGB devices for clean state
            {
                std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);
                hid_init();
                if (g_state.enableAura) SetAsusAura(0, 0, 0);
                if (g_state.enableMouse) SetSteelSeries(0, 0, 0);
                if (g_state.enableKeyboard) SetEVisionKeyboard(0, 0, 0, 0, 0, 0);
                if (g_state.enableEdge) SetEVisionEdge(0, 0, 0, EDGE_MODE_OFF, 0, 0);
                if (g_state.enableRAM) SetGSkillRAM(0, 0, 0);
                hid_exit();
            }
            AppendStatus(L"Devices off - ready for standby");
        }
        // Handle RESUME from sleep/hibernate
        else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            g_suspendSeen = true;   // authoritative: we really were suspended
            ScheduleResumeReset(hWnd);
        }
        // Display power state change. Monitor-on is NOT a resume - it fires on
        // every AFK dim/wake cycle. Only honoured when a suspend preceded it.
        else if (wParam == PBT_POWERSETTINGCHANGE) {
            POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)lParam;
            if (pbs && pbs->DataLength >= 4) {
                DWORD displayState = *((DWORD*)pbs->Data);
                if (displayState == 1) {
                    ScheduleResumeReset(hWnd);
                }
            }
        }
        return TRUE;
    }

    // Session change (lock/unlock). Unlock alone is not a resume either.
    case WM_WTSSESSION_CHANGE: {
        // WTS_SESSION_UNLOCK = 0x8
        if (wParam == 0x8) {
            ScheduleResumeReset(hWnd);
        }
        return TRUE;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_RESUME) {
            KillTimer(hWnd, ID_TIMER_RESUME);
            g_resetArmed = false;
            // Consume the suspend: further display-on/unlock events must not
            // queue another reset until the next genuine standby.
            g_suspendSeen = false;
            if (g_resetInFlight.exchange(true)) break;  // one at a time
            ClearStatus();
            AppendStatus(L"System resumed - resetting RGB...");
            // Off the UI thread: FullHIDReset() sleeps ~800ms and re-enumerates
            // the HID stack. Blocking the pump here lets power events pile up
            // and re-arm this timer over and over.
            std::thread([] {
                FullHIDReset();
                RequestApplyColors(true);
                g_lastResetTick = GetTickCount64();  // cooldown starts at completion
                g_resetInFlight = false;
            }).detach();
        }
        else if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            CommitStateAndApply(true);
        }
       
        break;

    case WM_DESTROY:
        // Save window position before exit
        SaveAppSettings();
        g_watcherRunning = false;  // Stop resume watcher thread
        StopApplyWorker();
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
        if (g_hWndBgBrush) { DeleteObject(g_hWndBgBrush); g_hWndBgBrush = NULL; }
        PostQuitMessage(0);
        break;

    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Forward declarations already at top: StaticSubclassProc
LRESULT CALLBACK StaticSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto drawStatic = [&](HDC hdc, const RECT& rc) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        FillCtrlBackground(hdcMem, hWnd, rc);

        wchar_t text[256];
        GetWindowTextW(hWnd, text, 256);

        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, g_currentTheme->textPrimary);
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

        RECT rcText = rc;
        DrawTextW(hdcMem, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdcMem, hOldFont);
        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
    };

    if (uMsg == WM_ERASEBKGND) return 1;
    if (uMsg == WM_PRINTCLIENT) {
        HDC hdc = (HDC)wParam;
        if (!hdc) return 0;
        RECT rc; GetClientRect(hWnd, &rc);
        drawStatic(hdc, rc);
        return 0;
    }
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        drawStatic(hdc, rc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SliderSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    CustomSliderData* data = (CustomSliderData*)dwRefData;
    ModernSlider& mslider = data->slider;

    auto drawSlider = [&](HDC hdc, const RECT& rc) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        FillCtrlBackground(hdcMem, hWnd, rc);

        mslider.value = (int)SendMessage(hWnd, TBM_GETPOS, 0, 0);
        mslider.rect = {10, 10, rc.right - 10, rc.bottom - 10};
        mslider.Draw(hdcMem);

        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
    };
    
    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PRINTCLIENT: {
            HDC hdc = (HDC)wParam;
            if (!hdc) return 0;
            RECT rc;
            GetClientRect(hWnd, &rc);
            drawSlider(hdc, rc);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            drawSlider(hdc, rc);
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
    LONG style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;
    bool isCheckbox = (style == BS_AUTOCHECKBOX || style == BS_CHECKBOX);

    auto drawCustom = [&](HDC hdcTarget, const RECT& rc) {
        HDC hdcMem = CreateCompatibleDC(hdcTarget);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdcTarget, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        FillCtrlBackground(hdcMem, hWnd, rc);

        bool isHovered = (bool)GetPropW(hWnd, L"hover");

        if (isCheckbox) {
            ModernCheckbox cb;
            cb.rect = {5, 5, rc.right - 5, rc.bottom - 5};
            GetWindowTextW(hWnd, cb.text, 64);
            cb.isChecked = (SendMessage(hWnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
            cb.isEnabled = IsWindowEnabled(hWnd);
            cb.isHovered = isHovered;
            cb.Draw(hdcMem);
        } else {
            ModernButton btn;
            btn.rect = {5, 5, rc.right - 5, rc.bottom - 5};
            GetWindowTextW(hWnd, btn.text, 64);
            btn.isPressed = (SendMessage(hWnd, BM_GETSTATE, 0, 0) & BST_PUSHED);
            btn.isEnabled = IsWindowEnabled(hWnd);
            btn.isHovered = isHovered;
            btn.isAccent = (GetWindowLong(hWnd, GWLP_ID) == ID_BTN_APPLY);

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

        BitBlt(hdcTarget, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
    };

    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PRINTCLIENT: {
            HDC hdc = (HDC)wParam;
            if (!hdc) return 0;
            RECT rc;
            GetClientRect(hWnd, &rc);
            drawCustom(hdc, rc);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            drawCustom(hdc, rc);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            if (isCheckbox) {
                SetCapture(hWnd);
                SetPropW(hWnd, L"pressed", (HANDLE)1);
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (isCheckbox) {
                RECT rc;
                GetClientRect(hWnd, &rc);
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                bool inside = PtInRect(&rc, pt) != FALSE;

                if (GetCapture() == hWnd) {
                    ReleaseCapture();
                }
                RemovePropW(hWnd, L"pressed");

                if (inside) {
                    LRESULT chk = SendMessage(hWnd, BM_GETCHECK, 0, 0);
                    SendMessage(hWnd, BM_SETCHECK, (chk == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED, 0);
                    HWND hParent = GetParent(hWnd);
                    if (hParent) {
                        SendMessage(hParent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), (LPARAM)hWnd);
                    }
                }

                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (isCheckbox && (wParam == VK_SPACE || wParam == VK_RETURN)) {
                LRESULT chk = SendMessage(hWnd, BM_GETCHECK, 0, 0);
                SendMessage(hWnd, BM_SETCHECK, (chk == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED, 0);
                HWND hParent = GetParent(hWnd);
                if (hParent) {
                    SendMessage(hParent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), (LPARAM)hWnd);
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_CAPTURECHANGED:
            if (isCheckbox) {
                RemovePropW(hWnd, L"pressed");
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            break;
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
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
            InvalidateRect(hWnd, NULL, FALSE);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ComboSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto drawCombo = [&](HDC hdc, const RECT& rc) {
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdcMem, hbmMem);

        FillCtrlBackground(hdcMem, hWnd, rc);

        ModernCombo combo;
        combo.rect = {0, 0, rc.right, rc.bottom};
        combo.isHovered = (bool)GetPropW(hWnd, L"hover");
        GetWindowTextW(hWnd, combo.selectedText, 128);
        combo.Draw(hdcMem);

        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOldBm);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
    };

    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PRINTCLIENT: {
        HDC hdc = (HDC)wParam;
        if (!hdc) return 0;
        RECT rc;
        GetClientRect(hWnd, &rc);
        drawCombo(hdc, rc);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        drawCombo(hdc, rc);
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
        // Draw modern rounded border — paint ONLY the NC area (4px ring), never the client area
        HDC hdc = GetWindowDC(hWnd);
        RECT rcWin; GetWindowRect(hWnd, &rcWin);
        OffsetRect(&rcWin, -rcWin.left, -rcWin.top);

        // Exclude client area (NC inset = 4px, see WM_NCCALCSIZE) so we do not
        // overdraw the control's content and trigger a recursive repaint
        ExcludeClipRect(hdc, 4, 4, rcWin.right - 4, rcWin.bottom - 4);

        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        Gdiplus::RectF r(0.0f, 0.0f, (float)rcWin.right, (float)rcWin.bottom);

        // Fill corners outside the rounded rect with the parent background colour
        Gdiplus::SolidBrush bgBrush(g_mTheme->bgPrimary);
        g.FillRectangle(&bgBrush, r);

        // Draw the themed rounded border
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

#include "hardware_probe.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    LogDebug("WinMain started");

    // Command line flags - parsed before ANY device branch below.
    //
    // These used to be read further down, after --probe, --identify, --kbdump,
    // --kbtest, --kbmode* and --mouse-zones-test had already executed and
    // returned. g_state.dryRun was therefore still false while those branches
    // ran: "--dry-run --kbmode-sweep" wrote 21 mode bytes into the keyboard's
    // flash, and "--dry-run --kbtest=lock" set the Win-lock flag for real.
    // The guard only ever protected the GUI apply path it was declared next to.
    bool startMinimized = (strstr(lpCmdLine, "--minimized") != nullptr);
    g_skipApplyOnStart  = (strstr(lpCmdLine, "--no-apply") != nullptr);
    g_state.dryRun      = (strstr(lpCmdLine, "--dry-run") != nullptr);
    bool forceForeground = (strstr(lpCmdLine, "--foreground") != nullptr);
    if (g_state.dryRun) LogDebug("[dry-run] active - no device writes will be sent");

    // =========================================================
    // PHASE 0 DIAGNOSTIC PROBE: --probe / --probe-interactive
    // Headless, read-mostly hardware capability dump. Must run before any
    // GUI/auto-apply so it does not perturb device state. Writes
    // docs/probe_results_<ts>.json + docs/Hardware_Capability_Report.md.
    // =========================================================
    if (strstr(lpCmdLine, "--probe")) {
        bool interactive = (strstr(lpCmdLine, "--probe-interactive") != nullptr);
        return probe::RunHardwareProbe(interactive);
    }

    // Headless zone Identify (verify zoneBit -> physical mapping):
    //   --identify=mouse:<zoneIndex 0..7>
    {
        const char* idArg = strstr(lpCmdLine, "--identify=");
        if (idArg) {
            const char* spec = idArg + strlen("--identify=");
            LoadAppSettings();  // loads g_config.mouseZones (with migration)
            if (strncmp(spec, "mouse:", 6) == 0)
                IdentifyMouseZone(atoi(spec + 6), 3000);
            return 0;
        }
    }

    // --kbdump : read-only hex dump of the whole on-board config memory to
    // %APPDATA%\OneClickRGB\docs\kbdump.txt. The probe only covers the three
    // 0x40-byte profile blocks (0x00..0xBF); the key-remap / macro table that
    // follows at 0xC0+ has never been captured, and that is where Win-Lock is
    // expected to live (the Win key remapped to a no-op rather than a flag).
    // Pure reads - command 0x05 only, no 0x06 writes anywhere in this path.
    if (strstr(lpCmdLine, "--kbdump")) {
        hid_init();
        hid_device* dev = nullptr;
        struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
        for (auto* c = devs; c; c = c->next)
            if (c->usage_page == Devices::EVISION_USAGE_PAGE) { dev = hid_open_path(c->path); break; }
        hid_free_enumeration(devs);
        if (dev) {
            EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); Sleep(20);
            uint8_t prof = 0;
            EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &prof);

            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            FILE* fp = _wfopen((dir + L"\\kbdump.txt").c_str(), L"w");
            if (fp) {
                fprintf(fp, "EVision GK650 config memory dump\nactiveProfile=%d\n\n", (int)prof);
                for (int off = 0; off < 0x400; off += 16) {
                    uint8_t buf[16] = {0};
                    int rr = EVisionQuery(dev, 0x05, (uint16_t)off, nullptr, 16, buf);
                    fprintf(fp, "%04X: ", off);
                    for (int i = 0; i < 16; i++) fprintf(fp, "%02X ", buf[i]);
                    fprintf(fp, "  rr=%d\n", rr);
                    if (rr < 0 && off > 0x100) break;   // stop once the device stops answering
                }
                fclose(fp);
            }
            EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
            hid_close(dev);
        }
        hid_exit();
        return 0;
    }

    // Keyboard Win-lock isolation test (no writes, just session commands):
    //   --kbtest=begin   send 0x01 begin-configure only
    //   --kbtest=end     send 0x02 end-configure only
    //   --kbtest=cycle   send 0x01 then 0x02
    // Lets us pinpoint which command locks/unlocks the Win key.
    {
        const char* kbt = strstr(lpCmdLine, "--kbtest=");
        if (kbt) {
            const char* what = kbt + strlen("--kbtest=");
            if (g_state.dryRun) {
                LogDebug("[dry-run] --kbtest skipped - it sends session commands "
                         "and (lock/unlock) a real flag write");
                return 0;
            }
            hid_init();
            hid_device* dev = nullptr;
            struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
            for (auto* c = devs; c; c = c->next)
                if (c->usage_page == Devices::EVISION_USAGE_PAGE) { dev = hid_open_path(c->path); break; }
            hid_free_enumeration(devs);
            if (dev) {
                if      (strncmp(what, "begin", 5) == 0) { EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); }
                else if (strncmp(what, "end",   3) == 0) { EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr); }
                else if (strncmp(what, "cycle", 5) == 0) { EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); Sleep(50); EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr); }
                else if (strncmp(what, "unlock", 6) == 0 || strncmp(what, "lock", 4) == 0) {
                    // Controlled, reversible test: set/clear the per-profile flag at
                    // profile_base+0x2E (win-lock/game-mode candidate). unlock->0x00, lock->0x01.
                    uint8_t val = (strncmp(what, "lock", 4) == 0 && strncmp(what, "unlock", 6) != 0) ? 0x01 : 0x00;
                    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); Sleep(20);
                    uint8_t p = 0; EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &p); if (p > 2) p = 0;
                    uint16_t off = (uint16_t)(p * 0x40 + 0x2E);
                    uint8_t before = 0xFF, after = 0xFF;
                    EVisionQuery(dev, 0x05, off, nullptr, 1, &before);
                    int wr = EVisionQuery(dev, 0x06, off, &val, 1, nullptr); Sleep(10);
                    EVisionQuery(dev, 0x05, off, nullptr, 1, &after);
                    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
                    // Absolute path next to config.json. A relative "docs" only
                    // resolved when the exe happened to be started from the repo
                    // root; everywhere else SHCreateDirectoryEx rejected it and
                    // the fopen failed, so the test silently produced no report
                    // even though the unlock write itself had gone through.
                    std::wstring dir = GetAppDataPath() + L"\\docs";
                    SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
                    std::wstring rep = dir + L"\\kbtest_unlock.txt";
                    FILE* fp = _wfopen(rep.c_str(), L"w");
                    if (fp) { fprintf(fp, "profile=%d off=0x%02X val=0x%02X before=0x%02X writeRes=%d after=0x%02X\n",
                                      p, off, val, before, wr, after); fclose(fp); }
                }
                hid_close(dev);
            }
            hid_exit();
            return 0;
        }
    }

    // Live keyboard-mode probe - resolves open item 4 in docs/Keyboard_Protocol.md
    // ("confirm which KB_MODE_* values actually animate"). Only 0x06 = STATIC was
    // ever cross-validated against the hardware; every other entry in
    // KB_MODE_TABLE is an unverified guess, which is why picking Breathing in the
    // UI can leave the keyboard exactly as it was.
    //
    // The probe deliberately drives the production path (SetEVisionKeyboard) so
    // it tests what the app really sends, and it reports the read-back from the
    // device - never the value we asked for (CLAUDE.md rule 1).
    //
    //   --kbmode=<n>          write one mode (decimal or 0x-hex), verify, exit
    //   --kbmode-sweep[=sec]  walk 0x00..0x14, hold each <sec> (default 5)
    //
    // Report: %APPDATA%\OneClickRGB\docs\kbmode_probe.txt
    {
        const char* sweepArg  = strstr(lpCmdLine, "--kbmode-sweep");
        const char* singleArg = strstr(lpCmdLine, "--kbmode=");
        if (sweepArg || singleArg) {
            // The sweep drives SetEVisionKeyboard, which is dry-run guarded -
            // but without this branch it would still walk 21 steps, sleep for
            // the full hold time each and fill the report with "READ FAILED"
            // rows. Nothing was attempted, so the report says exactly that.
            if (g_state.dryRun) {
                std::wstring dir = GetAppDataPath() + L"\\docs";
                SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
                FILE* fp = _wfopen((dir + L"\\kbmode_probe.txt").c_str(), L"w");
                if (fp) {
                    fprintf(fp, "EVision GK650 keyboard mode probe\n"
                                "DRY RUN - no write was sent and no value was read.\n"
                                "Run without --dry-run to probe the hardware.\n");
                    fclose(fp);
                }
                LogDebug("[dry-run] --kbmode/--kbmode-sweep skipped - "
                         "would write mode bytes to keyboard flash");
                return 0;
            }

            LoadSettings();

            const uint8_t pr = g_state.red, pg = g_state.green, pb = g_state.blue;
            // Full brightness so a working effect is unmistakable, and a non-zero
            // speed because an animation at speed 0 does not visibly move.
            const uint8_t pbright = 4;
            const uint8_t pspeed  = g_state.speed ? g_state.speed : (uint8_t)2;

            int holdMs = 5000;
            if (sweepArg) {
                const char* eq = sweepArg + strlen("--kbmode-sweep");
                if (*eq == '=') {
                    int s = atoi(eq + 1);
                    if (s >= 1 && s <= 60) holdMs = s * 1000;
                }
            }

            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            FILE* fp = _wfopen((dir + L"\\kbmode_probe.txt").c_str(), L"w");

            if (fp) {
                fprintf(fp, "EVision GK650 keyboard mode probe\n");
                fprintf(fp, "colour=%02X%02X%02X brightness=%d speed=%d hold=%dms\n",
                        pr, pg, pb, pbright, pspeed, holdMs);
                fprintf(fp, "'got' columns are read back from the device after the write.\n\n");
                fprintf(fp, "  t[s]  mode  writeRes readRes  got:mode br sp  rgb       verdict\n");
                fprintf(fp, "  ----  ----  -------- -------  -------- --  --  --------  -------\n");
                fflush(fp);
            }

            // Full read-back of the 18-byte block so the bytes the write path
            // does NOT set stay visible - notably +0x0A, which is where the edge
            // payload keeps its commit/save flag.
            auto dumpBlock = [&](const KbVerifyResult& v) {
                if (!fp || !v.valid) return;
                fprintf(fp, "        block +0x01..+0x12:");
                for (int i = 0; i < 18; i++) fprintf(fp, " %02X", v.got[i]);
                fprintf(fp, "\n        (+0x0A = %02X)\n", v.got[9]);
                fflush(fp);
            };

            auto probeOne = [&](uint8_t mode, int tSec) {
                hid_init();
                SetEVisionKeyboard(pr, pg, pb, mode, pbright, pspeed);
                hid_exit();

                const KbVerifyResult& v = g_lastKbVerify;
                const char* verdict = !v.valid          ? "READ FAILED"
                                    : v.got[0] != mode  ? "REJECTED"
                                    : "accepted";
                if (fp) {
                    fprintf(fp, "  %4d  0x%02X  %8d %7d      0x%02X %2d  %2d  %02X%02X%02X    %s\n",
                            tSec, mode, v.writeRes, v.readRes,
                            v.got[0], v.got[1], v.got[2],
                            v.got[5], v.got[6], v.got[7], verdict);
                    fflush(fp);
                }
                dumpBlock(v);
            };

            if (sweepArg) {
                // 0x00..0x14 rather than just the 11 table entries: if Breathing
                // is not 0x05, the real value is most likely a neighbour that the
                // table never lists.
                int tSec = 0;
                for (uint8_t m = 0x00; m <= 0x14; m++) {
                    probeOne(m, tSec);
                    Sleep(holdMs);
                    tSec += holdMs / 1000;
                }
                if (fp) {
                    fprintf(fp, "\nWatch the keyboard while this runs and note the wall-clock\n"
                                "second at which it animates; the t[s] column maps that back to\n"
                                "the mode byte. 'accepted' only means the byte was stored - it\n"
                                "does not prove the effect renders.\n");
                }
            } else {
                long m = strtol(singleArg + strlen("--kbmode="), nullptr, 0);
                if (m < 0 || m > 0xFF) m = KB_MODE_STATIC;
                probeOne((uint8_t)m, 0);
            }

            if (fp) fclose(fp);
            return 0;
        }
    }

    // Headless per-zone mouse test: 8 distinct colours, one per zone, then exit.
    if (strstr(lpCmdLine, "--mouse-zones-test")) {
        LoadAppSettings();
        static const uint8_t pal[8][3] = {
            {255,0,0},{0,255,0},{0,0,255},{255,255,0},
            {255,0,255},{0,255,255},{255,255,255},{255,128,0}
        };
        for (size_t i = 0; i < g_config.mouseZones.size() && i < 8; i++) {
            g_config.mouseZones[i].color   = { pal[i][0], pal[i][1], pal[i][2] };
            g_config.mouseZones[i].enabled = true;
        }
        hid_init();
        SetSteelSeriesZones();
        hid_exit();
        return 0;
    }

    // =========================================================
    // HEADLESS TEST MODE: --switch-test=<device>
    // Runs a hardware light sequence and exits without showing UI.
    // Usage: OneClickRGB.exe --switch-test=edge|mouse|aura|keyboard|all|aura-spectrum|all-spectrum
    // Sequence: OFF(2s) -> BLUE(4s) -> RED(4s) -> OFF(3s)
    // Additional mode: --switch-test=edge-diagnose
    // =========================================================
    const char* switchTestArg = strstr(lpCmdLine, "--switch-test=");
    if (switchTestArg) {
        const char* devName = switchTestArg + strlen("--switch-test=");

        // ---- edge-diagnose: enumerate all EVision HID interfaces + write probe ----
        if (strncmp(devName, "edge-diagnose", 13) == 0) {
            if (g_state.dryRun) {
                LogDebug("[dry-run] --switch-test=edge-diagnose skipped - it writes "
                         "a probe payload to the active profile");
                return 0;
            }
            hid_init();

            // Report goes next to config.json, not into the CWD. The old
            // fopen("edge_diagnose.txt") only landed somewhere useful when the
            // exe happened to be started from the repo root, and its result was
            // never checked - the very next fprintf dereferenced a possibly
            // NULL FILE*.
            std::wstring diagDir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, diagDir.c_str(), NULL);
            std::wstring diagPath = diagDir + L"\\edge_diagnose.txt";
            FILE* diag = _wfopen(diagPath.c_str(), L"w");
            if (!diag) {
                LogDebug("[EVision] edge-diagnose: cannot open report file - aborted");
                hid_exit();
                return 1;
            }
            fprintf(diag, "=== OneClickRGB Edge HID Diagnose ===\n");
            fprintf(diag, "Scanning VID=0x%04X (EVision/Endorfy)...\n\n",
                    (unsigned)Devices::EVISION_VID);

            // List every interface for the EVision VID
            struct hid_device_info* all = hid_enumerate(Devices::EVISION_VID, 0);
            int ifcount = 0;
            for (auto* cur = all; cur; cur = cur->next) {
                fprintf(diag,
                    "PID=0x%04X  IfaceNo=%2d  UsagePage=0x%04X  Usage=0x%04X  Path=%s\n",
                    (unsigned)cur->product_id,
                    cur->interface_number,
                    (unsigned)cur->usage_page,
                    (unsigned)cur->usage,
                    cur->path ? cur->path : "(no path)");
                ifcount++;
            }
            hid_free_enumeration(all);
            fprintf(diag, "\n%d interface(s) found.\n", ifcount);

            // ----------------------------------------------------------------
            // Write probe: ONE write, to the one documented edge slot.
            //
            // This branch used to carry the 15-offset brute-force verbatim
            // (0x13/0x16/0x19/0x1B/0x1E in every one of the three profile
            // blocks) - the exact code that was removed from SetEVisionEdge
            // because it scribbled the 10-byte payload straight through
            // +0x14..0x1D in all three profiles, five times over at
            // overlapping positions. That is where the "04 02 00 04 02 00 04
            // 00 04 02" pattern in the live dump comes from, and it is what
            // made the Windows-key lock stick. Running the diagnose was
            // corrupting the very state it was meant to explain, in profiles
            // the user may never have selected.
            //
            // CLAUDE.md rule 2: only documented offsets. The edge payload
            // lives at profile_base+0x1E (Keyboard_Protocol.md 3.1), so the
            // probe writes there and nowhere else, in the ACTIVE profile only.
            // Payload and commit flag are identical to SetEVisionEdge - a
            // diagnose that does not reproduce the production write proves
            // nothing about the production path.
            //
            // The pre-write bytes are recorded below so the previous edge
            // state stays recoverable; a normal apply restores it anyway.
            // ----------------------------------------------------------------
            fprintf(diag, "\n--- Write probe (single write, active profile +0x1E) ---\n");
            char path[512] = {};
            hid_device* dev = OpenEVisionEdgeDev(path, sizeof(path));
            int rc = 0;
            if (!dev) {
                fprintf(diag, "ERROR: Could not open any EVision RGB interface.\n");
                rc = 2;
            } else {
                fprintf(diag, "Opened: %s\n", path);
                EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); Sleep(25);
                uint8_t activeProfile = 0;
                EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &activeProfile);
                if (activeProfile > 2) activeProfile = 0;
                uint16_t edgeOff = (uint16_t)(activeProfile * 0x40 + 0x1E);
                fprintf(diag, "Active profile index: %d  ->  edge slot 0x%02X\n\n",
                        (int)activeProfile, (unsigned)edgeOff);

                // Probe payload: STATIC BLUE, commit flag set (byte[9]=0x01),
                // same as SetEVisionEdge builds for EDGE_MODE_STATIC.
                uint8_t testData[10] = {0x04, 4, 2, 0, 0, 0, 0, 255, 0, 0x01};

                uint8_t before[10] = {0};
                int beforeRes = EVisionQuery(dev, 0x05, edgeOff, nullptr, 10, before);
                fprintf(diag, "  before (readRes=%d):", beforeRes);
                for (int i = 0; i < 10; i++) fprintf(diag, " %02X", before[i]);
                fprintf(diag, "\n");

                int wr = EVisionQuery(dev, 0x06, edgeOff, testData, 10, nullptr);
                Sleep(10);

                // Read-back decides, not the write result: the firmware ACKs
                // writes it discards (CLAUDE.md rule 1).
                uint8_t after[10] = {0};
                int afterRes = EVisionQuery(dev, 0x05, edgeOff, nullptr, 10, after);
                fprintf(diag, "  wrote  (writeRes=%d):", wr);
                for (int i = 0; i < 10; i++) fprintf(diag, " %02X", testData[i]);
                fprintf(diag, "\n  after  (readRes=%d):", afterRes);
                for (int i = 0; i < 10; i++) fprintf(diag, " %02X", after[i]);
                fprintf(diag, "\n\n");

                bool verified = (afterRes >= 0) &&
                                after[0] == testData[0] && after[1] == testData[1] &&
                                after[2] == testData[2] && after[5] == testData[5] &&
                                after[6] == testData[6] && after[7] == testData[7];

                if (afterRes < 0) {
                    fprintf(diag, "VERDICT: read-back failed (%d) - nothing verified.\n", afterRes);
                    rc = 1;
                } else if (verified) {
                    fprintf(diag, "VERDICT: verified - 0x%02X holds the payload we sent.\n",
                            (unsigned)edgeOff);
                } else {
                    fprintf(diag, "VERDICT: MISMATCH - device kept mode=0x%02X br=%d sp=%d "
                                  "rgb=%02X%02X%02X\n",
                            after[0], after[1], after[2], after[5], after[6], after[7]);
                    for (int i = 0; i < 10; i++)
                        if (after[i] != testData[i])
                            fprintf(diag, "  byte[%d]: want %02X, got %02X\n",
                                    i, testData[i], after[i]);
                    rc = 1;
                }

                // No unlock write at 0x14 here anymore. It only existed to
                // repair what the brute-force above had just destroyed; with
                // the brute-force gone it is an unrelated side effect in a
                // diagnostic, and it was addressed to profile 0 regardless of
                // which profile is active. SetEVisionKeyboard clears the flag
                // for the active profile on every apply.
                EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
                hid_close(dev);

                fprintf(diag, "\nThe edge strip should now be blue. If the read-back says\n"
                              "verified but nothing lit up, the offset is right and the\n"
                              "problem is elsewhere (mode value, brightness, or wiring).\n");
            }
            fclose(diag);
            hid_exit();
            {
                char done[320];
                snprintf(done, sizeof(done),
                         "[EVision] edge-diagnose rc=%d - report: %%APPDATA%%\\OneClickRGB\\docs\\edge_diagnose.txt",
                         rc);
                LogDebug(done);
            }
            return rc;
        }

        // ---- normal switch-test mode ----
        bool doEdge     = (strncmp(devName, "edge",     4) == 0 || strncmp(devName, "all", 3) == 0);
        bool doMouse    = (strncmp(devName, "mouse",    5) == 0 || strncmp(devName, "all", 3) == 0);
        bool doAura     = (strncmp(devName, "aura",     4) == 0 || strncmp(devName, "all", 3) == 0);
        bool doKeyboard = (strncmp(devName, "keyboard", 8) == 0 || strncmp(devName, "all", 3) == 0);
        bool doSpectrum = (strstr(devName, "spectrum") != nullptr);

        // Load config so channel corrections are available
        LoadSettings();

        {
            char switchDbg[128];
            snprintf(switchDbg, sizeof(switchDbg),
                     "[switch-test] mode=%s edge=%d mouse=%d aura=%d kb=%d spectrum=%d",
                     devName, (int)doEdge, (int)doMouse, (int)doAura, (int)doKeyboard, (int)doSpectrum);
            LogDebug(switchDbg);
        }

        auto hueToRgb = [](int hueDeg, uint8_t& outR, uint8_t& outG, uint8_t& outB) {
            int h = hueDeg % 360;
            if (h < 0) h += 360;
            int sector = h / 60;
            int rem = h % 60;
            int t = (rem * 255) / 60;
            int q = 255 - t;

            switch (sector) {
                case 0: outR = 255; outG = (uint8_t)t; outB = 0; break;
                case 1: outR = (uint8_t)q; outG = 255; outB = 0; break;
                case 2: outR = 0; outG = 255; outB = (uint8_t)t; break;
                case 3: outR = 0; outG = (uint8_t)q; outB = 255; break;
                case 4: outR = (uint8_t)t; outG = 0; outB = 255; break;
                default: outR = 255; outG = 0; outB = (uint8_t)q; break;
            }
        };

        // Helper: apply color to all enabled devices.
        // isOff=true  → EDGE_MODE_OFF (hardware-level off, LEDs dark)
        // isOff=false → EDGE_MODE_STATIC with supplied RGB
        auto testApply = [&](uint8_t r, uint8_t g, uint8_t b, bool isOff) {
            char step[80];
            snprintf(step, sizeof(step),
                     "[switch-test] apply rgb=(%d,%d,%d) off=%d", r, g, b, (int)isOff);
            LogDebug(step);
            hid_init();
            if (doAura)     SetAsusAura(r, g, b);
            if (doMouse)    SetSteelSeries(r, g, b);
            if (doKeyboard) SetEVisionKeyboard(r, g, b, KB_MODE_STATIC, 4, 2);
            if (doEdge) {
                uint8_t eMode = isOff ? EDGE_MODE_OFF : EDGE_MODE_STATIC;
                bool ok = SetEVisionEdge(r, g, b, eMode, 4, 2);
                char edgeRes[64];
                snprintf(edgeRes, sizeof(edgeRes),
                         "[switch-test] SetEVisionEdge result=%d", (int)ok);
                LogDebug(edgeRes);
            }
            hid_exit();
        };

        if (doSpectrum) {
            // Sequence: OFF(1.5s) -> SPECTRUM sweep (~6s) -> OFF(2s)
            LogDebug("[switch-test] phase=OFF(initial)");
            testApply(0, 0, 0, true);      Sleep(1500);

            LogDebug("[switch-test] phase=SPECTRUM(start)");
            for (int h = 0; h < 360; h += 10) {
                uint8_t sr = 0, sg = 0, sb = 0;
                hueToRgb(h, sr, sg, sb);
                char sstep[96];
                snprintf(sstep, sizeof(sstep),
                         "[switch-test] spectrum h=%d rgb=(%d,%d,%d)",
                         h, (int)sr, (int)sg, (int)sb);
                LogDebug(sstep);
                testApply(sr, sg, sb, false);
                Sleep(170);
            }

            LogDebug("[switch-test] phase=OFF(final)");
            testApply(0, 0, 0, true);      Sleep(2000);
        } else {
            // Sequence: OFF(2s) -> BLUE(4s) -> RED(4s) -> OFF(3s)
            LogDebug("[switch-test] phase=OFF(initial)");
            testApply(0, 0, 0, true);      Sleep(2000);
            LogDebug("[switch-test] phase=BLUE");
            testApply(0, 0, 255, false);   Sleep(4000);
            LogDebug("[switch-test] phase=RED");
            testApply(255, 0, 0, false);   Sleep(4000);
            LogDebug("[switch-test] phase=OFF(final)");
            testApply(0, 0, 0, true);      Sleep(3000);
        }

        LogDebug("[switch-test] done");
        return 0;  // Exit without showing any window
    }

    // Initialize GDI+ for PNG loading
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    LogDebug("GDI+ started");

    // Load saved settings (language, theme)
    LoadAppSettings();
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
    // Hintergrundfarbe direkt aus dem aktiven Theme: verhindert den weißen Blitz
    // ("FOUC") bevor WM_PAINT die dunkle Oberfläche zeichnet.
    if (!g_hWndBgBrush)
        g_hWndBgBrush = CreateSolidBrush(g_currentTheme->bgWindowTop);
    wc.hbrBackground = g_hWndBgBrush;
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
        // Erzwinge einen vollständigen Repaint aller Child-Controls sofort,
        // bevor DWM den ersten Frame kompostiert → kein FOUC/Flicker.
        RedrawWindow(g_state.hWnd, NULL, NULL,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

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