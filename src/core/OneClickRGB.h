/*---------------------------------------------------------*\
| OneClickRGB.h                                             |
|                                                           |
| Main Application Class - Simple One-Click RGB Control     |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include "DeviceManager.h"
#include "ProfileManager.h"
#include <string>
#include <memory>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Application Configuration                                 |
\*---------------------------------------------------------*/
struct AppConfig
{
    // Startup behavior
    bool        start_minimized = false;
    bool        load_startup_profile = true;
    bool        auto_scan_devices = true;

    // Tray icon
    bool        show_tray_icon = true;
    bool        minimize_to_tray = true;

    // Performance
    bool        lazy_load_controllers = true;  // Only load when needed

    // Paths
    std::string config_directory;
    std::string profile_directory;
};

/*---------------------------------------------------------*\
| OneClickRGB Application Class                             |
\*---------------------------------------------------------*/
class Application
{
public:
    static Application& Get();

    // Prevent copying
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /*-----------------------------------------------------*\
    | Lifecycle                                             |
    \*-----------------------------------------------------*/
    bool Initialize(const AppConfig& config = AppConfig());
    void Run();
    void Shutdown();

    /*-----------------------------------------------------*\
    | Quick Access                                          |
    \*-----------------------------------------------------*/
    DeviceManager& Devices() { return DeviceManager::Get(); }
    ProfileManager& Profiles() { return *profile_manager; }

    /*-----------------------------------------------------*\
    | One-Click Actions (Main Feature)                      |
    \*-----------------------------------------------------*/

    // Apply color to all devices instantly
    void OneClickColor(const RGBColor& color);
    void OneClickColor(uint8_t r, uint8_t g, uint8_t b);

    // Quick presets
    void OneClickRed()      { OneClickColor(RGBColor::Red()); }
    void OneClickGreen()    { OneClickColor(RGBColor::Green()); }
    void OneClickBlue()     { OneClickColor(RGBColor::Blue()); }
    void OneClickWhite()    { OneClickColor(RGBColor::White()); }
    void OneClickOff()      { Devices().TurnOffAll(); }

    // Apply profile instantly
    void OneClickProfile(const std::string& profile_name);
    void OneClickProfile(size_t profile_index);

    /*-----------------------------------------------------*\
    | Status                                                |
    \*-----------------------------------------------------*/
    bool IsInitialized() const { return is_initialized; }
    std::string GetVersion() const { return "1.0.0"; }
    size_t GetDeviceCount() const;
    std::string GetStatusMessage() const;

    /*-----------------------------------------------------*\
    | Configuration                                         |
    \*-----------------------------------------------------*/
    AppConfig& Config() { return config; }
    void SaveConfig();
    void LoadConfig();

private:
    Application();
    ~Application();

    bool is_initialized = false;
    AppConfig config;
    std::unique_ptr<ProfileManager> profile_manager;
};

/*---------------------------------------------------------*\
| Convenience Macros for Ultra-Simple Usage                 |
\*---------------------------------------------------------*/

// Example usage:
// RGB_SET_COLOR(255, 0, 0);  // All devices red
// RGB_OFF();                 // All devices off
// RGB_PROFILE("Gaming");     // Load Gaming profile

#define RGB_APP         OneClickRGB::Application::Get()
#define RGB_DEVICES     OneClickRGB::Application::Get().Devices()
#define RGB_PROFILES    OneClickRGB::Application::Get().Profiles()

#define RGB_SET_COLOR(r, g, b)  RGB_APP.OneClickColor(r, g, b)
#define RGB_RED()               RGB_APP.OneClickRed()
#define RGB_GREEN()             RGB_APP.OneClickGreen()
#define RGB_BLUE()              RGB_APP.OneClickBlue()
#define RGB_WHITE()             RGB_APP.OneClickWhite()
#define RGB_OFF()               RGB_APP.OneClickOff()
#define RGB_PROFILE(name)       RGB_APP.OneClickProfile(name)

} // namespace OneClickRGB
