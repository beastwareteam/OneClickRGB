/**
 * devices/asus_aura.h - ASUS Aura mainboard controller (0x0B05:0x19AF)
 *
 * Protocol per OpenRGB's AsusAuraMainboardController.
 */

#pragma once
#include <cstdint>

#include "../hal/hid_backend.h"
#include "device_common.h"

namespace devices {
namespace aura {

constexpr uint8_t REPORT_ID       = 0xEC;
constexpr int     PACKET_SIZE     = 65;
constexpr int     LEDS_PER_PACKET = 20;
constexpr int     MAX_CHANNELS    = 16;

enum Request : uint8_t {
    REQ_DIRECT           = 0x40,
    REQ_MODE             = 0x43,
    REQ_CHANNEL_MODE     = 0x35,
    REQ_GEN1             = 0x52,
    REQ_FIRMWARE_VERSION = 0x82,
    REQ_CONFIG_TABLE     = 0xB0,
};

/// Mode value that hands a channel over to software control.
constexpr uint8_t MODE_DIRECT = 0xFF;

/// One controllable zone as reported by the device's config table.
struct Channel {
    bool present        = false;
    int  led_count      = 0;
    bool addressable    = false;
    int  direct_channel = 0;  ///< channel id actually sent to the device
    char name[64]       = {0};
};

struct HardwareConfig {
    bool valid = false;
    char firmware[17] = {0};
    uint8_t config_table[60] = {0};
    int num_mainboard_leds = 0;
    int num_rgb_headers = 0;
    int num_addressable_headers = 0;
    Channel channels[MAX_CHANNELS];
    int num_channels = 0;
};

/// Derives the channel layout from a raw config table. Pure function - this is
/// what the tests pin down, since it decides which LEDs get addressed at all.
void ParseConfig(HardwareConfig& cfg);

/// Builds one direct-mode colour packet.
/// `last` marks the final packet of a channel's sequence (sets bit 0x80).
void BuildDirectPacket(uint8_t out[PACKET_SIZE], int channel, int offset,
                       int count, uint8_t r, uint8_t g, uint8_t b, bool last);

/// Builds the SetGen1 handshake required before direct mode is accepted.
void BuildGen1Packet(uint8_t out[PACKET_SIZE]);

/// Builds the per-channel mode packet (REQ_MODE).
void BuildModePacket(uint8_t out[PACKET_SIZE], int channel, uint8_t mode);

bool ReadFirmware(hal::IHidDevice& dev, char firmware[17]);
bool ReadConfigTable(hal::IHidDevice& dev, uint8_t config_table[60]);

/// Opens the Aura control interface and performs the SetGen1 handshake.
std::unique_ptr<hal::IHidDevice> Open(hal::IHidBackend& hid);

/// Probes the device and fills `cfg`. False when no device answered.
bool Scan(hal::IHidBackend& hid, HardwareConfig& cfg);

/// Pushes `r,g,b` to one channel, chunked into LEDS_PER_PACKET-sized packets.
void SetChannel(hal::IHidBackend& hid, hal::IHidDevice& dev, int channel,
                int num_leds, uint8_t r, uint8_t g, uint8_t b);

/// Applies the colour to every enabled zone. `zones` is indexed per channel.
/// Returns the number of channels written, or -1 when the device is absent.
int SetAll(hal::IHidBackend& hid, const HardwareConfig& cfg,
           const ChannelConfig* zones, int zone_count,
           uint8_t r, uint8_t g, uint8_t b, const StatusFn& status);

/// Puts every channel into direct mode - used after resume from standby, where
/// the controller has fallen back to its onboard effect.
bool ResetToDirectMode(hal::IHidBackend& hid, const StatusFn& status);

}  // namespace aura
}  // namespace devices
