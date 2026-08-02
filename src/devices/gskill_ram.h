/**
 * devices/gskill_ram.h - G.Skill Trident Z5 (RGB / Neo) over SMBus
 *
 * The DIMMs carry an ENE controller reachable through the Intel I801 SMBus.
 * See docs/GSkill_Trident_Z5_RGB_DDR5_Protocol.md.
 */

#pragma once
#include <cstdint>

#include "../hal/smbus_backend.h"
#include "device_common.h"

namespace devices {
namespace gskill {

/// SMBus addresses DDR5 SPD hubs answer on.
constexpr uint8_t ADDR_FIRST = 0x70;
constexpr uint8_t ADDR_LAST  = 0x77;

/// Maximum number of modules whose colour we track separately.
constexpr int MAX_SLOTS = 4;

/// ENE register map.
constexpr uint16_t REG_NAME_BASE  = 0x1000;  ///< 16-byte ASCII device name
constexpr uint16_t REG_LED_COUNT  = 0x1C02;
constexpr uint16_t REG_DIRECT_ON  = 0x8020;  ///< enter direct/software mode
constexpr uint16_t REG_LED_BASE   = 0x8100;  ///< per-LED colour triples
constexpr uint16_t REG_APPLY      = 0x80A0;  ///< latch the written colours

/// Fallback when the module reports an implausible LED count.
constexpr uint8_t DEFAULT_LED_COUNT = 8;
constexpr uint8_t MAX_PLAUSIBLE_LED_COUNT = 20;

/// The ENE controller orders its colour bytes R, B, G - not R, G, B.
/// Getting this wrong swaps green and blue on the modules only.
inline void EncodeLedTriple(uint8_t out[3], uint8_t r, uint8_t g, uint8_t b) {
    out[0] = r;
    out[1] = b;
    out[2] = g;
}

/// Register address of LED `index`'s colour triple.
inline uint16_t LedRegister(int index) {
    return static_cast<uint16_t>(REG_LED_BASE + index * 3);
}

/// True when the 16-byte name read from a module identifies supported RAM.
bool IsSupportedModuleName(const char* name);

/// Low-level ENE accessors, exposed for tests.
void EneWrite(hal::ISmbusBackend& bus, uint8_t addr, uint16_t reg, uint8_t value);
uint8_t EneRead(hal::ISmbusBackend& bus, uint8_t addr, uint16_t reg);

struct Result {
    int modules_set = 0;
    bool bus_available = false;
    hal::SmbusError error = hal::SmbusError::None;
};

/// Applies the colour to every detected module. `zones` is indexed per slot.
/// Assumes the backend is already open.
Result SetColor(hal::ISmbusBackend& bus, const ChannelConfig* zones, int zone_count,
                uint8_t r, uint8_t g, uint8_t b, const StatusFn& status);

}  // namespace gskill
}  // namespace devices
