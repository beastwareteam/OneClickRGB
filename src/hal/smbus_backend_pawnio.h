/**
 * hal/smbus_backend_pawnio.h - Real SMBus backend over the PawnIO driver.
 */

#pragma once
#include <string>

#include "smbus_backend.h"

namespace hal {

class PawnIoBackend : public ISmbusBackend {
public:
    ~PawnIoBackend() override;

    bool Open() override;
    void Close() override;
    SmbusError LastError() const override { return last_error_; }
    int Xfer(uint8_t addr, SmbusRw rw, uint8_t cmd, int size,
             SmbusData* data) override;
    void Sleep(int ms) override;

    /// Open(), retrying while the PawnIO service is still coming up.
    /// At logon the driver is regularly not ready yet when we first ask, which
    /// is why the RAM stayed dark even once the process did run elevated.
    /// Retries only on DriverNotRunning - a missing DLL will never fix itself.
    bool OpenWithRetry(int attempts, int delay_ms);

private:
    void* dll_    = nullptr;  // HMODULE
    void* handle_ = nullptr;  // PawnIO HANDLE
    SmbusError last_error_ = SmbusError::None;

    bool LoadLibraryAndSymbols();
    bool LoadSmbusModule();
};

/// Process-wide instance used by the application (not by tests).
PawnIoBackend& RealSmbus();

}  // namespace hal
