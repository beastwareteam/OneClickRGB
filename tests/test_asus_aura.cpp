/**
 * Tests for the ASUS Aura mainboard protocol.
 */

#include "../src/devices/asus_aura.h"
#include "fakes/fake_hid.h"
#include "test_framework.h"

using namespace devices;
using namespace devices::aura;

namespace {

HardwareConfig MakeConfig(uint8_t mainboard_leds, uint8_t rgb_headers,
                          uint8_t addressable_headers) {
    HardwareConfig cfg;
    cfg.config_table[0x1B] = mainboard_leds;
    cfg.config_table[0x1D] = rgb_headers;
    cfg.config_table[0x02] = addressable_headers;
    return cfg;
}

bool HasChannel(const HardwareConfig& cfg, int direct_channel) {
    for (int i = 0; i < cfg.num_channels; ++i)
        if (cfg.channels[i].direct_channel == direct_channel) return true;
    return false;
}

}  // namespace

//=============================================================================
// Config table parsing
//=============================================================================

TEST(aura_config, empty_table_yields_no_channels) {
    HardwareConfig cfg;
    ParseConfig(cfg);
    CHECK(!cfg.valid);
    CHECK_EQ(cfg.num_channels, 0);
}

TEST(aura_config, mainboard_leds_create_the_mainboard_channel) {
    HardwareConfig cfg = MakeConfig(12, 0, 0);
    ParseConfig(cfg);

    CHECK(cfg.valid);
    CHECK_EQ(cfg.num_mainboard_leds, 12);
    CHECK(HasChannel(cfg, 0x04));

    // The mainboard zone must carry the reported LED count, not a guess.
    for (int i = 0; i < cfg.num_channels; ++i)
        if (cfg.channels[i].direct_channel == 0x04 && i == 0)
            CHECK_EQ(cfg.channels[i].led_count, 12);
}

TEST(aura_config, fewer_leds_than_headers_disables_the_header_channels) {
    // A board reporting 2 LEDs but 5 RGB headers is inconsistent; trusting the
    // header count would address channels that do not exist.
    HardwareConfig cfg = MakeConfig(2, 5, 0);
    ParseConfig(cfg);
    CHECK_EQ(cfg.num_rgb_headers, 0);
}

TEST(aura_config, addressable_headers_get_120_led_capacity) {
    HardwareConfig cfg = MakeConfig(0, 0, 2);
    ParseConfig(cfg);

    int addressable = 0;
    for (int i = 0; i < cfg.num_channels; ++i) {
        if (cfg.channels[i].addressable) {
            ++addressable;
            CHECK_EQ(cfg.channels[i].led_count, 120);
        }
    }
    CHECK_EQ(addressable, 2);
}

TEST(aura_config, channel_count_never_exceeds_the_array) {
    // A garbage table must not walk off the end of channels[].
    HardwareConfig cfg = MakeConfig(255, 255, 255);
    ParseConfig(cfg);
    CHECK(cfg.num_channels <= MAX_CHANNELS);
}

TEST(aura_config, extra_zones_are_not_duplicated) {
    HardwareConfig cfg = MakeConfig(30, 0, 0);
    ParseConfig(cfg);

    for (int i = 0; i < cfg.num_channels; ++i)
        for (int j = i + 1; j < cfg.num_channels; ++j)
            CHECK(cfg.channels[i].direct_channel != cfg.channels[j].direct_channel);
}

//=============================================================================
// Packet construction
//=============================================================================

TEST(aura_packet, direct_packet_header_and_colour_layout) {
    uint8_t pkt[PACKET_SIZE];
    BuildDirectPacket(pkt, 3, 0, 2, 0x11, 0x22, 0x33, false);

    CHECK_EQ((int)pkt[0x00], (int)REPORT_ID);
    CHECK_EQ((int)pkt[0x01], (int)REQ_DIRECT);
    CHECK_EQ((int)pkt[0x02], 3);      // channel, no last-packet bit
    CHECK_EQ((int)pkt[0x03], 0);      // offset
    CHECK_EQ((int)pkt[0x04], 2);      // count
    CHECK_EQ((int)pkt[0x05], 0x11);
    CHECK_EQ((int)pkt[0x06], 0x22);
    CHECK_EQ((int)pkt[0x07], 0x33);
    CHECK_EQ((int)pkt[0x08], 0x11);   // second LED
}

TEST(aura_packet, last_packet_sets_the_high_bit_on_the_channel) {
    uint8_t pkt[PACKET_SIZE];
    BuildDirectPacket(pkt, 3, 0, 1, 0, 0, 0, true);
    CHECK_EQ((int)pkt[0x02], 0x83);
}

TEST(aura_packet, colour_data_stays_inside_the_packet) {
    uint8_t pkt[PACKET_SIZE];
    // 20 LEDs * 3 bytes + 5 header bytes = 65, exactly the packet size.
    BuildDirectPacket(pkt, 0, 0, LEDS_PER_PACKET, 0xAB, 0xCD, 0xEF, true);
    CHECK_EQ((int)pkt[0x04], LEDS_PER_PACKET);
    CHECK_EQ((int)pkt[PACKET_SIZE - 1], 0xEF);
}

TEST(aura_packet, gen1_handshake_bytes) {
    uint8_t pkt[PACKET_SIZE];
    BuildGen1Packet(pkt);
    CHECK_EQ((int)pkt[0x00], (int)REPORT_ID);
    CHECK_EQ((int)pkt[0x01], (int)REQ_GEN1);
    CHECK_EQ((int)pkt[0x02], 0x53);
    CHECK_EQ((int)pkt[0x04], 0x01);
}

//=============================================================================
// Channel transmission
//=============================================================================

TEST(aura_transfer, long_channels_are_split_into_chunks) {
    fakes::FakeHidBackend hid;
    hid.AddDevice("aura0", ids::ASUS_VID, ids::ASUS_AURA_PID, ids::ASUS_USAGE_PAGE);
    auto dev = hid.Open("aura0");
    REQUIRE(dev != nullptr);

    SetChannel(hid, *dev, 1, 50, 1, 2, 3);

    // 50 LEDs at 20 per packet: 3 colour packets, plus one mode packet.
    const auto colour = hid.WritesWithByte(0x01, REQ_DIRECT);
    CHECK_EQ((int)colour.size(), 3);
    CHECK_EQ((int)hid.WritesWithByte(0x01, REQ_MODE).size(), 1);

    CHECK_EQ((int)colour[0]->data[0x03], 0);   // offsets 0, 20, 40
    CHECK_EQ((int)colour[1]->data[0x03], 20);
    CHECK_EQ((int)colour[2]->data[0x03], 40);

    CHECK_EQ((int)colour[0]->data[0x04], 20);  // counts 20, 20, 10
    CHECK_EQ((int)colour[1]->data[0x04], 20);
    CHECK_EQ((int)colour[2]->data[0x04], 10);

    // Only the final chunk carries the last-packet bit.
    CHECK_EQ((int)(colour[0]->data[0x02] & 0x80), 0);
    CHECK_EQ((int)(colour[1]->data[0x02] & 0x80), 0);
    CHECK_EQ((int)(colour[2]->data[0x02] & 0x80), 0x80);
}

TEST(aura_transfer, a_single_chunk_is_also_marked_last) {
    fakes::FakeHidBackend hid;
    hid.AddDevice("aura0", ids::ASUS_VID, ids::ASUS_AURA_PID, ids::ASUS_USAGE_PAGE);
    auto dev = hid.Open("aura0");
    REQUIRE(dev != nullptr);

    SetChannel(hid, *dev, 0, 5, 0, 0, 0);
    const auto colour = hid.WritesWithByte(0x01, REQ_DIRECT);
    REQUIRE(colour.size() == 1);
    CHECK_EQ((int)(colour[0]->data[0x02] & 0x80), 0x80);
}

TEST(aura_transfer, disabled_zones_are_skipped) {
    fakes::FakeHidBackend hid;
    hid.AddDevice("aura0", ids::ASUS_VID, ids::ASUS_AURA_PID, ids::ASUS_USAGE_PAGE);

    HardwareConfig cfg = MakeConfig(10, 0, 0);
    ParseConfig(cfg);

    ChannelConfig zones[MAX_CHANNELS];
    for (auto& z : zones) z.enabled = false;

    const int set = SetAll(hid, cfg, zones, MAX_CHANNELS, 1, 2, 3, NullStatus);
    CHECK_EQ(set, 0);
    CHECK_EQ((int)hid.WritesWithByte(0x01, REQ_DIRECT).size(), 0);
}

TEST(aura_transfer, missing_device_reports_minus_one) {
    fakes::FakeHidBackend hid;  // nothing registered
    HardwareConfig cfg;
    ChannelConfig zones[MAX_CHANNELS];
    CHECK_EQ(SetAll(hid, cfg, zones, MAX_CHANNELS, 1, 2, 3, NullStatus), -1);
}

TEST(aura_transfer, per_zone_correction_is_applied) {
    fakes::FakeHidBackend hid;
    hid.AddDevice("aura0", ids::ASUS_VID, ids::ASUS_AURA_PID, ids::ASUS_USAGE_PAGE);

    HardwareConfig cfg = MakeConfig(1, 0, 0);
    ParseConfig(cfg);

    ChannelConfig zones[MAX_CHANNELS];
    for (auto& z : zones) z.enabled = false;
    zones[0].enabled = true;
    zones[0].brightness = 50;  // half brightness on the first zone

    SetAll(hid, cfg, zones, MAX_CHANNELS, 200, 200, 200, NullStatus);

    const auto colour = hid.WritesWithByte(0x01, REQ_DIRECT);
    REQUIRE(!colour.empty());
    CHECK_EQ((int)colour[0]->data[0x05], 100);
}
