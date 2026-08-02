/**
 * hal/hid_backend_dryrun.h - Non-touching HID backend for --dry-run.
 *
 * Previously --dry-run only short-circuited ApplyColors(), so the startup
 * FullHIDReset() still talked to real hardware and a "dry" run could change the
 * lighting anyway. Substituting the backend instead means every protocol path
 * is covered by construction, including ones added later.
 *
 * Devices are reported as present so the full protocol runs and its bytes reach
 * the log; nothing is transmitted.
 */

#pragma once
#include <cstdio>
#include <functional>

#include "hid_backend.h"
#include "smbus_backend.h"

namespace hal {

class DryRunHidBackend : public IHidBackend {
public:
    /// Sink for the protocol trace; typically the application status log.
    using LogFn = std::function<void(const std::string&)>;

    explicit DryRunHidBackend(LogFn log) : log_(std::move(log)) {}

    /// Makes a device appear present, so the protocol code proceeds.
    void AddVirtualDevice(const HidDeviceInfo& info) { devices_.push_back(info); }

    bool Init() override { return true; }
    void Exit() override {}

    std::vector<HidDeviceInfo> Enumerate(uint16_t vid, uint16_t pid) override {
        std::vector<HidDeviceInfo> out;
        for (const auto& d : devices_)
            if (d.vendor_id == vid && d.product_id == pid) out.push_back(d);
        return out;
    }

    std::unique_ptr<IHidDevice> Open(const std::string& path) override;

    /// Dry runs must not take as long as a real apply.
    void Sleep(int) override {}

private:
    friend class DryRunHidDevice;
    LogFn log_;
    std::vector<HidDeviceInfo> devices_;
};

class DryRunHidDevice : public IHidDevice {
public:
    DryRunHidDevice(DryRunHidBackend* backend, std::string path)
        : backend_(backend), path_(std::move(path)) {}

    int Write(const uint8_t* data, size_t len) override {
        if (backend_->log_) {
            static const char* kHex = "0123456789ABCDEF";
            // A full 65-byte packet is unreadable in a status log; the header
            // is what identifies the command.
            const size_t shown = len < 12 ? len : 12;
            std::string line = "[DRY] " + path_ + " write " + std::to_string(len) + "B:";
            for (size_t i = 0; i < shown; ++i) {
                line += ' ';
                line += kHex[data[i] >> 4];
                line += kHex[data[i] & 0x0F];
            }
            if (shown < len) line += " ...";
            backend_->log_(line);
        }
        return static_cast<int>(len);
    }

    /// No device answers, so protocol reads fall back to their defaults.
    int ReadTimeout(uint8_t*, size_t, int) override { return 0; }

private:
    DryRunHidBackend* backend_;
    std::string path_;
};

inline std::unique_ptr<IHidDevice> DryRunHidBackend::Open(const std::string& path) {
    return std::unique_ptr<IHidDevice>(new DryRunHidDevice(this, path));
}

// There is deliberately no dry-run SMBus backend. The RAM path bails out before
// it gets that far, so a dry run never loads the PawnIO kernel driver at all -
// which is a stronger guarantee than a backend that merely declines to transfer.

}  // namespace hal
