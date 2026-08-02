#include "asus_aura.h"

#include <cstdio>
#include <cstring>

namespace devices {
namespace aura {

namespace {
/// Offsets into the 60-byte config table (per OpenRGB).
constexpr int CT_ADDRESSABLE_HEADERS = 0x02;
constexpr int CT_MAINBOARD_LEDS      = 0x1B;
constexpr int CT_RGB_HEADERS         = 0x1D;

/// Direct-channel id the mainboard's fixed LEDs live on.
constexpr int MAINBOARD_CHANNEL = 0x04;
/// Standard RGB headers start here and count upwards.
constexpr int RGB_HEADER_BASE   = 0x02;
/// Addressable headers support up to this many LEDs each.
constexpr int ADDRESSABLE_MAX_LEDS = 120;
}  // namespace

void ParseConfig(HardwareConfig& cfg) {
    cfg.num_mainboard_leds      = cfg.config_table[CT_MAINBOARD_LEDS];
    cfg.num_rgb_headers         = cfg.config_table[CT_RGB_HEADERS];
    cfg.num_addressable_headers = cfg.config_table[CT_ADDRESSABLE_HEADERS];

    // Fewer LEDs than headers is nonsense; treat the header count as unusable.
    if (cfg.num_mainboard_leds < cfg.num_rgb_headers) cfg.num_rgb_headers = 0;

    cfg.num_channels = 0;
    auto add = [&](int direct_channel, int leds, bool addressable, const char* fmt,
                   int arg) -> bool {
        if (cfg.num_channels >= MAX_CHANNELS) return false;
        Channel& ch = cfg.channels[cfg.num_channels];
        ch = Channel();
        ch.present        = true;
        ch.led_count      = leds;
        ch.addressable    = addressable;
        ch.direct_channel = direct_channel;
        std::snprintf(ch.name, sizeof(ch.name), fmt, arg);
        ++cfg.num_channels;
        return true;
    };

    if (cfg.num_mainboard_leds > 0)
        add(MAINBOARD_CHANNEL, cfg.num_mainboard_leds, false, "Mainboard (%d LEDs)",
            cfg.num_mainboard_leds);

    for (int i = 0; i < cfg.num_rgb_headers; ++i)
        if (!add(RGB_HEADER_BASE + i, 1, false, "RGB Header %d", i + 1)) break;

    // Some boards expose PCH/IO zones that the config table does not enumerate.
    // They are probed only when a mainboard zone exists, i.e. on a real board.
    if (cfg.num_mainboard_leds > 0) {
        const int extra_zones[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x0B, 0x0C};
        for (int zone_id : extra_zones) {
            bool exists = false;
            for (int j = 0; j < cfg.num_channels; ++j)
                if (cfg.channels[j].direct_channel == zone_id) exists = true;
            if (exists) continue;
            if (!add(zone_id, 30, false, "Zone (ID 0x%02X)", zone_id)) break;
        }
    }

    for (int i = 0; i < cfg.num_addressable_headers; ++i)
        if (!add(i, ADDRESSABLE_MAX_LEDS, true, "Addressable %d (max 120 LEDs)", i + 1))
            break;

    cfg.valid = (cfg.num_channels > 0);
}

void BuildGen1Packet(uint8_t out[PACKET_SIZE]) {
    std::memset(out, 0, PACKET_SIZE);
    out[0x00] = REPORT_ID;
    out[0x01] = REQ_GEN1;
    out[0x02] = 0x53;
    out[0x03] = 0x00;
    out[0x04] = 0x01;
}

void BuildModePacket(uint8_t out[PACKET_SIZE], int channel, uint8_t mode) {
    std::memset(out, 0, PACKET_SIZE);
    out[0x00] = REPORT_ID;
    out[0x01] = REQ_MODE;
    out[0x02] = static_cast<uint8_t>(channel);
    out[0x03] = mode;
}

void BuildDirectPacket(uint8_t out[PACKET_SIZE], int channel, int offset,
                       int count, uint8_t r, uint8_t g, uint8_t b, bool last) {
    std::memset(out, 0, PACKET_SIZE);
    out[0x00] = REPORT_ID;
    out[0x01] = REQ_DIRECT;
    out[0x02] = static_cast<uint8_t>((last ? 0x80 : 0x00) | channel);
    out[0x03] = static_cast<uint8_t>(offset);
    out[0x04] = static_cast<uint8_t>(count);
    for (int i = 0; i < count && (0x05 + i * 3 + 2) < PACKET_SIZE; ++i) {
        out[0x05 + i * 3 + 0] = r;
        out[0x05 + i * 3 + 1] = g;
        out[0x05 + i * 3 + 2] = b;
    }
}

bool ReadFirmware(hal::IHidDevice& dev, char firmware[17]) {
    uint8_t buf[PACKET_SIZE];
    std::memset(buf, 0, sizeof(buf));
    buf[0x00] = REPORT_ID;
    buf[0x01] = REQ_FIRMWARE_VERSION;

    if (dev.Write(buf, PACKET_SIZE) < 0) return false;
    if (dev.ReadTimeout(buf, PACKET_SIZE, 1000) < 0) return false;
    if (buf[1] != 0x02) return false;

    std::memcpy(firmware, &buf[2], 16);
    firmware[16] = '\0';
    return true;
}

bool ReadConfigTable(hal::IHidDevice& dev, uint8_t config_table[60]) {
    uint8_t buf[PACKET_SIZE];
    std::memset(buf, 0, sizeof(buf));
    buf[0x00] = REPORT_ID;
    buf[0x01] = REQ_CONFIG_TABLE;

    if (dev.Write(buf, PACKET_SIZE) < 0) return false;
    if (dev.ReadTimeout(buf, PACKET_SIZE, 1000) < 0) return false;
    if (buf[1] != 0x30) return false;

    std::memcpy(config_table, &buf[4], 60);
    return true;
}

std::unique_ptr<hal::IHidDevice> Open(hal::IHidBackend& hid) {
    std::string path = hal::FindDevicePath(hid, ids::ASUS_VID, ids::ASUS_AURA_PID,
                                           ids::ASUS_USAGE_PAGE);
    if (path.empty()) return nullptr;

    auto dev = hid.Open(path);
    if (!dev) return nullptr;

    uint8_t gen1[PACKET_SIZE];
    BuildGen1Packet(gen1);
    dev->Write(gen1, PACKET_SIZE);
    hid.Sleep(5);
    return dev;
}

bool Scan(hal::IHidBackend& hid, HardwareConfig& cfg) {
    auto dev = Open(hid);
    if (!dev) return false;

    cfg = HardwareConfig();
    ReadFirmware(*dev, cfg.firmware);
    if (!ReadConfigTable(*dev, cfg.config_table)) return false;

    ParseConfig(cfg);
    return cfg.valid;
}

void SetChannel(hal::IHidBackend& hid, hal::IHidDevice& dev, int channel,
                int num_leds, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t mode[PACKET_SIZE];
    BuildModePacket(mode, channel, MODE_DIRECT);
    dev.Write(mode, PACKET_SIZE);
    hid.Sleep(2);

    for (int offset = 0; offset < num_leds;) {
        int count = LEDS_PER_PACKET;
        if (offset + count > num_leds) count = num_leds - offset;
        const bool last = (offset + count >= num_leds);

        uint8_t buf[PACKET_SIZE];
        BuildDirectPacket(buf, channel, offset, count, r, g, b, last);
        dev.Write(buf, PACKET_SIZE);
        hid.Sleep(2);
        offset += count;
    }
}

int SetAll(hal::IHidBackend& hid, const HardwareConfig& cfg,
           const ChannelConfig* zones, int zone_count,
           uint8_t r, uint8_t g, uint8_t b, const StatusFn& status) {
    auto dev = Open(hid);
    if (!dev) {
        status("[ASUS Aura] Not found");
        return -1;
    }

    // Without a successful scan we fall back to the channel layout that covers
    // the boards seen so far; it over-addresses, which is harmless.
    struct Fallback { int channel; int leds; };
    static const Fallback kFallback[] = {
        {0x00, 60}, {0x01, 120}, {0x02, 120}, {0x03, 60},
        {0x04, 60}, {0x0B, 60}, {0x0C, 60},
    };

    const int count = cfg.valid ? cfg.num_channels
                                : static_cast<int>(sizeof(kFallback) / sizeof(kFallback[0]));

    int set_count = 0;
    for (int i = 0; i < count && i < zone_count; ++i) {
        if (!zones[i].enabled) continue;
        const Rgb c = Corrected(zones[i], r, g, b);
        const int channel = cfg.valid ? cfg.channels[i].direct_channel : kFallback[i].channel;
        const int leds    = cfg.valid ? cfg.channels[i].led_count      : kFallback[i].leds;
        SetChannel(hid, *dev, channel, leds, c.r, c.g, c.b);
        ++set_count;
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "[ASUS Aura] %d channels set", set_count);
    status(buf);
    return set_count;
}

bool ResetToDirectMode(hal::IHidBackend& hid, const StatusFn& status) {
    std::string path = hal::FindDevicePath(hid, ids::ASUS_VID, ids::ASUS_AURA_PID,
                                           ids::ASUS_USAGE_PAGE);
    if (path.empty()) {
        status("[WARN] ASUS Aura not found");
        return false;
    }
    auto dev = hid.Open(path);
    if (!dev) {
        status("[WARN] ASUS Aura could not be opened");
        return false;
    }

    uint8_t buf[PACKET_SIZE];

    std::memset(buf, 0, sizeof(buf));
    buf[0x00] = REPORT_ID;
    buf[0x01] = REQ_CONFIG_TABLE;
    dev->Write(buf, PACKET_SIZE);
    dev->ReadTimeout(buf, PACKET_SIZE, 500);
    hid.Sleep(20);

    BuildGen1Packet(buf);
    dev->Write(buf, PACKET_SIZE);
    hid.Sleep(50);

    // Every channel the protocol can address, not just the first eight - a
    // channel left in effect mode ignores the direct colour writes that follow.
    // Over-addressing is harmless; the board ignores channels it does not have.
    for (int ch = 0; ch < MAX_CHANNELS; ++ch) {
        std::memset(buf, 0, sizeof(buf));
        buf[0x00] = REPORT_ID;
        buf[0x01] = REQ_CHANNEL_MODE;
        buf[0x02] = static_cast<uint8_t>(ch);
        buf[0x05] = MODE_DIRECT;
        dev->Write(buf, PACKET_SIZE);
        hid.Sleep(5);
    }

    status("ASUS Aura reset OK");
    return true;
}

}  // namespace aura
}  // namespace devices
