/**
 * fakes/fake_hid.h - Recording HID backend for tests.
 *
 * Records every byte a protocol module writes and replays scripted responses,
 * so protocol expectations are asserted against exact wire bytes without any
 * hardware attached.
 */

#pragma once
#include <cstring>
#include <deque>
#include <map>
#include <vector>

#include "../../src/hal/hid_backend.h"

namespace fakes {

struct WriteRecord {
    std::string device_path;
    std::vector<uint8_t> data;
};

class FakeHidBackend;

class FakeHidDevice : public hal::IHidDevice {
public:
    FakeHidDevice(FakeHidBackend* backend, std::string path)
        : backend_(backend), path_(std::move(path)) {}

    int Write(const uint8_t* data, size_t len) override;
    int ReadTimeout(uint8_t* data, size_t len, int timeout_ms) override;

private:
    FakeHidBackend* backend_;
    std::string path_;
};

class FakeHidBackend : public hal::IHidBackend {
public:
    //--- Test setup -------------------------------------------------------

    /// Makes a device discoverable.
    void AddDevice(const hal::HidDeviceInfo& info) { devices_.push_back(info); }

    void AddDevice(const std::string& path, uint16_t vid, uint16_t pid,
                   uint16_t usage_page, int interface_number = -1) {
        hal::HidDeviceInfo info;
        info.path = path;
        info.vendor_id = vid;
        info.product_id = pid;
        info.usage_page = usage_page;
        info.interface_number = interface_number;
        devices_.push_back(info);
    }

    /// Queues one full response packet returned by the next ReadTimeout().
    void QueueResponse(std::vector<uint8_t> packet) {
        responses_.push_back(std::move(packet));
    }

    /// Makes Open() fail for every path, simulating an absent device.
    void SetOpenFails(bool fails) { open_fails_ = fails; }

    /// Makes Write() report an error.
    void SetWriteFails(bool fails) { write_fails_ = fails; }

    //--- Observation ------------------------------------------------------

    const std::vector<WriteRecord>& Writes() const { return writes_; }
    size_t WriteCount() const { return writes_.size(); }

    /// Writes whose byte at `index` equals `value` - the usual way to pick the
    /// packets belonging to one command out of a whole exchange.
    std::vector<const WriteRecord*> WritesWithByte(size_t index, uint8_t value) const {
        std::vector<const WriteRecord*> out;
        for (const auto& w : writes_)
            if (w.data.size() > index && w.data[index] == value) out.push_back(&w);
        return out;
    }

    int InitCount() const { return init_count_; }
    int ExitCount() const { return exit_count_; }
    int TotalSleepMs() const { return total_sleep_ms_; }

    void ClearWrites() { writes_.clear(); }

    //--- IHidBackend ------------------------------------------------------

    bool Init() override { ++init_count_; return true; }
    void Exit() override { ++exit_count_; }

    std::vector<hal::HidDeviceInfo> Enumerate(uint16_t vid, uint16_t pid) override {
        std::vector<hal::HidDeviceInfo> out;
        for (const auto& d : devices_)
            if (d.vendor_id == vid && d.product_id == pid) out.push_back(d);
        return out;
    }

    std::unique_ptr<hal::IHidDevice> Open(const std::string& path) override {
        if (open_fails_) return nullptr;
        bool known = false;
        for (const auto& d : devices_) if (d.path == path) known = true;
        if (!known) return nullptr;
        return std::unique_ptr<hal::IHidDevice>(new FakeHidDevice(this, path));
    }

    /// Accumulates instead of blocking, so the suite runs instantly.
    void Sleep(int ms) override { total_sleep_ms_ += ms; }

private:
    friend class FakeHidDevice;

    std::vector<hal::HidDeviceInfo> devices_;
    std::vector<WriteRecord> writes_;
    std::deque<std::vector<uint8_t>> responses_;
    int init_count_ = 0;
    int exit_count_ = 0;
    int total_sleep_ms_ = 0;
    bool open_fails_ = false;
    bool write_fails_ = false;
};

inline int FakeHidDevice::Write(const uint8_t* data, size_t len) {
    if (backend_->write_fails_) return -1;
    backend_->writes_.push_back({path_, std::vector<uint8_t>(data, data + len)});
    return static_cast<int>(len);
}

inline int FakeHidDevice::ReadTimeout(uint8_t* data, size_t len, int) {
    if (backend_->responses_.empty()) return 0;  // timeout
    std::vector<uint8_t> packet = backend_->responses_.front();
    backend_->responses_.pop_front();

    const size_t n = packet.size() < len ? packet.size() : len;
    std::memset(data, 0, len);
    std::memcpy(data, packet.data(), n);
    // The real backend reports how many bytes the report contained.
    return static_cast<int>(packet.size());
}

}  // namespace fakes
