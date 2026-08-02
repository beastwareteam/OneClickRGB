/**
 * devices/steelseries.h - SteelSeries Rival 600 (0x1038:0x1724)
 */

#pragma once
#include <cstdint>

#include "../hal/hid_backend.h"
#include "device_common.h"

namespace devices {
namespace steelseries {

/// The mouse exposes its RGB control endpoint on interface 0.
constexpr int CONTROL_INTERFACE = 0;

/// Number of individually addressable lighting zones.
constexpr int ZONE_COUNT = 8;

constexpr size_t COLOR_PACKET_SIZE = 8;
/// Only the first 7 bytes are transmitted; the 8th is padding in the report.
constexpr size_t COLOR_WRITE_LEN = 7;

constexpr size_t SAVE_PACKET_SIZE = 10;
constexpr size_t SAVE_WRITE_LEN   = 9;

/// Builds the colour packet for one zone (0-based).
void BuildColorPacket(uint8_t out[COLOR_PACKET_SIZE], int zone_index,
                      uint8_t r, uint8_t g, uint8_t b);

/// Builds the "persist to onboard memory" packet.
void BuildSavePacket(uint8_t out[SAVE_PACKET_SIZE]);

bool SetColor(hal::IHidBackend& hid, const ChannelConfig& zone,
              uint8_t r, uint8_t g, uint8_t b, const StatusFn& status);

}  // namespace steelseries
}  // namespace devices
