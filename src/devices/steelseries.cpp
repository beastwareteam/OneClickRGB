#include "steelseries.h"

#include <cstring>

namespace devices {
namespace steelseries {

void BuildColorPacket(uint8_t out[COLOR_PACKET_SIZE], int zone_index,
                      uint8_t r, uint8_t g, uint8_t b) {
    std::memset(out, 0, COLOR_PACKET_SIZE);
    out[0] = 0x1C;
    out[1] = 0x27;
    out[2] = 0x00;
    out[3] = static_cast<uint8_t>(1u << zone_index);  // zone bitmask
    out[4] = r;
    out[5] = g;
    out[6] = b;
}

void BuildSavePacket(uint8_t out[SAVE_PACKET_SIZE]) {
    std::memset(out, 0, SAVE_PACKET_SIZE);
    out[0] = 0x09;
}

bool SetColor(hal::IHidBackend& hid, const ChannelConfig& zone,
              uint8_t r, uint8_t g, uint8_t b, const StatusFn& status) {
    std::string path = hal::FindDevicePathByInterface(
        hid, ids::STEELSERIES_VID, ids::RIVAL_600_PID, CONTROL_INTERFACE);
    if (path.empty()) {
        status("[SteelSeries] Not found");
        return false;
    }
    auto dev = hid.Open(path);
    if (!dev) {
        status("[SteelSeries] Not found");
        return false;
    }

    const Rgb c = Corrected(zone, r, g, b);

    for (int i = 0; i < ZONE_COUNT; ++i) {
        uint8_t pkt[COLOR_PACKET_SIZE];
        BuildColorPacket(pkt, i, c.r, c.g, c.b);
        dev->Write(pkt, COLOR_WRITE_LEN);
        hid.Sleep(10);
    }

    uint8_t save[SAVE_PACKET_SIZE];
    BuildSavePacket(save);
    dev->Write(save, SAVE_WRITE_LEN);

    status("[SteelSeries] Rival 600 set");
    return true;
}

}  // namespace steelseries
}  // namespace devices
