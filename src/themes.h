#pragma once
//=============================================================================
// THEMES.H - Unified Theme System for OneClickRGB
//=============================================================================

#include <windows.h>
#include <gdiplus.h>

// Helper to convert COLORREF to Gdiplus::Color
inline Gdiplus::Color ToGdipColor(COLORREF c) {
    return Gdiplus::Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

//=============================================================================
// THEME STRUCTURE
//=============================================================================

struct AppTheme {
    // Identification
    const wchar_t* name;
    int id;  // 0=Dark, 1=Light, 2=Colorblind

    // Window backgrounds
    COLORREF bgWindowTop;       // Gradient top
    COLORREF bgWindowBottom;    // Gradient bottom

    // Control backgrounds
    COLORREF bgControl;         // General control background
    COLORREF bgControlHover;    // Hovered control
    COLORREF bgButton;          // Button background
    COLORREF bgButtonHover;     // Button hover
    COLORREF bgButtonPressed;   // Button pressed

    // Accent colors (Apply button, checked items)
    COLORREF bgAccent;
    COLORREF bgAccentHover;
    COLORREF bgAccentPressed;

    // Text colors
    COLORREF textPrimary;       // Main text
    COLORREF textSecondary;     // Muted text (labels)
    COLORREF textOnAccent;      // Text on accent buttons

    // Borders
    COLORREF border;            // Normal border
    COLORREF borderHover;       // Hovered border
    COLORREF borderFocus;       // Focused control (accessibility)

    // Status log
    COLORREF statusBg;
    COLORREF statusText;
    COLORREF statusBorder;

    // Group box
    COLORREF groupHeaderBg;
    COLORREF groupBodyBg;
    COLORREF groupBorder;
    COLORREF groupTitle;

    // Checkbox
    COLORREF checkboxBg;
    COLORREF checkboxBgChecked;
    COLORREF checkboxBorder;
    COLORREF checkboxBorderChecked;
    COLORREF checkboxCheck;     // Checkmark color

    // Slider channel colors (RGB)
    COLORREF sliderRed;
    COLORREF sliderGreen;
    COLORREF sliderBlue;
    COLORREF sliderTrack;       // Unfilled track
    COLORREF sliderThumb;       // Thumb color
};

//=============================================================================
// THEME DEFINITIONS
//=============================================================================

// Dark Theme (default)
inline AppTheme g_themeDark = {
    L"Dark", 0,

    // Window gradient
    RGB(20, 22, 30),        // bgWindowTop
    RGB(35, 40, 55),        // bgWindowBottom

    // Controls
    RGB(40, 45, 60),        // bgControl
    RGB(55, 60, 80),        // bgControlHover
    RGB(40, 45, 60),        // bgButton
    RGB(55, 60, 80),        // bgButtonHover
    RGB(50, 55, 70),        // bgButtonPressed

    // Accent
    RGB(40, 110, 200),      // bgAccent
    RGB(50, 130, 220),      // bgAccentHover
    RGB(30, 100, 180),      // bgAccentPressed

    // Text
    RGB(220, 225, 235),     // textPrimary
    RGB(160, 170, 185),     // textSecondary
    RGB(255, 255, 255),     // textOnAccent

    // Borders
    RGB(70, 80, 100),       // border
    RGB(100, 160, 220),     // borderHover
    RGB(150, 200, 255),     // borderFocus

    // Status log
    RGB(28, 32, 42),        // statusBg
    RGB(200, 210, 220),     // statusText
    RGB(60, 70, 90),        // statusBorder

    // Group box
    RGB(35, 40, 52),        // groupHeaderBg
    RGB(28, 32, 42),        // groupBodyBg
    RGB(55, 65, 85),        // groupBorder
    RGB(180, 190, 210),     // groupTitle

    // Checkbox
    RGB(30, 35, 48),        // checkboxBg
    RGB(50, 130, 210),      // checkboxBgChecked
    RGB(70, 80, 100),       // checkboxBorder
    RGB(80, 160, 240),      // checkboxBorderChecked
    RGB(255, 255, 255),     // checkboxCheck

    // Slider channels
    RGB(255, 60, 60),       // sliderRed
    RGB(60, 255, 60),       // sliderGreen
    RGB(60, 120, 255),      // sliderBlue
    RGB(50, 55, 65),        // sliderTrack
    RGB(200, 210, 225),     // sliderThumb
};

// Light Theme
inline AppTheme g_themeLight = {
    L"Light", 1,

    // Window gradient
    RGB(240, 242, 248),     // bgWindowTop
    RGB(225, 228, 235),     // bgWindowBottom

    // Controls
    RGB(255, 255, 255),     // bgControl
    RGB(240, 242, 248),     // bgControlHover
    RGB(245, 247, 250),     // bgButton
    RGB(230, 235, 242),     // bgButtonHover
    RGB(220, 225, 235),     // bgButtonPressed

    // Accent
    RGB(0, 100, 180),       // bgAccent
    RGB(0, 120, 200),       // bgAccentHover
    RGB(0, 80, 160),        // bgAccentPressed

    // Text
    RGB(30, 35, 45),        // textPrimary
    RGB(90, 100, 115),      // textSecondary
    RGB(255, 255, 255),     // textOnAccent

    // Borders
    RGB(200, 205, 215),     // border
    RGB(100, 150, 200),     // borderHover
    RGB(0, 100, 180),       // borderFocus

    // Status log
    RGB(252, 253, 255),     // statusBg
    RGB(40, 45, 55),        // statusText
    RGB(200, 205, 215),     // statusBorder

    // Group box
    RGB(248, 250, 252),     // groupHeaderBg
    RGB(255, 255, 255),     // groupBodyBg
    RGB(210, 215, 225),     // groupBorder
    RGB(50, 60, 75),        // groupTitle

    // Checkbox
    RGB(255, 255, 255),     // checkboxBg
    RGB(0, 100, 180),       // checkboxBgChecked
    RGB(180, 185, 195),     // checkboxBorder
    RGB(0, 100, 180),       // checkboxBorderChecked
    RGB(255, 255, 255),     // checkboxCheck

    // Slider channels
    RGB(220, 50, 50),       // sliderRed
    RGB(50, 180, 50),       // sliderGreen
    RGB(50, 100, 220),      // sliderBlue
    RGB(210, 215, 225),     // sliderTrack
    RGB(80, 90, 105),       // sliderThumb
};

// Colorblind Theme (Deuteranopia-friendly: Orange/Blue instead of Red/Green)
// Uses warm cream/beige tones to be visually distinct from Light theme
inline AppTheme g_themeColorblind = {
    L"Colorblind", 2,

    // Window gradient (warm cream tones - distinct from Light's cool gray)
    RGB(252, 248, 240),     // bgWindowTop - warm cream
    RGB(242, 235, 220),     // bgWindowBottom - warm beige

    // Controls (warm whites)
    RGB(255, 253, 248),     // bgControl - warm white
    RGB(248, 244, 235),     // bgControlHover
    RGB(252, 250, 245),     // bgButton
    RGB(245, 240, 228),     // bgButtonHover
    RGB(238, 232, 218),     // bgButtonPressed

    // Accent (warm orange - high contrast, colorblind-safe)
    RGB(200, 120, 50),      // bgAccent - warm orange
    RGB(220, 140, 70),      // bgAccentHover
    RGB(180, 100, 40),      // bgAccentPressed

    // Text (warm dark brown for better harmony)
    RGB(45, 35, 25),        // textPrimary - dark brown
    RGB(100, 85, 70),       // textSecondary - medium brown
    RGB(255, 255, 255),     // textOnAccent

    // Borders (warm tones)
    RGB(215, 205, 185),     // border - warm gray
    RGB(200, 120, 50),      // borderHover - matches accent
    RGB(180, 100, 40),      // borderFocus

    // Status log (warm paper tone)
    RGB(255, 252, 245),     // statusBg - cream paper
    RGB(50, 40, 30),        // statusText - dark brown
    RGB(215, 205, 185),     // statusBorder

    // Group box (warm tones)
    RGB(250, 246, 238),     // groupHeaderBg - light cream
    RGB(255, 253, 248),     // groupBodyBg - warm white
    RGB(220, 210, 190),     // groupBorder
    RGB(70, 55, 40),        // groupTitle - brown

    // Checkbox (warm accent)
    RGB(255, 253, 248),     // checkboxBg
    RGB(200, 120, 50),      // checkboxBgChecked - orange
    RGB(195, 185, 165),     // checkboxBorder
    RGB(200, 120, 50),      // checkboxBorderChecked
    RGB(255, 255, 255),     // checkboxCheck

    // Slider channels (Colorblind-safe: Orange, Blue, Cyan)
    RGB(230, 159, 0),       // sliderRed -> Orange
    RGB(0, 114, 178),       // sliderGreen -> Blue
    RGB(86, 180, 233),      // sliderBlue -> Sky Blue
    RGB(220, 210, 190),     // sliderTrack - warm gray
    RGB(70, 55, 40),        // sliderThumb - dark brown
};

//=============================================================================
// GLOBAL THEME POINTER
//=============================================================================

inline AppTheme* g_currentTheme = &g_themeDark;

// Theme switching
inline void SetTheme(int themeId) {
    switch (themeId) {
        case 0: g_currentTheme = &g_themeDark; break;
        case 1: g_currentTheme = &g_themeLight; break;
        case 2: g_currentTheme = &g_themeColorblind; break;
        default: g_currentTheme = &g_themeDark; break;
    }
}

inline int GetThemeId() {
    return g_currentTheme->id;
}
