#include "ModuleManager.h"
#include "../DeviceRegistry.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace OneClickRGB {

ModuleManager& ModuleManager::getInstance() {
    static ModuleManager instance;
    return instance;
}

bool ModuleManager::Initialize(const std::string& modulePath) {
    modulePath_ = modulePath;

    // Ensure module directory exists
    if (!fs::exists(modulePath_)) {
        try {
            fs::create_directories(modulePath_);
            std::cout << "[MODULES] Created module directory: " << modulePath_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[MODULES] Failed to create module directory: " << e.what() << std::endl;
            return false;
        }
    }

    std::cout << "[MODULES] Module system initialized. Path: " << modulePath_ << std::endl;
    return true;
}

void ModuleManager::Shutdown() {
    std::cout << "[MODULES] Shutting down module system..." << std::endl;

    // Shutdown all modules
    for (auto& pair : loadedModules_) {
        if (pair.second) {
            pair.second->Shutdown();
        }
    }

    // Clear all collections
    loadedModules_.clear();
    moduleInfos_.clear();
    moduleFactories_.clear();

    std::cout << "[MODULES] Module system shutdown complete" << std::endl;
}

ModuleLoadResult ModuleManager::LoadModule(const std::string& moduleName) {
    if (IsModuleLoaded(moduleName)) {
        return ModuleLoadResult::AlreadyLoaded;
    }

    std::string modulePath = modulePath_ + moduleName + GetModuleExtension();

    if (!fs::exists(modulePath)) {
        // Try built-in module factory first
        auto factoryIt = moduleFactories_.find(moduleName);
        if (factoryIt != moduleFactories_.end()) {
            try {
                auto module = factoryIt->second();
                if (ValidateModuleInterface(module.get())) {
                    loadedModules_[moduleName] = std::move(module);

                    ModuleInfo info;
                    info.name = moduleName;
                    info.loaded = true;
                    info.filename = "built-in";
                    info.supportedDevices = loadedModules_[moduleName]->GetSupportedDeviceTypes();
                    moduleInfos_[moduleName] = info;

                    std::cout << "[MODULES] Loaded built-in module: " << moduleName << std::endl;
                    return ModuleLoadResult::Success;
                } else {
                    return ModuleLoadResult::InvalidFormat;
                }
            } catch (const std::exception& e) {
                std::cerr << "[MODULES] Failed to create built-in module " << moduleName
                          << ": " << e.what() << std::endl;
                return ModuleLoadResult::InitializationFailed;
            }
        }

        return ModuleLoadResult::FileNotFound;
    }

    // Load from file
#ifdef _WIN32
    return LoadModule_Windows(modulePath);
#else
    return LoadModule_Linux(modulePath);
#endif
}

ModuleLoadResult ModuleManager::LoadModule_Windows(const std::string& modulePath) {
#ifdef _WIN32
    HMODULE hModule = LoadLibraryA(modulePath.c_str());
    if (!hModule) {
        std::cerr << "[MODULES] Failed to load module: " << modulePath << std::endl;
        return ModuleLoadResult::FileNotFound;
    }

    // Get the module factory function
    using ModuleFactory = IModule* (*)();
    ModuleFactory factory = reinterpret_cast<ModuleFactory>(
        GetProcAddress(hModule, "CreateModule")
    );

    if (!factory) {
        FreeLibrary(hModule);
        std::cerr << "[MODULES] Module missing CreateModule function: " << modulePath << std::endl;
        return ModuleLoadResult::InvalidFormat;
    }

    try {
        IModule* rawModule = factory();
        if (!rawModule) {
            FreeLibrary(hModule);
            return ModuleLoadResult::InitializationFailed;
        }

        std::unique_ptr<IModule> module(rawModule);

        if (!ValidateModuleInterface(module.get())) {
            FreeLibrary(hModule);
            return ModuleLoadResult::InvalidFormat;
        }

        // Store module info
        std::string moduleName = module->GetModuleName();
        ModuleInfo info;
        info.name = moduleName;
        info.version = module->GetModuleVersion();
        info.description = module->GetModuleDescription();
        info.filename = modulePath;
        info.loaded = true;
        info.supportedDevices = module->GetSupportedDeviceTypes();

        loadedModules_[moduleName] = std::move(module);
        moduleInfos_[moduleName] = info;

        std::cout << "[MODULES] Loaded module: " << moduleName << " v" << info.version << std::endl;
        return ModuleLoadResult::Success;

    } catch (const std::exception& e) {
        FreeLibrary(hModule);
        std::cerr << "[MODULES] Exception loading module: " << e.what() << std::endl;
        return ModuleLoadResult::InitializationFailed;
    }
#else
    return ModuleLoadResult::InvalidFormat;
#endif
}

ModuleLoadResult ModuleManager::LoadModule_Linux(const std::string& modulePath) {
#ifndef _WIN32
    void* handle = dlopen(modulePath.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "[MODULES] Failed to load module: " << dlerror() << std::endl;
        return ModuleLoadResult::FileNotFound;
    }

    // Get the module factory function
    using ModuleFactory = IModule* (*)();
    ModuleFactory factory = reinterpret_cast<ModuleFactory>(dlsym(handle, "CreateModule"));

    if (!factory) {
        dlclose(handle);
        std::cerr << "[MODULES] Module missing CreateModule function" << std::endl;
        return ModuleLoadResult::InvalidFormat;
    }

    try {
        IModule* rawModule = factory();
        if (!rawModule) {
            dlclose(handle);
            return ModuleLoadResult::InitializationFailed;
        }

        std::unique_ptr<IModule> module(rawModule);

        if (!ValidateModuleInterface(module.get())) {
            dlclose(handle);
            return ModuleLoadResult::InvalidFormat;
        }

        // Store module info
        std::string moduleName = module->GetModuleName();
        ModuleInfo info;
        info.name = moduleName;
        info.version = module->GetModuleVersion();
        info.description = module->GetModuleDescription();
        info.filename = modulePath;
        info.loaded = true;
        info.supportedDevices = module->GetSupportedDeviceTypes();

        loadedModules_[moduleName] = std::move(module);
        moduleInfos_[moduleName] = info;

        std::cout << "[MODULES] Loaded module: " << moduleName << " v" << info.version << std::endl;
        return ModuleLoadResult::Success;

    } catch (const std::exception& e) {
        dlclose(handle);
        std::cerr << "[MODULES] Exception loading module: " << e.what() << std::endl;
        return ModuleLoadResult::InitializationFailed;
    }
#else
    return ModuleLoadResult::InvalidFormat;
#endif
}

ModuleLoadResult ModuleManager::LoadModule_macOS(const std::string& modulePath) {
    // macOS uses the same dlopen as Linux
    return LoadModule_Linux(modulePath);
}

bool ModuleManager::UnloadModule(const std::string& moduleName) {
    auto it = loadedModules_.find(moduleName);
    if (it == loadedModules_.end()) {
        return false;
    }

    std::cout << "[MODULES] Unloading module: " << moduleName << std::endl;

    if (it->second) {
        it->second->Shutdown();
    }

    loadedModules_.erase(it);
    moduleInfos_.erase(moduleName);

    return true;
}

size_t ModuleManager::LoadAllModules() {
    size_t loadedCount = 0;
    auto availableModules = FindAvailableModules();

    for (const auto& moduleName : availableModules) {
        if (LoadModule(moduleName) == ModuleLoadResult::Success) {
            loadedCount++;
        }
    }

    // Also load built-in modules
    for (const auto& factory : moduleFactories_) {
        if (!IsModuleLoaded(factory.first)) {
            if (LoadModule(factory.first) == ModuleLoadResult::Success) {
                loadedCount++;
            }
        }
    }

    std::cout << "[MODULES] Loaded " << loadedCount << " modules total" << std::endl;
    return loadedCount;
}

bool ModuleManager::IsModuleLoaded(const std::string& moduleName) const {
    return loadedModules_.find(moduleName) != loadedModules_.end();
}

const ModuleInfo* ModuleManager::GetModuleInfo(const std::string& moduleName) const {
    auto it = moduleInfos_.find(moduleName);
    return it != moduleInfos_.end() ? &it->second : nullptr;
}

std::vector<ModuleInfo> ModuleManager::GetLoadedModules() const {
    std::vector<ModuleInfo> result;
    for (const auto& pair : moduleInfos_) {
        if (pair.second.loaded) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::string ModuleManager::FindModuleForDevice(uint16_t vendorId, uint16_t productId) const {
    for (const auto& pair : loadedModules_) {
        if (pair.second && pair.second->SupportsDevice(vendorId, productId)) {
            return pair.first;
        }
    }
    return "";
}

std::unique_ptr<RGBDevice> ModuleManager::CreateController(
    const DeviceInfo& deviceInfo,
    const std::string& devicePath
) {
    std::string moduleName = FindModuleForDevice(deviceInfo.vendorId, deviceInfo.productId);
    if (moduleName.empty()) {
        return nullptr;
    }

    auto moduleIt = loadedModules_.find(moduleName);
    if (moduleIt == loadedModules_.end() || !moduleIt->second) {
        return nullptr;
    }

    try {
        return moduleIt->second->CreateController(deviceInfo, devicePath);
    } catch (const std::exception& e) {
        std::cerr << "[MODULES] Exception creating controller: " << e.what() << std::endl;
        return nullptr;
    }
}

std::vector<std::string> ModuleManager::GetSupportedDeviceTypes() const {
    std::vector<std::string> types;
    for (const auto& pair : loadedModules_) {
        if (pair.second) {
            const auto& moduleTypes = pair.second->GetSupportedDeviceTypes();
            types.insert(types.end(), moduleTypes.begin(), moduleTypes.end());
        }
    }

    // Remove duplicates
    std::sort(types.begin(), types.end());
    auto last = std::unique(types.begin(), types.end());
    types.erase(last, types.end());

    return types;
}

void ModuleManager::RegisterModuleFactory(
    const std::string& moduleName,
    std::function<std::unique_ptr<IModule>()> factory
) {
    moduleFactories_[moduleName] = std::move(factory);
}

size_t ModuleManager::GetAvailableModuleCount() const {
    return FindAvailableModules().size() + moduleFactories_.size();
}

std::string ModuleManager::GetModuleExtension() const {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::vector<std::string> ModuleManager::FindAvailableModules() const {
    std::vector<std::string> modules;

    if (!fs::exists(modulePath_)) {
        return modules;
    }

    std::string extension = GetModuleExtension();
    for (const auto& entry : fs::directory_iterator(modulePath_)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.size() > extension.size() &&
                filename.substr(filename.size() - extension.size()) == extension) {
                // Remove extension to get module name
                std::string moduleName = filename.substr(0, filename.size() - extension.size());
                modules.push_back(moduleName);
            }
        }
    }

    return modules;
}

bool ModuleManager::ValidateModuleInterface(IModule* module) const {
    if (!module) return false;

    // Check required methods return valid data
    if (!module->GetModuleName() || strlen(module->GetModuleName()) == 0) return false;
    if (!module->GetModuleVersion() || strlen(module->GetModuleVersion()) == 0) return false;
    if (!module->GetModuleDescription() || strlen(module->GetModuleDescription()) == 0) return false;

    // Check that module can be initialized
    if (!module->Initialize()) return false;

    return true;
}

} // namespace OneClickRGB