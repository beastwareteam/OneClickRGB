/*---------------------------------------------------------*\
| OneClickRGB.cpp                                           |
|                                                           |
| Main Application Implementation                           |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "OneClickRGB.h"
#include "modules/ModuleManager.h"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Singleton Instance                                        |
\*---------------------------------------------------------*/
Application& Application::Get()
{
    static Application instance;
    return instance;
}

Application::Application()
{
}

Application::~Application()
{
    Shutdown();
}

/*---------------------------------------------------------*\
| Lifecycle                                                 |
\*---------------------------------------------------------*/
bool Application::Initialize(const AppConfig& cfg)
{
    if (is_initialized)
    {
        return true;
    }

    config = cfg;

    // Setup default directories if not specified
    if (config.config_directory.empty())
    {
#ifdef _WIN32
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
        {
            config.config_directory = std::string(path) + "\\OneClickRGB";
        }
        else
        {
            config.config_directory = "config";
        }
#else
        const char* home = std::getenv("HOME");
        if (home)
        {
            config.config_directory = std::string(home) + "/.config/OneClickRGB";
        }
        else
        {
            config.config_directory = "config";
        }
#endif
    }

    if (config.profile_directory.empty())
    {
        config.profile_directory = config.config_directory + "/profiles";
    }

    // Initialize module system
    auto& moduleManager = ModuleManager::getInstance();
    std::string modulePath = config.config_directory + "/modules";
    if (!moduleManager.Initialize(modulePath)) {
        std::cerr << "[OneClickRGB] Failed to initialize module system" << std::endl;
        return false;
    }

    // Register built-in modules (will be loaded from DLLs or built-in factories)
    // For now, we'll load them dynamically
    moduleManager.LoadModule("asus_aura");
    moduleManager.LoadModule("steelseries");

    // Load all available modules
    size_t loadedModules = moduleManager.LoadAllModules();
    std::cout << "[OneClickRGB] Loaded " << loadedModules << " modules" << std::endl;

    // Initialize profile manager
    profile_manager = std::make_unique<ProfileManager>();
    profile_manager->SetProfileDirectory(config.profile_directory);

    // Auto-scan devices if enabled
    if (config.auto_scan_devices)
    {
        DeviceManager::Get().QuickScan();
    }

    // Load startup profile if enabled
    if (config.load_startup_profile)
    {
        profile_manager->LoadStartupProfile();
    }

    is_initialized = true;
    return true;
}

void Application::Run()
{
    // Main loop would go here for GUI applications
    // For library usage, this is optional
}

void Application::Shutdown()
{
    if (!is_initialized)
    {
        return;
    }

    // Turn off all devices (optional, configurable)
    // Devices().TurnOffAll();

    // Shutdown device manager
    DeviceManager::Get().Shutdown();

    // Clear profile manager
    profile_manager.reset();

    is_initialized = false;
}

/*---------------------------------------------------------*\
| One-Click Actions                                         |
\*---------------------------------------------------------*/
void Application::OneClickColor(const RGBColor& color)
{
    auto& devices = DeviceManager::Get();

    // Scan if not done yet
    if (devices.GetDeviceCount() == 0)
    {
        devices.QuickScan();
    }

    // Set color and apply
    devices.SetAllDevicesColor(color);
    devices.UpdateAll();
}

void Application::OneClickColor(uint8_t r, uint8_t g, uint8_t b)
{
    OneClickColor(RGBColor(r, g, b));
}

void Application::OneClickProfile(const std::string& profile_name)
{
    if (profile_manager)
    {
        profile_manager->LoadProfile(profile_name);
    }
}

void Application::OneClickProfile(size_t profile_index)
{
    if (profile_manager)
    {
        profile_manager->ApplyProfileByIndex(profile_index);
    }
}

/*---------------------------------------------------------*\
| Status                                                    |
\*---------------------------------------------------------*/
size_t Application::GetDeviceCount() const
{
    return DeviceManager::Get().GetDeviceCount();
}

std::string Application::GetStatusMessage() const
{
    return DeviceManager::Get().GetStatusMessage();
}

/*---------------------------------------------------------*\
| Configuration                                             |
\*---------------------------------------------------------*/
void Application::SaveConfig()
{
    // TODO: Save config to JSON file
}

void Application::LoadConfig()
{
    // TODO: Load config from JSON file
}

} // namespace OneClickRGB
