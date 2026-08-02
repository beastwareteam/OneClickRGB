#include "smbus_backend.h"

namespace hal {

const char* SmbusErrorText(SmbusError err) {
    switch (err) {
        case SmbusError::None:             return "OK";
        case SmbusError::LibraryMissing:   return "PawnIOLib.dll not found";
        case SmbusError::LibraryInvalid:   return "PawnIOLib.dll is missing required exports";
        case SmbusError::DriverNotRunning: return "PawnIO driver not running (needs admin rights)";
        case SmbusError::ModuleMissing:    return "SmbusI801.bin not found";
        case SmbusError::ModuleLoadFailed: return "Driver rejected the SMBus module";
    }
    return "Unknown SMBus error";
}

}  // namespace hal
