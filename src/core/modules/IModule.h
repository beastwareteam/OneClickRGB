#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace OneClickRGB {

// Forward declarations
class RGBDevice;
struct DeviceInfo;

/**
 * @brief Interface for all RGB controller modules
 *
 * This interface defines the contract that all device-specific modules must implement.
 * Modules are loaded dynamically at runtime and provide device-specific RGB control.
 */
class IModule {
public:
    virtual ~IModule() = default;

    /**
     * @brief Get module metadata
     */
    virtual const char* GetModuleName() const = 0;
    virtual const char* GetModuleVersion() const = 0;
    virtual const char* GetModuleDescription() const = 0;

    /**
     * @brief Get supported device types
     * @return Vector of device type strings this module supports
     */
    virtual std::vector<std::string> GetSupportedDeviceTypes() const = 0;

    /**
     * @brief Check if module supports a specific device
     * @param vendorId Device vendor ID
     * @param productId Device product ID
     * @return true if supported
     */
    virtual bool SupportsDevice(uint16_t vendorId, uint16_t productId) const = 0;

    /**
     * @brief Create RGB controller for a detected device
     * @param deviceInfo Device information from registry
     * @param devicePath Hardware device path
     * @return Unique pointer to RGB controller, nullptr on failure
     */
    virtual std::unique_ptr<RGBDevice> CreateController(
        const DeviceInfo& deviceInfo,
        const std::string& devicePath
    ) = 0;

    /**
     * @brief Get module capabilities
     */
    virtual bool SupportsDirectMode() const { return false; }
    virtual bool SupportsEffectEngine() const { return false; }
    virtual bool SupportsCustomPatterns() const { return false; }
    virtual bool RequiresAdminPrivileges() const { return false; }

    /**
     * @brief Module lifecycle
     */
    virtual bool Initialize() { return true; }
    virtual void Shutdown() {}
};

/**
 * @brief Module loading result
 */
enum class ModuleLoadResult {
    Success,
    FileNotFound,
    InvalidFormat,
    MissingDependencies,
    InitializationFailed,
    AlreadyLoaded,
    IncompatibleVersion
};

/**
 * @brief Module information
 */
struct ModuleInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::string filename;
    std::vector<std::string> supportedDevices;
    bool loaded = false;
    ModuleLoadResult loadResult = ModuleLoadResult::Success;
    std::string errorMessage;
};

} // namespace OneClickRGB