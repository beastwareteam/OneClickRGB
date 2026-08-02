/**
 * devices/evision.h - EVision keyboard (VID 0x3299 / PID 0x4E9F)
 *
 * Covers the main key matrix and the "edge" side-LED zone.
 */

#pragma once
#include <cstdint>

#include "../hal/hid_backend.h"
#include "device_common.h"

namespace devices {
namespace evision {

constexpr uint8_t V2_REPORT_ID   = 4;
constexpr uint8_t V2_PACKET_SIZE = 64;

enum Command : uint8_t {
    CMD_BEGIN_CONFIG = 0x01,
    CMD_END_CONFIG   = 0x02,
    CMD_READ         = 0x05,
    CMD_WRITE        = 0x06,
};

enum KeyboardMode : uint8_t {
    KB_MODE_WAVE_SHORT  = 0x01,
    KB_MODE_WAVE_LONG   = 0x02,
    KB_MODE_COLOR_WHEEL = 0x03,
    KB_MODE_SPECTRUM    = 0x04,
    KB_MODE_BREATHING   = 0x05,
    KB_MODE_STATIC      = 0x06,
    KB_MODE_REACTIVE    = 0x07,
    KB_MODE_RIPPLE      = 0x08,
    KB_MODE_STARLIGHT   = 0x0A,
    KB_MODE_RAINBOW     = 0x0C,
    KB_MODE_HURRICANE   = 0x0D,
};

enum EdgeMode : uint8_t {
    EDGE_MODE_FREEZE    = 0x00,
    EDGE_MODE_WAVE      = 0x01,
    EDGE_MODE_SPECTRUM  = 0x02,
    EDGE_MODE_BREATHING = 0x03,
    EDGE_MODE_STATIC    = 0x04,
    EDGE_MODE_OFF       = 0x05,
};

/// Each of the three onboard profiles owns a 0x40-byte config block.
constexpr uint16_t PROFILE_STRIDE = 0x40;
constexpr uint16_t PROFILE_BASE   = 0x01;
constexpr uint8_t  PROFILE_COUNT  = 3;

/// Offset of the key-lock register *within* a profile block.
///
/// This is the bug behind the stuck Windows key: the register was previously
/// addressed as an absolute 0x14, which is only correct for profile 0. On
/// profile 1 or 2 the unlock landed in profile 0's block while the active
/// profile kept its lock bit, so Fn+Win stayed disabled after every apply.
constexpr uint16_t LOCK_OFFSET_IN_PROFILE = 0x13;

/// Size of the key-lock register in bytes.
constexpr uint8_t LOCK_SIZE = 2;

/// Value that disables all key locking (Windows key active).
constexpr uint8_t LOCK_DISABLED[LOCK_SIZE] = {0x00, 0x00};

/// Bit 0 of the first lock byte is the Windows-key lock.
constexpr uint8_t LOCK_BIT_WINKEY = 0x01;

/// Start of a profile's config block.
inline uint16_t ProfileOffset(uint8_t profile) {
    return static_cast<uint16_t>(profile * PROFILE_STRIDE + PROFILE_BASE);
}

/// Address of the key-lock register for a given profile.
inline uint16_t LockOffset(uint8_t profile) {
    return static_cast<uint16_t>(ProfileOffset(profile) + LOCK_OFFSET_IN_PROFILE);
}

/// Builds the 64-byte V2 packet, including the checksum over bytes 3..63.
/// Exposed so tests can assert the framing independently of any transport.
void BuildPacket(uint8_t out[V2_PACKET_SIZE], uint8_t cmd, uint16_t offset,
                 const uint8_t* idata, uint8_t size);

/// One request/response round trip.
/// Returns the response payload size, or a negative value on error.
int Query(hal::IHidDevice& dev, uint8_t cmd, uint16_t offset,
          const uint8_t* idata, uint8_t size, uint8_t* odata);

/// Outcome of the unlock attempt, reported back to the user.
enum class UnlockResult {
    Unlocked,      ///< written and confirmed by read-back
    StillLocked,   ///< written, but the device still reports a lock bit
    Unverified,    ///< written; device did not answer the read-back
    WriteFailed,
};

const char* UnlockResultText(UnlockResult r);

/// Clears the key lock for `profile` and reads the register back to confirm.
/// The read-back exists because the correct register address cannot be verified
/// without the physical keyboard - the status line reports what the device
/// actually returned.
UnlockResult ClearKeyLock(hal::IHidDevice& dev, uint8_t profile);

/// Reads the active onboard profile index (clamped to a valid range).
uint8_t ReadActiveProfile(hal::IHidDevice& dev);

struct KeyboardSettings {
    uint8_t mode       = KB_MODE_STATIC;
    uint8_t brightness = 4;  // 0-4
    uint8_t speed      = 2;  // 0-5
};

/// Applies colour + effect to the main key matrix and clears the key lock.
bool SetKeyboard(hal::IHidBackend& hid, const ChannelConfig& zone,
                 uint8_t r, uint8_t g, uint8_t b,
                 const KeyboardSettings& settings, const StatusFn& status);

/// Applies colour + effect to the side-LED (edge) zone.
bool SetEdge(hal::IHidBackend& hid, const ChannelConfig& zone,
             uint8_t r, uint8_t g, uint8_t b, uint8_t mode,
             const StatusFn& status);

}  // namespace evision
}  // namespace devices
