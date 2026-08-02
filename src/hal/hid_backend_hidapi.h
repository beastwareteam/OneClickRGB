/**
 * hal/hid_backend_hidapi.h - Real HID backend, forwards to hidapi.
 *
 * Only this file knows hidapi exists. Everything above it works against
 * IHidBackend, which is what makes the protocol modules testable.
 */

#pragma once
#include "hid_backend.h"

namespace hal {

/// Backing implementation over hidapi. Init()/Exit() are refcounted, so nested
/// use by several device modules within one ApplyColors() run is safe.
class HidapiBackend : public IHidBackend {
public:
    bool Init() override;
    void Exit() override;
    std::vector<HidDeviceInfo> Enumerate(uint16_t vid, uint16_t pid) override;
    std::unique_ptr<IHidDevice> Open(const std::string& path) override;
    void Sleep(int ms) override;

    /// Tears down hidapi completely and re-initialises it, regardless of the
    /// refcount. Used by the resume-from-standby recovery path, where the USB
    /// stack has been re-enumerated underneath us.
    void HardReset();

private:
    int init_count_ = 0;
};

/// Process-wide instance used by the application (not by tests).
HidapiBackend& RealHid();

}  // namespace hal
