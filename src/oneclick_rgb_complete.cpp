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
#include <algorithm>
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
#include "cli_args.h"        // token-exact flag parsing (unit-tested)
#include "effect_limits.h"   // brightness/speed clamps + edge mode table
#include "keyboard_layout.h" // key matrix decoded from the device (unit-tested)
#include "audio_probe.h"     // WASAPI tone + loopback verification (Phase 6)

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
// ID_BTN_PICK_COLOR (1002) ist entfallen. Der Knopf wurde bei der
// UI-Umgestaltung geloescht und durch den klickbaren Vorschau-Swatch
// ersetzt; sein Handler blieb als unerreichbarer Code stehen und liess die
// Datei behaupten, es gaebe diesen Knopf noch.
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
#define ID_BTN_KEY_LAYOUT 1113

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

// Edge modes (Endorfy), the ComboBox index mapping, NormalizeEdgeMode() and the
// brightness/speed clamps now live in src/effect_limits.h - they are the values
// that reach the keyboard's flash, and there they can be unit-tested without
// hardware (tests/test_cli_args.cpp) instead of only by looking at a light.

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

    // Atomic commit: apply loaded snapshot in one state update.
    //
    // Deliberate exception: a profile carries the global colour and the modes,
    // not per-channel overrides - the .rgb format keeps its 12 keys. An ASUS
    // channel the user overrode in the test dialog therefore keeps its colour
    // across a profile load; the precedence is resolved per channel in
    // ApplyAsusChannelColor, so nothing here has to know about it.
    g_state.red = red;
    g_state.green = green;
    g_state.blue = blue;
    // .rgb files are plain text and are also written by older builds, so the
    // values in them are input, not truth. edgeMode already went through
    // NormalizeEdgeMode; brightness and speed went in raw, straight from the
    // file into the profile block of the keyboard's flash. Clamped to the ranges
    // docs/Keyboard_Protocol.md section 3 documents (0..4 / 0..5) - the same
    // clamp the device setters now apply, so what the UI shows after a load and
    // what the hardware gets cannot drift apart.
    g_state.brightness = ClampBrightness(brightness);
    g_state.speed = ClampSpeed(speed);
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
// PROBE INSTANCE LOCK
//
// Every probe drives the keyboard over HID and reads its own writes back. That
// only proves something while this process is the ONLY one touching the device:
// g_state.deviceIoMutex serialises threads inside one process and is worth
// nothing across two of them. EVisionQuery writes a report and then reads
// whatever answer arrives next - with a second process on the same collection,
// that answer may belong to the other one's request.
//
// This is not hypothetical. On 2026-08-17 a --rendercheck=edge and a
// --rendercheck=kb ran at the same time. The two runs interleaved their reports
// into one file, each read back traffic the other had caused, one of them
// "successfully" read the keyboard block as 18 zero bytes - and then restored
// those zeros into flash. The keyboard went dark while every single line of the
// report said "verified". A read-back is only evidence under exclusivity.
//
// So: one hardware probe at a time. A second one refuses to start instead of
// producing a report that looks like a measurement. Acquired AFTER the
// --dry-run bail-outs, because a dry run touches nothing and two of them may
// overlap freely (check_dryrun_flags.ps1 relies on that).
//=============================================================================

static HANDLE g_probeLock = nullptr;

static bool AcquireProbeLock() {
    g_probeLock = CreateMutexW(nullptr, TRUE, L"Local\\OneClickRGB_HidProbe");
    if (!g_probeLock) return false;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_probeLock);
        g_probeLock = nullptr;
        return false;
    }
    return true;
}

static void ReleaseProbeLock() {
    if (!g_probeLock) return;
    ReleaseMutex(g_probeLock);
    CloseHandle(g_probeLock);
    g_probeLock = nullptr;
}

// Writes the standard refusal into a probe's own report file and logs it, so a
// blocked run leaves the same paper trail as any other refusal instead of an
// empty or, worse, a stale report from an earlier run.
static int ProbeLockBusy(const std::wstring& reportPath, const char* tag) {
    FILE* fp = _wfopen(reportPath.c_str(), L"w");
    if (fp) {
        fprintf(fp, "OneClickRGB probe\n"
                    "ABORTED: another OneClickRGB instance is already talking to the\n"
                    "device. Nothing was written and nothing was measured.\n\n"
                    "Two processes on the same HID collection read each other's\n"
                    "answers, so every read-back stops being evidence. Close the other\n"
                    "instance (GUI or probe) and run this again.\n");
        fclose(fp);
    }
    char dbg[192];
    snprintf(dbg, sizeof(dbg),
             "[%s] refused - another instance holds the probe lock; nothing was written", tag);
    LogDebug(dbg);
    return 3;
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

// Hardware configuration from device scan. Pure cache of what the board
// reported (firmware string, 0xB0 table, derived topology) - never user
// settings. Those live in g_config.aura[] and config.json, because
// ParseAsusConfig() rebuilds this struct from the device on every start and
// would clobber them.
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
        // Dead fields, kept only so the on-disk layout of asus_hw_config.bin
        // stays readable. Colour and enable state come from g_config.aura[]
        // now; nothing reads these any more.
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

// Returns true only if every packet of the channel was accepted by the driver.
// That is an *acknowledged send*, not a verified colour: Aura has no GET
// command, so nothing here can prove what the LEDs are actually showing.
bool SetAsusChannel(hid_device* dev, int channel, int numLEDs, uint8_t r, uint8_t g, uint8_t b) {
    if (!dev) return false;

    if (numLEDs < 1) numLEDs = 1;

    int offset = 0;
    bool allAcked = true;

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

        if (hid_write(dev, buf, 65) < 0) allAcked = false;
        Sleep(2);
        offset += count;
    }

    return allAcked;
}

// The single funnel for every Aura colour write: startup apply, slider preview,
// presets, profile load, resume after standby, and the test dialog all end up
// here. Because the override/global decision lives in ResolveChannelColor, no
// caller can accidentally bypass it.
//
// setCount counts *acknowledged sends*, not verified colours - see
// SetAsusChannel. attemptCount, if given, counts the channels that were
// actually tried, so a status line can say "3/5" instead of implying that the
// missing two were fine.
static void ApplyAsusChannelColor(hid_device* dev, int auraIndex, int directChannel, int ledCount,
                                  uint8_t baseR, uint8_t baseG, uint8_t baseB, bool applyCorrection,
                                  bool respectGlobalEnable,
                                  int& setCount, int* attemptCount = nullptr,
                                  bool ignoreOverride = false) {
    if (!dev) return;
    if (auraIndex < 0 || auraIndex >= AURA_CONFIG_CHANNELS) return;
    if (respectGlobalEnable && !g_config.aura[auraIndex].enabled) return;

    if (attemptCount) (*attemptCount)++;

    uint8_t cr = baseR, cg = baseG, cb = baseB;
    if (applyCorrection) {
        ResolveChannelColor(g_config.aura[auraIndex], baseR, baseG, baseB, cr, cg, cb,
                            ignoreOverride);
    } else if (g_config.aura[auraIndex].override_active && !ignoreOverride) {
        // No correction requested, but the override still decides the source
        // colour - otherwise an overridden channel would jump to the global
        // colour on this path.
        cr = g_config.aura[auraIndex].override_r;
        cg = g_config.aura[auraIndex].override_g;
        cb = g_config.aura[auraIndex].override_b;
    }

    if (SetAsusChannel(dev, directChannel, ledCount, cr, cg, cb)) setCount++;
}

// ignoreOverride ist der Ausschaltweg (siehe ResolveChannelColor). Vorgabe
// false, damit jede bestehende Aufrufstelle sich exakt wie bisher verhaelt.
bool SetAsusAura(uint8_t r, uint8_t g, uint8_t b, bool ignoreOverride = false) {
    if (DryRunSkip(L"ASUS Aura")) return false;

    hid_device* dev = OpenAsusAura();
    if (!dev) {
        AppendStatus(L"[ASUS Aura] Not found");
        return false;
    }

    int setCount = 0;
    int attempted = 0;

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
                setCount, &attempted, ignoreOverride);
        }
    } else {
        for (int i = 0; i < AURA_FALLBACK_COUNT; i++) {
            ApplyAsusChannelColor(dev, i, AURA_FALLBACK_CHANNELS[i].channel,
                                  AURA_FALLBACK_CHANNELS[i].leds,
                                  r, g, b, true, true, setCount, &attempted, ignoreOverride);
        }
    }

    hid_close(dev);

    // Aura has no GET command (see the note at AURA_FALLBACK_CHANNELS), so the
    // strongest true statement is "the driver took the packets". CLAUDE.md
    // rule 1 forbids calling that "set".
    wchar_t buf[128];
    swprintf(buf, 128, L"[ASUS Aura] %d/%d Kan\u00E4le geschrieben (ohne Read-back - nicht verifiziert)",
             setCount, attempted);
    AppendStatus(buf);
    // "Every channel I tried was acked." All channels switched off means
    // attempted == 0 -> nothing was tried, so there is nothing to report as
    // failed; the 0/0 in the status line says so plainly.
    return (setCount == attempted);
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

    // Clamp to the ranges the protocol documents (brightness 0..4, speed 0..5).
    // SetEVisionEdge has always done this; the keyboard path wrote both bytes
    // through unchecked, so a hand-edited .rgb profile or a probe argument could
    // put an out-of-range value into the flash and the read-back would then
    // "verify" that out-of-range value as faithfully stored.
    const uint8_t br = ClampBrightness(brightness);
    const uint8_t sp = ClampSpeed(speed);

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
    config[1] = br;             // +0x02 Brightness (0-4, clamped)
    // +0x03 Speed (0-5). Polarity is NOT established: the old comment here
    // claimed "inverted" while the --kbmode probe next door assumed the
    // opposite, and neither had been checked against a moving light. Until
    // --edgespeed-sweep has settled it on the edge strip (which shares this
    // payload layout), the value is passed through as the slider gives it.
    config[2] = sp;
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
                    got[0] == mode && got[1] == br && got[2] == sp &&
                    got[5] == cr && got[6] == cg && got[7] == cb;

    {
        char dbg[224];
        snprintf(dbg, sizeof(dbg),
                 "[EVision] KB profile=%d off=0x%02X writeRes=%d readRes=%d "
                 "want[mode=%02X br=%02X sp=%02X rgb=%02X%02X%02X] "
                 "got[mode=%02X br=%02X sp=%02X rgb=%02X%02X%02X] verified=%d",
                 (int)profile, (unsigned)profile_offset, writeRes,
                 g_lastKbVerify.readRes,
                 mode, br, sp, cr, cg, cb,
                 got[0], got[1], got[2], got[5], got[6], got[7], (int)verified);
        LogDebug(dbg);
    }

    // No "Win-key unlock" write here anymore.
    //
    // What used to stand at this spot was an unconditional, unverified
    // EVisionQuery(0x06, profile*0x40 + 0x14, {0x00,0x00}) on *every* apply.
    // Three things were wrong with it at once:
    //
    //  * It writes into +0x14..0x1D, whose field semantics are unknown
    //    (docs/Keyboard_Protocol.md section 3 marks the region [?]) - CLAUDE.md
    //    rule 2 forbids exactly that, and rule 3 forbids the blind write.
    //  * Its result was never read back, so it reported nothing either way -
    //    rule 1.
    //  * Section 4.1 of the same document already disproves its purpose: with
    //    the Win key locked, a live dump shows the remap table fully intact and
    //    clearing the per-profile flag at +0x2E did not unlock it either. The
    //    lock is keyboard-side Fn-layer state that this config memory does not
    //    expose. The comment that used to sit here argued from the *correlation*
    //    that the old brute-force stamped "04 02" into 0x14/0x15 - but that is
    //    evidence about the brute-force, not about a lock field.
    //
    // And if +0x14/+0x15 do hold the edge zone's brightness/speed - the open
    // suspicion this whole investigation started from - then zeroing them here,
    // on the keyboard path that runs *before* the edge write in ApplyColors, is
    // what freezes every edge animation at speed 0.
    //
    // The write survives as an explicit, read-back-verified one-shot action:
    // OneClickRGB.exe --unlock-winkey. It is not part of an apply.

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);  // End configure
    hid_close(dev);

    wchar_t buf[192];
    if (!g_lastKbVerify.valid) {
        swprintf(buf, 192, L"[EVision] Keyboard mode 0x%02X written, read-back failed (%d)",
                 mode, g_lastKbVerify.readRes);
    } else if (verified) {
        swprintf(buf, 192, L"[EVision] Keyboard verified: mode 0x%02X, brightness %d, speed %d",
                 mode, br, sp);
    } else {
        // Report the mismatch instead of a blanket success - this is how a mode
        // the firmware refuses becomes visible instead of silently doing nothing.
        swprintf(buf, 192,
                 L"[EVision] Keyboard MISMATCH - mode 0x%02X->0x%02X, brightness %d->%d, speed %d->%d",
                 mode, got[0], br, got[1], sp, got[2]);
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

// Result of the last SetEVisionEdge read-back, mirroring KbVerifyResult above.
// The edge path used to keep its read-back in locals, so the headless probes had
// no way to report what the firmware stored other than re-implementing the write
// - and a probe that does not drive the production path proves nothing about it
// (CLAUDE.md rule 1). All ten payload bytes are kept, including the commit flag
// at +0x27, because "mode stored but effect frozen" is exactly the class of bug
// where the bytes nobody prints are the interesting ones.
struct EdgeVerifyResult {
    bool     valid    = false;   // read-back succeeded
    int      writeRes = 0;
    int      readRes  = 0;
    uint8_t  profile  = 0;
    uint16_t offset   = 0;
    uint8_t  want[10] = {0};     // mode,bright,speed,dir,rand,R,G,B,coloff,save
    uint8_t  got[10]  = {0};     // what the edge slot actually holds now
};
static EdgeVerifyResult g_lastEdgeVerify;

// Opens the RGB collection, reads the 10-byte edge payload of the ACTIVE
// profile, closes again. Read-only (command 0x05), therefore usable under
// --dry-run: it is how a probe records the state it is about to disturb.
// Returns false if the device could not be opened or did not answer.
static bool ReadEVisionEdgePayload(uint8_t out[10], uint8_t* profileOut, uint16_t* offsetOut) {
    if (out) memset(out, 0, 10);
    if (profileOut) *profileOut = 0;
    if (offsetOut)  *offsetOut  = 0;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t off = (uint16_t)(profile * 0x40 + 0x1E);

    // Read twice and require both to agree. This value gets written back into
    // flash at the end of a probe, so a single bad read does not just spoil a
    // report - it corrupts the state the probe promised to preserve. That is
    // not theory: a snapshot taken while a second process used the same
    // collection came back as all zeros, "successfully", and was then restored.
    // Two identical reads are cheap; an unnoticed garbage restore is not.
    uint8_t buf[10] = {0}, buf2[10] = {0};
    int rr  = EVisionQuery(dev, 0x05, off, nullptr, 10, buf);
    Sleep(5);
    int rr2 = EVisionQuery(dev, 0x05, off, nullptr, 10, buf2);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    if (profileOut) *profileOut = profile;
    if (offsetOut)  *offsetOut  = off;
    if (rr < 0 || rr2 < 0) return false;
    if (memcmp(buf, buf2, 10) != 0) {
        LogDebug("[EVision] Edge snapshot unstable - two reads differ, refusing to "
                 "treat it as a restore point");
        return false;
    }
    if (out) memcpy(out, buf, 10);
    return true;
}

// Writes a 10-byte payload back to the edge slot of the ACTIVE profile and
// verifies it by read-back. Only ever called with a snapshot that
// ReadEVisionEdgePayload took earlier, so a probe can put the strip back the way
// it found it instead of leaving it parked on the last swept byte. Same offset,
// same 10 bytes, same production semantics - no new byte range (CLAUDE.md rule 2).
static bool RestoreEVisionEdgePayload(const uint8_t in[10]) {
    if (!in) return false;
    if (DryRunSkip(L"EVision Edge Restore")) return false;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t off = (uint16_t)(profile * 0x40 + 0x1E);

    EVisionQuery(dev, 0x06, off, in, 10, nullptr);
    Sleep(10);
    uint8_t back[10] = {0};
    int rr = EVisionQuery(dev, 0x05, off, nullptr, 10, back);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return rr >= 0 && memcmp(back, in, 10) == 0;
}

// ---------------------------------------------------------------------------
// The keyboard-block counterparts of the two edge helpers above: read the
// 18-byte block at profile_base+0x01, and put it back afterwards.
//
// The keyboard sweep needs exactly what the edge sweep needed. --kbmode-sweep
// walks 21 mode bytes and used to have no rollback at all, so it left the
// keyboard parked on whatever came last - 0x14, a value that is not even in
// KB_MODE_TABLE. The edge path has restored since it was written; this closes
// the same hole on the keyboard side before the sweep is run for the first time.
//
// Range: the same 18 bytes SetEVisionKeyboard itself reads and writes, so a
// restore touches nothing the production path does not already touch
// (CLAUDE.md rule 2). OpenEVisionEdgeDev despite the name - this device exposes
// only one vendor collection (docs/Keyboard_Protocol.md section 1), and the
// keyboard block and the edge payload live in the same config memory behind it.
// ---------------------------------------------------------------------------
static bool ReadEVisionKeyboardPayload(uint8_t out[18], uint8_t* profileOut, uint16_t* offsetOut) {
    if (out) memset(out, 0, 18);
    if (profileOut) *profileOut = 0;
    if (offsetOut)  *offsetOut  = 0;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t off = (uint16_t)(profile * 0x40 + 0x01);

    // Two agreeing reads before this counts as a restore point - see the same
    // guard in ReadEVisionEdgePayload. This is the exact block that was read as
    // 18 zero bytes under process contention and then written back, which put
    // mode 0, brightness 0 and commit 0 into the active profile and turned the
    // keyboard off with a report full of "verified".
    uint8_t buf[18] = {0}, buf2[18] = {0};
    int rr  = EVisionQuery(dev, 0x05, off, nullptr, 18, buf);
    Sleep(5);
    int rr2 = EVisionQuery(dev, 0x05, off, nullptr, 18, buf2);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    if (profileOut) *profileOut = profile;
    if (offsetOut)  *offsetOut  = off;
    if (rr < 0 || rr2 < 0) return false;
    if (memcmp(buf, buf2, 18) != 0) {
        LogDebug("[EVision] Keyboard snapshot unstable - two reads differ, refusing to "
                 "treat it as a restore point");
        return false;
    }
    if (out) memcpy(out, buf, 18);
    return true;
}

static bool RestoreEVisionKeyboardPayload(const uint8_t in[18]) {
    if (!in) return false;
    if (DryRunSkip(L"EVision Keyboard Restore")) return false;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return false;

    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    uint8_t profile = 0;
    EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
    if (profile > 2) profile = 0;
    uint16_t off = (uint16_t)(profile * 0x40 + 0x01);

    EVisionQuery(dev, 0x06, off, in, 18, nullptr);
    Sleep(10);
    uint8_t back[18] = {0};
    int rr = EVisionQuery(dev, 0x05, off, nullptr, 18, back);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    return rr >= 0 && memcmp(back, in, 18) == 0;
}

// ---------------------------------------------------------------------------
// Generic config-memory access: read a byte range, write a byte range.
//
// Pulled out of the --kbdump branch, where the read loop used to sit inline
// with a hardcoded 0x000..0x3FF window. That window is the reason the per-key
// colour table has never been seen whole: going by the matrix size it ends at
// 0x43A, i.e. 58 bytes past where the dump stopped looking. A dump that cannot
// be pointed at an address cannot answer a question about that address.
//
// Both work in 16-byte chunks, which is what EVisionQuery's size byte carries,
// and both take an already-open device inside an already-open configure session
// (0x01 ... 0x02) so a caller can bracket several operations in one session
// instead of opening one per chunk.
//
// ReadEVisionConfig is pure reads (command 0x05) and therefore safe under
// --dry-run: recording the state a probe is about to disturb is not a write.
// Returns the number of bytes actually read; a chunk the device refuses stops
// the read there, so the caller learns the real extent of the memory instead of
// getting a zero-filled tail that looks like data.
// ---------------------------------------------------------------------------
static int ReadEVisionConfig(hid_device* dev, uint16_t lo, uint16_t hi, uint8_t* out,
                             int* lastResOut = nullptr) {
    if (lastResOut) *lastResOut = 0;
    if (!dev || !out || hi < lo) return 0;

    const int total = (int)hi - (int)lo + 1;
    int done = 0;
    while (done < total) {
        const int want = (total - done) > 16 ? 16 : (total - done);
        uint8_t buf[16] = {0};
        const int rr = EVisionQuery(dev, 0x05, (uint16_t)(lo + done), nullptr,
                                    (uint8_t)want, buf);
        if (lastResOut) *lastResOut = rr;
        if (rr < 0) break;
        memcpy(out + done, buf, want);
        done += want;
    }
    return done;
}

// Writes a byte range and verifies it by reading the same range back. Returns
// true only when every byte matches - CLAUDE.md rule 1: the caller may report
// success for exactly what came back, never for what it sent.
//
// Callers are responsible for staying inside a range docs/Keyboard_Protocol.md
// documents (rule 2); this helper deliberately does no range policy of its own,
// because the one place that would be enforced is the one place a future caller
// would forget to update. The per-key paths above it each state their bounds.
static bool WriteEVisionConfigVerified(hid_device* dev, uint16_t lo, const uint8_t* data,
                                       int len, int* firstBadOffsetOut = nullptr) {
    if (firstBadOffsetOut) *firstBadOffsetOut = -1;
    if (!dev || !data || len <= 0) return false;

    int done = 0;
    while (done < len) {
        const int want = (len - done) > 16 ? 16 : (len - done);
        const int wr = EVisionQuery(dev, 0x06, (uint16_t)(lo + done), data + done,
                                    (uint8_t)want, nullptr);
        if (wr < 0) {
            if (firstBadOffsetOut) *firstBadOffsetOut = (int)lo + done;
            return false;
        }
        done += want;
        Sleep(5);
    }

    std::vector<uint8_t> back((size_t)len, 0);
    const int got = ReadEVisionConfig(dev, lo, (uint16_t)(lo + len - 1), back.data());
    if (got != len) {
        if (firstBadOffsetOut) *firstBadOffsetOut = (int)lo + got;
        return false;
    }
    for (int i = 0; i < len; i++) {
        if (back[(size_t)i] != data[i]) {
            if (firstBadOffsetOut) *firstBadOffsetOut = (int)lo + i;
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The key layout, read from the device.
//
// The remap table at 0xC0 says which key sits at which matrix position, so the
// layout is a measurement rather than a picture of a keyboard someone assumed
// this is. A device that does not answer produces an empty layout, and every
// caller has to handle that - showing a plausible default grid instead would
// mean the UI offers keys nobody has confirmed exist.
//
// `dev` must be open and inside a configure session (0x01 ... 0x02). Read-only.
// `colorsCompleteOut` reports whether the colour table could be read over its
// full assumed extent; it is false whenever the device stops answering early,
// which is itself the open question about where that table ends.
// ---------------------------------------------------------------------------
// `probeColorTable` decides whether the 0x2C0 region is read at all. The probes
// want the byte count (how far the device answers is part of what they report);
// the editor does not, because it does not paint those bytes - see
// kblayout::SeedKeyColors for why.
static std::vector<kblayout::Key> ReadKeyLayout(hid_device* dev,
                                                bool* colorsCompleteOut = nullptr,
                                                int* colorBytesReadOut  = nullptr,
                                                bool probeColorTable    = true) {
    if (colorsCompleteOut) *colorsCompleteOut = false;
    if (colorBytesReadOut) *colorBytesReadOut = 0;

    std::vector<kblayout::Key> keys;
    if (!dev) return keys;

    const int remapLen = (int)kblayout::REMAP_END - (int)kblayout::REMAP_BASE;
    std::vector<uint8_t> remap((size_t)remapLen, 0);
    const int gotRemap = ReadEVisionConfig(dev, kblayout::REMAP_BASE,
                                           (uint16_t)(kblayout::REMAP_END - 1), remap.data());
    if (gotRemap <= 0) return keys;
    keys = kblayout::BuildLayout(remap.data(), (size_t)gotRemap);

    if (probeColorTable) {
        const int colLen = (int)kblayout::KEYCOLOR_REGION_END - (int)kblayout::KEYCOLOR_BASE;
        std::vector<uint8_t> cols((size_t)colLen, 0);
        const int gotCols = ReadEVisionConfig(dev, kblayout::KEYCOLOR_BASE,
                                              (uint16_t)(kblayout::KEYCOLOR_REGION_END - 1),
                                              cols.data());
        if (colorsCompleteOut) *colorsCompleteOut = (gotCols == colLen);
        if (colorBytesReadOut) *colorBytesReadOut = gotCols;
    }
    return keys;
}

// Finds the key a colour-table offset is predicted to belong to. "Predicted",
// not "belongs to": the mapping slot -> key is the [MED] inference the
// --keyidentify probe exists to confirm or break.
static const kblayout::Key* PredictKeyForColorOffset(const std::vector<kblayout::Key>& keys,
                                                     uint16_t off) {
    for (const kblayout::Key& k : keys)
        if (k.colorOffset == off) return &k;
    return nullptr;
}

// Self-contained wrappers: open, one configure session, close. Used by the
// per-key probes and by the layout dialog, none of which want to hold a session
// open across a modal dialog or a user's thinking time.

// Reads a range twice and hands back only the prefix both reads agreed on. Two
// separate guards in one function, and both earn their place:
//
//  * Two agreeing reads - the same discipline ReadEVisionEdgePayload and
//    ReadEVisionKeyboardPayload carry. This value gets written back into flash
//    at the end of a probe, so a single bad read does not merely spoil a report,
//    it destroys the state the probe promised to preserve. That is not theory:
//    a snapshot taken while a second process used the collection came back as
//    all zeros, "successfully", and was restored.
//  * A short answer is a result, not a failure. Where the config memory ends is
//    one of the open questions here, so the caller is told how far the device
//    answered instead of getting a zero-padded buffer that looks complete.
//
// Returns the number of stable bytes; `out` is sized to exactly that.
static int SnapshotEVisionRange(uint16_t lo, int len, std::vector<uint8_t>& out) {
    out.clear();
    if (len <= 0) return 0;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return 0;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);

    std::vector<uint8_t> a((size_t)len, 0), b((size_t)len, 0);
    const int ga = ReadEVisionConfig(dev, lo, (uint16_t)(lo + len - 1), a.data());
    Sleep(5);
    const int gb = ReadEVisionConfig(dev, lo, (uint16_t)(lo + len - 1), b.data());

    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);

    const int common = ga < gb ? ga : gb;
    if (common <= 0) return 0;
    if (memcmp(a.data(), b.data(), (size_t)common) != 0) {
        LogDebug("[EVision] config snapshot unstable - two reads differ, refusing to "
                 "treat it as a restore point");
        return 0;
    }
    a.resize((size_t)common);
    out.swap(a);
    return common;
}

static bool WriteEVisionRangeVerified(uint16_t lo, const uint8_t* data, int len,
                                      int* firstBadOffsetOut = nullptr) {
    if (firstBadOffsetOut) *firstBadOffsetOut = -1;
    if (DryRunSkip(L"EVision Tastenfarben")) return false;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return false;
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    const bool ok = WriteEVisionConfigVerified(dev, lo, data, len, firstBadOffsetOut);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);
    return ok;
}

static std::vector<kblayout::Key> ReadKeyLayoutStandalone(bool* colorsCompleteOut = nullptr,
                                                          int* colorBytesReadOut = nullptr,
                                                          bool probeColorTable   = true) {
    if (colorsCompleteOut) *colorsCompleteOut = false;
    if (colorBytesReadOut) *colorBytesReadOut = 0;

    hid_device* dev = OpenEVisionEdgeDev(nullptr, 0);
    if (!dev) return std::vector<kblayout::Key>();
    EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
    Sleep(20);
    std::vector<kblayout::Key> keys = ReadKeyLayout(dev, colorsCompleteOut, colorBytesReadOut,
                                                    probeColorTable);
    EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
    hid_close(dev);
    return keys;
}

// ---------------------------------------------------------------------------
// Bridge between the decoded layout and RGBConfig::keyboardZones.
//
// keyboardZones has existed as an empty vector with a "filled from KB segment
// map (later)" note since the generic zone model went in; this is that later.
// No new persistence format - LightZone already saves everything a key needs,
// and it is already written and read by RGBConfig::Save/Load.
//
// The zone id is the matrix position (kblayout::KeyId), not the key's label.
// Colour belongs to a place on the board, not to a letter: remapping Y to Z must
// not swap two keys' colours behind the user's back.
// ---------------------------------------------------------------------------
static void KeyLayoutToZones(const std::vector<kblayout::Key>& keys,
                             std::vector<LightZone>& out) {
    out.clear();
    out.reserve(keys.size());
    for (const kblayout::Key& k : keys) {
        LightZone z;
        z.id      = kblayout::KeyId(k.col, k.row);
        z.name    = k.label;
        z.x       = k.col;
        z.y       = k.row;
        z.hwIndex = (int)k.colorOffset;
        z.enabled = true;
        // "verified" stays false: it means Identify confirmed the physical
        // mapping, and for these keys that is exactly what --keyidentify has to
        // establish first. Setting it here would be a claim, not a fact.
        z.color   = { k.r, k.g, k.b };
        out.push_back(z);
    }
}

// Copies saved colours onto a freshly read layout, matched by matrix position.
// A key the saved config does not know keeps whatever the device reported, so an
// added or newly assigned key shows its real colour rather than a default.
static void ZonesToKeyLayout(const std::vector<LightZone>& zones,
                             std::vector<kblayout::Key>& keys) {
    for (kblayout::Key& k : keys) {
        const std::string id = kblayout::KeyId(k.col, k.row);
        for (const LightZone& z : zones) {
            if (z.id != id) continue;
            k.r = z.color.r; k.g = z.color.g; k.b = z.color.b;
            k.colorKnown = true;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// The per-key write path.
//
// Read-modify-write over the documented colour-table extent, then read back and
// compare (rules 1 and 3). Only the triples of the keys handed in are changed;
// every other byte in the range keeps what the snapshot found, including the
// slots of unassigned matrix positions whose meaning nobody has established.
//
// Order matters and is deliberate. If a custom mode byte is configured it is
// written FIRST, through SetEVisionKeyboard: that write also carries the global
// colour into the profile block, and if the firmware fills the per-key table
// from that colour - the leading explanation for why 0x2C0.. was full of the
// profile's own colour - then doing it afterwards would erase the table we just
// wrote. With kbCustomMode still at the 0xFF sentinel nothing is written to the
// profile block at all, and the caller must not claim the keyboard shows
// anything.
// ---------------------------------------------------------------------------
struct KeyApplyResult {
    bool    deviceFound    = false;
    int     stableBytes    = 0;    // readable+stable extent of the colour table
    int     keysPatched    = 0;
    bool    tableVerified  = false;
    int     firstBadOffset = -1;
    bool    modeAttempted  = false;
    bool    modeVerified   = false;
    uint8_t modeWanted     = 0;
    uint8_t modeGot        = 0;
};

static KeyApplyResult ApplyKeyColorsToDevice(const std::vector<kblayout::Key>& keys) {
    KeyApplyResult res;
    if (keys.empty()) return res;
    if (g_state.dryRun) { DryRunSkip(L"EVision Tastenfarben"); return res; }

    // A configured custom mode goes down the production path, commit flag and
    // all (docs section 3.2), so this cannot become a second, divergent way of
    // writing the profile block.
    if (g_config.kbCustomMode != 0xFF) {
        res.modeAttempted = true;
        res.modeWanted    = g_config.kbCustomMode;
        SetEVisionKeyboard(g_state.red, g_state.green, g_state.blue,
                           g_config.kbCustomMode, g_state.brightness, g_state.speed);
        res.modeVerified = g_lastKbVerify.valid && g_lastKbVerify.got[0] == g_config.kbCustomMode;
        res.modeGot      = g_lastKbVerify.valid ? g_lastKbVerify.got[0] : (uint8_t)0;
    }

    const int tableLen = (int)kblayout::KEYCOLOR_REGION_END - (int)kblayout::KEYCOLOR_BASE;
    std::vector<uint8_t> table;
    res.stableBytes = SnapshotEVisionRange(kblayout::KEYCOLOR_BASE, tableLen, table);
    if (res.stableBytes <= 0) return res;   // no restore point -> no write
    res.deviceFound = true;

    for (const kblayout::Key& k : keys) {
        const int idx = (int)k.colorOffset - (int)kblayout::KEYCOLOR_BASE;
        if (idx < 0 || idx + 2 >= (int)table.size()) continue;   // past the readable end
        table[(size_t)idx + 0] = k.r;
        table[(size_t)idx + 1] = k.g;
        table[(size_t)idx + 2] = k.b;
        res.keysPatched++;
    }

    res.tableVerified = WriteEVisionRangeVerified(kblayout::KEYCOLOR_BASE, table.data(),
                                                  (int)table.size(), &res.firstBadOffset);
    return res;
}

// brightness 0-4, speed 0-5 - both come from the Effects group sliders. They
// used to be hardcoded here (brightness 4, speed 2) and were not even function
// parameters, which is why the Tempo slider had no effect on the edge strips
// and every effect always animated at the same rate.
bool SetEVisionEdge(uint8_t r, uint8_t g, uint8_t b, uint8_t mode,
                    uint8_t brightness, uint8_t speed) {
    // Invalidate the read-back record FIRST, before any early return can leave
    // the previous call's values standing - the same trap the keyboard path fell
    // into, where a device that dropped off the bus mid-sweep made every later
    // report row repeat the last successful read-back as if it were fresh.
    g_lastEdgeVerify = EdgeVerifyResult{};

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
    uint8_t bright = (mode == EDGE_MODE_OFF) ? 0 : ClampBrightness(brightness);
    uint8_t spd    = ClampSpeed(speed);
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
    g_lastEdgeVerify.profile  = profile;
    g_lastEdgeVerify.offset   = edgeOff;
    g_lastEdgeVerify.writeRes = res;
    memcpy(g_lastEdgeVerify.want, edgeData, 10);

    uint8_t* edgeBack = g_lastEdgeVerify.got;
    int edgeRead = EVisionQuery(dev, 0x05, edgeOff, nullptr, 10, edgeBack);
    g_lastEdgeVerify.readRes = edgeRead;
    g_lastEdgeVerify.valid   = (edgeRead >= 0);

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
                 L"[EVision] Edge: P%d+0x1E mode=0x%02X geschrieben, R\u00FCcklesen fehlgeschlagen (%d)",
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

// --- Resume gating -----------------------------------------------------------
// A post-resume reset is expensive: FullHIDReset() tears down and re-enumerates
// the whole HID stack and sleeps ~800ms, then every device is written again.
//
// Frueher konnten FUENF Quellen ihn anfordern: der Zeitsprung-Wachhund, die
// beiden APM-Resume-Nachrichten, Display-an und Entsperren. Drei davon feuern
// auch auf einer wachen Maschine. Jetzt ist es genau EINE - APM-Resume - und
// die Torsteuerung darunter ist die zweite Sicherung, nicht die erste.
//
// Warum der Wachhund weg ist: er verglich, ob ein Sleep(1000) laenger als 5 s
// gedauert hat, und setzte daraus g_suspendSeen - das eine Flag, dem die ganze
// Torsteuerung vertraut. Unter Last, beim Auslagern oder an einem Haltepunkt
// dauert ein Sleep(1000) laenger als 5 s, ohne dass irgendetwas geschlafen
// haette. Auf diesem Rechner ist automatischer Standby ausgeschaltet
// (powercfg: STANDBYIDLE = 0) und der Ruhezustand deaktiviert, PBT_APMSUSPEND
// kann also praktisch nie feuern - und trotzdem kamen Ausloeser. Per Ausschluss
// blieb nur der Wachhund.
//
// Ersetzt durch RegisterSuspendResumeNotification: ereignisgesteuert, im
// Leerlauf null CPU. Eine Pausenlogik "Wachhund schlaeft bei Benutzereingabe"
// haette den Sekundentakt nur ausgeduennt, nicht abgeschafft.
//
// Rules: only reset when a real suspend was observed, never more than one reset
// at a time, and at most one per cooldown window.
#define RESUME_COOLDOWN_MS 15000
std::atomic<bool> g_suspendSeen{false};        // genuine standby observed
std::atomic<bool> g_resetInFlight{false};      // reset currently running
std::atomic<bool> g_resetArmed{false};         // timer already scheduled
std::atomic<ULONGLONG> g_lastResetTick{0};     // completion time of last reset

// Ein Suspend kann uns jetzt auf zwei Wegen erreichen: als Standardnachricht an
// ein Top-Level-Fenster und ueber RegisterSuspendResumeNotification. Beide sind
// erwuenscht - der zweite traegt auch dort, wo der erste ausbleibt -, aber der
// Blackout soll deswegen nicht zweimal laufen.
#define SUSPEND_DEBOUNCE_MS 5000
std::atomic<ULONGLONG> g_lastSuspendTick{0};

// Rueckgabewerte der Benachrichtigungs-Registrierungen. Sie wurden bisher
// weggeworfen; damit war weder eine fehlgeschlagene Registrierung erkennbar
// noch das Abmelden moeglich.
HPOWERNOTIFY g_hDisplayNotify = NULL;
HPOWERNOTIFY g_hSuspendNotify = NULL;

// Die beiden Energie-GUIDs, ausgeschrieben mit ACHT gezaehlten Data4-Bytes.
//
// winnt.h deklariert sie ueber DEFINE_GUID nur als extern; die Definition kaeme
// aus einer zusaetzlichen Bibliothek. Hier stehen sie deshalb als eigene
// Konstanten, gegengelesen an Windows Kits/10/Include/10.0.26100.0/um/winnt.h
// Zeile 16306 und 16458:
//
//   GUID_CONSOLE_DISPLAY_STATE  6fe69556-704a-47a0-8f24-c28d936fda47
//   GUID_SYSTEM_AWAYMODE        98a7f580-01f7-48aa-9c0f-44352c29e5c0
//
// Genau hier stand der Fehler, der den ganzen Display-Zweig zu totem Code
// gemacht hat: Data4 mit sieben Eintraegen, das 0xc2 fehlte. Wer diese Zeilen
// aendert, zaehlt die geschweiften Klammern nach.
static const GUID kGuidConsoleDisplayState =
    { 0x6fe69556, 0x704a, 0x47a0, { 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47 } };
static const GUID kGuidSystemAwayMode =
    { 0x98a7f580, 0x01f7, 0x48aa, { 0x9c, 0x0f, 0x44, 0x35, 0x2c, 0x29, 0xe5, 0xc0 } };

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

        // Deterministic adapter order.
        //
        // Every setter's bool is collected instead of dropped. It used to be
        // discarded for all five devices, so "=== Done! ===" printed even when
        // the line right above it said "Edge ABWEICHUNG" or "Keyboard MISMATCH"
        // - the final line of the log contradicted the finding two lines up, and
        // the final line is the one people read. Same class of bug as reporting
        // an unverified write as success (CLAUDE.md rule 1), one level up.
        //
        // What the bool means differs per device and the wording below keeps
        // that apart: keyboard and edge read the value back and compare, so
        // theirs is "verified". Aura, mouse and RAM only report that the write
        // was handed to the device - their read-back verification is Phase 1 of
        // the remediation plan and is not done yet.
        const wchar_t* failedUnverified[3] = {nullptr, nullptr, nullptr};
        int nFailedUnverified = 0;
        const wchar_t* failedVerified[2] = {nullptr, nullptr};
        int nFailedVerified = 0;

        if (doAura) {
            if (!SetAsusAura(r, g, b)) failedUnverified[nFailedUnverified++] = L"ASUS Aura";
        }
        if (doMouse) {
            if (!SetSteelSeries(r, g, b)) failedUnverified[nFailedUnverified++] = L"SteelSeries";
        }
        if (doKeyboard) {
            if (!SetEVisionKeyboard(r, g, b, kbMode, brightness, speed))
                failedVerified[nFailedVerified++] = L"Tastatur";
        }
        if (doEdge) {
            if (!SetEVisionEdge(r, g, b, edgeMode, (uint8_t)brightness, (uint8_t)speed))
                failedVerified[nFailedVerified++] = L"Edge";
        }
        if (doRAM) {
            if (!SetGSkillRAM(r, g, b)) failedUnverified[nFailedUnverified++] = L"G.Skill RAM";
        }

        hid_exit();

        if (nFailedVerified == 0 && nFailedUnverified == 0) {
            AppendStatus(L"=== Done! ===");
        } else {
            wchar_t line[256];
            if (nFailedVerified > 0) {
                std::wstring names = failedVerified[0];
                for (int i = 1; i < nFailedVerified; i++) names += L", " + std::wstring(failedVerified[i]);
                swprintf(line, 256, L"=== NICHT VERIFIZIERT: %s ===", names.c_str());
                AppendStatus(line);
            }
            if (nFailedUnverified > 0) {
                std::wstring names = failedUnverified[0];
                for (int i = 1; i < nFailedUnverified; i++) names += L", " + std::wstring(failedUnverified[i]);
                swprintf(line, 256, L"=== FEHLGESCHLAGEN: %s ===", names.c_str());
                AppendStatus(line);
            }
            AppendStatus(L"=== Mit Abweichungen beendet - Details oben ===");
        }
    }

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
// Gibt zurueck, ob der Benutzer wirklich eine Farbe gewaehlt hat.
//
// Frueher void: der Aufrufer konnte "abgebrochen" nicht von "uebernommen"
// unterscheiden und musste daher entweder immer oder nie speichern. Der einzige
// verbliebene Aufrufer (der Vorschau-Swatch) hat sich fuer "nie" entschieden -
// siehe ColorPreviewSubclassProc.
bool PickColor() {
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
        return true;
    }
    return false;
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
    // Wer eine Farbe waehlt, will Licht. Bliebe lightsOff stehen, wuerde die
    // Konfiguration "aus" behaupten, waehrend die Geraete leuchten - und der
    // naechste Start haette sie wieder ausgeschaltet.
    g_config.lightsOff = false;
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
// ECHTES AUSSCHALTEN
//
// Es gab zwei verschiedene Vorstellungen von "aus", und die schwaechere sass am
// Knopf:
//
//   * Der Aus-Knopf setzte die Farbe auf 0,0,0 und liess Modus und Helligkeit
//     stehen. Bei jedem Nicht-Statik-Effekt (Spektrum, Regenbogen, Welle) ist
//     die Farbe aber gar nicht das, was die Firmware zeichnet - die Animation
//     lief weiter, und der Knopf sah kaputt aus.
//   * Der Suspend-Zweig kannte das echte Aus: EDGE_MODE_OFF und Helligkeit 0.
//
// ApplyLightsOff ist jetzt der eine Weg, den beide gehen. Zwei Fassungen
// derselben Sache laufen auseinander, sobald jemand nur eine pflegt.
//
// ignoreOverride=true bei Aura: "aus" ist kein Farbwunsch, sondern ein Zustand
// (siehe ResolveChannelColor in channel_config.h).
//
// Gemeldet wird nur, was zurueckgelesen wurde. Tastatur und Edge koennen das,
// Aura, Maus und RAM nicht - deren Zeilen sagen "geschrieben", nie "verifiziert"
// (Projektregel 1).
//=============================================================================

// lockTimeoutMs < 0 wartet auf den Geraetemutex, >= 0 gibt nach dieser Frist auf.
//
// Die Frist ist fuer die Nachrichtenzweige da (Standby, Herunterfahren).
// Windows gibt einem PBT_APMSUSPEND rund zwei Sekunden; haelt gerade ein Apply
// den Mutex, wuerde blockierendes Warten die Nachrichtenschleife anhalten und
// der Rechner schliefe mitten in unserem Write ein. Lieber melden, dass es
// nicht ging, als es zu erzwingen.
bool ApplyLightsOff(int lockTimeoutMs = -1) {
    if (DryRunSkip(L"Beleuchtung aus")) return false;

    bool kbOk = true, edgeOk = true;
    int  unverifiedTried = 0, unverifiedAcked = 0;

    std::unique_lock<std::mutex> ioLock(g_state.deviceIoMutex, std::defer_lock);
    if (lockTimeoutMs < 0) {
        ioLock.lock();
    } else {
        const ULONGLONG deadline = GetTickCount64() + (ULONGLONG)lockTimeoutMs;
        while (!ioLock.try_lock()) {
            if (GetTickCount64() >= deadline) {
                AppendStatus(L"=== Aus NICHT ausgeführt: Gerät belegt (Frist abgelaufen) ===");
                LogDebug("[lightsoff] device mutex busy - nothing was written");
                return false;
            }
            Sleep(20);
        }
    }

    {
        hid_init();

        if (g_state.enableAura) {
            unverifiedTried++;
            if (SetAsusAura(0, 0, 0, /*ignoreOverride*/ true)) unverifiedAcked++;
        }
        if (g_state.enableMouse) {
            unverifiedTried++;
            if (SetSteelSeries(0, 0, 0)) unverifiedAcked++;
        }
        if (g_state.enableKeyboard) {
            // KB_MODE_STATIC statt eines undokumentierten "Aus"-Modusbytes:
            // ausserhalb belegter Werte wird nicht geschrieben (Projektregel 2).
            // Ob Helligkeit 0 die Tastatur wirklich dunkel schaltet, sagt der
            // Read-back im Setter - nicht diese Zeile.
            kbOk = SetEVisionKeyboard(0, 0, 0, KB_MODE_STATIC, 0, g_state.speed);
        }
        if (g_state.enableEdge) {
            edgeOk = SetEVisionEdge(0, 0, 0, EDGE_MODE_OFF, 0, 0);
        }
        if (g_state.enableRAM) {
            unverifiedTried++;
            if (SetGSkillRAM(0, 0, 0)) unverifiedAcked++;
        }

        hid_exit();
    }

    const bool verified = kbOk && edgeOk;

    if (verified) {
        AppendStatus(L"=== Aus verifiziert (Tastatur und Edge zur\u00FCckgelesen) ===");
    } else {
        std::wstring names;
        if (!kbOk)   names = L"Tastatur";
        if (!edgeOk) names += (names.empty() ? L"Edge" : L", Edge");
        wchar_t line[192];
        swprintf(line, 192, L"=== Aus NICHT verifiziert: %s ===", names.c_str());
        AppendStatus(line);
    }

    if (unverifiedTried > 0) {
        wchar_t line[192];
        swprintf(line, 192,
                 L"Aura/Maus/RAM: %d/%d geschrieben (ohne Read-back - nicht verifiziert)",
                 unverifiedAcked, unverifiedTried);
        AppendStatus(line);
    }

    return verified;
}

// Der Aus-Knopf als Schalter. Erster Druck sichert den Stand und schaltet aus,
// zweiter stellt Farbe, Modi und Helligkeit exakt wieder her.
void ToggleLightsOff() {
    if (!g_config.lightsOff) {
        g_config.savedR          = g_state.red;
        g_config.savedG          = g_state.green;
        g_config.savedB          = g_state.blue;
        g_config.savedKbMode     = (uint8_t)g_state.kbMode;
        g_config.savedEdgeMode   = (uint8_t)g_state.edgeMode;
        g_config.savedBrightness = (uint8_t)g_state.brightness;
        g_config.lightsOff       = true;
        SaveSettings();

        ClearStatus();
        AppendStatus(L"=== Beleuchtung ausschalten ===");
        // Nicht im Nachrichtenzweig: die Setter schlafen zwischen den Writes,
        // und eine blockierte Pumpe laesst das Fenster einfrieren.
        std::thread([] { ApplyLightsOff(); }).detach();
    } else {
        g_state.red        = g_config.savedR;
        g_state.green      = g_config.savedG;
        g_state.blue       = g_config.savedB;
        g_state.kbMode     = g_config.savedKbMode;
        g_state.edgeMode   = NormalizeEdgeMode(g_config.savedEdgeMode);
        g_state.brightness = g_config.savedBrightness;
        g_config.lightsOff = false;

        UpdateAllControls();
        ClearStatus();
        AppendStatus(L"=== Beleuchtung wieder an ===");
        CommitStateAndApply(true);
    }
}

// Sagt einmal beim Start, welche Aura-Kanaele nicht der Globalfarbe folgen.
//
// Der Anlass ist gemessen: auf diesem Rechner standen aura[1] und aura[2] auf
// override_active, ohne dass es dafuer irgendwo einen Hinweis gab. Gesetzt
// werden sie beilaeufig - eine Reglerbewegung im ASUS-Testdialog genuegt - und
// danach ignoriert der Kanal jede Farbwahl. Wer das nicht weiss, haelt die
// Farbknoepfe fuer kaputt.
static void AppendOverrideNotice() {
    wchar_t list[128] = L"";
    int n = 0;
    for (int i = 0; i < AURA_CONFIG_CHANNELS; i++) {
        if (!g_config.aura[i].override_active) continue;
        wchar_t one[16];
        swprintf(one, 16, n ? L", %d" : L"%d", i);
        wcscat_s(list, 128, one);
        n++;
    }
    if (n == 0) return;

    wchar_t buf[256];
    swprintf(buf, 256,
             L"Hinweis: Aura-Kanal %s folgt nicht der Globalfarbe (eigene Farbe). "
             L"\"ASUS Test\" \u2192 \"Alle folgen Global\" hebt das auf.", list);
    AppendStatus(buf);
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
#define ID_ASUS_FOLLOW_ALL 6304
#define ID_ASUS_CH_FOLLOW_BASE 6350

// The dialog holds only window handles and the number of channels. Every value
// the user changes goes straight into g_config.aura[i] and is saved from there,
// so closing with X, Esc or a crash cannot lose it - and the apply path reads
// exactly the same fields.
struct AsusTestDialog {
    HWND hDlg;
    HWND hStatus;
    HWND hFirmwareLabel;
    HWND hColorPreview[8];
    HWND hCheckBox[8];
    HWND hFollowBox[8];
    HWND hSliderR[8];
    HWND hSliderG[8];
    HWND hSliderB[8];
    HWND hLabelR[8];
    HWND hLabelG[8];
    HWND hLabelB[8];
    int numChannels;
};

// The colour a channel row currently shows: its override if it has one, the
// global colour otherwise. Sliders, preview and the HID write all read this, so
// they cannot disagree.
static void AsusRowColor(int ch, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (ch < 0 || ch >= AURA_CONFIG_CHANNELS) { r = g = b = 0; return; }
    const ChannelConfig& c = g_config.aura[ch];
    if (c.override_active) { r = c.override_r; g = c.override_g; b = c.override_b; }
    else                   { r = g_state.red; g = g_state.green; b = g_state.blue; }
}

AsusTestDialog* g_asusTest = nullptr;

// Channel whose slider moved last. The debounce timer writes exactly this one;
// it used to be set and never read, so the timer re-sent every channel.
static int s_asusPendingChannel = -1;

// Test a single ASUS channel with a specific color
// 'channel' is the UI index (0, 1, 2...), we map it to the actual direct_channel
bool TestAsusChannel(int channel, uint8_t r, uint8_t g, uint8_t b) {
    // The last public setter that had no dry-run guard. The caller has already
    // written the value to g_config by the time we get here, so --dry-run
    // suppresses the HID write only - the setting itself is still stored.
    if (DryRunSkip(L"ASUS Aura (Testmen\u00FC)")) return false;

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

// Send one dialog row to the hardware. Returns whether the write was
// acknowledged - not whether the LEDs really changed; Aura cannot be read back.
// A channel with "Aktiv" unchecked goes to 0,0,0 through ApplyCorrection.
static bool AsusApplyRow(int ch) {
    if (ch < 0 || ch >= AURA_CONFIG_CHANNELS) return false;
    uint8_t r, g, b;
    AsusRowColor(ch, r, g, b);
    return TestAsusChannel(ch, r, g, b);
}

// Pull sliders, labels and preview of one row back from g_config - used after
// something other than a slider changed what the row should show.
static void AsusSyncRowControls(int ch) {
    if (!g_asusTest || ch < 0 || ch >= g_asusTest->numChannels) return;
    uint8_t r, g, b;
    AsusRowColor(ch, r, g, b);

    SendMessage(g_asusTest->hSliderR[ch], TBM_SETPOS, TRUE, r);
    SendMessage(g_asusTest->hSliderG[ch], TBM_SETPOS, TRUE, g);
    SendMessage(g_asusTest->hSliderB[ch], TBM_SETPOS, TRUE, b);

    wchar_t v[8];
    swprintf(v, 8, L"%d", r); SetWindowTextW(g_asusTest->hLabelR[ch], v);
    swprintf(v, 8, L"%d", g); SetWindowTextW(g_asusTest->hLabelG[ch], v);
    swprintf(v, 8, L"%d", b); SetWindowTextW(g_asusTest->hLabelB[ch], v);

    InvalidateRect(g_asusTest->hColorPreview[ch], NULL, TRUE);
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
            uint8_t r, g, b;
            AsusRowColor(index, r, g, b);
            color = RGB(r, g, b);
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

        // How many channels exist is hardware topology - g_asusHwConfig is the
        // right source for that. What colour they carry is user configuration
        // and comes from g_config.aura[] below.
        int numCh = g_asusHwConfig.valid ? g_asusHwConfig.numChannels : 3;
        if (numCh > 8) numCh = 8;
        if (numCh < 1) numCh = 1;
        g_asusTest->numChannels = numCh;

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

            // Channel header with checkbox. The "active" flag is
            // g_config.aura[i].enabled - the same field the channel-settings
            // dialog and the apply path use. There used to be a second,
            // competing flag in g_asusHwConfig; that one is gone.
            g_asusTest->hCheckBox[i] = CreateWindowW(L"BUTTON", chName,
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                15, y, 225, 20, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_CHECK_BASE + i), NULL, NULL);
            SendMessage(g_asusTest->hCheckBox[i], BM_SETCHECK,
                g_config.aura[i].enabled ? BST_CHECKED : BST_UNCHECKED, 0);

            // "Follows global" - checked means no override for this channel.
            g_asusTest->hFollowBox[i] = CreateWindowW(L"BUTTON", L"Folgt Global",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                245, y, 120, 20, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_FOLLOW_BASE + i), NULL, NULL);
            SendMessage(g_asusTest->hFollowBox[i], BM_SETCHECK,
                g_config.aura[i].override_active ? BST_UNCHECKED : BST_CHECKED, 0);

            // Color preview
            g_asusTest->hColorPreview[i] = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_SUNKEN,
                420, y + 25, 50, 50, hWnd, NULL, NULL, NULL);
            SetWindowSubclass(g_asusTest->hColorPreview[i], AsusColorPreviewProc, 0, (DWORD_PTR)i);

            // Sliders start on the colour this channel currently shows: its
            // override if it has one, the global colour otherwise.
            uint8_t chR, chG, chB;
            AsusRowColor(i, chR, chG, chB);

            wchar_t valR[8], valG[8], valB[8];
            swprintf(valR, 8, L"%d", chR);
            swprintf(valG, 8, L"%d", chG);
            swprintf(valB, 8, L"%d", chB);

            // R slider row
            CreateWindowW(L"STATIC", L"R:", WS_CHILD | WS_VISIBLE,
                25, y + 25, 20, 20, hWnd, NULL, NULL, NULL);
            g_asusTest->hSliderR[i] = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                50, y + 22, 300, 25, hWnd, (HMENU)(INT_PTR)(ID_ASUS_CH_TEST_BASE + i * 10 + 0), NULL, NULL);
            SendMessage(g_asusTest->hSliderR[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
            SendMessage(g_asusTest->hSliderR[i], TBM_SETPOS, TRUE, chR);
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
            SendMessage(g_asusTest->hSliderG[i], TBM_SETPOS, TRUE, chG);
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
                        SendMessage(g_asusTest->hSliderB[i], TBM_SETPOS, TRUE, chB);
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

        CreateWindowW(L"BUTTON", L"Alle folgen Global",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            225, btnY, 150, 28, hWnd, (HMENU)ID_ASUS_FOLLOW_ALL, NULL, NULL);

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

        const uint8_t pos = (uint8_t)SendMessage(hSlider, TBM_GETPOS, 0, 0);

        // Find which channel and color component this slider belongs to
        for (int ch = 0; ch < g_asusTest->numChannels; ch++) {
            bool updated = false;
            wchar_t val[8];

            if (hSlider == g_asusTest->hSliderR[ch]) {
                g_config.aura[ch].override_r = pos;
                swprintf(val, 8, L"%d", pos);
                SetWindowTextW(g_asusTest->hLabelR[ch], val);
                updated = true;
            }
            else if (hSlider == g_asusTest->hSliderG[ch]) {
                g_config.aura[ch].override_g = pos;
                swprintf(val, 8, L"%d", pos);
                SetWindowTextW(g_asusTest->hLabelG[ch], val);
                updated = true;
            }
            else if (hSlider == g_asusTest->hSliderB[ch]) {
                g_config.aura[ch].override_b = pos;
                swprintf(val, 8, L"%d", pos);
                SetWindowTextW(g_asusTest->hLabelB[ch], val);
                updated = true;
            }

            if (updated) {
                // Moving a slider is what makes a channel stop following the
                // global colour. The precedence is established here, not on
                // close - so it survives X, Esc and a crash alike.
                if (!g_config.aura[ch].override_active) {
                    // The other two components still hold whatever the row was
                    // showing; take them over so the channel does not jump.
                    uint8_t curR, curG, curB;
                    AsusRowColor(ch, curR, curG, curB);
                    if (hSlider != g_asusTest->hSliderR[ch]) g_config.aura[ch].override_r = curR;
                    if (hSlider != g_asusTest->hSliderG[ch]) g_config.aura[ch].override_g = curG;
                    if (hSlider != g_asusTest->hSliderB[ch]) g_config.aura[ch].override_b = curB;
                    g_config.aura[ch].override_active = true;
                    SendMessage(g_asusTest->hFollowBox[ch], BM_SETCHECK, BST_UNCHECKED, 0);
                }

                InvalidateRect(g_asusTest->hColorPreview[ch], NULL, TRUE);

                // Live apply with debouncing (150ms delay)
                s_asusPendingChannel = ch;
                KillTimer(hWnd, ID_TIMER_DEBOUNCE);
                SetTimer(hWnd, ID_TIMER_DEBOUNCE, 150, NULL);

                wchar_t buf[80];
                swprintf(buf, 80, L"Kanal %d: RGB(%d, %d, %d) - eigene Farbe", ch,
                        g_config.aura[ch].override_r, g_config.aura[ch].override_g,
                        g_config.aura[ch].override_b);
                SetWindowTextW(g_asusTest->hStatus, buf);
                break;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!g_asusTest) break;   // WM_COMMAND can arrive before WM_INITDIALOG

        // "Aktiv" per channel -> g_config.aura[i].enabled
        for (int i = 0; i < g_asusTest->numChannels; i++) {
            if (id == ID_ASUS_CH_CHECK_BASE + i) {
                g_config.aura[i].enabled =
                    (SendMessage(g_asusTest->hCheckBox[i], BM_GETCHECK, 0, 0) == BST_CHECKED);
                AsusApplyRow(i);
                SaveSettings();
                return TRUE;
            }
        }

        // "Folgt Global" per channel -> drop the override, back to global
        for (int i = 0; i < g_asusTest->numChannels; i++) {
            if (id == ID_ASUS_CH_FOLLOW_BASE + i) {
                bool follows = (SendMessage(g_asusTest->hFollowBox[i], BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_config.aura[i].override_active = !follows;
                if (follows) AsusSyncRowControls(i);   // sliders back to the global colour
                AsusApplyRow(i);
                SaveSettings();
                wchar_t buf[80];
                swprintf(buf, 80, follows ? L"Kanal %d folgt wieder der Globalfarbe"
                                          : L"Kanal %d beh\u00E4lt seine eigene Farbe", i);
                SetWindowTextW(g_asusTest->hStatus, buf);
                return TRUE;
            }
        }

        if (id == ID_ASUS_TEST_ALL) {
            int sent = 0;
            for (int i = 0; i < g_asusTest->numChannels; i++) {
                if (AsusApplyRow(i)) sent++;
            }
            SaveSettings();
            // No GET command for Aura -> "sent", never "applied". CLAUDE.md rule 1.
            wchar_t buf[128];
            swprintf(buf, 128, L"%d/%d Kan\u00E4le geschrieben (ohne Read-back - nicht verifiziert)",
                     sent, g_asusTest->numChannels);
            SetWindowTextW(g_asusTest->hStatus, buf);
        }
        else if (id == ID_ASUS_RESET) {
            // Persistent now: this used to write transient black that the next
            // apply undid.
            for (int i = 0; i < g_asusTest->numChannels; i++) {
                g_config.aura[i].enabled = false;
                SendMessage(g_asusTest->hCheckBox[i], BM_SETCHECK, BST_UNCHECKED, 0);
                AsusApplyRow(i);
            }
            SaveSettings();
            SetWindowTextW(g_asusTest->hStatus, L"Alle Kanaele deaktiviert und gespeichert");
        }
        else if (id == ID_ASUS_FOLLOW_ALL) {
            for (int i = 0; i < g_asusTest->numChannels; i++) {
                g_config.aura[i].override_active = false;
                SendMessage(g_asusTest->hFollowBox[i], BM_SETCHECK, BST_CHECKED, 0);
                AsusSyncRowControls(i);
            }
            SaveSettings();
            RequestApplyColors(true);
            SetWindowTextW(g_asusTest->hStatus, L"Alle Kanaele folgen wieder der Globalfarbe");
        }
        else if (id == ID_ASUS_CLOSE) {
            // asus_hw_config.bin stays a pure hardware cache (firmware, 0xB0
            // table, derived topology). Channel colours live in config.json.
            SaveSettings();
            EndDialog(hWnd, IDOK);
        }
        break;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_DEBOUNCE) {
            KillTimer(hWnd, ID_TIMER_DEBOUNCE);
            int ch = s_asusPendingChannel;
            s_asusPendingChannel = -1;
            if (g_asusTest && ch >= 0 && ch < g_asusTest->numChannels) {
                AsusApplyRow(ch);
                SaveSettings();   // survives X, Esc and a crash
            }
        }
        break;

    case WM_CLOSE:
        // Same as "Schliessen": the hardware was written long ago, so closing
        // must not be the one path that loses the setting.
        SaveSettings();
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
// PER-KEY LIGHTING - visual layout editor
//
// A custom-drawn grid instead of ~120 button controls: one child window paints
// every key from the model in WM_PAINT and resolves clicks with a hit test.
// 120 HWNDs would each need their own subclass, their own colour, their own
// focus handling and would repaint one at a time; the grid is one surface.
//
// What the grid shows is the key MATRIX as the device reports it (column,
// row) - not the physical geometry of the keycaps. That is deliberate: the
// matrix is what the remap table at 0xC0 actually contains, and the physical
// shape of a GK650 keycap is not in the config memory anywhere. Drawing a
// pretty keyboard picture would mean inventing the part that is not measured,
// and the write path addresses matrix slots regardless.
//
// The honesty rule this dialog is built around: it may state that N triples
// were written and read back identically, because it verifies that. It may NOT
// state that the keyboard now shows those colours - which mode byte makes the
// firmware render the table is unmeasured (docs section 5 item 5), and the
// status line says so instead of implying success.
//=============================================================================

#define ID_KEY_GRID       6400
#define ID_KEY_PICK       6401
#define ID_KEY_APPLY      6402
#define ID_KEY_SELECT_ALL 6403
#define ID_KEY_SELECT_NONE 6404
#define ID_KEY_RELOAD     6405
#define ID_KEY_CLOSE      6406
#define ID_KEY_MODE       6407
#define ID_KEY_STATUS     6408
#define ID_KEY_PREVIEW    6409

// One key unit in pixels, plus the inset that turns touching unit rectangles
// into separate-looking keycaps.
static const int KEYUNIT_PX  = 40;
static const int KEYCAP_GAP  = 3;
static const int KEYGRID_PAD = 8;

struct KeyLayoutDialog {
    HWND hDlg      = nullptr;
    HWND hGrid     = nullptr;
    HWND hStatus   = nullptr;
    HWND hPreview  = nullptr;
    HWND hModeCombo = nullptr;

    std::vector<kblayout::Key>    keys;
    std::vector<kblayout::KeyBox> boxes;    // parallel to keys, in key units
    std::vector<char>             selected; // parallel to keys; char, not bool,
                                            // so &selected[i] stays addressable

    // Rubber-band state
    bool  dragging   = false;
    bool  additive   = false;   // Ctrl was held when the drag started
    POINT dragStart  = {0, 0};
    POINT dragNow    = {0, 0};

    // Colour the picker last produced; applied to the selection.
    uint8_t r = 255, g = 255, b = 255;

    bool    layoutFromDevice  = false;
    bool    profileColorRead  = false;   // the live colour could be read
    uint8_t profileMode       = 0;       // what the profile block's mode byte holds

    HFONT font = nullptr;   // owned; released in WM_DESTROY
};

static KeyLayoutDialog* g_keyDlg = nullptr;

// The five bytes that are candidates for "renders the per-key table": every
// mode byte the 2026-08-17 keyboard sweep stored without animating anything.
// A mode that animates cannot be the one that shows a static per-key pattern,
// so these are the ones worth asking about - and until --ask=perkey has named
// one, the list is a list of candidates and nothing more.
static const uint8_t KEY_CUSTOM_MODE_CANDIDATES[] = { 0x00, 0x04, 0x09, 0x13, 0x14 };
static const int KEY_CUSTOM_MODE_COUNT =
    (int)(sizeof(KEY_CUSTOM_MODE_CANDIDATES) / sizeof(KEY_CUSTOM_MODE_CANDIDATES[0]));

// Unit box -> pixel keycap. The gap is taken out of the box rather than added
// between boxes, so a 2-unit key stays exactly twice as wide as a 1-unit one
// and the rows keep lining up.
static RECT KeyCapRect(const kblayout::KeyBox& b) {
    RECT r;
    r.left   = KEYGRID_PAD + (int)(b.x * KEYUNIT_PX) + KEYCAP_GAP;
    r.top    = KEYGRID_PAD + (int)(b.y * KEYUNIT_PX) + KEYCAP_GAP;
    r.right  = KEYGRID_PAD + (int)((b.x + b.w) * KEYUNIT_PX) - KEYCAP_GAP;
    r.bottom = KEYGRID_PAD + (int)((b.y + b.h) * KEYUNIT_PX) - KEYCAP_GAP;
    return r;
}

static int KeyGridWidth()  { return KEYGRID_PAD * 2 + (int)(kblayout::BOARD_WIDTH  * KEYUNIT_PX); }
static int KeyGridHeight() { return KEYGRID_PAD * 2 + (int)(kblayout::BOARD_HEIGHT * KEYUNIT_PX); }

static int KeyHitTest(int x, int y) {
    if (!g_keyDlg) return -1;
    for (size_t i = 0; i < g_keyDlg->boxes.size(); i++) {
        const RECT c = KeyCapRect(g_keyDlg->boxes[i]);
        if (x >= c.left && x < c.right && y >= c.top && y < c.bottom) return (int)i;
    }
    return -1;
}

static int KeySelectionCount() {
    if (!g_keyDlg) return 0;
    int n = 0;
    for (char s : g_keyDlg->selected) if (s) n++;
    return n;
}

static void KeySetStatus(const wchar_t* text) {
    if (g_keyDlg && g_keyDlg->hStatus) SetWindowTextW(g_keyDlg->hStatus, text);
}

// Reads the layout from the device, seeds every key with the colour the
// keyboard is actually showing, then overlays the colours the user last saved.
//
// The order is the whole point:
//   1. matrix + labels from the device - hardware facts, never from a stale
//      config file;
//   2. the profile's live colour (profile_base+0x06..0x08, section 3 [HIGH]) as
//      the starting point, because that IS what the board displays right now;
//   3. the user's own per-key choices on top, because those are settings and
//      have to survive a restart.
static void KeyReloadLayout() {
    if (!g_keyDlg) return;

    std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);
    hid_init();
    // The editor does not paint the 0x2C0 bytes, so it does not read them.
    std::vector<kblayout::Key> keys = ReadKeyLayoutStandalone(nullptr, nullptr, false);

    // The 18-byte keyboard block: [mode, bright, speed, dir, rand, R, G, B, ...]
    // starting at profile_base+0x01, so R/G/B are indices 5..7.
    uint8_t  kbBlock[18] = {0};
    uint8_t  prof = 0;
    uint16_t off  = 0;
    const bool haveProfile = ReadEVisionKeyboardPayload(kbBlock, &prof, &off);
    hid_exit();

    g_keyDlg->layoutFromDevice = !keys.empty();
    g_keyDlg->profileColorRead = haveProfile;
    g_keyDlg->profileMode      = haveProfile ? kbBlock[0] : (uint8_t)0;

    if (keys.empty()) {
        // No fallback board. A layout nobody read is not a layout, and offering
        // 126 clickable caps for a keyboard that did not answer would invite
        // writes to slots whose existence is unconfirmed.
        g_keyDlg->keys.clear();
        g_keyDlg->boxes.clear();
        g_keyDlg->selected.clear();
        KeySetStatus(L"Tastatur nicht erreichbar - kein Layout gelesen. "
                     L"Es wird nichts angezeigt und nichts geschrieben.");
    } else {
        // Live colour first, saved per-key choices second.
        if (haveProfile) {
            kblayout::SeedKeyColors(keys, kbBlock[5], kbBlock[6], kbBlock[7]);
            // Keep the picker on the same colour, so the first click does not
            // jump the board to something the user never chose.
            g_keyDlg->r = kbBlock[5];
            g_keyDlg->g = kbBlock[6];
            g_keyDlg->b = kbBlock[7];
            if (g_keyDlg->hPreview) InvalidateRect(g_keyDlg->hPreview, NULL, TRUE);
        }
        ZonesToKeyLayout(g_config.keyboardZones, keys);

        g_keyDlg->keys  = keys;
        g_keyDlg->boxes = kblayout::ComputeKeyBoxes(keys);
        g_keyDlg->selected.assign(keys.size(), 0);

        wchar_t msg[420];
        if (!haveProfile) {
            swprintf(msg, 420,
                     L"%d Tasten vom Ger\u00E4t gelesen. Die aktuelle Profilfarbe war nicht "
                     L"lesbar - die Tasten zeigen deshalb keine Farbe an.",
                     (int)keys.size());
        } else if (kbBlock[0] == (uint8_t)KB_MODE_STATIC) {
            swprintf(msg, 420,
                     L"%d Tasten. Aktuelle Profilfarbe %02X%02X%02X (P%d, Modus 0x%02X = "
                     L"statisch) - das zeigt die Tastatur gerade. Taste anklicken oder "
                     L"doppelklicken zum \u00C4ndern.",
                     (int)keys.size(), kbBlock[5], kbBlock[6], kbBlock[7],
                     (int)prof, (unsigned)kbBlock[0]);
        } else {
            // Not static: the board is running an effect, so what it shows is
            // not this one colour. Saying "das zeigt die Tastatur gerade" here
            // would be wrong, so it does not say it.
            swprintf(msg, 420,
                     L"%d Tasten. Profilfarbe %02X%02X%02X (P%d), aber Modus 0x%02X ist nicht "
                     L"statisch - die Tastatur l\u00E4uft gerade auf einem Effekt, die "
                     L"gezeigten Farben sind die Grundfarbe des Profils.",
                     (int)keys.size(), kbBlock[5], kbBlock[6], kbBlock[7],
                     (int)prof, (unsigned)kbBlock[0]);
        }
        KeySetStatus(msg);
    }
    if (g_keyDlg->hGrid) InvalidateRect(g_keyDlg->hGrid, NULL, TRUE);
}

static void KeyPaintGrid(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    RECT client;
    GetClientRect(hWnd, &client);

    // Double buffered: 126 cells drawn straight onto the DC flicker visibly on
    // every selection change.
    HDC     mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(g_currentTheme->groupBodyBg);
    FillRect(mem, &client, bg);
    DeleteObject(bg);

    HFONT font = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(mem, font);
    SetBkMode(mem, TRANSPARENT);

    if (g_keyDlg) {
        for (size_t i = 0; i < g_keyDlg->keys.size() && i < g_keyDlg->boxes.size(); i++) {
            const kblayout::Key& k = g_keyDlg->keys[i];
            RECT c = KeyCapRect(g_keyDlg->boxes[i]);

            // A key whose colour was never read is drawn as "unknown", not as
            // black: black is a colour someone might have chosen, and claiming
            // it for a byte nobody read would be the same lie as a status line
            // reporting an unverified write.
            const bool known = k.colorKnown;
            COLORREF fill = known ? RGB(k.r, k.g, k.b) : g_currentTheme->bgControl;

            const bool sel = (i < g_keyDlg->selected.size() && g_keyDlg->selected[i]);

            // Rounded caps, so the board reads as keys rather than as a table.
            HBRUSH capBrush = CreateSolidBrush(fill);
            HPEN   pen = CreatePen(PS_SOLID, sel ? 3 : 1,
                                   sel ? g_currentTheme->borderFocus : g_currentTheme->border);
            HGDIOBJ oldBr  = SelectObject(mem, capBrush);
            HPEN    oldPen = (HPEN)SelectObject(mem, pen);
            RoundRect(mem, c.left, c.top, c.right, c.bottom, 6, 6);
            SelectObject(mem, oldBr);
            SelectObject(mem, oldPen);
            DeleteObject(capBrush);
            DeleteObject(pen);

            // Label in whichever of black/white reads on this fill.
            COLORREF text = g_currentTheme->textSecondary;
            if (known) {
                const int lum = (k.r * 299 + k.g * 587 + k.b * 114) / 1000;
                text = (lum > 140) ? RGB(0, 0, 0) : RGB(255, 255, 255);
            }
            SetTextColor(mem, text);

            wchar_t label[32] = {0};
            MultiByteToWideChar(CP_ACP, 0, kblayout::ShortLabel(k.label), -1, label, 32);
            RECT t = c;
            t.left += 2; t.right -= 2;
            DrawTextW(mem, label, -1, &t,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        if (g_keyDlg->dragging) {
            RECT band;
            band.left   = std::min(g_keyDlg->dragStart.x, g_keyDlg->dragNow.x);
            band.right  = std::max(g_keyDlg->dragStart.x, g_keyDlg->dragNow.x);
            band.top    = std::min(g_keyDlg->dragStart.y, g_keyDlg->dragNow.y);
            band.bottom = std::max(g_keyDlg->dragStart.y, g_keyDlg->dragNow.y);
            HPEN pen = CreatePen(PS_DOT, 1, g_currentTheme->borderFocus);
            HPEN oldPen = (HPEN)SelectObject(mem, pen);
            HGDIOBJ oldBr = SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, band.left, band.top, band.right, band.bottom);
            SelectObject(mem, oldBr);
            SelectObject(mem, oldPen);
            DeleteObject(pen);
        }
    }

    SelectObject(mem, oldFont);
    DeleteObject(font);
    BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hWnd, &ps);
}

static void KeyUpdateSelectionStatus() {
    if (!g_keyDlg) return;
    const int n = KeySelectionCount();
    wchar_t msg[256];
    if (n == 0) {
        swprintf(msg, 256, L"Keine Taste ausgew\u00E4hlt. Klicken, Strg+Klick f\u00FCr mehrere, "
                           L"Ziehen f\u00FCr ein Rechteck.");
    } else if (n == 1) {
        for (size_t i = 0; i < g_keyDlg->selected.size(); i++) {
            if (!g_keyDlg->selected[i]) continue;
            const kblayout::Key& k = g_keyDlg->keys[i];
            wchar_t label[32] = {0};
            MultiByteToWideChar(CP_ACP, 0, k.label.c_str(), -1, label, 32);
            swprintf(msg, 256, L"%s ausgew\u00E4hlt - Matrix (%d,%d), Farb-Offset 0x%04X",
                     label, k.col, k.row, (unsigned)k.colorOffset);
            break;
        }
    } else {
        swprintf(msg, 256, L"%d Tasten ausgew\u00E4hlt.", n);
    }
    KeySetStatus(msg);
}

LRESULT CALLBACK KeyGridProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;   // WM_PAINT fills everything; erasing first only flickers

    case WM_PAINT:
        KeyPaintGrid(hWnd);
        return 0;

    case WM_LBUTTONDOWN: {
        if (!g_keyDlg) break;
        const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const int hit = KeyHitTest(x, y);

        if (hit >= 0) {
            if (ctrl) {
                g_keyDlg->selected[(size_t)hit] = g_keyDlg->selected[(size_t)hit] ? 0 : 1;
            } else {
                std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)0);
                g_keyDlg->selected[(size_t)hit] = 1;
            }
        } else if (!ctrl) {
            std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)0);
        }

        // A drag always starts, even on a hit: press-and-drag from a key is the
        // natural way to extend a selection, and a click that never moves is
        // just a click.
        g_keyDlg->dragging  = true;
        g_keyDlg->additive  = ctrl;
        g_keyDlg->dragStart.x = x; g_keyDlg->dragStart.y = y;
        g_keyDlg->dragNow     = g_keyDlg->dragStart;
        SetCapture(hWnd);
        InvalidateRect(hWnd, NULL, FALSE);
        KeyUpdateSelectionStatus();
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        // The shortest path to "colour this one key": double-click it. The
        // select-then-pick flow still exists for multiple keys, but making the
        // single-key case take two clicks in two different places is the kind of
        // friction that makes a working feature feel like it is not there.
        if (!g_keyDlg) break;
        const int hit = KeyHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit < 0) break;
        g_keyDlg->dragging = false;
        ReleaseCapture();
        std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)0);
        g_keyDlg->selected[(size_t)hit] = 1;
        InvalidateRect(hWnd, NULL, FALSE);
        PostMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(ID_KEY_PICK, BN_CLICKED), 0);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_keyDlg && g_keyDlg->dragging) {
            g_keyDlg->dragNow.x = GET_X_LPARAM(lParam);
            g_keyDlg->dragNow.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP: {
        if (!g_keyDlg || !g_keyDlg->dragging) break;
        ReleaseCapture();
        g_keyDlg->dragging = false;

        RECT band;
        band.left   = std::min(g_keyDlg->dragStart.x, g_keyDlg->dragNow.x);
        band.right  = std::max(g_keyDlg->dragStart.x, g_keyDlg->dragNow.x);
        band.top    = std::min(g_keyDlg->dragStart.y, g_keyDlg->dragNow.y);
        band.bottom = std::max(g_keyDlg->dragStart.y, g_keyDlg->dragNow.y);

        // Anything smaller than a few pixels was a click, and the click was
        // already handled in WM_LBUTTONDOWN.
        if ((band.right - band.left) > 3 || (band.bottom - band.top) > 3) {
            if (!g_keyDlg->additive)
                std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)0);
            for (size_t i = 0; i < g_keyDlg->boxes.size(); i++) {
                RECT c = KeyCapRect(g_keyDlg->boxes[i]);
                RECT tmp;
                if (IntersectRect(&tmp, &c, &band)) g_keyDlg->selected[i] = 1;
            }
        }
        InvalidateRect(hWnd, NULL, FALSE);
        KeyUpdateSelectionStatus();
        return 0;
    }

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Applies the picked colour to every selected key in the MODEL only. Nothing
// reaches the device until "Uebernehmen" - so a user can build a whole scheme
// and pay for one verified write instead of one per click.
static void KeyApplyColorToSelection(uint8_t r, uint8_t g, uint8_t b) {
    if (!g_keyDlg) return;
    int n = 0;
    for (size_t i = 0; i < g_keyDlg->keys.size(); i++) {
        if (!g_keyDlg->selected[i]) continue;
        g_keyDlg->keys[i].r = r;
        g_keyDlg->keys[i].g = g;
        g_keyDlg->keys[i].b = b;
        g_keyDlg->keys[i].colorKnown = true;   // now known: the user chose it
        n++;
    }
    if (g_keyDlg->hGrid) InvalidateRect(g_keyDlg->hGrid, NULL, FALSE);

    wchar_t msg[192];
    if (n == 0) swprintf(msg, 192, L"Keine Auswahl - die Farbe wurde nirgends gesetzt.");
    else        swprintf(msg, 192, L"Farbe %02X%02X%02X auf %d Taste%s gesetzt. "
                                   L"Noch nicht geschrieben - daf\u00FCr \"\u00DCbernehmen\".",
                         r, g, b, n, n == 1 ? L"" : L"n");
    KeySetStatus(msg);
}

INT_PTR CALLBACK KeyLayoutDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_keyDlg = new KeyLayoutDialog();
        g_keyDlg->hDlg = hWnd;
        g_keyDlg->r = g_state.red; g_keyDlg->g = g_state.green; g_keyDlg->b = g_state.blue;

        SetWindowTextW(hWnd, L"Tastenbeleuchtung - Layout aus dem Ger\u00E4t");

        const int gridW = KeyGridWidth();
        const int gridH = KeyGridHeight();
        const int clientW = gridW + 24;
        const int clientH = gridH + 190;

        RECT want = {0, 0, clientW, clientH};
        AdjustWindowRect(&want, (DWORD)GetWindowLongPtrW(hWnd, GWL_STYLE), FALSE);
        const int wndW = want.right - want.left, wndH = want.bottom - want.top;
        const int sx = (GetSystemMetrics(SM_CXSCREEN) - wndW) / 2;
        const int sy = (GetSystemMetrics(SM_CYSCREEN) - wndH) / 2;
        SetWindowPos(hWnd, NULL, sx > 0 ? sx : 0, sy > 0 ? sy : 0, wndW, wndH, SWP_NOZORDER);

        HFONT font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        g_keyDlg->font = font;

        int y = 8;
        HWND hInfo = CreateWindowW(L"STATIC",
            L"Klick w\u00E4hlt eine Taste, Doppelklick \u00F6ffnet gleich die Farbwahl. "
            L"Strg+Klick w\u00E4hlt mehrere, Ziehen w\u00E4hlt ein Rechteck. "
            L"Welche Tasten es gibt, kommt aus dem Ger\u00E4t.",
            WS_CHILD | WS_VISIBLE, 12, y, clientW - 24, 16, hWnd, NULL, NULL, NULL);
        SendMessageW(hInfo, WM_SETFONT, (WPARAM)font, TRUE);
        y += 22;

        g_keyDlg->hGrid = CreateWindowExW(0, L"OCRGBKeyGrid", L"",
            WS_CHILD | WS_VISIBLE, 12, y, gridW, gridH,
            hWnd, (HMENU)(INT_PTR)ID_KEY_GRID, GetModuleHandleW(NULL), NULL);
        y += gridH + 10;

        // Colour row
        g_keyDlg->hPreview = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 12, y, 40, 24,
            hWnd, (HMENU)(INT_PTR)ID_KEY_PREVIEW, NULL, NULL);
        HWND hPick = CreateWindowW(L"BUTTON", L"Farbe w\u00E4hlen...",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 60, y, 120, 24,
            hWnd, (HMENU)(INT_PTR)ID_KEY_PICK, NULL, NULL);
        HWND hAll = CreateWindowW(L"BUTTON", L"Alle ausw\u00E4hlen",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 188, y, 120, 24,
            hWnd, (HMENU)(INT_PTR)ID_KEY_SELECT_ALL, NULL, NULL);
        HWND hNone = CreateWindowW(L"BUTTON", L"Auswahl aufheben",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 316, y, 130, 24,
            hWnd, (HMENU)(INT_PTR)ID_KEY_SELECT_NONE, NULL, NULL);
        HWND hReload = CreateWindowW(L"BUTTON", L"Neu vom Ger\u00E4t lesen",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 454, y, 150, 24,
            hWnd, (HMENU)(INT_PTR)ID_KEY_RELOAD, NULL, NULL);

        HWND hModeLbl = CreateWindowW(L"STATIC", L"Custom-Modus:",
            WS_CHILD | WS_VISIBLE, 616, y + 4, 90, 18, hWnd, NULL, NULL, NULL);
        g_keyDlg->hModeCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            710, y, 170, 200, hWnd, (HMENU)(INT_PTR)ID_KEY_MODE, NULL, NULL);
        SendMessageW(g_keyDlg->hModeCombo, CB_ADDSTRING, 0,
                     (LPARAM)L"nicht gesetzt (ungemessen)");
        for (int i = 0; i < KEY_CUSTOM_MODE_COUNT; i++) {
            wchar_t item[48];
            swprintf(item, 48, L"0x%02X (Kandidat)", (unsigned)KEY_CUSTOM_MODE_CANDIDATES[i]);
            SendMessageW(g_keyDlg->hModeCombo, CB_ADDSTRING, 0, (LPARAM)item);
        }
        int modeSel = 0;
        for (int i = 0; i < KEY_CUSTOM_MODE_COUNT; i++)
            if (KEY_CUSTOM_MODE_CANDIDATES[i] == g_config.kbCustomMode) modeSel = i + 1;
        SendMessage(g_keyDlg->hModeCombo, CB_SETCURSEL, modeSel, 0);
        y += 32;

        HWND hApply = CreateWindowW(L"BUTTON", L"\u00DCbernehmen (schreibt die Tabelle)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 12, y, 240, 26,
            hWnd, (HMENU)(INT_PTR)ID_KEY_APPLY, NULL, NULL);
        HWND hClose = CreateWindowW(L"BUTTON", L"Schlie\u00DFen",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 260, y, 100, 26,
            hWnd, (HMENU)(INT_PTR)ID_KEY_CLOSE, NULL, NULL);
        y += 34;

        g_keyDlg->hStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, clientW - 24, 48,
            hWnd, (HMENU)(INT_PTR)ID_KEY_STATUS, NULL, NULL);

        HWND fontTargets[] = { hInfo, hPick, hAll, hNone, hReload, hModeLbl,
                               g_keyDlg->hModeCombo, hApply, hClose, g_keyDlg->hStatus };
        for (HWND h : fontTargets)
            if (h) SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);

        KeyReloadLayout();
        return TRUE;
    }

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, g_currentTheme->textPrimary);
        SetBkColor(hdc, g_currentTheme->groupBodyBg);
        // Kept alive for the lifetime of the process and rebuilt only when the
        // theme colour actually changes. Returning a brush and deleting it on
        // the next message hands Windows a handle it is still painting with.
        static HBRUSH   brush     = NULL;
        static COLORREF brushColor = 0;
        if (!brush || brushColor != g_currentTheme->groupBodyBg) {
            if (brush) DeleteObject(brush);
            brushColor = g_currentTheme->groupBodyBg;
            brush = CreateSolidBrush(brushColor);
        }
        return (INT_PTR)brush;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == ID_KEY_PREVIEW && g_keyDlg) {
            HBRUSH br = CreateSolidBrush(RGB(g_keyDlg->r, g_keyDlg->g, g_keyDlg->b));
            FillRect(dis->hDC, &dis->rcItem, br);
            DeleteObject(br);
            FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        break;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        // IDCANCEL can arrive before WM_INITDIALOG has built the state (Esc on a
        // dialog that is still coming up), so every branch below needs it to
        // exist first.
        if (!g_keyDlg) { if (id == IDCANCEL) EndDialog(hWnd, IDCANCEL); break; }
        if (id == ID_KEY_PICK) {
            CHOOSECOLORW cc = {0};
            static COLORREF custom[16] = {0};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hWnd;
            cc.lpCustColors = custom;
            cc.rgbResult = RGB(g_keyDlg->r, g_keyDlg->g, g_keyDlg->b);
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc)) {
                g_keyDlg->r = GetRValue(cc.rgbResult);
                g_keyDlg->g = GetGValue(cc.rgbResult);
                g_keyDlg->b = GetBValue(cc.rgbResult);
                InvalidateRect(g_keyDlg->hPreview, NULL, TRUE);
                KeyApplyColorToSelection(g_keyDlg->r, g_keyDlg->g, g_keyDlg->b);
            }
        }
        else if (id == ID_KEY_SELECT_ALL) {
            std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)1);
            InvalidateRect(g_keyDlg->hGrid, NULL, FALSE);
            KeyUpdateSelectionStatus();
        }
        else if (id == ID_KEY_SELECT_NONE) {
            std::fill(g_keyDlg->selected.begin(), g_keyDlg->selected.end(), (char)0);
            InvalidateRect(g_keyDlg->hGrid, NULL, FALSE);
            KeyUpdateSelectionStatus();
        }
        else if (id == ID_KEY_RELOAD) {
            KeyReloadLayout();
        }
        else if (id == ID_KEY_MODE && HIWORD(wParam) == CBN_SELCHANGE) {
            const int sel = (int)SendMessage(g_keyDlg->hModeCombo, CB_GETCURSEL, 0, 0);
            g_config.kbCustomMode = (sel > 0 && sel <= KEY_CUSTOM_MODE_COUNT)
                                  ? KEY_CUSTOM_MODE_CANDIDATES[sel - 1]
                                  : (uint8_t)0xFF;
            SaveSettings();
            if (g_config.kbCustomMode == 0xFF) {
                KeySetStatus(L"Kein Custom-Modus gesetzt: \u00DCbernehmen schreibt nur die "
                             L"Farbtabelle und fasst den Profilblock nicht an.");
            } else {
                wchar_t msg[256];
                swprintf(msg, 256,
                         L"Custom-Modus 0x%02X - ungemessener Kandidat. \u00DCbernehmen setzt "
                         L"ihn vor der Tabelle und meldet nur, was zur\u00FCckgelesen wurde.",
                         (unsigned)g_config.kbCustomMode);
                KeySetStatus(msg);
            }
        }
        else if (id == ID_KEY_APPLY) {
            if (g_keyDlg->keys.empty()) {
                KeySetStatus(L"Kein Layout geladen - es wird nichts geschrieben.");
                break;
            }
            KeySetStatus(L"Schreibe Farbtabelle...");
            UpdateWindow(g_keyDlg->hStatus);

            KeyApplyResult res;
            {
                std::lock_guard<std::mutex> ioLock(g_state.deviceIoMutex);
                hid_init();
                res = ApplyKeyColorsToDevice(g_keyDlg->keys);
                hid_exit();
            }

            // The model is persisted regardless of what the hardware did: the
            // user's colour choices are settings, and losing them because a
            // write failed would punish them for a device problem.
            KeyLayoutToZones(g_keyDlg->keys, g_config.keyboardZones);
            SaveSettings();

            // Every clause below is something that was read back. What the
            // keyboard SHOWS is deliberately not claimed - no measurement names
            // the mode byte that renders this table yet (docs section 5 item 5).
            wchar_t msg[640];
            if (g_state.dryRun) {
                swprintf(msg, 640, L"[DRY] Kein Write gesendet. Auswahl gespeichert.");
            } else if (!res.deviceFound) {
                swprintf(msg, 640,
                         L"Kein stabiler Snapshot der Farbtabelle - es wurde NICHTS "
                         L"geschrieben. Ohne Wiederherstellungspunkt wird hier nicht "
                         L"geschrieben.");
            } else if (res.tableVerified) {
                swprintf(msg, 640,
                         L"%d Tasten-Tripel geschrieben und zur\u00FCckgelesen: identisch "
                         L"(0x%04X, %d Bytes).%s%s\n"
                         L"Verifiziert ist damit der Speicher - ob die Tastatur diese Farben "
                         L"ANZEIGT, ist ungemessen.",
                         res.keysPatched, (unsigned)kblayout::KEYCOLOR_BASE, res.stableBytes,
                         res.modeAttempted
                            ? (res.modeVerified ? L"  Custom-Modus verifiziert."
                                                : L"  Custom-Modus NICHT best\u00E4tigt.")
                            : L"  Kein Custom-Modus gesetzt.",
                         res.stableBytes < (int)kblayout::KEYCOLOR_REGION_END - (int)kblayout::KEYCOLOR_BASE
                            ? L"  Die Tabelle ist k\u00FCrzer als angenommen." : L"");
            } else {
                swprintf(msg, 640,
                         L"Schreiben NICHT best\u00E4tigt: R\u00FCcklesung weicht ab "
                         L"(erste Abweichung 0x%04X). Es wird kein Erfolg gemeldet.",
                         (unsigned)(res.firstBadOffset >= 0 ? res.firstBadOffset : 0));
            }
            KeySetStatus(msg);

            // Show what the device holds now rather than what we sent.
            if (!g_state.dryRun && res.deviceFound) KeyReloadLayout();
        }
        else if (id == ID_KEY_CLOSE || id == IDCANCEL) {
            EndDialog(hWnd, IDOK);
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(hWnd, IDCANCEL);
        break;

    case WM_DESTROY:
        if (g_keyDlg) {
            if (g_keyDlg->font) DeleteObject(g_keyDlg->font);
            delete g_keyDlg;
            g_keyDlg = nullptr;
        }
        break;
    }
    return FALSE;
}

void ShowKeyLayoutDialog(HWND hWnd) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
        wc.style         = CS_DBLCLKS;   // without this the grid never sees a double click
        wc.lpfnWndProc   = KeyGridProc;
        wc.hInstance     = GetModuleHandleW(NULL);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"OCRGBKeyGrid";
        RegisterClassExW(&wc);
        registered = true;
    }

    BYTE dlgTemplate[512] = {0};
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)dlgTemplate;
    // Size is set for real in WM_INITDIALOG via AdjustWindowRect - dialog units
    // depend on the dialog font, and this grid has to line up with pixels.
    pDlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    pDlg->cx = 400; pDlg->cy = 260;
    DialogBoxIndirectW(GetModuleHandle(NULL), pDlg, hWnd, KeyLayoutDlgProc);
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
        CreateWindowW(L"BUTTON", L"Tastenfarben", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            gx+278, btnY2, 110, BTN_H, hWnd, (HMENU)ID_BTN_KEY_LAYOUT, hInst, NULL);
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

        // Wer beim Beenden ausgeschaltet hatte, will beim Start nicht
        // geblendet werden.
        AppendOverrideNotice();
        if (!g_skipApplyOnStart) {
            if (g_config.lightsOff) {
                AppendStatus(L"Beleuchtung war ausgeschaltet - bleibt aus");
                std::thread([] { ApplyLightsOff(); }).detach();
            } else {
                RequestApplyColors(true);
            }
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
        case ID_HOTKEY_OFF: ToggleLightsOff(); break;
        }
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_APPLY) {
            // Das Hex-Feld einlesen, wenn es noch den Fokus hat.
            //
            // Die Nachrichtenschleife ruft IsDialogMessage; Enter aktiviert
            // damit den BS_DEFPUSHBUTTON APPLY, OHNE dass das Edit-Feld den
            // Fokus verliert. EN_KILLFOCUS - der einzige Ort, an dem der
            // Hex-Text bisher gelesen wurde - feuert dann nie, und APPLY schickt
            // die vorherige Farbe an die Hardware, waehrend im Feld die neue
            // steht. Ein Klick auf APPLY nimmt dem Feld den Fokus und lief
            // deshalb richtig; nur Enter war betroffen.
            if (g_state.hEditHex && GetFocus() == g_state.hEditHex) {
                wchar_t hex[16];
                GetWindowTextW(g_state.hEditHex, hex, 16);
                ParseHexColor(hex);
                UpdatePreview();
                UpdateSliders();
            }
            CommitStateAndApply(true);
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
            // Open ASUS Test dialog. It saves its own changes as they happen;
            // there is no read-back for Aura, so no success is claimed here.
            ShowAsusTestDialog(hWnd);
            AppendStatus(L"ASUS-Kanaleinstellungen gespeichert (Hardware nicht r\u00FCcklesbar)");
        }
        else if (id == ID_BTN_KEY_LAYOUT) {
            // Per-key lighting. The dialog reads the layout from the device and
            // reports only what it read back, so nothing is claimed here.
            ShowKeyLayoutDialog(hWnd);
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
        else if (id == ID_BTN_PRESET_OFF) ToggleLightsOff();
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
        else if (id == ID_TRAY_OFF) { ToggleLightsOff(); }
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
        // Energiesparmodus betreten. Standby und Ruhezustand melden sich beide
        // hier - unterscheidbar sind sie an dieser Stelle nicht, und sie muessen
        // es auch nicht sein: dunkelschalten ist fuer beide richtig.
        if (wParam == PBT_APMSUSPEND) {
            const ULONGLONG now  = GetTickCount64();
            const ULONGLONG last = g_lastSuspendTick.load();
            if (last && (now - last) < SUSPEND_DEBOUNCE_MS) {
                LogDebug("[power] PBT_APMSUSPEND doppelt innerhalb der Entprellung - ignoriert");
                return TRUE;
            }
            g_lastSuspendTick = now;
            g_suspendSeen     = true;

            ClearStatus();
            AppendStatus(L"Energiesparmodus - Beleuchtung wird ausgeschaltet");
            // Derselbe Weg wie der Aus-Knopf. Mit Frist statt blockierend: haelt
            // gerade ein Apply den Geraetemutex, wuerde ein blockierendes Warten
            // die Nachrichtenschleife anhalten, und Windows gibt einem
            // PBT_APMSUSPEND nur rund zwei Sekunden.
            ApplyLightsOff(1200);
        }
        // Aufwachen. Der EINZIGE verbliebene Ausloeser des HID-Resets.
        else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            g_suspendSeen = true;   // authoritative: we really were suspended
            ScheduleResumeReset(hWnd);
        }
        // Eine registrierte Energieeinstellung hat sich geaendert.
        //
        // Nach pbs->PowerSetting verzweigen, nicht nur nach der Nutzlast: es ist
        // mehr als eine GUID registriert, und die Nutzlast der einen als
        // Display-Zustand der anderen zu lesen waere ein Messfehler, den nichts
        // mehr auffangen kann.
        else if (wParam == PBT_POWERSETTINGCHANGE) {
            const POWERBROADCAST_SETTING* pbs = (const POWERBROADCAST_SETTING*)lParam;
            if (pbs && pbs->DataLength >= 4) {
                const DWORD value = *((const DWORD*)pbs->Data);

                if (IsEqualGUID(pbs->PowerSetting, kGuidConsoleDisplayState)) {
                    // Die erste eintreffende Nachricht ist der einzige Beleg
                    // dafuer, dass die Anmeldung nicht nur angenommen, sondern
                    // auch bedient wird. Mit der alten, um ein Byte verkuerzten
                    // GUID kam hier nie etwas an, obwohl die Anmeldung gelang.
                    static bool s_firstDisplayNotify = true;
                    if (s_firstDisplayNotify) {
                        s_firstDisplayNotify = false;
                        LogDebug("[power] erste Display-Benachrichtigung erhalten "
                                 "- die GUID stimmt");
                    }
                    // Monitor aus/an loest NICHTS mehr aus, es wird nur
                    // protokolliert. Auf diesem Rechner geht der Monitor alle
                    // 15 Minuten aus (powercfg: VIDEOIDLE = 900 s), waehrend
                    // automatischer Standby ganz abgeschaltet ist - Display-an
                    // war hier also nie ein Aufwachen, sondern der Normalfall.
                    LogDebug(value == 0 ? "[power] Display aus"
                           : value == 1 ? "[power] Display an"
                                        : "[power] Display gedimmt");
                }
                else if (IsEqualGUID(pbs->PowerSetting, kGuidSystemAwayMode)) {
                    LogDebug(value ? "[power] Abwesenheitsmodus betreten"
                                   : "[power] Abwesenheitsmodus verlassen");
                }
            }
        }
        return TRUE;
    }

    // Sperren und Entsperren loesen nichts mehr aus.
    //
    // Entsperren war einer von fuenf Wegen zum HID-Reset und der
    // irrefuehrendste: es feuert Sekunden bis Stunden nach dem eigentlichen
    // Aufwachen, und genauso nach einer Mittagspause, in der die Maschine
    // durchgelaufen ist. Was ein Aufwachen ist, sagt PBT_APMRESUME* - und sonst
    // nichts.
    case WM_WTSSESSION_CHANGE: {
        if (wParam == WTS_SESSION_LOCK)        LogDebug("[power] Sitzung gesperrt");
        else if (wParam == WTS_SESSION_UNLOCK) LogDebug("[power] Sitzung entsperrt");
        return TRUE;
    }

    // Herunterfahren und Abmelden. Bisher gar nicht behandelt: die Beleuchtung
    // blieb an, waehrend Standby sie dunkel schaltete - dieselbe Maschine, zwei
    // Ergebnisse. Viele Boards halten die RGB-Schienen in S5 unter Spannung,
    // also bleibt es sichtbar stehen.
    case WM_QUERYENDSESSION:
        // Nicht widersprechen; nur ankuendigen, dass wir gleich aufraeumen.
        return TRUE;

    case WM_ENDSESSION:
        if (wParam) {
            LogDebug("[power] WM_ENDSESSION - Beleuchtung wird ausgeschaltet");
            ApplyLightsOff(1200);
        }
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER_RESUME) {
            KillTimer(hWnd, ID_TIMER_RESUME);
            g_resetArmed = false;

            // Laeuft schon einer, wird NEU scharfgestellt statt aufzugeben.
            //
            // Vorher stand die Ruecknahme von g_suspendSeen VOR diesem Test: der
            // Suspend war damit verbraucht, der Reset fand aber nicht statt, und
            // ein neuer war bis zum naechsten echten Standby nicht mehr moeglich.
            // Die Geraete blieben dunkel oder falsch, bis jemand einen Regler
            // anfasste.
            if (g_resetInFlight.load()) {
                g_resetArmed = true;
                SetTimer(hWnd, ID_TIMER_RESUME, 3000, NULL);
                break;
            }

            // Erst jetzt den Suspend verbrauchen - ab hier laeuft der Reset
            // wirklich.
            g_suspendSeen = false;
            if (g_resetInFlight.exchange(true)) break;
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
        StopApplyWorker();
        // Unregister hotkeys
        UnregisterHotKey(hWnd, ID_HOTKEY_BLUE);
        UnregisterHotKey(hWnd, ID_HOTKEY_RED);
        UnregisterHotKey(hWnd, ID_HOTKEY_GREEN);
        UnregisterHotKey(hWnd, ID_HOTKEY_WHITE);
        UnregisterHotKey(hWnd, ID_HOTKEY_OFF);
        UnregisterHotKey(hWnd, ID_HOTKEY_TOGGLE);
        WTSUnRegisterSessionNotification(hWnd);
        // Die Rueckgabewerte wurden bisher weggeworfen, also konnte sich nichts
        // abmelden.
        if (g_hDisplayNotify) { UnregisterPowerSettingNotification(g_hDisplayNotify); g_hDisplayNotify = NULL; }
        if (g_hSuspendNotify) { UnregisterSuspendResumeNotification(g_hSuspendNotify); g_hSuspendNotify = NULL; }
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
        // Der Swatch hat den geloeschten Knopf ID_BTN_PICK_COLOR ersetzt, aber
        // dessen zweite Zeile nicht mitgenommen: der Knopf rief PickColor() UND
        // CommitStateAndApply(). Ohne das schreibt PickColor() nur g_state und
        // die Oberflaeche - die gewaehlte Farbe erreichte weder die Hardware
        // noch config.json, und der Bediener sah eine Farbe, die es nirgends
        // sonst gab.
        //
        // Nur bei tatsaechlicher Auswahl committen: ein abgebrochener Dialog
        // darf keinen Schreibvorgang und keinen Apply ausloesen.
        if (PickColor()) CommitStateAndApply(false);
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

    // --kbdump : read-only hex dump of the on-board config memory to
    // %APPDATA%\OneClickRGB\docs\kbdump.txt. Pure reads - command 0x05 only, no
    // 0x06 writes anywhere in this path.
    //
    //   --kbdump                     dump 0x000..0x3FF (unchanged default)
    //   --kbdump --kbdump-range=lo-hi   dump exactly that range
    //
    // Why the range exists. The window was hardcoded to 0x400 bytes, and the
    // per-key colour table starts at 0x2C0: if it holds one triple per matrix
    // position it ends at 0x43A, so the last 58 bytes of the thing this dump is
    // meant to decode have never been read. Worse, the dump *looks* complete -
    // it just stops. The 16-byte pattern that appears from 0x2F0 onwards is the
    // other half of the same question, and neither half can be settled by
    // reading the same 0x400 bytes again.
    //
    // The range is deliberately not clamped to some assumed memory size. Where
    // the config memory ends is one of the things being measured; the read stops
    // where the device stops answering and the report says at which offset that
    // was, which is a finding rather than a guess.
    if (cli::Find(lpCmdLine, "--kbdump").present ||
        cli::Find(lpCmdLine, "--kbdump-range").present) {
        const std::wstring dumpPath = GetAppDataPath() + L"\\docs\\kbdump.txt";
        std::wstring dir = GetAppDataPath() + L"\\docs";
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);

        uint16_t dumpLo = 0x0000, dumpHi = 0x03FF;
        const cli::Flag rangeArg = cli::Find(lpCmdLine, "--kbdump-range");
        if (rangeArg.present) {
            if (!rangeArg.hasValue || !cli::ParseRange16(rangeArg.value, dumpLo, dumpHi)) {
                FILE* fe = _wfopen(dumpPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 config memory dump\n"
                                "ERROR: --kbdump-range needs <lo>-<hi>, both 0..0xFFFF and\n"
                                "lo <= hi (e.g. --kbdump-range=0x2C0-0x4FF). Nothing was read.\n");
                    fclose(fe);
                }
                LogDebug("[kbdump] --kbdump-range: invalid range - nothing was read");
                return 2;
            }
        }

        // A dump taken while another instance writes is not a reference state -
        // and this dump is exactly what the collateral checks compare against.
        if (!AcquireProbeLock()) return ProbeLockBusy(dumpPath, "kbdump");
        hid_init();
        hid_device* dev = nullptr;
        struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
        for (auto* c = devs; c; c = c->next)
            if (c->usage_page == Devices::EVISION_USAGE_PAGE) { dev = hid_open_path(c->path); break; }
        hid_free_enumeration(devs);
        int rc = 0;
        if (dev) {
            EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr); Sleep(20);
            uint8_t prof = 0;
            EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &prof);

            FILE* fp = _wfopen(dumpPath.c_str(), L"w");
            if (fp) {
                fprintf(fp, "EVision GK650 config memory dump\nactiveProfile=%d\n", (int)prof);
                fprintf(fp, "range=0x%04X-0x%04X\n\n", (unsigned)dumpLo, (unsigned)dumpHi);

                // Line by line rather than one big ReadEVisionConfig call: the
                // per-line rr= column is what kbdump_diff.ps1 uses to tell a
                // failed read apart from sixteen bytes that really are zero.
                for (int off = dumpLo; off <= (int)dumpHi; off += 16) {
                    const int want = ((int)dumpHi - off + 1) > 16 ? 16 : ((int)dumpHi - off + 1);
                    uint8_t buf[16] = {0};
                    int rr = 0;
                    ReadEVisionConfig(dev, (uint16_t)off, (uint16_t)(off + want - 1), buf, &rr);
                    fprintf(fp, "%04X: ", off);
                    for (int i = 0; i < 16; i++) {
                        if (i < want) fprintf(fp, "%02X ", buf[i]);
                        else          fprintf(fp, "   ");
                    }
                    fprintf(fp, "  rr=%d\n", rr);
                    if (rr < 0) {
                        // Where the device stops answering is a measurement, so
                        // it is named instead of being left as a silent tail of
                        // zeros. The old loop only broke past 0x100 and filled
                        // everything before that with buffer zeros.
                        fprintf(fp, "\nread refused at 0x%04X (rr=%d) - the config memory does not\n"
                                    "extend this far, or the device stopped answering. Nothing past\n"
                                    "this offset was read; treat it as unknown, not as zero.\n",
                                (unsigned)off, rr);
                        break;
                    }
                }
                fclose(fp);
            }
            EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
            hid_close(dev);
        } else {
            FILE* fe = _wfopen(dumpPath.c_str(), L"w");
            if (fe) {
                fprintf(fe, "EVision GK650 config memory dump\n"
                            "ERROR: keyboard not found - nothing was read.\n");
                fclose(fe);
            }
            LogDebug("[kbdump] keyboard not found - nothing was read");
            rc = 1;
        }
        hid_exit();
        ReleaseProbeLock();
        return rc;
    }

    // --keyidentify=<off>[,<off>...] : which key does a colour-table offset drive?
    //
    // The mapping "matrix slot -> colour triple at 0x2C0 + slot*3" is an
    // inference from a dump, not a measurement. This probe turns it into one, the
    // same way IdentifyMouseZone does for the mouse: set exactly ONE triple to
    // white, name the key the model predicts for that offset, and ask whether
    // that is the key which lit up. Then restore, verified.
    //
    // Sampling is the point - first column, last column, a couple of row changes.
    // If the predictions hold at the edges and at the wrap-around between two
    // columns, the order holds everywhere; if they do not, that is worth more
    // than a confirmation, and it costs the same six dialogs.
    //
    // Deviation from a plain OK/Cancel: the dialog is Yes/No/Cancel, because
    // "the user dismissed it" must not be recorded as "no". That distinction is
    // load-bearing everywhere else in this file (rule 1) and there is no reason
    // for this probe to be the exception. When the answer is "no", the report
    // asks for the key that really lit - a free-text answer a MessageBox cannot
    // collect, and a wrong guess written into the report would be worse than a
    // gap.
    //
    // Rule 2: the only bytes written are three at a time inside
    // [0x2C0, 0x43A) - the extent docs/Keyboard_Protocol.md section 5 item 5
    // documents for the colour table - and every one of them is put back before
    // the probe returns.
    {
        const cli::Flag idFlag = cli::Find(lpCmdLine, "--keyidentify");
        if (idFlag.present) {
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            const std::wstring reportPath = dir + L"\\keyidentify.txt";

            std::vector<uint16_t> offsets;
            bool argOk = idFlag.hasValue && cli::ParseOffsetList(idFlag.value, offsets, 64);
            uint16_t badOff = 0;
            if (argOk) {
                for (uint16_t o : offsets) {
                    if (!kblayout::IsKeyColorOffset(o)) { argOk = false; badOff = o; break; }
                }
            }
            if (!argOk) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 per-key identify\n"
                                "ERROR: --keyidentify needs one or more colour-table offsets,\n"
                                "comma separated, each inside 0x%04X..0x%04X and a multiple of %d\n"
                                "bytes from the base (e.g. --keyidentify=0x2C0,0x2C3).\n"
                                "Nothing was written.\n",
                            (unsigned)kblayout::KEYCOLOR_BASE,
                            (unsigned)(kblayout::KEYCOLOR_REGION_END - 1),
                            kblayout::KEYCOLOR_STRIDE);
                    if (badOff) fprintf(fe, "First offending value: 0x%04X\n", (unsigned)badOff);
                    fclose(fe);
                }
                LogDebug("[keyidentify] invalid or missing offset list - nothing was written");
                return 2;
            }

            // Before the first dialog, so an unattended check_dryrun_flags run
            // cannot hang on a MessageBox waiting for a click.
            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 per-key identify\n"
                                "DRY RUN - nothing was written and no question was asked.\n"
                                "Run without --dry-run to probe the hardware.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --keyidentify skipped - would write colour triples and open dialogs");
                return 0;
            }

            if (!AcquireProbeLock()) return ProbeLockBusy(reportPath, "keyidentify");

            hid_init();
            bool colorsComplete = false;
            int  colorBytes = 0;
            const std::vector<kblayout::Key> keys =
                ReadKeyLayoutStandalone(&colorsComplete, &colorBytes);

            FILE* fp = _wfopen(reportPath.c_str(), L"w");
            int  rc = 0;
            if (fp) {
                fprintf(fp, "EVision GK650 per-key identify\n");
                fprintf(fp, "layout read from the device: %d assigned matrix positions\n",
                        (int)keys.size());
                fprintf(fp, "colour table 0x%04X..0x%04X: %d of %d bytes readable%s\n",
                        (unsigned)kblayout::KEYCOLOR_BASE, (unsigned)(kblayout::KEYCOLOR_REGION_END - 1),
                        colorBytes,
                        (int)kblayout::KEYCOLOR_REGION_END - (int)kblayout::KEYCOLOR_BASE,
                        colorsComplete ? "" : "   <- SHORT, the assumed extent is not all readable");
                fprintf(fp, "\nOne triple is set to white at a time and restored right after the\n"
                            "question. 'predicted' is what the model derived from the remap table\n"
                            "expects at that offset; 'answer' is what a human saw. A dismissed\n"
                            "dialog is recorded as unanswered - never as no.\n\n");
                fprintf(fp, "  offset  slot(col,row)  predicted      written   answer\n");
                fprintf(fp, "  ------  -------------  -------------  --------  ------------\n");
                fflush(fp);
            }

            bool aborted = false;
            int  confirmed = 0, contradicted = 0;

            for (size_t i = 0; i < offsets.size() && !aborted; i++) {
                const uint16_t off = offsets[i];
                int col = -1, row = -1;
                const bool haveSlot = kblayout::KeyColorOffsetToSlot(off, col, row);
                const kblayout::Key* pred = PredictKeyForColorOffset(keys, off);
                // Three distinct cases, and they must not read alike in the
                // report: a key the model names, a matrix hole, and an offset
                // past the matrix altogether. The last one is the interesting
                // one - the region holds two more triples than the matrix has
                // slots (docs 5.5), and whatever lights up there is the answer
                // to what the extra triples are.
                const char* predName = pred      ? pred->label.c_str()
                                     : haveSlot  ? "(matrix hole)"
                                                 : "(past matrix)";
                char slotText[16];
                if (haveSlot) snprintf(slotText, sizeof(slotText), "(%2d,%d)", col, row);
                else          snprintf(slotText, sizeof(slotText), "( --,-)");

                std::vector<uint8_t> before;
                if (SnapshotEVisionRange(off, kblayout::KEYCOLOR_STRIDE, before)
                        != kblayout::KEYCOLOR_STRIDE) {
                    if (fp) fprintf(fp, "  0x%04X  %-13s  %-13s  SNAPSHOT FAILED - skipped\n",
                                    (unsigned)off, slotText, predName);
                    rc = 1;
                    continue;
                }

                const uint8_t white[3] = {0xFF, 0xFF, 0xFF};
                const bool written = WriteEVisionRangeVerified(off, white, 3);
                if (!written) rc = 1;

                wchar_t wpred[64] = {0};
                MultiByteToWideChar(CP_ACP, 0, predName, -1, wpred, 64);
                wchar_t wslot[32] = {0};
                MultiByteToWideChar(CP_ACP, 0, slotText, -1, wslot, 32);
                wchar_t msg[640];
                swprintf(msg, 640,
                         L"Offset 0x%04X, Matrixplatz %s steht jetzt auf WEISS.\n\n"
                         L"Erwartet wird die Taste: %s\n\n"
                         L"Leuchtet genau diese Taste weiss?\n\n"
                         L"Nein = eine andere (oder keine) Taste - bitte anschliessend im\n"
                         L"Report notieren, welche es war.\n"
                         L"Abbrechen beendet den Durchlauf und stellt alles wieder her.",
                         (unsigned)off, wslot, wpred);
                const int res = MessageBoxW(NULL, msg, L"OneClickRGB Tasten-Identify",
                                            MB_YESNOCANCEL | MB_ICONQUESTION |
                                            MB_SETFOREGROUND | MB_TOPMOST);

                const char* answer = "unbeantwortet";
                if      (res == IDYES) { answer = "ja";   confirmed++; }
                else if (res == IDNO)  { answer = "nein"; contradicted++; }
                else                   { aborted = true; }

                // Restored whether or not the question was answered, and whether
                // or not the write verified: a probe leaves the device the way it
                // found it, especially when it failed.
                const bool restored = WriteEVisionRangeVerified(off, before.data(), 3);
                if (!restored) rc = 1;

                if (fp) {
                    fprintf(fp, "  0x%04X  %-13s  %-13s  %-8s  %s%s\n",
                            (unsigned)off, slotText, predName,
                            written ? "verified" : "NO",
                            answer,
                            restored ? "" : "   <- RESTORE FAILED");
                    fprintf(fp, "          restored to %02X %02X %02X -> %s\n",
                            before[0], before[1], before[2], restored ? "verified" : "FAILED");
                    fflush(fp);
                }
            }

            if (fp) {
                if (aborted) {
                    rc = 2;
                    fprintf(fp, "\nABGEBROCHEN - die restlichen Offsets wurden nicht gestellt.\n"
                                "Der Report ist unvollstaendig und belegt nichts ueber sie.\n");
                } else if (confirmed > 0 && contradicted == 0) {
                    fprintf(fp, "\nVERDICT: %d von %d Stichproben bestaetigt, keine widerlegt.\n"
                                "Die aus der Remap-Tabelle abgeleitete Reihenfolge haelt an den\n"
                                "geprueften Stellen. Erst damit darf 0x2C0+ in\n"
                                "docs/Keyboard_Protocol.md von [MED] auf [HIGH] gehen - und nur\n"
                                "fuer die geprueften Offsets.\n", confirmed, (int)offsets.size());
                } else if (contradicted > 0) {
                    fprintf(fp, "\nVERDICT: %d Stichprobe(n) widersprechen der Vorhersage.\n"
                                "Das ist das wertvollere Ergebnis: die Reihenfolge slot -> Triple\n"
                                "ist so nicht richtig. Welche Taste stattdessen leuchtete, gehoert\n"
                                "hier hinein, bevor irgendein UI diese Zuordnung benutzt.\n",
                            contradicted);
                } else {
                    fprintf(fp, "\nVERDICT: keine einzige Frage wurde mit ja oder nein beantwortet -\n"
                                "es liegt keine Messung vor.\n");
                }
                fclose(fp);
            }

            hid_exit();
            ReleaseProbeLock();
            {
                char done[224];
                snprintf(done, sizeof(done),
                         "[keyidentify] rc=%d ja=%d nein=%d - report: "
                         "%%APPDATA%%\\OneClickRGB\\docs\\keyidentify.txt",
                         rc, confirmed, contradicted);
                LogDebug(done);
            }
            return rc;
        }
    }

    // --keypattern[=restore] : stage 3's test pattern.
    //
    // Finding the mode byte that makes the firmware render the per-key table
    // needs something in that table that is unmistakably per-key. A single
    // colour cannot show it - it looks exactly like the global colour, which is
    // how "the app already fills it" went unnoticed in the first place. So this
    // writes alternating red/blue over the assigned matrix positions and leaves
    // it there, and the mode walk (--kbmode-only=... --ask=perkey --confirm)
    // then has a question with a visible answer: are different keys different
    // colours, yes or no.
    //
    //   --keypattern           write the pattern, keep it, save what it replaced
    //   --keypattern=restore   write the saved bytes back, verified
    //
    // The backup is a file rather than an in-process snapshot because the two
    // halves are separate runs: the pattern has to survive while a different
    // process walks the mode bytes. Without the file, "restore" would have
    // nothing to restore to, and the table would keep the probe's pattern
    // forever - the exact failure the edge sweep had before it learned to roll
    // back.
    {
        const cli::Flag patFlag = cli::Find(lpCmdLine, "--keypattern");
        if (patFlag.present) {
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            const std::wstring reportPath = dir + L"\\keypattern.txt";
            const std::wstring backupPath = dir + L"\\keycolor_backup.bin";

            const bool restore = patFlag.hasValue && patFlag.value == "restore";
            if (patFlag.hasValue && !restore) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 per-key test pattern\n"
                                "ERROR: --keypattern takes no value, or the value 'restore'.\n"
                                "Nothing was written.\n");
                    fclose(fe);
                }
                LogDebug("[keypattern] unknown value - nothing was written");
                return 2;
            }

            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 per-key test pattern\n"
                                "DRY RUN - no write was sent and no value was read.\n"
                                "Run without --dry-run to probe the hardware.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --keypattern skipped - would write the per-key colour table");
                return 0;
            }

            if (!AcquireProbeLock()) return ProbeLockBusy(reportPath, "keypattern");

            hid_init();
            const int tableLen = (int)kblayout::KEYCOLOR_REGION_END - (int)kblayout::KEYCOLOR_BASE;
            int rc = 0;
            FILE* fp = _wfopen(reportPath.c_str(), L"w");

            if (restore) {
                std::vector<uint8_t> saved;
                FILE* bf = _wfopen(backupPath.c_str(), L"rb");
                if (bf) {
                    saved.resize((size_t)tableLen, 0);
                    const size_t got = fread(saved.data(), 1, (size_t)tableLen, bf);
                    fclose(bf);
                    saved.resize(got);
                }
                if (saved.empty()) {
                    if (fp) fprintf(fp, "EVision GK650 per-key test pattern\n"
                                        "ERROR: no backup at %%APPDATA%%\\OneClickRGB\\docs\\"
                                        "keycolor_backup.bin - nothing to restore, nothing written.\n");
                    if (fp) fclose(fp);
                    hid_exit();
                    ReleaseProbeLock();
                    LogDebug("[keypattern] restore: no backup file - nothing was written");
                    return 2;
                }
                int bad = -1;
                const bool ok = WriteEVisionRangeVerified(kblayout::KEYCOLOR_BASE,
                                                          saved.data(), (int)saved.size(), &bad);
                if (!ok) rc = 1;
                if (fp) {
                    fprintf(fp, "EVision GK650 per-key test pattern - RESTORE\n");
                    fprintf(fp, "wrote %d bytes back to 0x%04X -> %s\n",
                            (int)saved.size(), (unsigned)kblayout::KEYCOLOR_BASE,
                            ok ? "verified" : "FAILED (read-back differs)");
                    if (!ok && bad >= 0)
                        fprintf(fp, "first mismatch at 0x%04X\n", (unsigned)bad);
                }
            } else {
                bool colorsComplete = false;
                int  colorBytes = 0;
                const std::vector<kblayout::Key> keys =
                    ReadKeyLayoutStandalone(&colorsComplete, &colorBytes);

                std::vector<uint8_t> table;
                const int stableBytes =
                    SnapshotEVisionRange(kblayout::KEYCOLOR_BASE, tableLen, table);

                if (fp) {
                    fprintf(fp, "EVision GK650 per-key test pattern\n");
                    fprintf(fp, "layout: %d assigned matrix positions\n", (int)keys.size());
                    fprintf(fp, "colour table 0x%04X..0x%04X: %d of %d bytes readable, "
                                "%d stable across two reads\n",
                            (unsigned)kblayout::KEYCOLOR_BASE,
                            (unsigned)(kblayout::KEYCOLOR_REGION_END - 1), colorBytes, tableLen,
                            stableBytes);
                    if (stableBytes > 0 && stableBytes < tableLen)
                        fprintf(fp, "NOTE: the device answered for %d bytes, not the %d the matrix\n"
                                    "size predicts. Only the readable part is touched, and that\n"
                                    "shortfall is itself a measurement - record it in section 5.\n",
                                stableBytes, tableLen);
                }

                if (stableBytes <= 0 || keys.empty()) {
                    if (fp) fprintf(fp, "\nERROR: could not take a stable snapshot of the table (or the\n"
                                        "layout could not be read). Nothing was written - a pattern\n"
                                        "without a restore point is how a table stays patterned.\n");
                    if (fp) fclose(fp);
                    hid_exit();
                    ReleaseProbeLock();
                    LogDebug("[keypattern] no snapshot or no layout - nothing was written");
                    return 1;
                }

                // Save first, write second. The other order loses the original
                // colours if the process dies between the two.
                FILE* bf = _wfopen(backupPath.c_str(), L"wb");
                if (bf) { fwrite(table.data(), 1, table.size(), bf); fclose(bf); }
                else {
                    if (fp) fprintf(fp, "\nERROR: could not write the backup file - nothing written.\n");
                    if (fp) fclose(fp);
                    hid_exit();
                    ReleaseProbeLock();
                    LogDebug("[keypattern] backup file could not be written - nothing was written");
                    return 1;
                }

                // Read-modify-write: only the triples of assigned positions are
                // touched, everything else keeps the bytes the snapshot found
                // (rule 3). Alternating by slot index, so neighbouring keys
                // differ in every direction of the matrix.
                std::vector<uint8_t> patterned = table;
                int painted = 0;
                for (const kblayout::Key& k : keys) {
                    const int idx = (int)k.colorOffset - (int)kblayout::KEYCOLOR_BASE;
                    if (idx < 0 || idx + 2 >= (int)patterned.size()) continue;
                    const bool red = ((kblayout::SlotIndex(k.col, k.row) % 2) == 0);
                    patterned[(size_t)idx + 0] = red ? 0xFF : 0x00;
                    patterned[(size_t)idx + 1] = 0x00;
                    patterned[(size_t)idx + 2] = red ? 0x00 : 0xFF;
                    painted++;
                }

                int bad = -1;
                const bool ok = WriteEVisionRangeVerified(kblayout::KEYCOLOR_BASE,
                                                          patterned.data(),
                                                          (int)patterned.size(), &bad);
                if (!ok) rc = 1;
                if (fp) {
                    fprintf(fp, "\npattern: %d keys alternating red/blue, %d bytes written to 0x%04X\n",
                            painted, (int)patterned.size(), (unsigned)kblayout::KEYCOLOR_BASE);
                    fprintf(fp, "read-back: %s\n", ok ? "verified" : "FAILED (read-back differs)");
                    if (!ok && bad >= 0)
                        fprintf(fp, "first mismatch at 0x%04X - the table is shorter than assumed,\n"
                                    "or this region is not writable. That is a finding: note it in\n"
                                    "docs/Keyboard_Protocol.md section 5 item 5.\n", (unsigned)bad);
                    fprintf(fp, "backup of the previous %d bytes: "
                                "%%APPDATA%%\\OneClickRGB\\docs\\keycolor_backup.bin\n"
                                "Put it back with --keypattern=restore.\n", (int)table.size());
                    fprintf(fp, "\nThe pattern is stored. Whether it is RENDERED is the next\n"
                                "question - walk the non-animating mode candidates with\n"
                                "  --kbmode-only=0x00,0x04,0x09,0x13,0x14 --ask=perkey --confirm\n"
                                "and answer per step whether different keys show different colours.\n");
                }
            }

            if (fp) fclose(fp);
            hid_exit();
            ReleaseProbeLock();
            {
                char done[224];
                snprintf(done, sizeof(done),
                         "[keypattern] %s rc=%d - report: "
                         "%%APPDATA%%\\OneClickRGB\\docs\\keypattern.txt",
                         restore ? "restore" : "apply", rc);
                LogDebug(done);
            }
            return rc;
        }
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
    //
    // Flags are matched as whole tokens via cli::Find, not with strstr over the
    // command line. Two things that used to go wrong: anything merely starting
    // with the flag name ("--kbmode-sweep5" with the '=' left out) began a
    // 21-step sweep of writes into the keyboard's flash at the default hold time,
    // and giving both flags at once ran the sweep without mentioning that
    // --kbmode had been ignored - the sweep was simply tested first.
    {
        const cli::Flag sweepArg   = cli::Find(lpCmdLine, "--kbmode-sweep");
        const cli::Flag singleArg  = cli::Find(lpCmdLine, "--kbmode");
        const cli::Flag confirmArg = cli::Find(lpCmdLine, "--confirm");
        const cli::Flag onlyArg    = cli::Find(lpCmdLine, "--kbmode-only");
        const cli::Flag askArg     = cli::Find(lpCmdLine, "--ask");
        const cli::Flag colorArg   = cli::Find(lpCmdLine, "--kbcolor");
        if (cli::CountPresent({&sweepArg, &singleArg}) > 1) {
            LogDebug("[kbmode] --kbmode and --kbmode-sweep are mutually exclusive - nothing was written");
            return 2;
        }

        // --kbmode-only restricts the walk, so it only means anything together
        // with the walk. Ignoring it silently would run all 21 modes while the
        // user believes five were tested - the report would then look like a
        // measurement of something nobody asked for.
        if (onlyArg.present && !sweepArg.present) {
            LogDebug("[kbmode] --kbmode-only only applies to --kbmode-sweep - nothing was written");
            return 2;
        }

        // --kbcolor overrides the colour this probe writes, for exactly one
        // purpose: the differential dump that locates the per-key colour table.
        // Write colour A, dump, write colour B, dump - the bytes that moved with
        // the colour ARE the table, and its extent, stride and order fall out of
        // the diff instead of being inferred from a pattern. Refused on its own
        // for the same reason as --kbmode-only: a modifier that modifies nothing
        // would let the user believe a colour was set.
        uint8_t ovrR = 0, ovrG = 0, ovrB = 0;
        bool haveColorOverride = false;
        if (colorArg.present) {
            if (!sweepArg.present && !singleArg.present) {
                LogDebug("[kbmode] --kbcolor only applies to --kbmode/--kbmode-sweep - nothing was written");
                return 2;
            }
            if (!colorArg.hasValue || !cli::ParseRgb(colorArg.value, ovrR, ovrG, ovrB)) {
                LogDebug("[kbmode] --kbcolor needs exactly six hex digits (RRGGBB) - nothing was written");
                return 2;
            }
            haveColorOverride = true;
        }

        // Which question --confirm asks. A malformed --ask is refused rather
        // than defaulted: the whole value of these dialogs is that the answer
        // belongs to the question that was on screen, and quietly substituting a
        // different question destroys exactly that (rule 1).
        cli::AskKind askKind = cli::ASK_MOTION;
        if (!cli::ResolveAsk(askArg, askKind)) {
            LogDebug("[kbmode] --ask needs motion|lit|perkey - nothing was written");
            return 2;
        }

        // The bytes the sweep walks. Default is the full 0x00..0x14 range; with
        // --kbmode-only it is exactly the listed values, in the given order.
        std::vector<uint8_t> sweepModes;
        if (onlyArg.present) {
            if (!onlyArg.hasValue || !cli::ParseByteList(onlyArg.value, sweepModes, 64)) {
                LogDebug("[kbmode] --kbmode-only needs a comma-separated byte list - nothing was written");
                return 2;
            }
        } else {
            for (int m = 0x00; m <= 0x14; m++) sweepModes.push_back((uint8_t)m);
        }

        if (sweepArg.present || singleArg.present) {
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

            if (!AcquireProbeLock())
                return ProbeLockBusy(GetAppDataPath() + L"\\docs\\kbmode_probe.txt", "kbmode");

            LoadSettings();

            const uint8_t pr = haveColorOverride ? ovrR : g_state.red;
            const uint8_t pg = haveColorOverride ? ovrG : g_state.green;
            const uint8_t pb = haveColorOverride ? ovrB : g_state.blue;
            // Full brightness so a working effect is unmistakable, and a non-zero
            // speed because an animation at speed 0 does not visibly move.
            const uint8_t pbright = 4;
            const uint8_t pspeed  = g_state.speed ? g_state.speed : (uint8_t)2;

            const int holdMs = cli::HoldSeconds(sweepArg, 5, 1, 60) * 1000;

            // Snapshot before the first write. Read-only, so it also answers
            // "does the device respond at all" before anything is sent - and it
            // is what the rollback at the end restores. Each probe step below
            // does its own hid_init/hid_exit, so the snapshot and the restore
            // bracket themselves the same way instead of nesting.
            uint8_t  kbBefore[18] = {0};
            uint8_t  snapProfile  = 0;
            uint16_t snapOffset   = 0;
            hid_init();
            const bool haveSnapshot = ReadEVisionKeyboardPayload(kbBefore, &snapProfile, &snapOffset);
            hid_exit();

            // Only the sweep rolls back. --kbmode=<n> means "put the keyboard on
            // this mode and leave it there", so the header must not promise a
            // restore that never happens - a report that claims an action it did
            // not take is the same defect as a status line claiming an unverified
            // write succeeded.
            const bool willRestore = sweepArg.present && haveSnapshot;

            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            FILE* fp = _wfopen((dir + L"\\kbmode_probe.txt").c_str(), L"w");

            if (fp) {
                fprintf(fp, "EVision GK650 keyboard mode probe\n");
                fprintf(fp, "colour=%02X%02X%02X%s brightness=%d speed=%d hold=%dms\n",
                        pr, pg, pb,
                        haveColorOverride ? " (--kbcolor, not the saved colour)" : "",
                        pbright, pspeed, holdMs);
                if (haveSnapshot) {
                    fprintf(fp, "active profile=%d  keyboard block=0x%02X\n",
                            (int)snapProfile, (unsigned)snapOffset);
                    fprintf(fp, "block before probe:");
                    for (int i = 0; i < 18; i++) fprintf(fp, " %02X", kbBefore[i]);
                    fprintf(fp, "%s\n", willRestore
                            ? "   (restored at the end)"
                            : "   (single mode - NOT restored, see below)");
                } else {
                    fprintf(fp, "WARNING: could not read the keyboard block before starting -\n"
                                "         no snapshot, so nothing will be restored afterwards.\n");
                }
                fprintf(fp, "'got' columns are read back from the device after the write.\n");
                if (sweepArg.present) {
                    fprintf(fp, "modes walked (%d):", (int)sweepModes.size());
                    for (size_t si = 0; si < sweepModes.size(); si++)
                        fprintf(fp, " 0x%02X", sweepModes[si]);
                    fprintf(fp, "%s\n", onlyArg.present ? "   (--kbmode-only)" : "   (default range)");
                }
                // Which question was asked belongs in the report, not just in
                // the command line: an answer only means something together
                // with the question it answered.
                fprintf(fp, "question asked per step: %s\n\n",
                        askKind == cli::ASK_PERKEY
                            ? "perkey - are DIFFERENT keys showing DIFFERENT colours?"
                        : askKind == cli::ASK_LIT
                            ? "lit - is the keyboard lit at all?"
                            : "motion - does anything move?");
                fprintf(fp, "%s\n",
                        confirmArg.present
                            ? "The animated? column is answered per step in a dialog, while the\n"
                              "mode is still on the keyboard."
                            : "The animated? column has to be filled in by hand afterwards -\n"
                              "use --confirm to be asked per step instead.");
                fprintf(fp, "  t[s]  mode  writeRes readRes  got:mode br sp  rgb       "
                            "verdict      animated?\n");
                fprintf(fp, "  ----  ----  -------- -------  -------- --  --  --------  "
                            "-----------  -------------\n");
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

            int rc = 0;

            // --confirm turns the hold time into a question, exactly as in the
            // edge probe: the state stays on the keyboard while the dialog asks
            // about it, so nothing has to be reconstructed from a t[s] column
            // afterwards. Without it the run behaves as before.
            const bool confirm = confirmArg.present;
            bool sweepAborted  = false;

            // A REJECTED row is a *result*, not an error - which mode bytes the
            // firmware refuses is exactly what this probe is for. A failed
            // read-back is an error: then the step measured nothing, and the exit
            // code has to say so instead of letting a gap-filled report look
            // finished. Same split as the edge probe below.
            auto probeOne = [&](uint8_t mode, int tSec) {
                hid_init();
                SetEVisionKeyboard(pr, pg, pb, mode, pbright, pspeed);
                hid_exit();

                const KbVerifyResult& v = g_lastKbVerify;
                const char* verdict = !v.valid          ? "READ FAILED"
                                    : v.got[0] != mode  ? "REJECTED"
                                    : "accepted";
                if (!v.valid) rc = 1;

                // Asked while this mode is still on the keyboard. A dismissed
                // dialog is "abgebrochen", never "nein" (rule 1).
                const char* answer = "______";
                if (confirm) {
                    // --ask picks the question. "Does anything move?" is the
                    // right question for an animation sweep and the wrong one
                    // for the per-key hunt, where the modes under test are
                    // precisely the ones that do NOT animate - answering "nein"
                    // to motion there says nothing about whether the colour
                    // table renders.
                    const wchar_t* question =
                        askKind == cli::ASK_PERKEY
                            ? L"Zeigen jetzt VERSCHIEDENE Tasten VERSCHIEDENE Farben?"
                        : askKind == cli::ASK_LIT
                            ? L"Leuchtet die Tastatur jetzt ueberhaupt?"
                            : L"Bewegt sich an der Tastaturbeleuchtung etwas?";
                    wchar_t msg[640];
                    swprintf(msg, 640,
                             L"Modus 0x%02X ist jetzt gesetzt.\n\n"
                             L"%s\n\n"
                             L"Abbrechen beendet den Sweep und stellt den vorherigen\n"
                             L"Zustand wieder her.",
                             (unsigned)mode, question);
                    const int res = MessageBoxW(NULL, msg, L"OneClickRGB Tastatur-Sweep",
                                                MB_YESNOCANCEL | MB_ICONQUESTION |
                                                MB_SETFOREGROUND | MB_TOPMOST);
                    if      (res == IDYES) answer = "ja";
                    else if (res == IDNO)  answer = "nein";
                    else { answer = "abgebrochen"; sweepAborted = true; }
                }

                if (fp) {
                    fprintf(fp, "  %4d  0x%02X  %8d %7d      0x%02X %2d  %2d  %02X%02X%02X    "
                                "%-11s  %s\n",
                            tSec, mode, v.writeRes, v.readRes,
                            v.got[0], v.got[1], v.got[2],
                            v.got[5], v.got[6], v.got[7], verdict, answer);
                    fflush(fp);
                }
                dumpBlock(v);
            };

            if (sweepArg.present) {
                // Default 0x00..0x14 rather than just the 11 table entries: if
                // Breathing is not 0x05, the real value is most likely a
                // neighbour that the table never lists. --kbmode-only narrows
                // that to a named set - five candidates instead of twenty-one is
                // the difference between a question someone answers and a
                // question someone abandons halfway.
                int tSec = 0;
                for (size_t si = 0; si < sweepModes.size(); si++) {
                    probeOne(sweepModes[si], tSec);
                    if (sweepAborted) break;
                    if (!confirm) Sleep(holdMs);   // --confirm waits on the dialog
                    tSec += holdMs / 1000;
                }
                if (fp && !confirm) {
                    fprintf(fp, "\nWatch the keyboard while this runs and note the wall-clock\n"
                                "second at which it animates; the t[s] column maps that back to\n"
                                "the mode byte. 'accepted' only means the byte was stored - it\n"
                                "does not prove the effect renders.\n");
                }
            } else {
                // Strict parse: strtol() used to turn "--kbmode=banana" into 0
                // and then probe mode 0x00 while the report claimed the user's
                // request. A bad argument now says so and writes nothing.
                uint8_t m = 0;
                if (!singleArg.hasValue || !cli::ParseByte(singleArg.value, m)) {
                    if (fp) {
                        fprintf(fp, "  ERROR: --kbmode needs a value 0..255 "
                                    "(decimal or 0x-hex). Nothing was written.\n");
                        fclose(fp);
                    }
                    LogDebug("[kbmode] --kbmode: invalid or missing value - nothing was written");
                    return 2;
                }
                probeOne(m, 0);
            }

            if (sweepAborted) {
                rc = 2;
                if (fp) fprintf(fp, "\nABGEBROCHEN - die restlichen Modi wurden nicht gestellt.\n"
                                    "Der Report ist unvollstaendig und belegt nichts ueber sie.\n");
            }

            // Put the keyboard back the way it was found. Same 18 bytes, same
            // offset, verified by read-back - a diagnostic that changes state it
            // was only supposed to measure is how the edge strip ended up parked
            // on an undocumented mode after the earlier edge-diagnose run.
            if (willRestore) {
                hid_init();
                const bool restored = RestoreEVisionKeyboardPayload(kbBefore);
                hid_exit();
                if (fp) {
                    fprintf(fp, "\nrestore of the pre-probe block:");
                    for (int i = 0; i < 18; i++) fprintf(fp, " %02X", kbBefore[i]);
                    fprintf(fp, "  -> %s\n", restored ? "verified" : "FAILED (read-back differs)");
                }
                if (!restored) rc = 1;
            } else if (singleArg.present && fp) {
                fprintf(fp, "\nSingle mode set - not restored (that is the point of\n"
                            "--kbmode=<n>). Run an apply to return to the UI state.\n");
            }

            if (fp) fclose(fp);
            ReleaseProbeLock();
            {
                char done[256];
                snprintf(done, sizeof(done),
                         "[kbmode] rc=%d - report: %%APPDATA%%\\OneClickRGB\\docs\\kbmode_probe.txt",
                         rc);
                LogDebug(done);
            }
            return rc;
        }
    }

    // Live EDGE probe - the mirror image of the --kbmode block above, for the
    // edge payload at profile_base+0x1E instead of the keyboard block at +0x01.
    //
    // Why it has to exist: the edge strip takes colour and static/off, but no
    // effect moves - and the mode byte writes, reads back and matches. Under
    // CLAUDE.md rule 1 a matching read-back proves only that the firmware
    // *stored* the byte, never that it *renders* it. Distinguishing the two
    // needs a human watching a light while something walks the byte values, and
    // --kbmode/--kbmode-sweep cannot do it: they address +0x01.
    //
    //   --edgemode=<n>            write one mode (decimal or 0x-hex), verify, exit
    //   --edgemode-sweep[=sec]    walk 0x00..0x0A, hold each <sec> (default 5)
    //   --edgespeed-sweep[=sec]   hold one animated mode, walk speed 0..5
    //   --edgespeed-mode=<n>      which mode --edgespeed-sweep holds (default 0x03)
    //
    // Report: %APPDATA%\OneClickRGB\docs\edgemode_probe.txt
    //
    // Rule 2 stays intact: the only bytes written are the ten documented ones at
    // profile_base+0x1E, through SetEVisionEdge - the production path. Only the
    // *values* vary, never the offsets. The payload found before the first step
    // is restored afterwards, so a probe run does not leave the strip parked on
    // whatever byte happened to come last (which is how the strip ended up on
    // the undocumented mode 0x04 after the earlier edge-diagnose run).
    {
        const cli::Flag oneArg    = cli::Find(lpCmdLine, "--edgemode");
        const cli::Flag modeSweep = cli::Find(lpCmdLine, "--edgemode-sweep");
        const cli::Flag spdSweep  = cli::Find(lpCmdLine, "--edgespeed-sweep");
        const cli::Flag spdMode   = cli::Find(lpCmdLine, "--edgespeed-mode");
        const cli::Flag confirmArg = cli::Find(lpCmdLine, "--confirm");
        const cli::Flag askArg     = cli::Find(lpCmdLine, "--ask");
        const int actions = cli::CountPresent({&oneArg, &modeSweep, &spdSweep});

        if (actions > 0) {
            // Same modifier as on the keyboard path, and refused the same way
            // when it is malformed: a dialog that asks a different question than
            // the one the user selected collects an answer to nothing.
            cli::AskKind askKind = cli::ASK_MOTION;
            if (!cli::ResolveAsk(askArg, askKind)) {
                LogDebug("[edgeprobe] --ask needs motion|lit|perkey - nothing was written");
                return 2;
            }
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            const std::wstring reportPath = dir + L"\\edgemode_probe.txt";

            // Mutually exclusive actions are refused, not ranked. cli::Find
            // matches whole tokens, so --edgemode= and --edgemode-sweep cannot
            // catch each other the way substring matching would; if both are
            // given anyway, that is a mistake worth naming rather than resolving
            // by precedence behind the user's back.
            if (actions > 1) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 edge probe\n"
                                "ERROR: --edgemode / --edgemode-sweep / --edgespeed-sweep are\n"
                                "mutually exclusive. Nothing was written.\n");
                    fclose(fe);
                }
                LogDebug("[edgeprobe] more than one action flag given - nothing was written");
                return 2;
            }

            // Dry run bails out here, before the first sleep. The old kbmode
            // probe walked all its steps under --dry-run, slept the full hold
            // time each and filled the report with READ FAILED rows for writes
            // it had never attempted. Nothing was tried, so the report says
            // exactly that and the process exits at once.
            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 edge probe\n"
                                "DRY RUN - no write was sent and no value was read.\n"
                                "Run without --dry-run to probe the hardware.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --edgemode/--edgemode-sweep/--edgespeed-sweep skipped - "
                         "would write the edge payload to keyboard flash");
                return 0;
            }

            if (!AcquireProbeLock()) return ProbeLockBusy(reportPath, "edgeprobe");

            LoadSettings();

            const uint8_t pr = g_state.red, pg = g_state.green, pb = g_state.blue;

            // Fixed, not taken from the saved settings: a probe whose parameters
            // depend on whatever the user last had in the UI is not reproducible.
            // Brightness 4 so a running effect is unmistakable, speed 2 as a
            // mid-range value that means "moving" whichever way the polarity of
            // +0x20 turns out to run - that polarity is what --edgespeed-sweep
            // is here to settle.
            const uint8_t pbright = EFFECT_BRIGHTNESS_MAX;
            const uint8_t pspeed  = 2;

            int holdMs = 5000;
            if (modeSweep.present) holdMs = cli::HoldSeconds(modeSweep, 5, 1, 60) * 1000;
            if (spdSweep.present)  holdMs = cli::HoldSeconds(spdSweep,  5, 1, 60) * 1000;

            hid_init();

            // Snapshot first - read-only, so it also tells us whether the device
            // answers at all before any write is attempted.
            uint8_t before[10] = {0};
            uint8_t snapProfile = 0;
            uint16_t snapOffset = 0;
            const bool haveSnapshot = ReadEVisionEdgePayload(before, &snapProfile, &snapOffset);

            // Only the sweeps roll back; --edgemode=<n> is meant to leave the
            // strip where it put it. The header must not promise otherwise -
            // a report that claims a restore that never happens is the same
            // defect as a status line claiming an unverified write succeeded.
            const bool willRestore = (modeSweep.present || spdSweep.present) && haveSnapshot;

            FILE* fp = _wfopen(reportPath.c_str(), L"w");
            if (fp) {
                fprintf(fp, "EVision GK650 edge mode probe (payload at profile_base+0x1E)\n");
                // During a speed sweep the speed is the variable, so naming one
                // fixed value here would describe a run that did not happen. The
                // per-step spd column is the truth either way.
                if (spdSweep.present) {
                    fprintf(fp, "colour=%02X%02X%02X brightness=%d speed=sweep 0..%d hold=%dms\n",
                            pr, pg, pb, pbright, (int)EFFECT_SPEED_MAX, holdMs);
                } else {
                    fprintf(fp, "colour=%02X%02X%02X brightness=%d speed=%d hold=%dms\n",
                            pr, pg, pb, pbright, pspeed, holdMs);
                }
                if (haveSnapshot) {
                    fprintf(fp, "active profile=%d  edge slot=0x%02X\n",
                            (int)snapProfile, (unsigned)snapOffset);
                    fprintf(fp, "payload before probe:");
                    for (int i = 0; i < 10; i++) fprintf(fp, " %02X", before[i]);
                    fprintf(fp, "%s\n", willRestore
                            ? "   (restored at the end)"
                            : "   (single mode - NOT restored, see below)");
                } else {
                    fprintf(fp, "WARNING: could not read the edge slot before starting -\n"
                                "         no snapshot, so nothing will be restored afterwards.\n");
                }
                fprintf(fp, "\nAll 'got' bytes are read back from the device after the write.\n"
                            "Layout: [mode bright speed dir rand R G B coloff save]\n"
                            "The verdict only says whether the firmware STORED the payload.\n"
                            "%s\n\n",
                        confirmArg.present
                            ? "The animated? column is answered per step in a dialog, while the\n"
                              "state is still on the strip."
                            : "Whether the strip animates is the column you fill in yourself.");
                fprintf(fp, "  t[s]  mode  spd  wRes rRes  got[0..9]                        "
                            "verdict      confidence   animated?\n");
                fprintf(fp, "  ----  ----  ---  ---- ----  -------------------------------  "
                            "-----------  -----------  -------------\n");
                fflush(fp);
            }

            int rc = 0;

            // --confirm turns the hold time into a question. Why it exists: an
            // animated? column that a human is meant to fill in afterwards stays
            // empty - the run on 2026-08-17 wrote all eleven modes flawlessly and
            // measured nothing, because the protocol asked for 55 seconds of
            // uninterrupted watching plus a seconds-to-byte reconstruction. The
            // dialog holds the state while the question is on screen, so there is
            // nothing to reconstruct and nothing to remember.
            const bool confirm = confirmArg.present;
            bool sweepAborted  = false;

            // One step: drive the production path, then report what came back.
            // A REJECTED row is a *result*, not an error - finding out which mode
            // bytes the firmware refuses is the point. A failed read-back is an
            // error though: then the probe measured nothing, and the exit code
            // has to say so instead of letting an incomplete report look finished.
            auto probeStep = [&](uint8_t mode, uint8_t spd, int tSec) {
                SetEVisionEdge(pr, pg, pb, mode, pbright, spd);

                const EdgeVerifyResult& v = g_lastEdgeVerify;
                // Brightness is forced to 0 for OFF by the production path, so
                // compare against what it actually intended to write, not against
                // the probe's own parameters.
                bool stored = v.valid &&
                              v.got[0] == v.want[0] && v.got[1] == v.want[1] &&
                              v.got[2] == v.want[2] && v.got[5] == v.want[5] &&
                              v.got[6] == v.want[6] && v.got[7] == v.want[7];
                const char* verdict = !v.valid ? "READ FAILED" : (stored ? "accepted" : "REJECTED");
                if (!v.valid) rc = 1;
                const char* conf = (EdgeModeConfidenceOf(mode) == EDGE_CONF_RENDER_SEEN)
                                 ? "seen"      // already observed rendering
                                 : "stored?";  // never observed rendering

                // Asked while this state is still showing, never afterwards. A
                // dismissed dialog is "abgebrochen", not "nein" - an answer
                // nobody gave must not become a measurement (rule 1).
                const char* answer = "______";
                if (confirm) {
                    const wchar_t* question =
                        askKind == cli::ASK_PERKEY
                            ? L"Zeigen jetzt VERSCHIEDENE Stellen VERSCHIEDENE Farben?"
                        : askKind == cli::ASK_LIT
                            ? L"Leuchtet die Randbeleuchtung jetzt ueberhaupt?"
                            : L"Bewegt sich an der Randbeleuchtung etwas?";
                    wchar_t msg[640];
                    swprintf(msg, 640,
                             L"Modus 0x%02X, Tempo %d ist jetzt gesetzt.\n\n"
                             L"%s\n\n"
                             L"Abbrechen beendet den Sweep und stellt den vorherigen\n"
                             L"Zustand wieder her.",
                             (unsigned)mode, (int)spd, question);
                    const int res = MessageBoxW(NULL, msg, L"OneClickRGB Edge-Sweep",
                                                MB_YESNOCANCEL | MB_ICONQUESTION |
                                                MB_SETFOREGROUND | MB_TOPMOST);
                    if      (res == IDYES) answer = "ja";
                    else if (res == IDNO)  answer = "nein";
                    else { answer = "abgebrochen"; sweepAborted = true; }
                }

                if (fp) {
                    fprintf(fp, "  %4d  0x%02X  %3d  %4d %4d ", tSec, mode, spd,
                            v.writeRes, v.readRes);
                    for (int i = 0; i < 10; i++) fprintf(fp, " %02X", v.got[i]);
                    fprintf(fp, "  %-11s  %-11s  %-13s  (%s)\n",
                            verdict, conf, answer, EdgeModeName(mode));
                    if (v.valid && !stored) {
                        for (int i = 0; i < 10; i++)
                            if (v.got[i] != v.want[i])
                                fprintf(fp, "        byte[%d]: want %02X, got %02X\n",
                                        i, v.want[i], v.got[i]);
                    }
                    fflush(fp);
                }
            };

            if (modeSweep.present) {
                // 0x00..0x0A: the four documented values plus their neighbours.
                // Only 0x00 and 0x05 have ever been seen on the strip; if
                // Breathing is not 0x03, the real byte is most likely nearby.
                int tSec = 0;
                for (uint8_t m = 0x00; m <= 0x0A; m++) {
                    probeStep(m, pspeed, tSec);
                    if (sweepAborted) break;
                    if (!confirm) Sleep(holdMs);   // --confirm waits on the dialog
                    tSec += holdMs / 1000;
                }
                if (fp && !confirm) {
                    fprintf(fp, "\nWatch the strip while this runs and note the wall-clock second\n"
                                "at which it starts moving; the t[s] column maps that back to the\n"
                                "mode byte. 'accepted' means stored, nothing more.\n");
                }
            } else if (spdSweep.present) {
                // Which mode to hold while the speed byte walks. Default 0x03
                // (Breathing) because it is the one the user reported as frozen.
                uint8_t holdMode = EDGE_MODE_BREATHING;
                if (spdMode.present) {
                    if (!spdMode.hasValue || !cli::ParseByte(spdMode.value, holdMode)) {
                        if (fp) {
                            fprintf(fp, "  ERROR: --edgespeed-mode needs a value 0..255 "
                                        "(decimal or 0x-hex). Nothing was written.\n");
                            fclose(fp);
                        }
                        LogDebug("[edgeprobe] --edgespeed-mode: invalid value - nothing was written");
                        hid_exit();
                        return 2;
                    }
                }
                if (fp) fprintf(fp, "  (holding mode 0x%02X while speed walks 0..%d)\n",
                                holdMode, (int)EFFECT_SPEED_MAX);
                int tSec = 0;
                for (uint8_t s = 0; s <= EFFECT_SPEED_MAX; s++) {
                    probeStep(holdMode, s, tSec);
                    if (sweepAborted) break;
                    if (!confirm) Sleep(holdMs);   // --confirm waits on the dialog
                    tSec += holdMs / 1000;
                }
                if (fp) {
                    fprintf(fp, "\nThis settles the open question about +0x20: the comment in\n"
                                "SetEVisionKeyboard claimed speed was inverted while the kbmode\n"
                                "probe next to it assumed the opposite, and neither had been\n"
                                "checked against a moving light. Note for each row whether the\n"
                                "strip moves and how fast - if 0 is the fast end, the slider is\n"
                                "inverted and Beast.rgb's speed=0 was never 'stopped' at all.\n");
                }
            } else {
                uint8_t m = 0;
                if (!oneArg.hasValue || !cli::ParseByte(oneArg.value, m)) {
                    if (fp) {
                        fprintf(fp, "  ERROR: --edgemode needs a value 0..255 "
                                    "(decimal or 0x-hex). Nothing was written.\n");
                        fclose(fp);
                    }
                    LogDebug("[edgeprobe] --edgemode: invalid or missing value - nothing was written");
                    hid_exit();
                    return 2;
                }
                probeStep(m, pspeed, 0);
                // A single --edgemode is an explicit "put the strip on this mode
                // and leave it there", so it keeps what it set instead of being
                // rolled back. Only the sweeps restore.
                if (fp) fprintf(fp, "\nSingle mode set - not restored (that is the point of\n"
                                    "--edgemode=<n>). Run an apply to return to the UI state.\n");
                if (!g_lastEdgeVerify.valid || g_lastEdgeVerify.got[0] != m) rc = 1;
            }

            if (sweepAborted) {
                rc = 2;
                if (fp) fprintf(fp, "\nABGEBROCHEN - die restlichen Modi wurden nicht gestellt.\n"
                                    "Der Report ist unvollstaendig und belegt nichts ueber sie.\n");
            }

            // Put the strip back the way it was found. Same offset, same ten
            // bytes, verified by read-back - a diagnostic that changes state it
            // was only supposed to measure is how the previous run left the strip
            // on an undocumented mode. An aborted run needs this most of all.
            if (willRestore) {
                const bool restored = RestoreEVisionEdgePayload(before);
                if (fp) {
                    fprintf(fp, "\nrestore of the pre-probe payload:");
                    for (int i = 0; i < 10; i++) fprintf(fp, " %02X", before[i]);
                    fprintf(fp, "  -> %s\n", restored ? "verified" : "FAILED (read-back differs)");
                }
                if (!restored) rc = 1;
            }

            if (fp) fclose(fp);
            hid_exit();
            ReleaseProbeLock();
            {
                char done[256];
                snprintf(done, sizeof(done),
                         "[edgeprobe] rc=%d - report: %%APPDATA%%\\OneClickRGB\\docs\\edgemode_probe.txt",
                         rc);
                LogDebug(done);
            }
            return rc;
        }
    }

    // Anchor probe: does the firmware render this block AT ALL?
    //
    //   --rendercheck=edge    four held states on the edge payload (+0x1E)
    //   --rendercheck=kb      the same on the keyboard block (+0x01)
    //
    // Why this exists. Every measurement so far produced a technically flawless
    // report - written, read back byte-identical, restored, exit 0 - and settled
    // nothing, because the only column that carries the answer ("did the light
    // change?") is the one no program can fill. The protocol asked a human to
    // watch a strip for 55 seconds straight and afterwards map wall-clock
    // seconds back to byte values through a t[s] column. Nobody measures
    // reliably that way, and an empty animated? column is what it produced.
    //
    // So this probe inverts the arrangement: it holds ONE state, asks ONE
    // yes/no question about exactly that state while it is still showing, and
    // writes the answer into the report itself. Four steps are enough to decide
    // whether this memory drives the lighting at all:
    //
    //   white -> "is it white?"    off   -> "is it dark?"
    //   red   -> "is it red?"      green -> "is it green?"
    //
    // If off does not darken and no colour arrives, the block is storage rather
    // than state, and every further mode sweep is wasted - that is the branch
    // this probe exists to decide, and it prints the verdict itself.
    //
    // A dismissed dialog is recorded as "unanswered", never as "no": an answer
    // nobody gave must not turn into a measurement (CLAUDE.md rule 1).
    //
    // Rule 2 is untouched - the states go out through SetEVisionEdge /
    // SetEVisionKeyboard, so only documented payload bytes move, and the
    // pre-probe payload is snapshotted and restored exactly like the sweeps do.
    //
    // Dialog strings are deliberately ASCII-only: this file has no BOM and the
    // build passes no /utf-8, so raw UTF-8 bytes inside L"..." would be decoded
    // as CP1252 and reach the user as mojibake.
    {
        const cli::Flag rcFlag = cli::Find(lpCmdLine, "--rendercheck");
        if (rcFlag.present) {
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);

            const bool wantEdge = rcFlag.hasValue && rcFlag.value == "edge";
            const bool wantKb   = rcFlag.hasValue && rcFlag.value == "kb";

            // One report per target, not one shared file. Two runs writing the
            // same path interleaved their output into a single unreadable
            // report on 2026-08-17 - a header from one run above rows and a
            // restore line from the other. Separate names also mean the edge
            // result survives when the keyboard run follows it.
            const std::wstring reportPath =
                dir + (wantEdge ? L"\\rendercheck_edge.txt"
                     : wantKb   ? L"\\rendercheck_kb.txt"
                                : L"\\rendercheck.txt");

            if (!wantEdge && !wantKb) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 render check\n"
                                "ERROR: --rendercheck needs a target - use --rendercheck=edge\n"
                                "or --rendercheck=kb. Nothing was written.\n");
                    fclose(fe);
                }
                LogDebug("[rendercheck] missing or unknown target - nothing was written");
                return 2;
            }

            // Must bail out BEFORE the first dialog: check_dryrun_flags.ps1 runs
            // unattended, and a MessageBox waiting for a click would hang it
            // until the test's timeout instead of failing honestly.
            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "EVision GK650 render check\n"
                                "DRY RUN - nothing was written and no question was asked.\n"
                                "Run without --dry-run to probe the hardware.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --rendercheck skipped - would write the payload and open dialogs");
                return 0;
            }

            // Exclusivity before the first HID access - without it the
            // read-backs below are not evidence about our own writes.
            if (!AcquireProbeLock()) return ProbeLockBusy(reportPath, "rendercheck");

            LoadSettings();
            hid_init();

            uint8_t  edgeBefore[10] = {0};
            uint8_t  kbBefore[18]   = {0};
            uint8_t  snapProfile    = 0;
            uint16_t snapOffset     = 0;
            const bool haveSnapshot = wantEdge
                ? ReadEVisionEdgePayload(edgeBefore, &snapProfile, &snapOffset)
                : ReadEVisionKeyboardPayload(kbBefore, &snapProfile, &snapOffset);

            const char* targetName = wantEdge ? "edge" : "keyboard";
            const wchar_t* targetW = wantEdge ? L"Randbeleuchtung (Edge)" : L"Tastaturbeleuchtung";

            FILE* fp = _wfopen(reportPath.c_str(), L"w");
            if (fp) {
                fprintf(fp, "EVision GK650 render check - target: %s\n", targetName);
                if (haveSnapshot) {
                    fprintf(fp, "active profile=%d  slot=0x%02X\n",
                            (int)snapProfile, (unsigned)snapOffset);
                    fprintf(fp, "payload before probe:");
                    const int n = wantEdge ? 10 : 18;
                    const uint8_t* src = wantEdge ? edgeBefore : kbBefore;
                    for (int i = 0; i < n; i++) fprintf(fp, " %02X", src[i]);
                    fprintf(fp, "   (restored at the end)\n");
                } else {
                    fprintf(fp, "WARNING: could not read the slot before starting - no\n"
                                "         snapshot, so nothing will be restored afterwards.\n");
                }
                fprintf(fp, "\nEach step holds ONE state and asks ONE question about it while\n"
                            "that state is still showing. 'verified' is the read-back from the\n"
                            "device; 'answer' is what a human saw. A dismissed dialog is\n"
                            "recorded as unanswered - never as no.\n\n");
                fprintf(fp, "  step  state  verified  answer\n");
                fprintf(fp, "  ----  -----  --------  ------------\n");
                fflush(fp);
            }

            struct RcStep {
                const wchar_t* question;
                const char*    label;
                uint8_t r, g, b;
                uint8_t bright;
                bool    dark;     // edge: EDGE_MODE_OFF, keyboard: brightness 0
            };
            const RcStep steps[4] = {
                { L"Leuchtet es jetzt WEISS (hell)?", "white", 255, 255, 255, 4, false },
                { L"Ist es jetzt AUS (dunkel)?",      "off",     0,   0,   0, 0, true  },
                { L"Leuchtet es jetzt ROT?",          "red",   255,   0,   0, 4, false },
                { L"Leuchtet es jetzt GRUEN?",        "green",   0, 255,   0, 4, false },
            };

            int  rc        = 0;
            bool aborted   = false;
            // 0 = unanswered, 1 = yes, 2 = no. Index matches steps[].
            int  answers[4] = {0, 0, 0, 0};

            for (int i = 0; i < 4 && !aborted; i++) {
                const RcStep& s = steps[i];

                bool verified;
                if (wantEdge) {
                    const uint8_t mode = s.dark ? (uint8_t)EDGE_MODE_OFF
                                                : (uint8_t)EDGE_MODE_STATIC;
                    SetEVisionEdge(s.r, s.g, s.b, mode, s.bright, 2);
                    verified = g_lastEdgeVerify.valid && g_lastEdgeVerify.writeRes >= 0 &&
                               g_lastEdgeVerify.got[0] == g_lastEdgeVerify.want[0];
                } else {
                    SetEVisionKeyboard(s.r, s.g, s.b, (uint8_t)KB_MODE_STATIC, s.bright, 2);
                    verified = g_lastKbVerify.valid &&
                               g_lastKbVerify.got[0] == (uint8_t)KB_MODE_STATIC &&
                               g_lastKbVerify.got[1] == s.bright;
                }
                if (!verified) rc = 1;

                wchar_t msg[512];
                swprintf(msg, 512,
                         L"Schritt %d von 4 - %s\n\n"
                         L"%s\n\n"
                         L"Ja / Nein beantworten. Abbrechen beendet den Test und stellt\n"
                         L"den vorherigen Zustand wieder her.",
                         i + 1, targetW, s.question);

                const int res = MessageBoxW(NULL, msg, L"OneClickRGB Render-Check",
                                            MB_YESNOCANCEL | MB_ICONQUESTION |
                                            MB_SETFOREGROUND | MB_TOPMOST);
                if      (res == IDYES) answers[i] = 1;
                else if (res == IDNO)  answers[i] = 2;
                else { aborted = true; answers[i] = 0; }

                if (fp) {
                    const char* ans = answers[i] == 1 ? "ja"
                                    : answers[i] == 2 ? "nein"
                                    : "unbeantwortet";
                    fprintf(fp, "  %4d  %-5s  %-8s  %s\n",
                            i + 1, s.label, verified ? "yes" : "NO", ans);
                    fflush(fp);
                }
            }

            if (aborted) {
                rc = 2;
                if (fp) fprintf(fp, "\nABGEBROCHEN - die restlichen Schritte wurden nicht gestellt.\n"
                                    "Der Report ist unvollstaendig und belegt nichts ueber sie.\n");
            }

            // The verdict this probe exists for. Only stated when every step was
            // actually answered - a partial run must not produce a conclusion.
            if (fp && !aborted) {
                const bool anyYes = answers[0] == 1 || answers[1] == 1 ||
                                    answers[2] == 1 || answers[3] == 1;
                const bool allYes = answers[0] == 1 && answers[1] == 1 &&
                                    answers[2] == 1 && answers[3] == 1;
                fprintf(fp, "\nVERDICT: ");
                if (allYes) {
                    fprintf(fp, "the firmware renders this block live - colour and off both\n"
                                "arrive. What is left is which MODE bytes it implements; that is\n"
                                "what --%smode-sweep --confirm measures next.\n",
                            wantEdge ? "edge" : "kb");
                } else if (!anyYes) {
                    fprintf(fp, "nothing visible reacted to any of the four states, although the\n"
                                "device stored and returned every payload. For this target the\n"
                                "config memory is storage, not live state - further mode sweeps\n"
                                "cannot answer anything. Next lever is --kbwatch (read-only):\n"
                                "let the keyboard change its own lighting and watch which bytes\n"
                                "move.\n");
                } else {
                    fprintf(fp, "partial - some states arrived, others did not. Note exactly which\n"
                                "in docs/Keyboard_Protocol.md before drawing any conclusion; a\n"
                                "half-reacting block is a finding, not a measurement error.\n");
                }
            }

            // Put it back the way it was found, verified by read-back.
            if (haveSnapshot) {
                const bool restored = wantEdge
                    ? RestoreEVisionEdgePayload(edgeBefore)
                    : RestoreEVisionKeyboardPayload(kbBefore);
                if (fp) {
                    fprintf(fp, "\nrestore of the pre-probe payload:");
                    const int n = wantEdge ? 10 : 18;
                    const uint8_t* src = wantEdge ? edgeBefore : kbBefore;
                    for (int i = 0; i < n; i++) fprintf(fp, " %02X", src[i]);
                    fprintf(fp, "  -> %s\n", restored ? "verified" : "FAILED (read-back differs)");
                }
                if (!restored && rc == 0) rc = 1;
            }

            if (fp) fclose(fp);
            hid_exit();
            ReleaseProbeLock();
            {
                char done[256];
                snprintf(done, sizeof(done),
                         "[rendercheck] target=%s rc=%d - report: "
                         "%%APPDATA%%\\OneClickRGB\\docs\\rendercheck_%s.txt",
                         targetName, rc, wantEdge ? "edge" : "kb");
                LogDebug(done);
            }
            return rc;
        }
    }

    // Explicit, one-shot Win-key unlock: --unlock-winkey
    //
    // This is the write that used to run unconditionally and unverified inside
    // SetEVisionKeyboard, on every single apply. It is kept as an opt-in action
    // for one reason only: it is the write whose removal the user might notice,
    // so it stays reachable and testable rather than vanishing silently. What it
    // does is clear the two bytes at profile_base+0x14 in the ACTIVE profile.
    //
    // Read the report before believing it did anything. Section 4.1 of
    // docs/Keyboard_Protocol.md is evidence *against* this write mattering: with
    // the Win key locked, the key-remap table is intact and clearing the
    // per-profile flag at +0x2E left the key locked. The region +0x14..0x1D is
    // marked [?] - undecoded - and the current suspicion is that it holds the
    // edge zone's brightness and speed, in which case zeroing it is what froze
    // the edge animation in the first place.
    {
        const cli::Flag unlockArg = cli::Find(lpCmdLine, "--unlock-winkey");
        if (unlockArg.present) {
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            const std::wstring reportPath = dir + L"\\unlock_winkey.txt";

            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "Win-key unlock (profile_base+0x14)\n"
                                "DRY RUN - no write was sent and no value was read.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --unlock-winkey skipped - would write 00 00 to profile+0x14");
                return 0;
            }

            if (!AcquireProbeLock()) return ProbeLockBusy(reportPath, "unlock-winkey");

            hid_init();
            hid_device* dev = nullptr;
            struct hid_device_info* devs = hid_enumerate(Devices::EVISION_VID, Devices::EVISION_PID);
            for (auto* c = devs; c; c = c->next)
                if (c->usage_page == Devices::EVISION_USAGE_PAGE) { dev = hid_open_path(c->path); break; }
            hid_free_enumeration(devs);

            int rc = 2;
            FILE* fp = _wfopen(reportPath.c_str(), L"w");
            if (!dev) {
                if (fp) fprintf(fp, "ERROR: EVision RGB interface not found - nothing written.\n");
                LogDebug("[unlock-winkey] device not found");
            } else {
                EVisionQuery(dev, 0x01, 0, nullptr, 0, nullptr);
                Sleep(20);
                uint8_t profile = 0;
                EVisionQuery(dev, 0x05, 0x00, nullptr, 1, &profile);
                if (profile > 2) profile = 0;
                const uint16_t off = (uint16_t)(profile * 0x40 + 0x14);

                // Read-modify-write scope: the full ten bytes of the undecoded
                // region are recorded so the previous contents stay recoverable,
                // but only the two bytes the old code touched are written. No
                // widening of the write range (CLAUDE.md rule 2).
                uint8_t region[10] = {0};
                const int beforeRes = EVisionQuery(dev, 0x05, off, nullptr, 10, region);
                const uint8_t zero[2] = {0x00, 0x00};
                const int wr = EVisionQuery(dev, 0x06, off, zero, 2, nullptr);
                Sleep(10);
                uint8_t after[10] = {0};
                const int afterRes = EVisionQuery(dev, 0x05, off, nullptr, 10, after);
                EVisionQuery(dev, 0x02, 0, nullptr, 0, nullptr);
                hid_close(dev);

                const bool verified = (afterRes >= 0) && after[0] == 0x00 && after[1] == 0x00;
                rc = verified ? 0 : 1;

                if (fp) {
                    fprintf(fp, "Win-key unlock write\n");
                    fprintf(fp, "profile=%d  offset=0x%02X  writeRes=%d\n",
                            (int)profile, (unsigned)off, wr);
                    fprintf(fp, "before +0x14..0x1D (readRes=%d):", beforeRes);
                    for (int i = 0; i < 10; i++) fprintf(fp, " %02X", region[i]);
                    fprintf(fp, "\nafter  +0x14..0x1D (readRes=%d):", afterRes);
                    for (int i = 0; i < 10; i++) fprintf(fp, " %02X", after[i]);
                    fprintf(fp, "\nVERDICT: %s\n", verified
                            ? "the two bytes now read 00 00"
                            : "NOT verified - the device kept different bytes");
                    fprintf(fp, "\nThis says nothing about the Win key itself. Section 4.1 of\n"
                                "docs/Keyboard_Protocol.md found the lock is Fn-layer hardware\n"
                                "state that this config memory does not expose. If the key is\n"
                                "still locked after this, that is the expected outcome, and the\n"
                                "'before' line above is what to write back.\n");
                }
                {
                    char dbg[160];
                    snprintf(dbg, sizeof(dbg),
                             "[unlock-winkey] profile=%d off=0x%02X writeRes=%d verified=%d",
                             (int)profile, (unsigned)off, wr, (int)verified);
                    LogDebug(dbg);
                }
            }
            if (fp) fclose(fp);
            hid_exit();
            return rc;
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
                //
                // The mode byte used to be the literal 0x04 while this comment
                // already claimed parity with EDGE_MODE_STATIC. It is not: static
                // is 0x00, and 0x04 is the legacy value NormalizeEdgeMode() maps
                // away precisely because it produced rainbow-only behaviour. So
                // the "identical to the production path" claim held for the
                // offset and the commit flag but not for the effect byte, and
                // every run of this diagnostic parked the strip on an
                // undocumented mode. Named constant, no literal.
                uint8_t testData[10] = {
                    EDGE_MODE_STATIC, EFFECT_BRIGHTNESS_MAX, 2, 0, 0, 0, 0, 255, 0, 0x01
                };

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

    // ========================================================================
    // AUDIO-SONDE (Phase 6.1-6.3)
    //
    // Misst, ob die JVC Bassrolle eine Signal-Sense-Automatik hat. Sie steuert
    // nichts: sie gibt Toene aus, liest ueber WASAPI-Loopback zurueck, was
    // tatsaechlich hinausging, und laesst den Menschen eintragen, wann die Rolle
    // angesprungen ist. Die beiden Aussagen bleiben getrennt - GEMELDET ist,
    // was die Loopback-Messung belegt, ERKLAERT ist, was der Mensch beobachtet
    // hat (globale Konvention Paragraph 3).
    // ========================================================================
    {
        const cli::Flag apRun  = cli::Find(lpCmdLine, "--audioprobe");
        const cli::Flag apSelf = cli::Find(lpCmdLine, "--audioprobe-selftest");
        const cli::Flag apList = cli::Find(lpCmdLine, "--audioprobe-list");
        const cli::Flag apEp   = cli::Find(lpCmdLine, "--audio-endpoint");

        if (apRun.present || apSelf.present || apList.present) {
            std::wstring dir = GetAppDataPath() + L"\\docs";
            SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
            const std::wstring reportPath = dir + L"\\audio_probe.txt";

            // Genau eine Aktion. Zwei gleichzeitig werden abgelehnt, statt eine
            // davon still gewinnen zu lassen - derselbe Grund wie bei den
            // kbmode- und edgemode-Bloecken.
            if (cli::CountPresent({ &apRun, &apSelf, &apList }) > 1) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    fprintf(fe, "OneClickRGB Audio-Sonde\n"
                                "ABGEBROCHEN: --audioprobe, --audioprobe-selftest und\n"
                                "--audioprobe-list are mutually exclusive. Nichts wurde\n"
                                "ausgegeben und nichts gemessen.\n");
                    fclose(fe);
                }
                LogDebug("[audioprobe] more than one action flag - refused");
                return 2;
            }

            // --dry-run: die Sonde erzeugt hoerbaren Schall und oeffnet Dialoge.
            // Beides ist unter --dry-run verboten, und der Ausstieg liegt VOR
            // dem ersten Ton und VOR dem ersten Dialog, damit
            // check_dryrun_flags.ps1 unbeaufsichtigt durchlaufen kann.
            if (g_state.dryRun) {
                FILE* fe = _wfopen(reportPath.c_str(), L"w");
                if (fe) {
                    // Wortlaut bewusst so umbrochen, dass "no tone was emitted"
                    // und "no question was asked" jeweils GANZ auf einer Zeile
                    // stehen: check_dryrun_flags.ps1 sucht diese Saetze als
                    // Regex im Rohtext, und ein Zeilenumbruch mitten im Satz
                    // laesst die Pruefung ins Leere laufen, ohne dass sie
                    // fehlschlaegt - sie wuerde nur nichts mehr belegen.
                    fprintf(fe, "OneClickRGB Audio-Sonde\n"
                                "DRY RUN - no tone was emitted.\n"
                                "Nothing was measured and no question was asked.\n"
                                "Run without --dry-run to probe the stereo.\n");
                    fclose(fe);
                }
                LogDebug("[dry-run] --audioprobe skipped - would emit audible tones and open dialogs");
                return 0;
            }

            std::vector<audioprobe::Endpoint> eps;
            std::string epErr;
            const bool haveList = audioprobe::ListRenderEndpoints(eps, epErr);

            if (apList.present) {
                FILE* fpl = _wfopen(reportPath.c_str(), L"w");
                if (fpl) {
                    fprintf(fpl, "OneClickRGB Audio-Sonde - Wiedergabe-Endpunkte\n\n");
                    if (!haveList) {
                        fprintf(fpl, "UNBEKANNT: Endpunkte konnten nicht gelesen werden (%s)\n",
                                epErr.c_str());
                    } else if (eps.empty()) {
                        fprintf(fpl, "UNBEKANNT: kein aktiver Wiedergabe-Endpunkt gefunden\n");
                    } else {
                        for (size_t i = 0; i < eps.size(); ++i)
                            fprintf(fpl, "  [%u]%s %ls\n", (unsigned)i,
                                    eps[i].isDefault ? " (Standard)" : "          ",
                                    eps[i].name.c_str());
                    }
                    fclose(fpl);
                }
                LogDebug("[audioprobe] endpoint list written");
                return haveList ? 0 : 1;
            }

            // Endpunktwahl: ohne --audio-endpoint das Standard-Wiedergabegeraet,
            // mit, der erste Endpunkt, dessen Name den Teilstring enthaelt.
            std::wstring endpointId;
            std::wstring endpointName = L"(Standard-Wiedergabegeraet)";
            if (apEp.present && apEp.hasValue && !apEp.value.empty() && haveList) {
                const int wlen = MultiByteToWideChar(CP_UTF8, 0, apEp.value.c_str(), -1, NULL, 0);
                std::wstring want;
                if (wlen > 1) {
                    want.resize((size_t)(wlen - 1));
                    MultiByteToWideChar(CP_UTF8, 0, apEp.value.c_str(), -1, &want[0], wlen);
                }
                for (size_t i = 0; i < eps.size(); ++i) {
                    if (!want.empty() && eps[i].name.find(want) != std::wstring::npos) {
                        endpointId   = eps[i].id;
                        endpointName = eps[i].name;
                        break;
                    }
                }
                if (endpointId.empty()) {
                    FILE* fe = _wfopen(reportPath.c_str(), L"w");
                    if (fe) {
                        fprintf(fe, "OneClickRGB Audio-Sonde\n"
                                    "ABGEBROCHEN: kein Endpunkt enthaelt \"%s\".\n"
                                    "Nichts wurde ausgegeben. --audioprobe-list zeigt die Namen.\n",
                                apEp.value.c_str());
                        fclose(fe);
                    }
                    LogDebug("[audioprobe] endpoint substring did not match - refused");
                    return 2;
                }
            } else if (haveList) {
                for (size_t i = 0; i < eps.size(); ++i)
                    if (eps[i].isDefault) { endpointName = eps[i].name; break; }
            }

            FILE* fp = _wfopen(reportPath.c_str(), L"w");
            if (!fp) { LogDebug("[audioprobe] cannot open report file"); return 1; }

            SYSTEMTIME st; GetLocalTime(&st);
            fprintf(fp, "OneClickRGB Audio-Sonde (Phase 6)\n");
            fprintf(fp, "Zeit      : %04d-%02d-%02d %02d:%02d:%02d\n",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            fprintf(fp, "Endpunkt  : %ls\n", endpointName.c_str());
            fprintf(fp, "Grenzen   : max %.0f dBFS, min %.0f Hz (hart im Code)\n\n",
                    audioprobe::Limits::kMaxDbfs, audioprobe::Limits::kMinFreqHz);

            // ---------------- Selbstprobe (Phase 6.1) ----------------------
            // Laeuft ohne Anlage und ohne Dialog. Sie beantwortet die Frage, die
            // vor jeder Messreihe steht: misst dieses Geraet ueberhaupt?
            {
                fprintf(fp, "=== SELBSTPROBE (ohne Anlage) ===\n");
                int failed = 0;

                audioprobe::ToneRequest q;
                q.endpointId = endpointId;
                q.freqHz = 1000.0; q.dbfs = -30.0; q.holdMs = 1500;
                audioprobe::ToneResult r;
                audioprobe::RunTone(q, r);
                fprintf(fp, "  Ton 1000 Hz @ -30 dBFS    -> gemessen %8.1f Hz  %6.1f dBFS  %s\n",
                        r.measuredFreqHz, r.measuredDbfs,
                        r.verified ? "verifiziert" : "NICHT verifiziert");
                if (!r.error.empty()) fprintf(fp, "        Fehler: %s\n", r.error.c_str());
                if (r.sampleRate)
                    fprintf(fp, "        Mixer: %u Hz, %u Kanaele, %u Frames ausgewertet\n",
                            r.sampleRate, r.channels, r.framesAnalyzed);
                if (!r.verified) failed++;

                audioprobe::ToneRequest qs;
                qs.endpointId = endpointId; qs.silent = true; qs.holdMs = 1500;
                audioprobe::ToneResult rs;
                audioprobe::RunTone(qs, rs);
                fprintf(fp, "  Stille                    -> gemessen %6.1f dBFS  %s\n",
                        rs.measuredDbfs,
                        rs.verified ? "verifiziert (< -80)" : "NICHT verifiziert (Fremdton?)");
                if (!rs.error.empty()) fprintf(fp, "        Fehler: %s\n", rs.error.c_str());
                if (!rs.verified) failed++;

                // Die Pegelgrenze wird an einer Anforderung geprueft, die sie
                // ueberschreitet: 0 dBFS muss auf -12 begrenzt UND gemeldet werden.
                audioprobe::ToneRequest qc;
                qc.endpointId = endpointId; qc.freqHz = 1000.0; qc.dbfs = 0.0; qc.holdMs = 800;
                audioprobe::ToneResult rc2;
                audioprobe::RunTone(qc, rc2);
                fprintf(fp, "  Grenze 0 dBFS angefordert -> benutzt  %6.1f dBFS  %s\n",
                        rc2.usedDbfs, rc2.levelClamped ? "begrenzt und gemeldet" : "NICHT begrenzt");
                if (!rc2.levelClamped || rc2.usedDbfs > audioprobe::Limits::kMaxDbfs) failed++;

                // Und an der Frequenzuntergrenze: 10 Hz muss auf 40 Hz hoch.
                audioprobe::ToneRequest qf;
                qf.endpointId = endpointId; qf.freqHz = 10.0; qf.dbfs = -40.0; qf.holdMs = 800;
                audioprobe::ToneResult rf;
                audioprobe::RunTone(qf, rf);
                fprintf(fp, "  Grenze 10 Hz angefordert  -> benutzt %8.1f Hz    %s\n",
                        rf.usedFreqHz, rf.freqClamped ? "begrenzt und gemeldet" : "NICHT begrenzt");
                if (!rf.freqClamped || rf.usedFreqHz < audioprobe::Limits::kMinFreqHz) failed++;

                fprintf(fp, "\n  Ergebnis: %d von 4 Proben fehlgeschlagen\n\n", failed);

                if (apSelf.present) {
                    fclose(fp);
                    char dbg[96];
                    snprintf(dbg, sizeof(dbg), "[audioprobe] selftest done, %d failed", failed);
                    LogDebug(dbg);
                    return failed == 0 ? 0 : 1;
                }

                // Eine Messreihe aus einem durchgefallenen Messgeraet ist keine
                // Messung, sondern eine Behauptung mit Zahlen. Also nicht fahren.
                if (failed > 0) {
                    fprintf(fp, "ABGEBROCHEN: Die Selbstprobe ist durchgefallen. Es wurde KEINE\n"
                                "Messreihe gefahren - ein Messgeraet, das sich selbst nicht\n"
                                "zurueckliest, kann ueber die Bassrolle nichts aussagen.\n");
                    fclose(fp);
                    LogDebug("[audioprobe] selftest failed - staircase not run");
                    return 1;
                }
            }

            // ---------------- Messreihe (Phase 6.2) ------------------------
            const double kLevels[] = { -60.0, -50.0, -40.0, -30.0, -20.0, -12.0 };
            const double kFreqs[]  = { 40.0, 50.0, 63.0, 80.0, 100.0, 125.0, 160.0, 1000.0 };
            const int    nLevels   = (int)(sizeof(kLevels) / sizeof(kLevels[0]));
            const int    nFreqs    = (int)(sizeof(kFreqs)  / sizeof(kFreqs[0]));
            const int    holdMs    = cli::HoldSeconds(apRun, 5, 1, 20) * 1000;

            const int go = MessageBoxW(NULL,
                L"Audio-Sonde: Messreihe fuer die Signal-Sense-Automatik.\n\n"
                L"Vorher pruefen:\n"
                L"  - Ist die Bassrolle AKTIV (eigenes Netzkabel)? Eine passive\n"
                L"    Rolle hat keine Elektronik, die etwas ausloesen koennte -\n"
                L"    dann ist die Messung sinnlos.\n"
                L"  - Lautstaerke an der Anlage herunterdrehen.\n\n"
                L"Es werden Toene von 40 bis 160 Hz plus ein Kontrollton bei\n"
                L"1 kHz ausgegeben, hoechstens -12 dBFS. Nach jeder Pegelstufe\n"
                L"werden Sie gefragt, ob die Rolle angesprungen ist.\n\n"
                L"Zwischen zwei Toenen bricht ESC ab.",
                L"OneClickRGB Audio-Sonde", MB_OKCANCEL | MB_ICONINFORMATION);
            if (go != IDOK) {
                fprintf(fp, "ABGEBROCHEN vom Benutzer vor dem ersten Ton.\n");
                fclose(fp);
                LogDebug("[audioprobe] user cancelled before first tone");
                return 4;
            }

            fprintf(fp, "=== MESSREIHE ===\n");
            fprintf(fp, "GEMELDET = Loopback-Messung. ERKLAERT = Beobachtung des Menschen.\n");
            fprintf(fp, "Die beiden werden nicht vermengt.\n\n");

            double triggerLevel = 0.0;
            double triggerFreq  = 0.0;
            bool   triggered    = false;
            bool   aborted      = false;

            for (int li = 0; li < nLevels && !triggered && !aborted; ++li) {
                fprintf(fp, "-- Pegel %.0f dBFS --\n", kLevels[li]);
                fprintf(fp, "   %-9s %-13s %-15s %s\n",
                        "Soll-Hz", "GEMELDET Hz", "GEMELDET dBFS", "Status");

                for (int fi = 0; fi < nFreqs && !aborted; ++fi) {
                    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { aborted = true; break; }

                    audioprobe::ToneRequest q;
                    q.endpointId = endpointId;
                    q.freqHz = kFreqs[fi];
                    q.dbfs   = kLevels[li];
                    q.holdMs = holdMs;

                    audioprobe::ToneResult r;
                    audioprobe::RunTone(q, r);

                    fprintf(fp, "   %-9.0f %-13.1f %-15.1f %s%s\n",
                            kFreqs[fi], r.measuredFreqHz, r.measuredDbfs,
                            r.verified ? "verifiziert" : "NICHT verifiziert",
                            (kFreqs[fi] > 500.0) ? "  (Kontrollton)" : "");
                    if (!r.error.empty()) fprintf(fp, "        Fehler: %s\n", r.error.c_str());
                    fflush(fp);
                }

                if (aborted) break;

                wchar_t ask[512];
                swprintf_s(ask, 512,
                    L"Pegelstufe %.0f dBFS ist komplett durchlaufen\n"
                    L"(40-160 Hz plus Kontrollton 1 kHz).\n\n"
                    L"Ist die Bassrolle angesprungen?\n\n"
                    L"Ja      = ja, bei dieser Stufe\n"
                    L"Nein    = nein, naechste (lautere) Stufe\n"
                    L"Abbruch = Messreihe beenden",
                    kLevels[li]);
                const int ans = MessageBoxW(NULL, ask, L"OneClickRGB Audio-Sonde",
                                            MB_YESNOCANCEL | MB_ICONQUESTION);
                if (ans == IDCANCEL) { aborted = true; break; }
                if (ans == IDYES) { triggered = true; triggerLevel = kLevels[li]; }
            }

            // Feinsuche: welche Frequenz war es? Erst jetzt sinnvoll, weil erst
            // jetzt eine Stufe bekannt ist, bei der ueberhaupt etwas passiert.
            if (triggered && !aborted) {
                fprintf(fp, "\n-- Feinsuche bei %.0f dBFS --\n", triggerLevel);
                MessageBoxW(NULL,
                    L"Jetzt wird dieselbe Pegelstufe Frequenz fuer Frequenz\n"
                    L"wiederholt, um die ausloesende Frequenz einzugrenzen.\n\n"
                    L"Bitte die Rolle vorher wieder ausschalten bzw. in den\n"
                    L"Standby laufen lassen.",
                    L"OneClickRGB Audio-Sonde", MB_OK | MB_ICONINFORMATION);

                for (int fi = 0; fi < nFreqs && !aborted; ++fi) {
                    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { aborted = true; break; }

                    audioprobe::ToneRequest q;
                    q.endpointId = endpointId;
                    q.freqHz = kFreqs[fi];
                    q.dbfs   = triggerLevel;
                    q.holdMs = holdMs;

                    audioprobe::ToneResult r;
                    audioprobe::RunTone(q, r);

                    wchar_t ask[320];
                    swprintf_s(ask, 320,
                        L"%.0f Hz @ %.0f dBFS wurde ausgegeben.\n"
                        L"Gemeldet: %.1f Hz, %.1f dBFS (%s)\n\n"
                        L"Ist die Rolle bei DIESEM Ton angesprungen?",
                        kFreqs[fi], triggerLevel, r.measuredFreqHz, r.measuredDbfs,
                        r.verified ? L"verifiziert" : L"NICHT verifiziert");
                    const int ans = MessageBoxW(NULL, ask, L"OneClickRGB Audio-Sonde",
                                                MB_YESNOCANCEL | MB_ICONQUESTION);

                    fprintf(fp, "   %-9.0f %-13.1f %-15.1f %-18s ERKLAERT: %s\n",
                            kFreqs[fi], r.measuredFreqHz, r.measuredDbfs,
                            r.verified ? "verifiziert" : "NICHT verifiziert",
                            ans == IDYES ? "ausgeloest" : (ans == IDNO ? "nein" : "abgebrochen"));
                    fflush(fp);

                    if (ans == IDCANCEL) { aborted = true; break; }
                    if (ans == IDYES) { triggerFreq = kFreqs[fi]; break; }
                }
            }

            // ---------------- Ergebnis -------------------------------------
            fprintf(fp, "\n=== ERGEBNIS ===\n");
            if (aborted) {
                fprintf(fp, "ABGEBROCHEN. Kein Ergebnis - UNBEKANNT, ob die Rolle eine\n"
                            "Signal-Sense-Automatik hat.\n");
                fclose(fp);
                LogDebug("[audioprobe] aborted");
                return 4;
            }
            if (!triggered) {
                fprintf(fp, "Bis -12 dBFS hat die Bassrolle NICHT reagiert.\n\n"
                            "Deutung (Confidence: mittel - eine Messung, ein Geraet):\n"
                            "Diese Rolle hat keine Signal-Sense-Automatik, oder ihre\n"
                            "Schwelle liegt oberhalb dessen, was hier zugelassen ist.\n"
                            "Ueber Cinch/Klinke ist damit nichts zu erreichen. Der\n"
                            "naechste Schritt waere ein schaltbarer Zwischenstecker,\n"
                            "kein lauterer Ton.\n\n"
                            "Phase 6.4 (Anbindung an den Energiemanager) entfaellt:\n"
                            "ohne belegte Schwelle gibt es nichts zu schalten.\n");
                fclose(fp);
                LogDebug("[audioprobe] no trigger up to -12 dBFS");
                return 1;
            }

            fprintf(fp, "ERKLAERT: Die Rolle ist bei %.0f dBFS angesprungen.\n", triggerLevel);
            if (triggerFreq > 0.0)
                fprintf(fp, "ERKLAERT: ausloesende Frequenz %.0f Hz.\n", triggerFreq);
            else
                fprintf(fp, "ERKLAERT: einzelne Frequenz nicht eingegrenzt (Feinsuche ohne Treffer).\n");
            fprintf(fp, "GEMELDET: alle Toene dieser Stufe wurden per Loopback auf\n"
                        "          Frequenz und Pegel geprueft (siehe oben).\n\n"
                        "Confidence: mittel. Eine Messung an einem Geraet, an einem Tag.\n"
                        "Vor der Nutzung in Phase 6.4 mindestens einmal wiederholen -\n"
                        "eine Schwelle, die nur einmal gesehen wurde, ist eine Beobachtung,\n"
                        "keine Kennlinie.\n");
            fclose(fp);

            char dbg[160];
            snprintf(dbg, sizeof(dbg),
                     "[audioprobe] trigger at %.0f dBFS, freq %.0f Hz", triggerLevel, triggerFreq);
            LogDebug(dbg);
            return 0;
        }
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

    // Energiebenachrichtigungen anmelden.
    //
    // Hier stand eine von Hand getippte GUID mit SIEBEN statt acht Data4-Bytes -
    // das 0xc2 fehlte, der Compiler fuellte den achten mit 0 auf.
    //
    // Wichtig fuer jeden, der das nachprueft: die Anmeldung GELINGT damit
    // trotzdem. RegisterPowerSettingNotification prueft die GUID nicht und gibt
    // ein gueltiges Handle zurueck. Nur ankommen tut nie etwas. A/B gemessen
    // auf diesem Rechner, je 2 s Nachrichten gepumpt:
    //
    //     alte GUID (0xc2 fehlt) -> Anmeldung gelungen, 0 Benachrichtigungen
    //     korrekte GUID          -> Anmeldung gelungen, 1 Benachrichtigung
    //
    // Der PBT_POWERSETTINGCHANGE-Zweig war also toter Code, aber nicht wegen
    // einer fehlgeschlagenen Anmeldung - dieser Erklaerungsversuch war falsch.
    // Deshalb sagt die Statuszeile unten "angemeldet" und nicht "OK": ein
    // Handle ist keine Zustellung. Belegt ist die Zustellung erst, wenn
    // tatsaechlich eine Nachricht eintrifft - und genau das protokolliert der
    // Zweig beim ersten Mal.
    g_hDisplayNotify = RegisterPowerSettingNotification(
        g_state.hWnd, &kGuidConsoleDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE);
    g_hSuspendNotify = RegisterSuspendResumeNotification(
        g_state.hWnd, DEVICE_NOTIFY_WINDOW_HANDLE);

    const BOOL wtsOk = WTSRegisterSessionNotification(g_state.hWnd, NOTIFY_FOR_THIS_SESSION);

    {
        // "angemeldet", nicht "OK": das Handle beweist nur, dass Windows die
        // Anmeldung entgegengenommen hat, nicht dass je eine Nachricht kommt.
        wchar_t line[256];
        swprintf(line, 256,
                 L"Energie-Benachrichtigungen angemeldet: Display %s, Suspend/Resume %s, Sitzung %s",
                 g_hDisplayNotify ? L"ja" : L"NEIN",
                 g_hSuspendNotify ? L"ja" : L"NEIN",
                 wtsOk            ? L"ja" : L"NEIN");
        AppendStatus(line);

        char dbg[224];
        snprintf(dbg, sizeof(dbg),
                 "[power] register display=%s suspend=%s session=%s",
                 g_hDisplayNotify ? "ok" : "FAILED",
                 g_hSuspendNotify ? "ok" : "FAILED",
                 wtsOk            ? "ok" : "FAILED");
        LogDebug(dbg);
    }

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