/**
 * hal/hid_backend.h - USB HID abstraction
 *
 * Device protocols talk to this interface instead of calling hidapi directly.
 * The real backend forwards to hidapi; tests substitute a recording fake and
 * assert on the exact bytes each protocol puts on the wire.
 *
 * Deliberately free of <windows.h> so the protocol modules and their tests
 * build on any platform.
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace hal {

struct HidDeviceInfo {
    std::string path;
    uint16_t vendor_id        = 0;
    uint16_t product_id       = 0;
    uint16_t usage_page       = 0;
    int      interface_number = -1;
};

class IHidDevice {
public:
    virtual ~IHidDevice() = default;

    /// Returns bytes written, or <0 on error.
    virtual int Write(const uint8_t* data, size_t len) = 0;

    /// Returns bytes read, 0 on timeout, or <0 on error.
    virtual int ReadTimeout(uint8_t* data, size_t len, int timeout_ms) = 0;
};

class IHidBackend {
public:
    virtual ~IHidBackend() = default;

    virtual bool Init() = 0;
    virtual void Exit() = 0;

    virtual std::vector<HidDeviceInfo> Enumerate(uint16_t vid, uint16_t pid) = 0;

    /// Returns nullptr when the path cannot be opened.
    virtual std::unique_ptr<IHidDevice> Open(const std::string& path) = 0;

    /// Injected so tests run without burning wall-clock time on protocol delays.
    virtual void Sleep(int ms) = 0;
};

/// Convenience: first device matching vid/pid on the given usage page.
/// Returns an empty path when nothing matches.
inline std::string FindDevicePath(IHidBackend& hid, uint16_t vid, uint16_t pid,
                                  uint16_t usage_page) {
    for (const auto& dev : hid.Enumerate(vid, pid)) {
        if (dev.usage_page == usage_page) return dev.path;
    }
    return std::string();
}

/// Same, but selecting on the USB interface number (some devices expose their
/// control endpoint there rather than on a distinctive usage page).
inline std::string FindDevicePathByInterface(IHidBackend& hid, uint16_t vid,
                                             uint16_t pid, int interface_number) {
    for (const auto& dev : hid.Enumerate(vid, pid)) {
        if (dev.interface_number == interface_number) return dev.path;
    }
    return std::string();
}

}  // namespace hal
