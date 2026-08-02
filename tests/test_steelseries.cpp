/**
 * Tests for the SteelSeries Rival 600 protocol.
 */

#include "../src/devices/steelseries.h"
#include "fakes/fake_hid.h"
#include "test_framework.h"

using namespace devices;
using namespace devices::steelseries;

namespace {

fakes::FakeHidBackend MakeMouseBackend() {
    fakes::FakeHidBackend hid;
    // The control endpoint is on interface 0; the mouse also exposes other
    // interfaces that must not be picked.
    hid.AddDevice("mouse_if1", ids::STEELSERIES_VID, ids::RIVAL_600_PID, 0x0001, 1);
    hid.AddDevice("mouse_if0", ids::STEELSERIES_VID, ids::RIVAL_600_PID, 0x0001, 0);
    return hid;
}

}  // namespace

TEST(steelseries, colour_packet_layout) {
    uint8_t pkt[COLOR_PACKET_SIZE];
    BuildColorPacket(pkt, 0, 0xAA, 0xBB, 0xCC);

    CHECK_EQ((int)pkt[0], 0x1C);
    CHECK_EQ((int)pkt[1], 0x27);
    CHECK_EQ((int)pkt[2], 0x00);
    CHECK_EQ((int)pkt[3], 0x01);  // zone 0 bitmask
    CHECK_EQ((int)pkt[4], 0xAA);
    CHECK_EQ((int)pkt[5], 0xBB);
    CHECK_EQ((int)pkt[6], 0xCC);
}

TEST(steelseries, each_zone_gets_its_own_bit) {
    for (int zone = 0; zone < ZONE_COUNT; ++zone) {
        uint8_t pkt[COLOR_PACKET_SIZE];
        BuildColorPacket(pkt, zone, 0, 0, 0);
        CHECK_EQ((int)pkt[3], 1 << zone);
    }
}

TEST(steelseries, save_packet_is_the_persist_command) {
    uint8_t pkt[SAVE_PACKET_SIZE];
    BuildSavePacket(pkt);
    CHECK_EQ((int)pkt[0], 0x09);
    for (size_t i = 1; i < SAVE_PACKET_SIZE; ++i) CHECK_EQ((int)pkt[i], 0);
}

TEST(steelseries, control_endpoint_is_selected_by_interface_number) {
    auto hid = MakeMouseBackend();
    SetColor(hid, ChannelConfig(), 1, 2, 3, NullStatus);

    REQUIRE(hid.WriteCount() > 0);
    for (const auto& w : hid.Writes())
        CHECK_MSG(w.device_path == "mouse_if0", "wrote to the wrong USB interface");
}

TEST(steelseries, all_zones_are_written_then_persisted) {
    auto hid = MakeMouseBackend();
    const bool ok = SetColor(hid, ChannelConfig(), 0x11, 0x22, 0x33, NullStatus);
    CHECK(ok);

    // Eight zone packets plus one save packet.
    CHECK_EQ((int)hid.WriteCount(), ZONE_COUNT + 1);

    const auto& writes = hid.Writes();
    for (int i = 0; i < ZONE_COUNT; ++i) {
        CHECK_EQ((int)writes[i].data.size(), (int)COLOR_WRITE_LEN);
        CHECK_EQ((int)writes[i].data[3], 1 << i);
        CHECK_EQ((int)writes[i].data[4], 0x11);
    }
    CHECK_EQ((int)writes[ZONE_COUNT].data[0], 0x09);
    CHECK_EQ((int)writes[ZONE_COUNT].data.size(), (int)SAVE_WRITE_LEN);
}

TEST(steelseries, colour_correction_is_applied) {
    auto hid = MakeMouseBackend();
    ChannelConfig zone;
    zone.green_adjust = 0;

    SetColor(hid, zone, 100, 100, 100, NullStatus);
    REQUIRE(hid.WriteCount() > 0);
    CHECK_EQ((int)hid.Writes()[0].data[4], 100);  // red untouched
    CHECK_EQ((int)hid.Writes()[0].data[5], 0);    // green zeroed
}

TEST(steelseries, disabled_zone_sends_black) {
    auto hid = MakeMouseBackend();
    ChannelConfig zone;
    zone.enabled = false;

    SetColor(hid, zone, 200, 200, 200, NullStatus);
    REQUIRE(hid.WriteCount() > 0);
    CHECK_EQ((int)hid.Writes()[0].data[4], 0);
    CHECK_EQ((int)hid.Writes()[0].data[5], 0);
    CHECK_EQ((int)hid.Writes()[0].data[6], 0);
}

TEST(steelseries, missing_mouse_writes_nothing) {
    fakes::FakeHidBackend hid;
    std::vector<std::string> log;
    const bool ok = SetColor(hid, ChannelConfig(), 1, 2, 3,
                             [&](const std::string& s) { log.push_back(s); });
    CHECK(!ok);
    CHECK_EQ((int)hid.WriteCount(), 0);
    REQUIRE(!log.empty());
    CHECK(log[0].find("Not found") != std::string::npos);
}
