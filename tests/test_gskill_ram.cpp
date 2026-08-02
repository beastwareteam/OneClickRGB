/**
 * Tests for the G.Skill Trident Z5 SMBus protocol.
 *
 * This is the device that cannot be exercised on CI (kernel driver, admin
 * rights), which is exactly why its register sequence is pinned down here.
 */

#include "../src/devices/gskill_ram.h"
#include "fakes/fake_smbus.h"
#include "test_framework.h"

using namespace devices;
using namespace devices::gskill;

namespace {

fakes::FakeSmbusBackend MakeBusWithModule(uint8_t addr = 0x70,
                                          const char* name = "Trident Z5",
                                          uint8_t leds = 8) {
    fakes::FakeSmbusBackend bus;
    bus.AddModule(addr, name, leds);
    bus.Open();
    return bus;
}

}  // namespace

//=============================================================================
// Colour encoding - the easiest thing to get subtly wrong
//=============================================================================

TEST(gskill, led_triple_is_encoded_red_blue_green) {
    // The ENE controller does not use RGB order. Swapping these silently
    // exchanges green and blue on the RAM only, while every other device is
    // correct - a symptom that is easy to misdiagnose as a colour-correction
    // problem.
    uint8_t triple[3];
    EncodeLedTriple(triple, 0x11, 0x22, 0x33);
    CHECK_EQ((int)triple[0], 0x11);  // red
    CHECK_EQ((int)triple[1], 0x33);  // blue
    CHECK_EQ((int)triple[2], 0x22);  // green
}

TEST(gskill, led_registers_are_three_bytes_apart) {
    CHECK_EQ((int)LedRegister(0), (int)REG_LED_BASE);
    CHECK_EQ((int)LedRegister(1), (int)REG_LED_BASE + 3);
    CHECK_EQ((int)LedRegister(7), (int)REG_LED_BASE + 21);
}

//=============================================================================
// Module identification
//=============================================================================

TEST(gskill, recognises_the_known_module_names) {
    CHECK(IsSupportedModuleName("Trident Z5"));
    CHECK(IsSupportedModuleName("AUDA0000"));
    CHECK(IsSupportedModuleName("DIMM_LED"));
}

TEST(gskill, rejects_unrelated_devices) {
    CHECK(!IsSupportedModuleName("Corsair"));
    CHECK(!IsSupportedModuleName(""));
    CHECK(!IsSupportedModuleName(nullptr));
}

//=============================================================================
// Register access
//=============================================================================

TEST(gskill, ene_write_selects_the_byte_swapped_register_then_writes) {
    auto bus = MakeBusWithModule();
    EneWrite(bus, 0x70, 0x8100, 0x42);

    CHECK_EQ((int)bus.Register(0x70, 0x8100), 0x42);

    // The selector must go out byte-swapped as a word write to command 0x00.
    bool found_selector = false;
    for (const auto& t : bus.Transfers()) {
        if (t.rw == hal::SmbusRw::Write && t.cmd == 0x00 &&
            t.size == hal::SMBUS_WORD_DATA && t.word == 0x0081) {
            found_selector = true;
        }
    }
    CHECK_MSG(found_selector, "register selector was not byte-swapped");
}

TEST(gskill, ene_read_returns_what_the_module_holds) {
    auto bus = MakeBusWithModule();
    EneWrite(bus, 0x70, 0x1234, 0x99);
    CHECK_EQ((int)EneRead(bus, 0x70, 0x1234), 0x99);
}

//=============================================================================
// SetColor
//=============================================================================

TEST(gskill, writes_every_led_and_latches_the_result) {
    auto bus = MakeBusWithModule(0x70, "Trident Z5", 4);
    ChannelConfig zones[MAX_SLOTS];

    const Result r = SetColor(bus, zones, MAX_SLOTS, 0x10, 0x20, 0x30, NullStatus);
    CHECK_EQ(r.modules_set, 1);

    for (int i = 0; i < 4; ++i) {
        const uint16_t reg = LedRegister(i);
        CHECK_EQ((int)bus.Register(0x70, reg + 0), 0x10);  // R
        CHECK_EQ((int)bus.Register(0x70, reg + 1), 0x30);  // B
        CHECK_EQ((int)bus.Register(0x70, reg + 2), 0x20);  // G
    }

    CHECK_MSG(bus.RegisterWasWritten(0x70, REG_DIRECT_ON),
              "direct mode was never enabled");
    CHECK_MSG(bus.RegisterWasWritten(0x70, REG_APPLY),
              "colours were written but never latched");
}

TEST(gskill, implausible_led_count_falls_back_to_the_default) {
    auto bus = MakeBusWithModule(0x70, "Trident Z5", 200);
    ChannelConfig zones[MAX_SLOTS];
    SetColor(bus, zones, MAX_SLOTS, 1, 2, 3, NullStatus);

    // Exactly DEFAULT_LED_COUNT triples, no more.
    CHECK(bus.RegisterWasWritten(0x70, LedRegister(DEFAULT_LED_COUNT - 1)));
    CHECK(!bus.RegisterWasWritten(0x70, LedRegister(DEFAULT_LED_COUNT)));
}

TEST(gskill, zero_led_count_falls_back_to_the_default) {
    auto bus = MakeBusWithModule(0x70, "Trident Z5", 0);
    ChannelConfig zones[MAX_SLOTS];
    SetColor(bus, zones, MAX_SLOTS, 1, 2, 3, NullStatus);
    CHECK(bus.RegisterWasWritten(0x70, LedRegister(0)));
    CHECK(bus.RegisterWasWritten(0x70, LedRegister(DEFAULT_LED_COUNT - 1)));
}

TEST(gskill, several_modules_are_all_addressed) {
    fakes::FakeSmbusBackend bus;
    bus.AddModule(0x70, "Trident Z5", 2);
    bus.AddModule(0x72, "Trident Z5", 2);
    bus.Open();

    ChannelConfig zones[MAX_SLOTS];
    const Result r = SetColor(bus, zones, MAX_SLOTS, 5, 6, 7, NullStatus);
    CHECK_EQ(r.modules_set, 2);
    CHECK_EQ((int)bus.Register(0x70, LedRegister(0)), 5);
    CHECK_EQ((int)bus.Register(0x72, LedRegister(0)), 5);
}

TEST(gskill, foreign_devices_on_the_bus_are_left_alone) {
    fakes::FakeSmbusBackend bus;
    bus.AddModule(0x70, "SomeOtherChip", 8);
    bus.Open();

    ChannelConfig zones[MAX_SLOTS];
    const Result r = SetColor(bus, zones, MAX_SLOTS, 1, 2, 3, NullStatus);
    CHECK_EQ(r.modules_set, 0);
    CHECK_MSG(!bus.RegisterWasWritten(0x70, REG_APPLY),
              "wrote colour registers to an unidentified SMBus device");
}

TEST(gskill, per_slot_correction_is_applied) {
    fakes::FakeSmbusBackend bus;
    bus.AddModule(0x70, "Trident Z5", 1);
    bus.AddModule(0x71, "Trident Z5", 1);
    bus.Open();

    ChannelConfig zones[MAX_SLOTS];
    zones[1].brightness = 50;  // dim the second module only

    SetColor(bus, zones, MAX_SLOTS, 200, 200, 200, NullStatus);
    CHECK_EQ((int)bus.Register(0x70, LedRegister(0)), 200);
    CHECK_EQ((int)bus.Register(0x71, LedRegister(0)), 100);
}

TEST(gskill, disabled_slot_is_not_written) {
    auto bus = MakeBusWithModule(0x70, "Trident Z5", 2);
    ChannelConfig zones[MAX_SLOTS];
    zones[0].enabled = false;

    const Result r = SetColor(bus, zones, MAX_SLOTS, 1, 2, 3, NullStatus);
    CHECK_EQ(r.modules_set, 0);
    CHECK(!bus.RegisterWasWritten(0x70, REG_APPLY));
}

TEST(gskill, empty_bus_reports_no_modules) {
    fakes::FakeSmbusBackend bus;
    bus.Open();
    ChannelConfig zones[MAX_SLOTS];

    std::vector<std::string> log;
    const Result r = SetColor(bus, zones, MAX_SLOTS, 1, 2, 3,
                              [&](const std::string& s) { log.push_back(s); });
    CHECK_EQ(r.modules_set, 0);
    REQUIRE(!log.empty());
    CHECK(log[0].find("No RAM modules") != std::string::npos);
}

//=============================================================================
// Driver availability diagnostics
//=============================================================================

TEST(gskill_diagnostics, every_failure_mode_has_a_distinct_message) {
    const hal::SmbusError errors[] = {
        hal::SmbusError::LibraryMissing,   hal::SmbusError::LibraryInvalid,
        hal::SmbusError::DriverNotRunning, hal::SmbusError::ModuleMissing,
        hal::SmbusError::ModuleLoadFailed,
    };
    for (auto a : errors) {
        const std::string text = hal::SmbusErrorText(a);
        CHECK(!text.empty());
        for (auto b : errors)
            if (a != b) CHECK(text != hal::SmbusErrorText(b));
    }
}

TEST(gskill_diagnostics, driver_not_running_is_named_explicitly) {
    // This is the message the user sees when the app runs unelevated, so it
    // has to point at the actual cause.
    const std::string text = hal::SmbusErrorText(hal::SmbusError::DriverNotRunning);
    CHECK(text.find("admin") != std::string::npos);
}
