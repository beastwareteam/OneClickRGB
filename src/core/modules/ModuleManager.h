#pragma once

#include "IModule.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace OneClickRGB {

// Forward declarations
class RGBDevice;
struct DeviceInfo;

/**
 * @brief Manages dynamic loading and unloading of RGB controller modules
 *
 * The ModuleManager provides a centralized way to:
 * - Load modules from DLLs/shared libraries
 * - Register modules for specific device types
 * - Create controllers through appropriate modules
 * - Manage module lifecycle
 */
class ModuleManager {
public:
    static ModuleManager& getInstance();

    /**
     * @brief Initialize the module system
     * @param modulePath Directory containing module files
     * @return true on success
     */
    bool Initialize(const std::string& modulePath = "modules/");

    /**
     * @brief Shutdown all loaded modules
     */
    void Shutdown();

    /**
     * @brief Load a specific module
     * @param moduleName Name of the module file (without extension)
     * @return Load result
     */
    ModuleLoadResult LoadModule(const std::string& moduleName);

    /**
     * @brief Unload a specific module
     * @param moduleName Name of the module
     * @return true on success
     */
    bool UnloadModule(const std::string& moduleName);

    /**
     * @brief Load all available modules in the module directory
     * @return Number of successfully loaded modules
     */
    size_t LoadAllModules();

    /**
     * @brief Check if a module is loaded
     * @param moduleName Module name
     * @return true if loaded
     */
    bool IsModuleLoaded(const std::string& moduleName) const;

    /**
     * @brief Get module information
     * @param moduleName Module name
     * @return Module info or nullptr if not found
     */
    const ModuleInfo* GetModuleInfo(const std::string& moduleName) const;

    /**
     * @brief Get all loaded modules
     * @return Vector of module information
     */
    std::vector<ModuleInfo> GetLoadedModules() const;

    /**
     * @brief Find module that supports a specific device
     * @param vendorId Device vendor ID
     * @param productId Device product ID
     * @return Module name or empty string if none found
     */
    std::string FindModuleForDevice(uint16_t vendorId, uint16_t productId) const;

    /**
     * @brief Create RGB controller using appropriate module
     * @param deviceInfo Device information
     * @param devicePath Hardware path
     * @return Controller instance or nullptr
     */
    std::unique_ptr<RGBDevice> CreateController(
        const DeviceInfo& deviceInfo,
        const std::string& devicePath
    );

    /**
     * @brief Get supported device types across all modules
     * @return Vector of unique device types
     */
    std::vector<std::string> GetSupportedDeviceTypes() const;

    /**
     * @brief Register a module factory function (for built-in modules)
     * @param moduleName Module name
     * @param factory Factory function that creates the module
     */
    void RegisterModuleFactory(
        const std::string& moduleName,
        std::function<std::unique_ptr<IModule>()> factory
    );

    /**
     * @brief Get module statistics
     */
    size_t GetLoadedModuleCount() const { return loadedModules_.size(); }
    size_t GetAvailableModuleCount() const;

private:
    ModuleManager() = default;
    ~ModuleManager() = default;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    // Module storage
    std::unordered_map<std::string, std::unique_ptr<IModule>> loadedModules_;
    std::unordered_map<std::string, ModuleInfo> moduleInfos_;
    std::unordered_map<std::string, std::function<std::unique_ptr<IModule>()>> moduleFactories_;

    // Configuration
    std::string modulePath_;

    // Platform-specific module loading
    ModuleLoadResult LoadModule_Windows(const std::string& modulePath);
    ModuleLoadResult LoadModule_Linux(const std::string& modulePath);
    ModuleLoadResult LoadModule_macOS(const std::string& modulePath);

    // Helper functions
    std::string GetModuleExtension() const;
    std::vector<std::string> FindAvailableModules() const;
    bool ValidateModuleInterface(IModule* module) const;
};

} // namespace OneClickRGB