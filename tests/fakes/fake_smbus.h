/**
 * fakes/fake_smbus.h - In-memory SMBus device for tests.
 *
 * Emulates enough of the ENE controller to exercise the G.Skill register
 * sequence: a per-address register file addressed through the same
 * "write register number to cmd 0x00, then read/write cmd 0x01/0x81" dance the
 * real part uses.
 */

#pragma once
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../../src/hal/smbus_backend.h"

namespace fakes {

struct XferRecord {
    uint8_t addr;
    hal::SmbusRw rw;
    uint8_t cmd;
    int size;
    uint16_t word;  ///< payload as seen on the bus
    uint8_t byte;
};

class FakeSmbusBackend : public hal::ISmbusBackend {
public:
    //--- Test setup -------------------------------------------------------

    /// Registers a responding module and seeds its 16-byte name register.
    void AddModule(uint8_t addr, const std::string& name, uint8_t led_count) {
        present_.insert(addr);
        for (int i = 0; i < 16; ++i)
            registers_[addr][static_cast<uint16_t>(0x1000 + i)] =
                i < static_cast<int>(name.size()) ? static_cast<uint8_t>(name[i]) : 0;
        registers_[addr][0x1C02] = led_count;
    }

    void SetOpenFails(hal::SmbusError error) { open_error_ = error; }

    //--- Observation ------------------------------------------------------

    /// Value currently held in a module's register, 0 when never written.
    uint8_t Register(uint8_t addr, uint16_t reg) const {
        auto it = registers_.find(addr);
        if (it == registers_.end()) return 0;
        auto r = it->second.find(reg);
        return r == it->second.end() ? 0 : r->second;
    }

    bool RegisterWasWritten(uint8_t addr, uint16_t reg) const {
        auto it = written_.find(addr);
        return it != written_.end() && it->second.count(reg) > 0;
    }

    const std::vector<XferRecord>& Transfers() const { return transfers_; }
    bool IsOpen() const { return open_; }
    int OpenAttempts() const { return open_attempts_; }
    int TotalSleepMs() const { return total_sleep_ms_; }

    //--- ISmbusBackend ----------------------------------------------------

    bool Open() override {
        ++open_attempts_;
        if (open_error_ != hal::SmbusError::None) return false;
        open_ = true;
        return true;
    }

    void Close() override { open_ = false; }

    hal::SmbusError LastError() const override { return open_error_; }

    int Xfer(uint8_t addr, hal::SmbusRw rw, uint8_t cmd, int size,
             hal::SmbusData* data) override {
        if (!open_) return -1;

        XferRecord rec{addr, rw, cmd, size, 0, 0};
        if (data) { rec.word = data->word; rec.byte = data->byte; }
        transfers_.push_back(rec);

        if (present_.count(addr) == 0) return -1;  // nothing answers here

        if (rw == hal::SmbusRw::Write) {
            if (cmd == 0x00 && data) {
                // Register selector, transmitted byte-swapped.
                selected_[addr] = static_cast<uint16_t>(
                    ((data->word << 8) & 0xFF00) | ((data->word >> 8) & 0x00FF));
            } else if (cmd == 0x01 && data) {
                const uint16_t reg = selected_[addr];
                registers_[addr][reg] = data->byte;
                written_[addr].insert(reg);
            }
            return 0;
        }

        // Read
        if (!data) return 0;
        std::memset(data, 0, sizeof(*data));
        if (cmd == 0x81) {
            data->byte = Register(addr, selected_[addr]);
        } else {
            data->byte = 0;  // bare probe: the address ACKs, value irrelevant
        }
        return 0;
    }

    void Sleep(int ms) override { total_sleep_ms_ += ms; }

private:
    std::set<uint8_t> present_;
    std::map<uint8_t, std::map<uint16_t, uint8_t>> registers_;
    std::map<uint8_t, std::set<uint16_t>> written_;
    std::map<uint8_t, uint16_t> selected_;
    std::vector<XferRecord> transfers_;
    hal::SmbusError open_error_ = hal::SmbusError::None;
    bool open_ = false;
    int open_attempts_ = 0;
    int total_sleep_ms_ = 0;
};

}  // namespace fakes
