#include "gskill_ram.h"

#include <cstdio>
#include <cstring>

namespace devices {
namespace gskill {

namespace {

/// ENE registers are addressed by writing the byte-swapped register number to
/// command 0x00 as a word, then reading/writing the payload.
uint16_t SwapReg(uint16_t reg) {
    return static_cast<uint16_t>(((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
}

void WriteWord(hal::ISmbusBackend& bus, uint8_t addr, uint8_t cmd, uint16_t value) {
    hal::SmbusData d;
    std::memset(&d, 0, sizeof(d));
    d.word = value;
    bus.Xfer(addr, hal::SmbusRw::Write, cmd, hal::SMBUS_WORD_DATA, &d);
}

void WriteByte(hal::ISmbusBackend& bus, uint8_t addr, uint8_t cmd, uint8_t value) {
    hal::SmbusData d;
    std::memset(&d, 0, sizeof(d));
    d.byte = value;
    bus.Xfer(addr, hal::SmbusRw::Write, cmd, hal::SMBUS_BYTE_DATA, &d);
}

/// Probes whether anything answers at `addr`. Returns <0 when nothing is there.
int ProbeAddress(hal::ISmbusBackend& bus, uint8_t addr) {
    hal::SmbusData d;
    std::memset(&d, 0, sizeof(d));
    if (bus.Xfer(addr, hal::SmbusRw::Read, 0, hal::SMBUS_BYTE, &d) < 0) return -1;
    return d.byte;
}

}  // namespace

void EneWrite(hal::ISmbusBackend& bus, uint8_t addr, uint16_t reg, uint8_t value) {
    WriteWord(bus, addr, 0x00, SwapReg(reg));
    bus.Sleep(1);
    WriteByte(bus, addr, 0x01, value);
    bus.Sleep(1);
}

uint8_t EneRead(hal::ISmbusBackend& bus, uint8_t addr, uint16_t reg) {
    WriteWord(bus, addr, 0x00, SwapReg(reg));
    bus.Sleep(1);
    hal::SmbusData d;
    std::memset(&d, 0, sizeof(d));
    bus.Xfer(addr, hal::SmbusRw::Read, 0x81, hal::SMBUS_BYTE_DATA, &d);
    return d.byte;
}

bool IsSupportedModuleName(const char* name) {
    if (!name) return false;
    return std::strstr(name, "AUDA") != nullptr ||
           std::strstr(name, "DIMM") != nullptr ||
           std::strstr(name, "Trident") != nullptr;
}

Result SetColor(hal::ISmbusBackend& bus, const ChannelConfig* zones, int zone_count,
                uint8_t r, uint8_t g, uint8_t b, const StatusFn& status) {
    Result result;
    result.bus_available = true;

    int slot = 0;
    for (uint8_t addr = ADDR_FIRST; addr <= ADDR_LAST; ++addr) {
        if (ProbeAddress(bus, addr) < 0) continue;

        char name[17] = {0};
        for (int i = 0; i < 16; ++i)
            name[i] = static_cast<char>(
                EneRead(bus, addr, static_cast<uint16_t>(REG_NAME_BASE + i)));

        if (!IsSupportedModuleName(name)) continue;

        Rgb c{r, g, b};
        if (slot < zone_count) {
            if (!zones[slot].enabled) { ++slot; continue; }
            c = Corrected(zones[slot], r, g, b);
        }

        uint8_t led_count = EneRead(bus, addr, REG_LED_COUNT);
        if (led_count == 0 || led_count > MAX_PLAUSIBLE_LED_COUNT)
            led_count = DEFAULT_LED_COUNT;

        EneWrite(bus, addr, REG_DIRECT_ON, 0x01);
        bus.Sleep(5);

        for (int i = 0; i < led_count; ++i) {
            uint8_t triple[3];
            EncodeLedTriple(triple, c.r, c.g, c.b);
            const uint16_t reg = LedRegister(i);
            EneWrite(bus, addr, static_cast<uint16_t>(reg + 0), triple[0]);
            EneWrite(bus, addr, static_cast<uint16_t>(reg + 1), triple[1]);
            EneWrite(bus, addr, static_cast<uint16_t>(reg + 2), triple[2]);
        }
        bus.Sleep(5);
        EneWrite(bus, addr, REG_APPLY, 0x01);

        ++result.modules_set;
        ++slot;
    }

    if (result.modules_set > 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[G.Skill] %d module(s) set", result.modules_set);
        status(buf);
    } else {
        status("[G.Skill] No RAM modules found on SMBus");
    }
    return result;
}

}  // namespace gskill
}  // namespace devices
