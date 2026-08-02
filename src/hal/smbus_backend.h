/**
 * hal/smbus_backend.h - SMBus abstraction
 *
 * The G.Skill RAM protocol reaches the DIMMs over SMBus, which on Windows means
 * the PawnIO kernel driver. That path needs administrator rights and a running
 * service, so it is the one device that cannot be exercised on CI - hence the
 * interface, so the register sequence can be tested against a fake.
 */

#pragma once
#include <cstdint>
#include <cstddef>

namespace hal {

/// Mirrors the Linux i2c_smbus_data union that PawnIO's ioctl_smbus_xfer expects.
union SmbusData {
    uint8_t  byte;
    uint16_t word;
    uint8_t  block[34];
};

enum class SmbusRw : uint8_t {
    Write = 0,
    Read  = 1,
};

enum SmbusSize {
    SMBUS_BYTE      = 1,
    SMBUS_BYTE_DATA = 2,
    SMBUS_WORD_DATA = 3,
};

/// Why the SMBus path is unavailable - drives the user-facing diagnostic.
enum class SmbusError {
    None = 0,
    LibraryMissing,   ///< PawnIOLib.dll not found next to the exe
    LibraryInvalid,   ///< DLL found but exports missing
    DriverNotRunning, ///< pawnio_open failed - service down or not elevated
    ModuleMissing,    ///< SmbusI801.bin not found
    ModuleLoadFailed, ///< driver rejected the SMBus module blob
};

const char* SmbusErrorText(SmbusError err);

class ISmbusBackend {
public:
    virtual ~ISmbusBackend() = default;

    /// Opens the driver and loads the SMBus module. False on any failure;
    /// LastError() then explains which stage failed.
    virtual bool Open() = 0;
    virtual void Close() = 0;

    virtual SmbusError LastError() const = 0;

    /// Returns 0 on success, <0 on transfer failure.
    virtual int Xfer(uint8_t addr, SmbusRw rw, uint8_t cmd, int size,
                     SmbusData* data) = 0;

    virtual void Sleep(int ms) = 0;
};

}  // namespace hal
