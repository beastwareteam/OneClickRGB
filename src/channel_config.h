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

