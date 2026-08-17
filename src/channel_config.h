/**
 * Channel Configuration - Per-channel color correction and settings
 *
 * ChannelConfig: lightweight struct, one instance per device zone.
 * Management (Save/Load/enumerate) is handled by RGBConfig in app_config.h.
 */

#pragma once
#include <cstdint>
#include <string>

struct ChannelConfig {
    std::string name;
    bool enabled = true;

    // Color correction (0-200, 100 = neutral)
    int red_adjust   = 100;
    int green_adjust = 100;
    int blue_adjust  = 100;

    // Brightness (0-100)
    int brightness = 100;

    // Per-channel override: this channel ignores the global colour and keeps
    // the colour the user dialled in for it. Set when a slider in the ASUS test
    // dialog is moved, cleared again by "Folgt Global". Default false, so a
    // channel nobody touched follows the global colour exactly as before.
    bool    override_active = false;
    uint8_t override_r = 0, override_g = 34, override_b = 255;

    // Apply color correction in-place
    void ApplyCorrection(uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (!enabled) { r = g = b = 0; return; }
        int nr = (r * red_adjust   * brightness) / 10000;
        int ng = (g * green_adjust * brightness) / 10000;
        int nb = (b * blue_adjust  * brightness) / 10000;
        r = (nr > 255) ? 255 : (nr < 0 ? 0 : (uint8_t)nr);
        g = (ng > 255) ? 255 : (ng < 0 ? 0 : (uint8_t)ng);
        b = (nb > 255) ? 255 : (nb < 0 ? 0 : (uint8_t)nb);
    }
};

// Which colour a channel actually gets. The *only* place where the precedence
// between the global colour and a per-channel override is decided - every apply
// path (startup, slider, preset, profile load, resume) funnels through here, so
// they cannot drift apart.
//
// The channel correction stays in effect for overridden channels too: the
// dialog's sliders and the apply path must compute the same bytes, otherwise
// closing and reopening the dialog would shift the colour.
inline void ResolveChannelColor(const ChannelConfig& c,
                                uint8_t gr, uint8_t gg, uint8_t gb,
                                uint8_t& r, uint8_t& g, uint8_t& b) {
    if (c.override_active) { r = c.override_r; g = c.override_g; b = c.override_b; }
    else                   { r = gr; g = gg; b = gb; }
    c.ApplyCorrection(r, g, b);   // handles !enabled -> 0,0,0
}

