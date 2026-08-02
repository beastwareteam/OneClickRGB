#include "evision.h"

#include <cstdio>
#include <cstring>

namespace devices {
namespace evision {

void BuildPacket(uint8_t out[V2_PACKET_SIZE], uint8_t cmd, uint16_t offset,
                 const uint8_t* idata, uint8_t size) {
    std::memset(out, 0, V2_PACKET_SIZE);
    out[0] = V2_REPORT_ID;
    out[3] = cmd;
    out[4] = size;
    out[5] = offset & 0xFF;
    out[6] = (offset >> 8) & 0xFF;
    if (idata && size > 0) {
        uint8_t copy = size;
        if (copy > V2_PACKET_SIZE - 8) copy = V2_PACKET_SIZE - 8;
        std::memcpy(out + 8, idata, copy);
    }
    uint16_t checksum = 0;
    for (int i = 3; i < V2_PACKET_SIZE; ++i) checksum += out[i];
    out[1] = checksum & 0xFF;
    out[2] = (checksum >> 8) & 0xFF;
}

int Query(hal::IHidDevice& dev, uint8_t cmd, uint16_t offset,
          const uint8_t* idata, uint8_t size, uint8_t* odata) {
    uint8_t buffer[V2_PACKET_SIZE];
    BuildPacket(buffer, cmd, offset, idata, size);

    if (dev.Write(buffer, sizeof(buffer)) < 0) return -1;

    int bytes_read = 0;
    int retries = 10;
    do {
        bytes_read = dev.ReadTimeout(buffer, sizeof(buffer), 100);
        --retries;
    } while (bytes_read > 0 && buffer[0] != V2_REPORT_ID && retries > 0);

    if (bytes_read != static_cast<int>(sizeof(buffer))) return -2;
    if (buffer[7] != 0) return -buffer[7];
    if (odata && buffer[4] > 0) std::memcpy(odata, buffer + 8, buffer[4]);
    return buffer[4];
}

const char* UnlockResultText(UnlockResult r) {
    switch (r) {
        case UnlockResult::Unlocked:    return "Windows key unlocked";
        case UnlockResult::StillLocked: return "Windows key STILL LOCKED - device rejected unlock";
        case UnlockResult::Unverified:  return "Windows key unlock sent (no read-back from device)";
        case UnlockResult::WriteFailed: return "Windows key unlock FAILED to send";
    }
    return "Windows key state unknown";
}

uint8_t ReadActiveProfile(hal::IHidDevice& dev) {
    uint8_t profile = 0;
    if (Query(dev, CMD_READ, 0x00, nullptr, 1, &profile) < 0) return 0;
    if (profile >= PROFILE_COUNT) return 0;
    return profile;
}

UnlockResult ClearKeyLock(hal::IHidDevice& dev, uint8_t profile) {
    if (profile >= PROFILE_COUNT) profile = 0;
    const uint16_t offset = LockOffset(profile);

    if (Query(dev, CMD_WRITE, offset, LOCK_DISABLED, LOCK_SIZE, nullptr) < 0)
        return UnlockResult::WriteFailed;

    uint8_t readback[LOCK_SIZE] = {0xFF, 0xFF};
    int n = Query(dev, CMD_READ, offset, nullptr, LOCK_SIZE, readback);
    if (n < LOCK_SIZE) return UnlockResult::Unverified;

    return (readback[0] & LOCK_BIT_WINKEY) ? UnlockResult::StillLocked
                                           : UnlockResult::Unlocked;
}

namespace {

/// Opens the EVision control interface, or nullptr when absent.
std::unique_ptr<hal::IHidDevice> OpenKeyboard(hal::IHidBackend& hid) {
    std::string path = hal::FindDevicePath(hid, ids::EVISION_VID, ids::EVISION_PID,
                                           ids::EVISION_USAGE_PAGE);
    if (path.empty()) return nullptr;
    return hid.Open(path);
}

}  // namespace

bool SetKeyboard(hal::IHidBackend& hid, const ChannelConfig& zone,
                 uint8_t r, uint8_t g, uint8_t b,
                 const KeyboardSettings& settings, const StatusFn& status) {
    auto dev = OpenKeyboard(hid);
    if (!dev) {
        status("[EVision] Keyboard not found");
        return false;
    }

    Query(*dev, CMD_BEGIN_CONFIG, 0, nullptr, 0, nullptr);
    hid.Sleep(20);

    const Rgb c = Corrected(zone, r, g, b);
    const uint8_t profile = ReadActiveProfile(*dev);

    uint8_t config[18] = {0};
    config[0] = settings.mode;
    config[1] = settings.brightness;
    config[2] = settings.speed;
    config[3] = 0;  // direction
    config[4] = 0;  // random colour off
    config[5] = c.r;
    config[6] = c.g;
    config[7] = c.b;
    config[8] = 0;  // colour offset

    Query(*dev, CMD_WRITE, ProfileOffset(profile), config, sizeof(config), nullptr);
    hid.Sleep(10);

    const UnlockResult unlock = ClearKeyLock(*dev, profile);

    Query(*dev, CMD_END_CONFIG, 0, nullptr, 0, nullptr);

    char buf[96];
    std::snprintf(buf, sizeof(buf), "[EVision] Keyboard set (Mode: 0x%02X, Profile: %u)",
                  settings.mode, static_cast<unsigned>(profile));
    status(buf);

    std::snprintf(buf, sizeof(buf), "[EVision] %s", UnlockResultText(unlock));
    status(buf);

    return true;
}

bool SetEdge(hal::IHidBackend& hid, const ChannelConfig& zone,
             uint8_t r, uint8_t g, uint8_t b, uint8_t mode,
             const StatusFn& status) {
    auto dev = OpenKeyboard(hid);
    if (!dev) {
        // Reported rather than returned silently: with the keyboard absent the
        // edge zone produced no log line at all, so "the edge lighting does not
        // work" looked identical to the feature being broken.
        status("[EVision] Edge: keyboard not found");
        return false;
    }

    Query(*dev, CMD_BEGIN_CONFIG, 0, nullptr, 0, nullptr);
    hid.Sleep(20);

    const Rgb c = Corrected(zone, r, g, b);
    const uint8_t profile = ReadActiveProfile(*dev);

    // The side-LED block sits at a different offset per keyboard variant and
    // there is no reliable way to identify the variant, so all known layouts
    // are written; the ones that do not apply are ignored by the device.
    const uint16_t offsets[] = {
        static_cast<uint16_t>(ProfileOffset(profile) + 0x1A),  // standard Thyrus
        static_cast<uint16_t>(ProfileOffset(profile) + 0x15),  // some Omnis variants
        static_cast<uint16_t>(0x1E),                           // direct edge id
    };

    for (uint16_t off : offsets) {
        uint8_t edge_data[10] = {mode, 0x04, 0x02, 0x00, 0x00,
                                 c.r, c.g, c.b, 0x00, 0x01};
        Query(*dev, CMD_WRITE, off, edge_data, sizeof(edge_data), nullptr);
    }

    ClearKeyLock(*dev, profile);
    Query(*dev, CMD_END_CONFIG, 0, nullptr, 0, nullptr);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "[EVision] Edge set (Mode: 0x%02X)", mode);
    status(buf);
    return true;
}

}  // namespace evision
}  // namespace devices
