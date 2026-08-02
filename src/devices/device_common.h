/**
 * devices/device_common.h - Shared types for the device protocol modules.
 *
 * The protocol modules are deliberately UI-free: they report progress through a
 * callback instead of writing into the status log directly, so the same code
 * runs under the GUI and under the test harness.
 */

#pragma once
#include <cstdint>
#include <functional>
#include <string>

#include "../channel_config.h"

namespace devices {

/// Receives human-readable progress lines (ASCII; the GUI widens them).
using StatusFn = std::function<void(const std::string&)>;

/// A no-op sink, for callers that do not care.
inline void NullStatus(const std::string&) {}

namespace ids {
constexpr uint16_t ASUS_VID          = 0x0B05;
constexpr uint16_t ASUS_AURA_PID     = 0x19AF;
constexpr uint16_t ASUS_USAGE_PAGE   = 0xFF72;

constexpr uint16_t STEELSERIES_VID   = 0x1038;
constexpr uint16_t RIVAL_600_PID     = 0x1724;

constexpr uint16_t EVISION_VID       = 0x3299;
constexpr uint16_t EVISION_PID       = 0x4E9F;
constexpr uint16_t EVISION_USAGE_PAGE = 0xFF1C;
}  // namespace ids

/// An RGB triple after per-zone correction has been applied.
struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};

inline Rgb Corrected(const ChannelConfig& zone, uint8_t r, uint8_t g, uint8_t b) {
    Rgb out{r, g, b};
    zone.ApplyCorrection(out.r, out.g, out.b);
    return out;
}

}  // namespace devices
