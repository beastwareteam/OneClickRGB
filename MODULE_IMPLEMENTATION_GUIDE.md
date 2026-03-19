# OneClickRGB - MODUL-SYSTEM IMPLEMENTIERUNGS-HANDBUCH
## Praktische Schritt-für-Schritt Umsetzung

**Version:** 1.0  
**Zielgruppe:** Entwickler  
**Geschätzter Aufwand:** 5-7 Tage

---

## SCHRITT 1: IModule.h Erstellen (1 Stunde)

**Datei:** `src/core/ModuleSystem/IModule.h`

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace OneClickRGB {

// Forward declarations
class IDeviceController;

/*──────────────────────────────────────────────────────────*/
/* Module Metadata - Beschreibung des Moduls               */
/*──────────────────────────────────────────────────────────*/

struct ModuleMetadata {
    std::string id;              // "com.oneclickrgb.devices.asus"
    std::string name;            // "ASUS Aura"
    std::string version;         // "1.0.0"
    std::string author;          // "OneClickRGB Team"
    std::string description;     // "Support for ASUS Aura RGB"
    int api_version;             // 100 = v1.0.0
    std::vector<std::string> dependencies;  // ["libhidapi>=0.12"]
};

/*──────────────────────────────────────────────────────────*/
/* Device Info - Welche Devices das Modul unterstützt      */
/*──────────────────────────────────────────────────────────*/

struct SupportedDevice {
    uint16_t vendor_id;          // z.B. 0x0b05 (ASUS)
    uint16_t product_id;         // z.B. 0x19af
    std::string name;            // "ASUS Aura LED Controller"
    std::string device_type;     // "rgb_controller", "mouse", "keyboard"
    std::vector<std::string> features;  // "color", "brightness", "effects"
};

/*──────────────────────────────────────────────────────────*/
/* IDeviceController - Interface für alle Devices         */
/*──────────────────────────────────────────────────────────*/

class IDeviceController {
public:
    virtual ~IDeviceController() = default;

    // Device-Info
    virtual std::string GetName() const = 0;
    virtual uint16_t GetVendorId() const = 0;
    virtual uint16_t GetProductId() const = 0;

    // Funktionalität
    virtual bool IsConnected() const = 0;
    virtual bool SetColor(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual bool SetBrightness(uint8_t brightness) = 0;
    virtual bool GetSupportedFeatures(std::vector<std::string>& features) = 0;
};

/*──────────────────────────────────────────────────────────*/
/* IModule Base Class - Jedes Modul implementiert dies     */
/*──────────────────────────────────────────────────────────*/

class IModule {
public:
    virtual ~IModule() = default;

    // 1. Modul identifizieren
    virtual ModuleMetadata GetMetadata() const = 0;

    // 2. Initialisierung (nach Laden aufgerufen)
    virtual bool Initialize() = 0;

    // 3. Cleanup (vor Entladen)
    virtual void Shutdown() = 0;

    // 4. Alle unterstützten Devices auflisten
    virtual std::vector<SupportedDevice> GetSupportedDevices() const = 0;

    // 5. Device erstellen basierend auf VID:PID
    // Return: nullptr wenn Device nicht unterstützt
    virtual IDeviceController* CreateController(
        uint16_t vendor_id, 
        uint16_t product_id
    ) = 0;

    // 6. API-Kompatibilität prüfen
    virtual bool IsCompatibleWithApi(int api_version) const = 0;

    // 7. Optional: Diagnostische Funktionen
    virtual std::string GetLastError() const { return ""; }
};

/*──────────────────────────────────────────────────────────*/
/* DLL Export Functions - Extern C für DLL-Laden          */
/*──────────────────────────────────────────────────────────*/

extern "C" {
    // Beim Laden aufgerufen
    IModule* __cdecl CreateModule();
    
    // Beim Entladen aufgerufen
    void __cdecl DestroyModule(IModule* module);
    
    // Versions-Info (für Kompatibilität)
    const char* __cdecl GetModuleVersion();
    const char* __cdecl GetModuleId();
    int __cdecl GetModuleApiVersion();
}

} // namespace OneClickRGB
```

---

## SCHRITT 2: ModuleManager.h/cpp Erstellen (2 Stunden)

**Datei:** `src/core/ModuleSystem/ModuleManager.h`

```cpp
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "IModule.h"

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE ModuleHandle;
#else
    typedef void* ModuleHandle;
#endif

namespace OneClickRGB {

/*────────────────────────────────────────────────────────*/
/* ModuleManager - Zentrale Verwaltung aller Module       */
/*────────────────────────────────────────────────────────*/

class ModuleManager {
public:
    // Singleton
    static ModuleManager& Get() {
        static ModuleManager instance;
        return instance;
    }

    // --- Module Loading ---
    
    // Modul von Pfad laden
    bool LoadModule(const std::string& module_path);
    
    // Modul entladen
    bool UnloadModule(const std::string& module_id);
    
    // Alle Module aus Verzeichnis laden
    bool LoadModulesFromDirectory(const std::string& directory);
    
    // --- Device Management ---
    
    // Hardware scannen und benötigte Module laden
    std::vector<IDeviceController*> ScanAndLoadHardware();
    
    // Device auf Basis VID:PID erstellen
    IDeviceController* CreateDevice(uint16_t vendor_id, uint16_t product_id);
    
    // --- Module Info ---
    
    // Liste aller geladenen Module
    std::vector<ModuleMetadata> GetLoadedModules() const;
    
    // Ist Modul geladen?
    bool IsModuleLoaded(const std::string& module_id) const;
    
    // Modul-Info abrufen
    IModule* GetModule(const std::string& module_id) const;
    
    // --- Dependency Management ---
    
    // Abhängigkeiten auflösen
    bool ResolveDependencies(const std::vector<std::string>& dependencies);
    
    // Ist Abhängigkeit verfügbar?
    bool CheckDependency(const std::string& dependency) const;

    // Fehlerbehandlung
    std::string GetLastError() const { return last_error; }

private:
    ModuleManager() = default;
    ~ModuleManager();
    
    // Nicht kopierbar
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    // --- Interne Struktur ---
    
    struct LoadedModule {
        std::unique_ptr<IModule> module;
        ModuleHandle handle;
        std::string path;
        bool is_builtin;
    };
    
    std::map<std::string, LoadedModule> modules;
    
    // Device -> Modul Mapping
    std::map<std::pair<uint16_t, uint16_t>, std::string> device_registry;
    
    std::string last_error;
    
    // Internal methods
    bool LoadModuleDLL(const std::string& path);
    void RegisterDevices(IModule* module);
    bool VerifyModuleCompatibility(IModule* module) const;
};

} // namespace OneClickRGB
```

**Datei:** `src/core/ModuleSystem/ModuleManager.cpp`

```cpp
#include "ModuleManager.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

#ifndef _WIN32
    #include <dlfcn.h>
#endif

namespace OneClickRGB {

const int CURRENT_API_VERSION = 100;  // v1.0.0

ModuleManager::~ModuleManager() {
    // Alle Module entladen
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
        UnloadModule(it->first);
    }
}

/*────────────────────────────────────────────────────────*/
/* Module Loading                                         */
/*────────────────────────────────────────────────────────*/

bool ModuleManager::LoadModule(const std::string& module_path) {
    try {
        // 1. Datei existiert?
        if (!std::filesystem::exists(module_path)) {
            last_error = "Module file not found: " + module_path;
            std::cerr << "[MODULE] " << last_error << std::endl;
            return false;
        }

        // 2. DLL laden
        std::cout << "[MODULE] Loading: " << module_path << std::endl;
        
        ModuleHandle handle = nullptr;
        
        #ifdef _WIN32
            handle = LoadLibraryA(module_path.c_str());
            if (!handle) {
                last_error = "LoadLibrary failed: " + std::to_string(GetLastError());
                std::cerr << "[MODULE] " << last_error << std::endl;
                return false;
            }
        #else
            handle = dlopen(module_path.c_str(), RTLD_LAZY);
            if (!handle) {
                last_error = std::string("dlopen failed: ") + dlerror();
                std::cerr << "[MODULE] " << last_error << std::endl;
                return false;
            }
        #endif

        // 3. Entry-Punkt laden
        typedef IModule* (*CreateModuleFunc)();
        
        #ifdef _WIN32
            CreateModuleFunc CreateModule = 
                (CreateModuleFunc)GetProcAddress(handle, "CreateModule");
        #else
            CreateModuleFunc CreateModule = 
                (CreateModuleFunc)dlsym(handle, "CreateModule");
        #endif
        
        if (!CreateModule) {
            last_error = "Module doesn't export CreateModule()";
            std::cerr << "[MODULE] " << last_error << std::endl;
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;
        }

        // 4. Modul instanziieren
        IModule* module = nullptr;
        try {
            module = CreateModule();
        } catch (const std::exception& e) {
            last_error = std::string("CreateModule() exception: ") + e.what();
            std::cerr << "[MODULE] " << last_error << std::endl;
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;
        }

        if (!module) {
            last_error = "CreateModule() returned nullptr";
            std::cerr << "[MODULE] " << last_error << std::endl;
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;
        }

        // 5. Metadaten abrufen
        ModuleMetadata meta = module->GetMetadata();
        
        // Ist bereits geladen?
        if (modules.find(meta.id) != modules.end()) {
            last_error = "Module already loaded: " + meta.id;
            std::cout << "[MODULE] " << last_error << std::endl;
            
            // Cleanup
            typedef void (*DestroyModuleFunc)(IModule*);
            #ifdef _WIN32
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)GetProcAddress(handle, "DestroyModule");
            #else
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)dlsym(handle, "DestroyModule");
            #endif
            
            if (DestroyModule) DestroyModule(module);
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;  // oder nur ignorieren?
        }

        // 6. API-Kompatibilität prüfen
        if (!module->IsCompatibleWithApi(CURRENT_API_VERSION)) {
            last_error = "Module not compatible with API v" + 
                        std::to_string(CURRENT_API_VERSION);
            std::cerr << "[MODULE] " << last_error << std::endl;
            
            typedef void (*DestroyModuleFunc)(IModule*);
            #ifdef _WIN32
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)GetProcAddress(handle, "DestroyModule");
            #else
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)dlsym(handle, "DestroyModule");
            #endif
            
            if (DestroyModule) DestroyModule(module);
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;
        }

        // 7. Abhängigkeiten prüfen
        if (!meta.dependencies.empty()) {
            if (!ResolveDependencies(meta.dependencies)) {
                last_error = "Dependencies not satisfied";
                std::cerr << "[MODULE] " << last_error << std::endl;
                
                typedef void (*DestroyModuleFunc)(IModule*);
                #ifdef _WIN32
                    DestroyModuleFunc DestroyModule = 
                        (DestroyModuleFunc)GetProcAddress(handle, "DestroyModule");
                #else
                    DestroyModuleFunc DestroyModule = 
                        (DestroyModuleFunc)dlsym(handle, "DestroyModule");
                #endif
                
                if (DestroyModule) DestroyModule(module);
                
                #ifdef _WIN32
                    FreeLibrary(handle);
                #else
                    dlclose(handle);
                #endif
                return false;
            }
        }

        // 8. Initialisieren
        if (!module->Initialize()) {
            last_error = "Module initialization failed: " + 
                        module->GetLastError();
            std::cerr << "[MODULE] " << last_error << std::endl;
            
            typedef void (*DestroyModuleFunc)(IModule*);
            #ifdef _WIN32
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)GetProcAddress(handle, "DestroyModule");
            #else
                DestroyModuleFunc DestroyModule = 
                    (DestroyModuleFunc)dlsym(handle, "DestroyModule");
            #endif
            
            if (DestroyModule) DestroyModule(module);
            
            #ifdef _WIN32
                FreeLibrary(handle);
            #else
                dlclose(handle);
            #endif
            return false;
        }

        // 9. Speichern
        LoadedModule loaded;
        loaded.module = std::unique_ptr<IModule>(module);
        loaded.handle = handle;
        loaded.path = module_path;
        loaded.is_builtin = false;
        
        modules[meta.id] = std::move(loaded);
        
        // 10. Device-Registry aktualisieren
        RegisterDevices(module);

        std::cout << "[MODULE] ✓ Loaded " << meta.name << " v" 
                  << meta.version << std::endl;
        return true;

    } catch (const std::exception& e) {
        last_error = std::string("Exception: ") + e.what();
        std::cerr << "[MODULE] " << last_error << std::endl;
        return false;
    }
}

bool ModuleManager::UnloadModule(const std::string& module_id) {
    auto it = modules.find(module_id);
    if (it == modules.end()) {
        last_error = "Module not found: " + module_id;
        return false;
    }

    auto& loaded = it->second;
    
    // 1. Shutdown aufrufen
    try {
        loaded.module->Shutdown();
    } catch (...) {
        std::cerr << "[MODULE] Exception during Shutdown()" << std::endl;
    }

    // 2. DLL entladen
    ModuleHandle handle = loaded.handle;
    
    #ifdef _WIN32
        FreeLibrary(handle);
    #else
        dlclose(handle);
    #endif

    // 3. Aus Registry entfernen
    modules.erase(it);
    
    // 4. Device-Registry aufräumen
    auto device_it = device_registry.begin();
    while (device_it != device_registry.end()) {
        if (device_it->second == module_id) {
            device_it = device_registry.erase(device_it);
        } else {
            ++device_it;
        }
    }

    std::cout << "[MODULE] ✓ Unloaded " << module_id << std::endl;
    return true;
}

bool ModuleManager::LoadModulesFromDirectory(const std::string& directory) {
    if (!std::filesystem::exists(directory)) {
        last_error = "Directory not found: " + directory;
        return false;
    }

    int loaded_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            
            #ifdef _WIN32
                if (ext == ".dll" || ext == ".ocrgbmod") {
            #else
                if (ext == ".so" || ext == ".ocrgbmod") {
            #endif
                if (LoadModule(entry.path().string())) {
                    loaded_count++;
                }
            }
        }
    }

    std::cout << "[MODULE] ✓ Loaded " << loaded_count << " modules from " 
              << directory << std::endl;
    return true;
}

/*────────────────────────────────────────────────────────*/
/* Device Management                                      */
/*────────────────────────────────────────────────────────*/

std::vector<IDeviceController*> ModuleManager::ScanAndLoadHardware() {
    std::vector<IDeviceController*> devices;
    
    std::cout << "[HARDWARE] Scanning..." << std::endl;
    
    // Hardware scannen zur Identifizierung erforderlicher Module
    // (Über existierenden HardwareScanner)
    
    // TODO: Mit HardwareScanner integrieren
    
    return devices;
}

IDeviceController* ModuleManager::CreateDevice(
    uint16_t vendor_id, 
    uint16_t product_id
) {
    auto key = std::make_pair(vendor_id, product_id);
    
    auto it = device_registry.find(key);
    if (it == device_registry.end()) {
        last_error = "No controller for VID:PID 0x" + 
                     std::to_string(vendor_id) + ":0x" + 
                     std::to_string(product_id);
        return nullptr;
    }

    auto module_it = modules.find(it->second);
    if (module_it == modules.end()) {
        last_error = "Module not loaded: " + it->second;
        return nullptr;
    }

    try {
        return module_it->second.module->CreateController(vendor_id, product_id);
    } catch (const std::exception& e) {
        last_error = std::string("CreateController exception: ") + e.what();
        return nullptr;
    }
}

/*────────────────────────────────────────────────────────*/
/* Module Info                                            */
/*────────────────────────────────────────────────────────*/

std::vector<ModuleMetadata> ModuleManager::GetLoadedModules() const {
    std::vector<ModuleMetadata> result;
    
    for (const auto& pair : modules) {
        result.push_back(pair.second.module->GetMetadata());
    }
    
    return result;
}

bool ModuleManager::IsModuleLoaded(const std::string& module_id) const {
    return modules.find(module_id) != modules.end();
}

IModule* ModuleManager::GetModule(const std::string& module_id) const {
    auto it = modules.find(module_id);
    if (it != modules.end()) {
        return it->second.module.get();
    }
    return nullptr;
}

/*────────────────────────────────────────────────────────*/
/* Helper Methods                                         */
/*────────────────────────────────────────────────────────*/

void ModuleManager::RegisterDevices(IModule* module) {
    auto devices = module->GetSupportedDevices();
    ModuleMetadata meta = module->GetMetadata();
    
    for (const auto& device : devices) {
        auto key = std::make_pair(device.vendor_id, device.product_id);
        device_registry[key] = meta.id;
        
        std::cout << "[REGISTRY] " << meta.name << " -> VID:0x" 
                  << std::hex << device.vendor_id << " PID:0x" 
                  << device.product_id << std::dec << std::endl;
    }
}

bool ModuleManager::ResolveDependencies(
    const std::vector<std::string>& dependencies
) {
    for (const auto& dep : dependencies) {
        if (!CheckDependency(dep)) {
            std::cerr << "[MODULES] Dependency not found: " << dep << std::endl;
            return false;
        }
    }
    return true;
}

bool ModuleManager::CheckDependency(const std::string& dependency) const {
    // TODO: Implementieren
    // Könnte prüfen:
    // - Ob DLL existiert (z.B. hidapi.dll)
    // - Ob anderes Modul geladen ist
    // - Versionskompatibilität
    
    return true;  // Für jetzt immer true
}

} // namespace OneClickRGB
```

---

## SCHRITT 3: Komplett Test-Module erstellen (2 Stunden)

**Datei:** `src/modules/TestModule.h`

```cpp
#pragma once

#include "../core/ModuleSystem/IModule.h"
#include <iostream>

namespace OneClickRGB {

/*────────────────────────────────────────────────────────*/
/* Test Device Controller                                 */
/*────────────────────────────────────────────────────────*/

class TestDeviceController : public IDeviceController {
private:
    uint8_t current_r, current_g, current_b;
    uint8_t current_brightness;

public:
    TestDeviceController()
        : current_r(0), current_g(0), current_b(0), current_brightness(100) {}

    std::string GetName() const override {
        return "Test RGB Device";
    }

    uint16_t GetVendorId() const override {
        return 0xFFFF;  // Test VID
    }

    uint16_t GetProductId() const override {
        return 0xFFFF;  // Test PID
    }

    bool IsConnected() const override {
        return true;  // Immer verbunden (Test)
    }

    bool SetColor(uint8_t r, uint8_t g, uint8_t b) override {
        current_r = r;
        current_g = g;
        current_b = b;
        std::cout << "[TEST] Color: RGB(" << (int)r << "," 
                  << (int)g << "," << (int)b << ")" << std::endl;
        return true;
    }

    bool SetBrightness(uint8_t brightness) override {
        current_brightness = brightness;
        std::cout << "[TEST] Brightness: " << (int)brightness << "%" << std::endl;
        return true;
    }

    bool GetSupportedFeatures(std::vector<std::string>& features) override {
        features = {"color", "brightness"};
        return true;
    }
};

/*────────────────────────────────────────────────────────*/
/* Test Module                                            */
/*────────────────────────────────────────────────────────*/

class TestModule : public IModule {
private:
    bool initialized;

public:
    TestModule() : initialized(false) {}

    ModuleMetadata GetMetadata() const override {
        return {
            "com.oneclick.test.device",
            "Test RGB Module",
            "1.0.0",
            "OneClickRGB Team",
            "Test module for module system verification",
            100,  // API v1.0.0
            {}    // no dependencies
        };
    }

    bool Initialize() override {
        std::cout << "[TEST MODULE] Initializing..." << std::endl;
        initialized = true;
        return true;
    }

    void Shutdown() override {
        std::cout << "[TEST MODULE] Shutting down..." << std::endl;
        initialized = false;
    }

    std::vector<SupportedDevice> GetSupportedDevices() const override {
        return {
            SupportedDevice{
                0xFFFF,  // vendor_id
                0xFFFF,  // product_id
                "Test RGB Device",
                "test_device",
                {"color", "brightness"}
            }
        };
    }

    IDeviceController* CreateController(
        uint16_t vendor_id,
        uint16_t product_id
    ) override {
        if (vendor_id == 0xFFFF && product_id == 0xFFFF) {
            return new TestDeviceController();
        }
        return nullptr;
    }

    bool IsCompatibleWithApi(int api_version) const override {
        return api_version >= 100;  // v1.0.0+
    }
};

} // namespace OneClickRGB

/*────────────────────────────────────────────────────────*/
/* DLL Exports                                            */
/*────────────────────────────────────────────────────────*/

extern "C" {
    OneClickRGB::IModule* __cdecl CreateModule() {
        return new OneClickRGB::TestModule();
    }

    void __cdecl DestroyModule(OneClickRGB::IModule* module) {
        delete module;
    }

    const char* __cdecl GetModuleVersion() {
        return "1.0.0";
    }

    const char* __cdecl GetModuleId() {
        return "com.oneclick.test.device";
    }

    int __cdecl GetModuleApiVersion() {
        return 100;  // v1.0.0
    }
}
```

---

## SCHRITT 4: CMakeLists.txt für Module (1 Stunde)

**Datei:** `src/modules/CMakeLists.txt`

```cmake
# Module Library (DLL)
add_library(test_module SHARED
    TestModule.h
    # Later: AsusModule.cpp, SteelSeriesModule.cpp, etc.
)

# Include directories
target_include_directories(test_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# C++ Standard
set_target_properties(test_module PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# DLL Output
set_target_properties(test_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
)

# Windows: .dll, Linux: .so
if(MSVC)
    set_target_properties(test_module PROPERTIES SUFFIX ".ocrgbmod")
endif()
```

---

## SCHRITT 5: Integration in Hauptanwendung (2 Stunden)

**Datei:** `src/core/OnClickRGB.cpp` (angepasst)

```cpp
#include "OneClickRGB.h"
#include "ModuleSystem/ModuleManager.h"
#include <iostream>

namespace OneClickRGB {

bool Application::Initialize(const AppConfig& cfg) {
    std::cout << "[APP] Initializing OneClickRGB..." << std::endl;

    try {
        // 1. Module System starten
        std::cout << "[APP] Loading module system..." << std::endl;
        auto& module_mgr = ModuleManager::Get();

        // 2. Module laden
        std::cout << "[APP] Loading modules..." << std::endl;
        
        // Test-Module laden
        std::string module_dir = cfg.app_directory + "/modules";
        if (!module_mgr.LoadModulesFromDirectory(module_dir)) {
            std::cerr << "[APP] Warning: Could not load modules from " 
                      << module_dir << std::endl;
        }

        // 3. Hardware scannen
        std::cout << "[APP] Scanning hardware..." << std::endl;
        auto devices = module_mgr.ScanAndLoadHardware();
        
        std::cout << "[APP] ✓ Found " << devices.size() << " RGB device(s)" 
                  << std::endl;

        // 4. Profile Manager starten
        profile_manager = std::make_unique<ProfileManager>();

        is_initialized = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[APP] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void Application::SetAllDevicesColor(uint8_t r, uint8_t g, uint8_t b) {
    auto& module_mgr = ModuleManager::Get();
    
    // Alle Devices durchgehen und Farbe setzen
    auto modules = module_mgr.GetLoadedModules();
    
    for (const auto& meta : modules) {
        auto module = module_mgr.GetModule(meta.id);
        if (module) {
            auto devices = module->GetSupportedDevices();
            for (const auto& device : devices) {
                auto controller = module_mgr.CreateDevice(
                    device.vendor_id,
                    device.product_id
                );
                if (controller) {
                    controller->SetColor(r, g, b);
                    delete controller;
                }
            }
        }
    }
}

} // namespace OneClickRGB
```

---

## SCHRITT 6: Test und Debug (1 Stunde)

**Datei:** `src/cli/main.cpp` (erweitert)

```cpp
#include "../core/OneClickRGB.h"
#include "../core/ModuleSystem/ModuleManager.h"
#include <iostream>

int main(int argc, char** argv) {
    using namespace OneClickRGB;
    
    std::cout << "OneClickRGB - Module System Test" << std::endl;
    std::cout << "================================" << std::endl;

    // 1. ModuleManager testen
    std::cout << "\n[TEST] Loading modules..." << std::endl;
    auto& mgr = ModuleManager::Get();
    
    try {
        if (!mgr.LoadModule("modules/test_module.ocrgbmod")) {
            std::cerr << "Error: " << mgr.GetLastError() << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    // 2. Geladene Module anzeigen
    std::cout << "\n[TEST] Loaded modules:" << std::endl;
    auto modules = mgr.GetLoadedModules();
    for (const auto& meta : modules) {
        std::cout << "  - " << meta.name << " v" << meta.version << std::endl;
    }

    // 3. Device erstellen
    std::cout << "\n[TEST] Creating device..." << std::endl;
    auto device = mgr.CreateDevice(0xFFFF, 0xFFFF);
    if (device) {
        std::cout << "  ✓ " << device->GetName() << std::endl;
        
        // Test: Farbe setzen
        device->SetColor(255, 0, 0);
        device->SetBrightness(100);
        
        delete device;
    } else {
        std::cerr << "  ✗ Failed to create device" << std::endl;
        return 1;
    }

    std::cout << "\n✓ All tests passed!" << std::endl;
    return 0;
}
```

---

## BUILD-ANLEITUNG

### CMakeLists.txt anpassen (Root)

```cmake
# Unter "add_subdirectory(src)":
add_subdirectory(src/core/ModuleSystem)
add_subdirectory(src/modules)
```

### Bauen:

```bash
cd OneClickRGB
cmake -B build -S .
cmake --build build --config Release --parallel 4
```

### Testen:

```bash
# Test-Module sollte in build/modules/ sein
# App ausführen:
build/bin/Release/oneclickrgb.exe
```

---

## NÄCHSTE SCHRITTE

1. **ASUS-Module migrieren** (2-3 Stunden)
2. **SteelSeries-Module migrieren** (2-3 Stunden)
3. **E-Vision-Module migrieren** (2-3 Stunden)
4. **Hardware-Scan mit Module-Loading** (3-4 Stunden)
5. **GUI Module-Manager** (4-5 Stunden)

---

**Total für v1.0: ~15-20 Stunden Entwicklung**

