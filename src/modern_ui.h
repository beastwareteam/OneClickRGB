/**
 * Modern UI Components for OneClickRGB
 * Custom-drawn controls with GDI+ for a sleek, modern appearance
 */

#ifndef MODERN_UI_H
#define MODERN_UI_H

#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")

// Use std::min/max
using std::min;
using std::max;

//=============================================================================
// MODERN THEME COLORS
//=============================================================================

struct ModernTheme {
    // Background colors
    Gdiplus::Color bgPrimary;
    Gdiplus::Color bgSecondary;
    Gdiplus::Color bgTertiary;
    Gdiplus::Color bgCard;

    // Accent colors
    Gdiplus::Color accent;
    Gdiplus::Color accentHover;
    Gdiplus::Color accentGlow;

    // Text colors
    Gdiplus::Color textPrimary;
    Gdiplus::Color textSecondary;
    Gdiplus::Color textMuted;

    // Border & effects
    Gdiplus::Color border;
    Gdiplus::Color shadow;
    Gdiplus::Color glow;

    // State colors
    Gdiplus::Color success;
    Gdiplus::Color warning;
    Gdiplus::Color error;

    bool isDark;
};

// Cyberpunk-inspired dark theme
inline ModernTheme g_modernDark = {
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

// Clean light theme
inline ModernTheme g_modernLight = {
    // Backgrounds
    Gdiplus::Color(255, 250, 250, 252),   // bgPrimary
    Gdiplus::Color(255, 255, 255, 255),   // bgSecondary
    Gdiplus::Color(255, 245, 245, 248),   // bgTertiary
    Gdiplus::Color(230, 255, 255, 255),   // bgCard

    // Accent
    Gdiplus::Color(255, 0, 120, 215),     // accent
    Gdiplus::Color(255, 30, 140, 230),    // accentHover
    Gdiplus::Color(60, 0, 120, 215),      // accentGlow

    // Text
    Gdiplus::Color(255, 20, 20, 30),      // textPrimary
    Gdiplus::Color(255, 80, 80, 90),      // textSecondary
    Gdiplus::Color(255, 140, 140, 150),   // textMuted

    // Border & effects
    Gdiplus::Color(255, 220, 220, 225),   // border
    Gdiplus::Color(30, 0, 0, 0),          // shadow
    Gdiplus::Color(40, 0, 120, 215),      // glow

    // State
    Gdiplus::Color(255, 0, 180, 100),     // success
    Gdiplus::Color(255, 240, 180, 0),     // warning
    Gdiplus::Color(255, 220, 50, 80),     // error

    false
};

inline ModernTheme* g_mTheme = &g_modernDark;

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

inline Gdiplus::Color BlendColors(Gdiplus::Color c1, Gdiplus::Color c2, float t) {
    return Gdiplus::Color(
        (BYTE)(c1.GetA() + (c2.GetA() - c1.GetA()) * t),
        (BYTE)(c1.GetR() + (c2.GetR() - c1.GetR()) * t),
        (BYTE)(c1.GetG() + (c2.GetG() - c1.GetG()) * t),
        (BYTE)(c1.GetB() + (c2.GetB() - c1.GetB()) * t)
    );
}

inline Gdiplus::Color ColorFromRGB(COLORREF rgb, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}

//=============================================================================
// MODERN DRAWING FUNCTIONS
//=============================================================================

// Draw rounded rectangle with optional gradient fill
inline void DrawRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect, float radius,
                            Gdiplus::Color fillColor, Gdiplus::Color borderColor = Gdiplus::Color(0, 0, 0, 0),
                            float borderWidth = 0) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2;

    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();

    if (fillColor.GetA() > 0) {
        Gdiplus::SolidBrush brush(fillColor);
        g.FillPath(&brush, &path);
    }

    if (borderWidth > 0 && borderColor.GetA() > 0) {
        Gdiplus::Pen pen(borderColor, borderWidth);
        g.DrawPath(&pen, &path);
    }
}

// Draw rounded rectangle with gradient
inline void DrawGradientRoundedRect(Gdiplus::Graphics& g, Gdiplus::RectF rect, float radius,
                                     Gdiplus::Color topColor, Gdiplus::Color bottomColor,
                                     Gdiplus::Color borderColor = Gdiplus::Color(0, 0, 0, 0),
                                     float borderWidth = 0) {
    Gdiplus::GraphicsPath path;
    float d = radius * 2;

    path.AddArc(rect.X, rect.Y, d, d, 180, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::LinearGradientBrush brush(
        Gdiplus::PointF(rect.X, rect.Y),
        Gdiplus::PointF(rect.X, rect.Y + rect.Height),
        topColor, bottomColor
    );
    g.FillPath(&brush, &path);

    if (borderWidth > 0 && borderColor.GetA() > 0) {
        Gdiplus::Pen pen(borderColor, borderWidth);
        g.DrawPath(&pen, &path);
    }
}

// Draw soft shadow
inline void DrawShadow(Gdiplus::Graphics& g, Gdiplus::RectF rect, float radius,
                       float shadowRadius = 8, float offsetY = 4) {
    for (int i = (int)shadowRadius; i > 0; i--) {
        float alpha = (shadowRadius - i) / shadowRadius * 30;
        Gdiplus::Color shadowColor(BYTE(alpha), 0, 0, 0);
        Gdiplus::RectF shadowRect(
            rect.X - i + 2,
            rect.Y - i + offsetY + 2,
            rect.Width + i * 2 - 4,
            rect.Height + i * 2 - 4
        );
        DrawRoundedRect(g, shadowRect, radius + i/2, shadowColor);
    }
}

// Draw glow effect
inline void DrawGlow(Gdiplus::Graphics& g, Gdiplus::RectF rect, float radius,
                     Gdiplus::Color glowColor, float glowRadius = 10) {
    for (int i = (int)glowRadius; i > 0; i--) {
        float alpha = (glowRadius - i) / glowRadius * glowColor.GetA();
        Gdiplus::Color c(BYTE(alpha), glowColor.GetR(), glowColor.GetG(), glowColor.GetB());
        Gdiplus::RectF glowRect(
            rect.X - i,
            rect.Y - i,
            rect.Width + i * 2,
            rect.Height + i * 2
        );
        DrawRoundedRect(g, glowRect, radius + i/2, c);
    }
}

//=============================================================================
// MODERN BUTTON
//=============================================================================

struct ModernButton {
    HWND hWnd;
    RECT rect;
    wchar_t text[64];
    bool isHovered;
    bool isPressed;
    bool isAccent;  // Use accent color
    bool isEnabled;
    int id;

    void Draw(HDC hdc) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        Gdiplus::RectF r((float)rect.left, (float)rect.top,
                         (float)(rect.right - rect.left),
                         (float)(rect.bottom - rect.top));

        float radius = 6.0f;

        // Determine colors based on state
        Gdiplus::Color bgTop, bgBottom, borderColor, textColor;

        if (!isEnabled) {
            bgTop = g_mTheme->bgTertiary;
            bgBottom = g_mTheme->bgTertiary;
            borderColor = g_mTheme->border;
            textColor = g_mTheme->textMuted;
        } else if (isAccent) {
            if (isPressed) {
                bgTop = BlendColors(g_mTheme->accent, Gdiplus::Color(255, 0, 0, 0), 0.2f);
                bgBottom = BlendColors(g_mTheme->accent, Gdiplus::Color(255, 0, 0, 0), 0.3f);
            } else if (isHovered) {
                bgTop = g_mTheme->accentHover;
                bgBottom = g_mTheme->accent;
                // Draw glow on hover
                DrawGlow(g, r, radius, g_mTheme->accentGlow, 8);
            } else {
                bgTop = BlendColors(g_mTheme->accent, Gdiplus::Color(255, 255, 255, 255), 0.1f);
                bgBottom = g_mTheme->accent;
            }
            borderColor = Gdiplus::Color(0, 0, 0, 0);
            textColor = Gdiplus::Color(255, 255, 255, 255);
        } else {
            if (isPressed) {
                bgTop = g_mTheme->bgSecondary;
                bgBottom = g_mTheme->bgPrimary;
            } else if (isHovered) {
                bgTop = g_mTheme->bgTertiary;
                bgBottom = g_mTheme->bgSecondary;
            } else {
                bgTop = BlendColors(g_mTheme->bgSecondary, g_mTheme->bgTertiary, 0.5f);
                bgBottom = g_mTheme->bgSecondary;
            }
            borderColor = isHovered ? g_mTheme->accent : g_mTheme->border;
            textColor = g_mTheme->textPrimary;
        }

        // Draw shadow (subtle)
        if (!isPressed && isEnabled) {
            DrawShadow(g, r, radius, 4, 2);
        }

        // Draw button background
        DrawGradientRoundedRect(g, r, radius, bgTop, bgBottom, borderColor, 1.0f);

        // Draw text
        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 10, isAccent ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(textColor);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        g.DrawString(text, -1, &font, r, &format, &textBrush);
    }
};

//=============================================================================
// MODERN SLIDER (RGB)
//=============================================================================

struct ModernSlider {
    HWND hWnd;
    RECT rect;
    int value;      // 0-255
    int maxValue;
    char channel;   // 'R', 'G', 'B', or 'X' for neutral
    bool isHovered;
    bool isDragging;
    int id;

    Gdiplus::Color GetChannelColor() {
        switch (channel) {
            case 'R': return Gdiplus::Color(255, 255, 60, 60);
            case 'G': return Gdiplus::Color(255, 60, 255, 60);
            case 'B': return Gdiplus::Color(255, 60, 120, 255);
            default:  return g_mTheme->accent;
        }
    }

    Gdiplus::Color GetChannelGlow() {
        switch (channel) {
            case 'R': return Gdiplus::Color(80, 255, 60, 60);
            case 'G': return Gdiplus::Color(80, 60, 255, 60);
            case 'B': return Gdiplus::Color(80, 60, 120, 255);
            default:  return g_mTheme->accentGlow;
        }
    }

    void Draw(HDC hdc) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        float x = (float)rect.left;
        float y = (float)rect.top;
        float w = (float)(rect.right - rect.left);
        float h = (float)(rect.bottom - rect.top);

        float trackHeight = 8.0f;
        float trackY = y + (h - trackHeight) / 2;
        float knobRadius = 10.0f;
        float progress = (float)value / maxValue;
        float knobX = x + progress * (w - knobRadius * 2) + knobRadius;
        float knobY = y + h / 2;

        // Track background (dark)
        Gdiplus::RectF trackRect(x, trackY, w, trackHeight);
        DrawRoundedRect(g, trackRect, trackHeight / 2, g_mTheme->bgPrimary, g_mTheme->border, 1);

        // Filled portion with gradient
        if (progress > 0.01f) {
            Gdiplus::RectF filledRect(x, trackY, (knobX - x), trackHeight);
            Gdiplus::Color channelColor = GetChannelColor();
            Gdiplus::Color darkColor = BlendColors(channelColor, Gdiplus::Color(255, 0, 0, 0), 0.4f);

            Gdiplus::GraphicsPath fillPath;
            float d = trackHeight;
            fillPath.AddArc(filledRect.X, filledRect.Y, d, d, 180, 90);
            fillPath.AddLine(filledRect.X + d/2, filledRect.Y, filledRect.X + filledRect.Width, filledRect.Y);
            fillPath.AddLine(filledRect.X + filledRect.Width, filledRect.Y, filledRect.X + filledRect.Width, filledRect.Y + filledRect.Height);
            fillPath.AddLine(filledRect.X + filledRect.Width, filledRect.Y + filledRect.Height, filledRect.X + d/2, filledRect.Y + filledRect.Height);
            fillPath.AddArc(filledRect.X, filledRect.Y, d, d, 90, 90);
            fillPath.CloseFigure();

            Gdiplus::LinearGradientBrush fillBrush(
                Gdiplus::PointF(filledRect.X, filledRect.Y),
                Gdiplus::PointF(filledRect.X, filledRect.Y + filledRect.Height),
                channelColor, darkColor
            );
            g.FillPath(&fillBrush, &fillPath);
        }

        // Knob glow (when hovered/dragging)
        if (isHovered || isDragging) {
            DrawGlow(g, Gdiplus::RectF(knobX - knobRadius, knobY - knobRadius, knobRadius * 2, knobRadius * 2),
                     knobRadius, GetChannelGlow(), 12);
        }

        // Knob shadow
        Gdiplus::Color shadowColor(40, 0, 0, 0);
        g.FillEllipse(&Gdiplus::SolidBrush(shadowColor),
                      knobX - knobRadius + 1, knobY - knobRadius + 2, knobRadius * 2, knobRadius * 2);

        // Knob
        Gdiplus::Color knobTop = isDragging ? GetChannelColor() :
                                 (isHovered ? BlendColors(g_mTheme->bgTertiary, GetChannelColor(), 0.3f) :
                                  g_mTheme->bgTertiary);
        Gdiplus::Color knobBottom = BlendColors(knobTop, Gdiplus::Color(255, 0, 0, 0), 0.2f);

        Gdiplus::GraphicsPath knobPath;
        knobPath.AddEllipse(knobX - knobRadius, knobY - knobRadius, knobRadius * 2, knobRadius * 2);

        Gdiplus::LinearGradientBrush knobBrush(
            Gdiplus::PointF(knobX, knobY - knobRadius),
            Gdiplus::PointF(knobX, knobY + knobRadius),
            knobTop, knobBottom
        );
        g.FillPath(&knobBrush, &knobPath);

        // Knob border
        Gdiplus::Pen knobPen(isHovered ? GetChannelColor() : g_mTheme->border, 1.5f);
        g.DrawEllipse(&knobPen, knobX - knobRadius, knobY - knobRadius, knobRadius * 2, knobRadius * 2);

        // Inner highlight
        Gdiplus::Color highlight(60, 255, 255, 255);
        g.FillEllipse(&Gdiplus::SolidBrush(highlight),
                      knobX - knobRadius/2, knobY - knobRadius/2 - 2, knobRadius, knobRadius/2);
    }

    int HitTest(int mouseX) {
        float w = (float)(rect.right - rect.left);
        float knobRadius = 10.0f;
        int newValue = (int)(((mouseX - rect.left - knobRadius) / (w - knobRadius * 2)) * maxValue);
        return max(0, min(maxValue, newValue));
    }
};

//=============================================================================
// MODERN CARD (GROUP BOX REPLACEMENT)
//=============================================================================

struct ModernCard {
    RECT rect;
    wchar_t title[64];
    bool hasGlow;
    Gdiplus::Color glowColor;

    void Draw(HDC hdc) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        Gdiplus::RectF r((float)rect.left, (float)rect.top,
                         (float)(rect.right - rect.left),
                         (float)(rect.bottom - rect.top));

        float radius = 12.0f;

        // Shadow
        DrawShadow(g, r, radius, 10, 5);

        // Optional glow
        if (hasGlow) {
            DrawGlow(g, r, radius, glowColor, 6);
        }

        // Card background with subtle gradient
        Gdiplus::Color topColor = BlendColors(g_mTheme->bgCard, Gdiplus::Color(255, 255, 255, 255), 0.03f);
        DrawGradientRoundedRect(g, r, radius, topColor, g_mTheme->bgCard, g_mTheme->border, 1.0f);

        // Title bar gradient
        Gdiplus::RectF titleRect(r.X, r.Y, r.Width, 32);
        Gdiplus::GraphicsPath titlePath;
        float d = radius * 2;
        titlePath.AddArc(titleRect.X, titleRect.Y, d, d, 180, 90);
        titlePath.AddArc(titleRect.X + titleRect.Width - d, titleRect.Y, d, d, 270, 90);
        titlePath.AddLine(titleRect.X + titleRect.Width, titleRect.Y + titleRect.Height,
                          titleRect.X, titleRect.Y + titleRect.Height);
        titlePath.CloseFigure();

        Gdiplus::Color titleTop = BlendColors(g_mTheme->accent, g_mTheme->bgCard, 0.85f);
        Gdiplus::Color titleBottom = g_mTheme->bgCard;
        Gdiplus::LinearGradientBrush titleBrush(
            Gdiplus::PointF(titleRect.X, titleRect.Y),
            Gdiplus::PointF(titleRect.X, titleRect.Y + titleRect.Height),
            titleTop, titleBottom
        );
        g.FillPath(&titleBrush, &titlePath);

        // Title text
        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 11, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(g_mTheme->textPrimary);

        Gdiplus::RectF textRect(r.X + 16, r.Y + 6, r.Width - 32, 24);
        g.DrawString(title, -1, &font, textRect, nullptr, &textBrush);

        // Accent line under title
        Gdiplus::Pen accentPen(g_mTheme->accent, 2);
        g.DrawLine(&accentPen, r.X + 16, r.Y + 30, r.X + 60, r.Y + 30);
    }
};

//=============================================================================
// MODERN COLOR PREVIEW
//=============================================================================

struct ModernColorPreview {
    RECT rect;
    BYTE r, g, b;
    bool isAnimating;
    float pulsePhase;

    void Draw(HDC hdc) {
        Gdiplus::Graphics gfx(hdc);
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        float x = (float)rect.left;
        float y = (float)rect.top;
        float w = (float)(rect.right - rect.left);
        float h = (float)(rect.bottom - rect.top);
        float cx = x + w / 2;
        float cy = y + h / 2;
        float radius = min(w, h) / 2 - 8;

        // Outer glow
        Gdiplus::Color glowColor(60, r, g, b);
        for (int i = 20; i > 0; i--) {
            float alpha = (20.0f - i) / 20.0f * 60;
            Gdiplus::Color c((BYTE)alpha, r, g, b);
            Gdiplus::SolidBrush brush(c);
            gfx.FillEllipse(&brush, cx - radius - i, cy - radius - i,
                           (radius + i) * 2, (radius + i) * 2);
        }

        // Shadow
        Gdiplus::Color shadowColor(50, 0, 0, 0);
        gfx.FillEllipse(&Gdiplus::SolidBrush(shadowColor),
                        cx - radius + 3, cy - radius + 5, radius * 2, radius * 2);

        // Main color circle with gradient
        Gdiplus::GraphicsPath circlePath;
        circlePath.AddEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

        Gdiplus::Color lightColor((BYTE)min(255, r + 40), (BYTE)min(255, g + 40), (BYTE)min(255, b + 40));
        Gdiplus::Color darkColor((BYTE)(r * 0.7f), (BYTE)(g * 0.7f), (BYTE)(b * 0.7f));

        Gdiplus::LinearGradientBrush colorBrush(
            Gdiplus::PointF(cx, cy - radius),
            Gdiplus::PointF(cx, cy + radius),
            lightColor, darkColor
        );
        gfx.FillPath(&colorBrush, &circlePath);

        // Inner highlight
        Gdiplus::Color highlight(80, 255, 255, 255);
        gfx.FillEllipse(&Gdiplus::SolidBrush(highlight),
                        cx - radius * 0.6f, cy - radius * 0.7f, radius * 1.2f, radius * 0.5f);

        // Border
        Gdiplus::Pen borderPen(g_mTheme->border, 2);
        gfx.DrawEllipse(&borderPen, cx - radius, cy - radius, radius * 2, radius * 2);
    }
};

//=============================================================================
// MODERN CHECKBOX
//=============================================================================

struct ModernCheckbox {
    RECT rect;
    wchar_t text[64];
    bool isChecked;
    bool isHovered;
    bool isEnabled;
    int id;

    void Draw(HDC hdc) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        float boxSize = 18.0f;
        float x = (float)rect.left;
        float y = (float)rect.top + ((rect.bottom - rect.top) - boxSize) / 2;

        Gdiplus::RectF boxRect(x, y, boxSize, boxSize);
        float radius = 4.0f;

        // Box background
        Gdiplus::Color bgColor = isChecked ? g_mTheme->accent : g_mTheme->bgSecondary;
        Gdiplus::Color borderColor = isChecked ? g_mTheme->accent :
                                     (isHovered ? g_mTheme->accent : g_mTheme->border);

        if (isHovered && isChecked) {
            DrawGlow(g, boxRect, radius, g_mTheme->accentGlow, 6);
        }

        DrawRoundedRect(g, boxRect, radius, bgColor, borderColor, 1.5f);

        // Checkmark
        if (isChecked) {
            Gdiplus::Pen checkPen(Gdiplus::Color(255, 255, 255, 255), 2.5f);
            checkPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);

            Gdiplus::PointF points[3] = {
                Gdiplus::PointF(x + 4, y + boxSize/2),
                Gdiplus::PointF(x + boxSize/2.5f, y + boxSize - 5),
                Gdiplus::PointF(x + boxSize - 4, y + 5)
            };
            g.DrawLines(&checkPen, points, 3);
        }

        // Label
        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::Color textColor = isEnabled ? g_mTheme->textPrimary : g_mTheme->textMuted;
        Gdiplus::SolidBrush textBrush(textColor);

        Gdiplus::RectF textRect(x + boxSize + 8, (float)rect.top,
                                (float)(rect.right - rect.left) - boxSize - 8,
                                (float)(rect.bottom - rect.top));
        Gdiplus::StringFormat format;
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        g.DrawString(text, -1, &font, textRect, &format, &textBrush);
    }
};

//=============================================================================
// MODERN COMBOBOX (DROPDOWN)
//=============================================================================

struct ModernCombo {
    RECT rect;
    wchar_t selectedText[128];
    bool isHovered;
    bool isOpen;
    int id;

    void Draw(HDC hdc) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        Gdiplus::RectF r((float)rect.left, (float)rect.top,
                         (float)(rect.right - rect.left),
                         (float)(rect.bottom - rect.top));

        float radius = 6.0f;

        // Background
        Gdiplus::Color bgColor = isHovered ? g_mTheme->bgTertiary : g_mTheme->bgSecondary;
        Gdiplus::Color borderColor = isHovered ? g_mTheme->accent : g_mTheme->border;

        DrawRoundedRect(g, r, radius, bgColor, borderColor, 1.0f);

        // Text
        Gdiplus::FontFamily fontFamily(L"Segoe UI");
        Gdiplus::Font font(&fontFamily, 9, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush textBrush(g_mTheme->textPrimary);

        Gdiplus::RectF textRect(r.X + 10, r.Y, r.Width - 30, r.Height);
        Gdiplus::StringFormat format;
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        g.DrawString(selectedText, -1, &font, textRect, &format, &textBrush);

        // Dropdown arrow
        float arrowX = r.X + r.Width - 20;
        float arrowY = r.Y + r.Height / 2;
        Gdiplus::PointF arrowPoints[3] = {
            Gdiplus::PointF(arrowX - 4, arrowY - 2),
            Gdiplus::PointF(arrowX + 4, arrowY - 2),
            Gdiplus::PointF(arrowX, arrowY + 3)
        };
        Gdiplus::SolidBrush arrowBrush(g_mTheme->textSecondary);
        g.FillPolygon(&arrowBrush, arrowPoints, 3);
    }
};

#endif // MODERN_UI_H
