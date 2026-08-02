/**
 * Tests for the EVision keyboard protocol.
 *
 * The key-lock cases pin down the bug that left the Windows key disabled: the
 * unlock register was addressed absolutely instead of relative to the active
 * onboard profile.
 */

#include "../src/devices/evision.h"
#include "fakes/fake_hid.h"
#include "test_framework.h"

using namespace devices;
using namespace devices::evision;

namespace {

/// A well-formed response packet: report id, no error in byte 7, payload size.
std::vector<uint8_t> MakeResponse(uint8_t payload_size,
                                  std::vector<uint8_t> payload = {}) {
    std::vector<uint8_t> pkt(V2_PACKET_SIZE, 0);
    pkt[0] = V2_REPORT_ID;
    pkt[4] = payload_size;
    pkt[7] = 0;  // status OK
    for (size_t i = 0; i < payload.size() && 8 + i < pkt.size(); ++i)
        pkt[8 + i] = payload[i];
    return pkt;
}

fakes::FakeHidBackend MakeKeyboardBackend() {
    fakes::FakeHidBackend hid;
    hid.AddDevice("kbd0", ids::EVISION_VID, ids::EVISION_PID, ids::EVISION_USAGE_PAGE);
    return hid;
}

/// Reads the 16-bit offset field out of a recorded request packet.
uint16_t OffsetOf(const std::vector<uint8_t>& packet) {
    return static_cast<uint16_t>(packet[5] | (packet[6] << 8));
}

}  // namespace

//=============================================================================
// Packet framing
//=============================================================================

TEST(evision, packet_carries_command_offset_and_size) {
    uint8_t pkt[V2_PACKET_SIZE];
    const uint8_t payload[2] = {0xAA, 0xBB};
    BuildPacket(pkt, CMD_WRITE, 0x1234, payload, 2);

    CHECK_EQ((int)pkt[0], (int)V2_REPORT_ID);
    CHECK_EQ((int)pkt[3], (int)CMD_WRITE);
    CHECK_EQ((int)pkt[4], 2);
    CHECK_EQ((int)pkt[5], 0x34);  // offset low byte first
    CHECK_EQ((int)pkt[6], 0x12);
    CHECK_EQ((int)pkt[8], 0xAA);
    CHECK_EQ((int)pkt[9], 0xBB);
}

TEST(evision, packet_checksum_covers_bytes_3_to_63) {
    uint8_t pkt[V2_PACKET_SIZE];
    const uint8_t payload[2] = {0x11, 0x22};
    BuildPacket(pkt, CMD_WRITE, 0x0014, payload, 2);

    uint16_t expected = 0;
    for (int i = 3; i < V2_PACKET_SIZE; ++i) expected += pkt[i];

    const uint16_t actual = static_cast<uint16_t>(pkt[1] | (pkt[2] << 8));
    CHECK_EQ((int)actual, (int)expected);
}

TEST(evision, oversized_payload_cannot_overrun_the_packet) {
    uint8_t pkt[V2_PACKET_SIZE];
    uint8_t payload[200];
    std::memset(payload, 0xFF, sizeof(payload));

    // size is a uint8_t, so a caller can legally ask for more than fits.
    BuildPacket(pkt, CMD_WRITE, 0, payload, 255);

    // Nothing to assert beyond "did not crash and stayed in bounds"; ASan/the
    // /GS canary would catch an overrun. Confirm the tail was still written.
    CHECK_EQ((int)pkt[V2_PACKET_SIZE - 1], 0xFF);
}

//=============================================================================
// Key-lock addressing - the Windows-key bug
//=============================================================================

TEST(evision_keylock, lock_register_is_relative_to_the_active_profile) {
    // Profile 0 happens to land on 0x14, which is why the old absolute
    // address appeared to work as long as profile 0 was active - and why the
    // problem looked intermittent rather than like a plain bug.
    CHECK_EQ((int)LockOffset(0), 0x14);
    CHECK_EQ((int)LockOffset(1), 0x54);
    CHECK_EQ((int)LockOffset(2), 0x94);
}

TEST(evision_keylock, unlock_reaches_a_different_address_on_each_profile) {
    // The whole defect was one address serving all three profiles.
    CHECK(LockOffset(0) != LockOffset(1));
    CHECK(LockOffset(1) != LockOffset(2));
    CHECK(LockOffset(0) != LockOffset(2));
}

TEST(evision_keylock, lock_register_sits_inside_its_own_profile_block) {
    for (uint8_t p = 0; p < PROFILE_COUNT; ++p) {
        const uint16_t base = ProfileOffset(p);
        const uint16_t lock = LockOffset(p);
        CHECK(lock >= base);
        CHECK(lock < base + PROFILE_STRIDE);
    }
}

TEST(evision_keylock, lock_register_does_not_collide_with_the_18_byte_config) {
    // The colour/effect block is 18 bytes at ProfileOffset(); if the lock
    // register fell inside it, every apply would clobber the lock state.
    constexpr uint16_t kConfigSize = 18;
    for (uint8_t p = 0; p < PROFILE_COUNT; ++p) {
        const uint16_t config_end = ProfileOffset(p) + kConfigSize;
        CHECK(LockOffset(p) >= config_end);
    }
}

TEST(evision_keylock, unlock_writes_zero_to_the_active_profiles_register) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    // Write ack, then the read-back reporting both lock bytes clear.
    hid.QueueResponse(MakeResponse(0));
    hid.QueueResponse(MakeResponse(LOCK_SIZE, {0x00, 0x00}));

    const UnlockResult result = ClearKeyLock(*dev, 2);
    CHECK(result == UnlockResult::Unlocked);

    const auto writes = hid.Writes();
    REQUIRE(writes.size() >= 1);

    // First write must be the unlock, aimed at profile 2's register.
    const auto& unlock = writes[0].data;
    CHECK_EQ((int)unlock[3], (int)CMD_WRITE);
    CHECK_EQ((int)OffsetOf(unlock), (int)LockOffset(2));
    CHECK_EQ((int)unlock[4], (int)LOCK_SIZE);
    CHECK_EQ((int)unlock[8], 0x00);
    CHECK_EQ((int)unlock[9], 0x00);
}

TEST(evision_keylock, readback_showing_the_winkey_bit_reports_still_locked) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    hid.QueueResponse(MakeResponse(0));
    // Device insists the Windows-key lock bit is still set.
    hid.QueueResponse(MakeResponse(LOCK_SIZE, {LOCK_BIT_WINKEY, 0x00}));

    CHECK(ClearKeyLock(*dev, 0) == UnlockResult::StillLocked);
}

TEST(evision_keylock, silent_device_reports_unverified_rather_than_success) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    hid.QueueResponse(MakeResponse(0));
    // No response queued for the read-back.

    CHECK(ClearKeyLock(*dev, 0) == UnlockResult::Unverified);
}

TEST(evision_keylock, failed_write_is_reported_as_such) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);
    hid.SetWriteFails(true);

    CHECK(ClearKeyLock(*dev, 0) == UnlockResult::WriteFailed);
}

TEST(evision_keylock, out_of_range_profile_falls_back_to_profile_zero) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    hid.QueueResponse(MakeResponse(0));
    hid.QueueResponse(MakeResponse(LOCK_SIZE, {0x00, 0x00}));

    ClearKeyLock(*dev, 99);
    REQUIRE(hid.WriteCount() >= 1);
    CHECK_EQ((int)OffsetOf(hid.Writes()[0].data), (int)LockOffset(0));
}

//=============================================================================
// Profile selection
//=============================================================================

TEST(evision, active_profile_is_read_from_the_device) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    hid.QueueResponse(MakeResponse(1, {2}));
    CHECK_EQ((int)ReadActiveProfile(*dev), 2);
}

TEST(evision, implausible_profile_index_is_clamped) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    hid.QueueResponse(MakeResponse(1, {77}));
    CHECK_EQ((int)ReadActiveProfile(*dev), 0);
}

TEST(evision, silent_device_yields_profile_zero) {
    auto hid = MakeKeyboardBackend();
    auto dev = hid.Open("kbd0");
    REQUIRE(dev != nullptr);

    CHECK_EQ((int)ReadActiveProfile(*dev), 0);
}

//=============================================================================
// SetKeyboard end to end
//=============================================================================

TEST(evision, set_keyboard_always_clears_the_key_lock) {
    auto hid = MakeKeyboardBackend();

    hid.QueueResponse(MakeResponse(0));           // begin config
    hid.QueueResponse(MakeResponse(1, {1}));      // active profile = 1
    hid.QueueResponse(MakeResponse(0));           // config write ack
    hid.QueueResponse(MakeResponse(0));           // unlock write ack
    hid.QueueResponse(MakeResponse(LOCK_SIZE, {0x00, 0x00}));  // read-back
    hid.QueueResponse(MakeResponse(0));           // end config

    ChannelConfig zone;
    KeyboardSettings settings;
    settings.mode = KB_MODE_STATIC;

    std::vector<std::string> log;
    const bool ok = SetKeyboard(hid, zone, 10, 20, 30, settings,
                                [&](const std::string& s) { log.push_back(s); });
    CHECK(ok);

    // Spelled out rather than derived from LockOffset(), so this test is not
    // comparing the implementation against itself: profile 1's block starts at
    // 1 * 0x40 + 0x01, and the lock register sits 0x13 into it.
    constexpr uint16_t kExpectedLockOffset = 1 * 0x40 + 0x01 + 0x13;  // 0x54

    bool found = false;
    for (const auto& w : hid.Writes()) {
        if (w.data[3] == CMD_WRITE && OffsetOf(w.data) == kExpectedLockOffset &&
            w.data[4] == LOCK_SIZE && w.data[8] == 0 && w.data[9] == 0) {
            found = true;
        }
    }
    CHECK_MSG(found,
              "no key-lock clear addressed at the active profile - this is the "
              "regression that left the Windows key disabled on profiles 1 and 2");
}

TEST(evision, set_keyboard_reports_the_verified_lock_state) {
    auto hid = MakeKeyboardBackend();

    hid.QueueResponse(MakeResponse(0));
    hid.QueueResponse(MakeResponse(1, {0}));
    hid.QueueResponse(MakeResponse(0));
    hid.QueueResponse(MakeResponse(0));
    hid.QueueResponse(MakeResponse(LOCK_SIZE, {LOCK_BIT_WINKEY, 0x00}));
    hid.QueueResponse(MakeResponse(0));

    ChannelConfig zone;
    std::vector<std::string> log;
    SetKeyboard(hid, zone, 1, 2, 3, KeyboardSettings(),
                [&](const std::string& s) { log.push_back(s); });

    bool warned = false;
    for (const auto& line : log)
        if (line.find("STILL LOCKED") != std::string::npos) warned = true;
    CHECK_MSG(warned, "a rejected unlock must be surfaced to the user");
}

TEST(evision, colour_correction_reaches_the_config_block) {
    auto hid = MakeKeyboardBackend();
    for (int i = 0; i < 6; ++i) hid.QueueResponse(MakeResponse(0));

    ChannelConfig zone;
    zone.red_adjust = 50;   // half red
    zone.brightness = 100;

    SetKeyboard(hid, zone, 200, 100, 50, KeyboardSettings(), NullStatus);

    // The 18-byte config write carries the corrected triple at payload 5..7.
    const auto candidates = hid.WritesWithByte(4, 18);
    REQUIRE(!candidates.empty());
    const auto& cfg = candidates[0]->data;
    CHECK_EQ((int)cfg[8 + 5], 100);  // 200 * 50% = 100
    CHECK_EQ((int)cfg[8 + 6], 100);
    CHECK_EQ((int)cfg[8 + 7], 50);
}

TEST(evision, missing_keyboard_is_reported_and_writes_nothing) {
    fakes::FakeHidBackend hid;  // no devices registered

    std::vector<std::string> log;
    const bool ok = SetKeyboard(hid, ChannelConfig(), 1, 2, 3, KeyboardSettings(),
                                [&](const std::string& s) { log.push_back(s); });
    CHECK(!ok);
    CHECK_EQ((int)hid.WriteCount(), 0);
    REQUIRE(!log.empty());
    CHECK(log[0].find("not found") != std::string::npos);
}

//=============================================================================
// Mode validity
//
// The app persists these bytes and hands them straight to the device, so a
// value the protocol does not define must never survive a round trip through
// the settings. A zero-initialised setting used to do exactly that: 0 is not a
// keyboard mode at all, and as an edge mode it means FREEZE.
//=============================================================================

TEST(evision, zero_is_not_a_keyboard_mode) {
    CHECK(!IsValidKeyboardMode(0x00));
}

TEST(evision, gaps_in_the_keyboard_mode_range_are_rejected) {
    // The range is not contiguous, so a bounds check would wrongly accept these.
    CHECK(!IsValidKeyboardMode(0x09));
    CHECK(!IsValidKeyboardMode(0x0B));
    CHECK(!IsValidKeyboardMode(0x0E));
    CHECK(!IsValidKeyboardMode(0xFF));
}

TEST(evision, every_named_keyboard_mode_is_accepted) {
    const uint8_t all[] = {KB_MODE_WAVE_SHORT, KB_MODE_WAVE_LONG, KB_MODE_COLOR_WHEEL,
                           KB_MODE_SPECTRUM, KB_MODE_BREATHING, KB_MODE_STATIC,
                           KB_MODE_REACTIVE, KB_MODE_RIPPLE, KB_MODE_STARLIGHT,
                           KB_MODE_RAINBOW, KB_MODE_HURRICANE};
    for (uint8_t mode : all) CHECK(IsValidKeyboardMode(mode));
}

TEST(evision, edge_modes_stop_after_off) {
    CHECK(IsValidEdgeMode(EDGE_MODE_FREEZE));
    CHECK(IsValidEdgeMode(EDGE_MODE_OFF));
    CHECK(!IsValidEdgeMode(EDGE_MODE_OFF + 1));
    CHECK(!IsValidEdgeMode(0xFF));
}

TEST(evision, the_defaults_are_modes_the_device_implements) {
    CHECK(IsValidKeyboardMode(KB_MODE_DEFAULT));
    CHECK(IsValidEdgeMode(EDGE_MODE_DEFAULT));
    // Static, specifically - the default must be a plain colour, not an effect.
    CHECK_EQ((int)KB_MODE_DEFAULT, (int)KB_MODE_STATIC);
    CHECK_EQ((int)EDGE_MODE_DEFAULT, (int)EDGE_MODE_STATIC);
}

TEST(evision, the_mode_a_setting_carries_is_the_byte_that_goes_out) {
    // Guards the index/value confusion: SetEdge must transmit the mode it was
    // given, not translate it. EDGE_MODE_OFF is 0x05 while its position in the
    // UI list is 4 - sending the index turned "Off" into STATIC.
    auto hid = MakeKeyboardBackend();
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(0));

    SetEdge(hid, ChannelConfig(), 10, 20, 30, EDGE_MODE_OFF, 4, 2, NullStatus);

    const auto edge_writes = hid.WritesWithByte(4, 10);  // the 10-byte payload
    REQUIRE(!edge_writes.empty());
    CHECK_EQ((int)edge_writes[0]->data[8], (int)EDGE_MODE_OFF);
}

TEST(evision, missing_keyboard_is_reported_for_the_edge_zone_too) {
    // The main matrix said "not found" while the edge zone returned silently,
    // so a user with no keyboard attached saw nothing about the edge at all.
    fakes::FakeHidBackend hid;  // no devices registered

    std::vector<std::string> log;
    const bool ok = SetEdge(hid, ChannelConfig(), 1, 2, 3, EDGE_MODE_STATIC, 4, 2,
                            [&](const std::string& s) { log.push_back(s); });
    CHECK(!ok);
    CHECK_EQ((int)hid.WriteCount(), 0);
    REQUIRE(!log.empty());
    CHECK(log[0].find("not found") != std::string::npos);
}

//=============================================================================
// Edge (side LED) parameter block
//
// Every keyboard here carries VID 0x3299 - SPC Gear / ENDORFY - whose edge
// parameters live at 0x1a..0x23 inside the active profile block. The previous
// implementation wrote that block and then wrote twice more at profile+0x15 and
// at an absolute 0x1E, both of which overlap it, so the parameters it had just
// written were overwritten with misaligned bytes and the side lighting never
// followed the colour or the mode.
//=============================================================================

namespace {

/// The edge write is the one carrying a payload of EDGE_BLOCK_SIZE bytes.
const std::vector<uint8_t>* FindEdgeWrite(fakes::FakeHidBackend& hid) {
    const auto writes = hid.WritesWithByte(4, EDGE_BLOCK_SIZE);
    return writes.empty() ? nullptr : &writes[0]->data;
}

}  // namespace

TEST(evision, edge_is_written_once_to_the_profile_edge_block) {
    auto hid = MakeKeyboardBackend();
    // The profile read answers 0, so the block belongs to profile 0.
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(1, {0}));

    SetEdge(hid, ChannelConfig(), 1, 2, 3, EDGE_MODE_STATIC, 4, 2, NullStatus);

    const auto edge_writes = hid.WritesWithByte(4, EDGE_BLOCK_SIZE);
    CHECK_EQ((int)edge_writes.size(), 1);
    REQUIRE(!edge_writes.empty());
    CHECK_EQ((int)OffsetOf(edge_writes[0]->data), (int)EdgeOffset(0));
}

TEST(evision, no_write_lands_on_the_offsets_that_used_to_clobber_the_block) {
    auto hid = MakeKeyboardBackend();
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(1, {0}));

    SetEdge(hid, ChannelConfig(), 1, 2, 3, EDGE_MODE_STATIC, 4, 2, NullStatus);

    for (const auto& write : hid.Writes()) {
        if (write.data.size() < 8) continue;
        const uint16_t offset = OffsetOf(write.data);
        CHECK(offset != (uint16_t)(ProfileOffset(0) + 0x15));
        CHECK(offset != (uint16_t)0x1E);
    }
}

TEST(evision, the_edge_block_carries_mode_brightness_speed_and_colour) {
    auto hid = MakeKeyboardBackend();
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(1, {0}));

    SetEdge(hid, ChannelConfig(), 0x11, 0x22, 0x33, EDGE_MODE_BREATHING, 3, 5, NullStatus);

    const auto* block = FindEdgeWrite(hid);
    REQUIRE(block != nullptr);
    const uint8_t* p = block->data() + 8;  // payload starts after the header
    CHECK_EQ((int)p[EDGE_INDEX_MODE],       (int)EDGE_MODE_BREATHING);
    CHECK_EQ((int)p[EDGE_INDEX_BRIGHTNESS], 3);
    CHECK_EQ((int)p[EDGE_INDEX_SPEED],      5);
    CHECK_EQ((int)p[EDGE_INDEX_RANDOM],     0);
    CHECK_EQ((int)p[EDGE_INDEX_COLOUR + 0], 0x11);
    CHECK_EQ((int)p[EDGE_INDEX_COLOUR + 1], 0x22);
    CHECK_EQ((int)p[EDGE_INDEX_COLOUR + 2], 0x33);
    CHECK_EQ((int)p[EDGE_INDEX_ON_OFF],     1);
}

TEST(evision, switching_the_edge_off_also_clears_its_on_off_byte) {
    auto hid = MakeKeyboardBackend();
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(1, {0}));

    SetEdge(hid, ChannelConfig(), 255, 255, 255, EDGE_MODE_OFF, 4, 2, NullStatus);

    const auto* block = FindEdgeWrite(hid);
    REQUIRE(block != nullptr);
    const uint8_t* p = block->data() + 8;
    CHECK_EQ((int)p[EDGE_INDEX_MODE],   (int)EDGE_MODE_OFF);
    CHECK_EQ((int)p[EDGE_INDEX_ON_OFF], 0);
}

TEST(evision, the_edge_block_follows_the_active_profile) {
    auto hid = MakeKeyboardBackend();
    // Active profile 2 - the block moves by two strides of 0x40.
    for (int i = 0; i < 8; ++i) hid.QueueResponse(MakeResponse(1, {2}));

    SetEdge(hid, ChannelConfig(), 1, 2, 3, EDGE_MODE_STATIC, 4, 2, NullStatus);

    const auto* block = FindEdgeWrite(hid);
    REQUIRE(block != nullptr);
    CHECK_EQ((int)OffsetOf(*block), (int)EdgeOffset(2));
}
