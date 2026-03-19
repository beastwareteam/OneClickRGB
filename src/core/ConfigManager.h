/*---------------------------------------------------------*\
| ConfigManager.h                                           |
|                                                           |
| Persistent configuration storage                          |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace OneClickRGB {

using json = nlohmann::json;

/*---------------------------------------------------------*\
| Application Settings (persisted)                          |
\*---------------------------------------------------------*/
struct AppSettings
{
    // Startup
    bool            autostart_enabled = false;
    std::string     startup_profile;
    bool            apply_on_startup = true;

    // Behavior
    bool            minimize_to_tray = true;
    bool            start_minimized = false;
    bool            show_notifications = true;

    // Performance
    bool            lazy_load = true;
    int             scan_interval_ms = 5000;

    // Last state (for "remember last")
    std::string     last_profile;
    uint32_t        last_color = 0xFFFFFF;  // White
    uint8_t         last_brightness = 100;

    // Window state
    int             window_x = -1;
    int             window_y = -1;
    int             window_width = 800;
    int             window_height = 600;
};

/*---------------------------------------------------------*\
| ConfigManager Class                                       |
\*---------------------------------------------------------*/
class ConfigManager
{
public:
    static ConfigManager& Get();

    // Prevent copying
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /*-----------------------------------------------------*\
    | Configuration Directory                               |
    \*-----------------------------------------------------*/
    void SetConfigDirectory(const std::string& path);
    std::string GetConfigDirectory() const { return config_directory; }
    std::string GetConfigFilePath() const;

    /*-----------------------------------------------------*\
    | Settings Access                                       |
    \*-----------------------------------------------------*/
    AppSettings& Settings() { return settings; }
    const AppSettings& Settings() const { return settings; }

    /*-----------------------------------------------------*\
    | Persistence                                           |
    \*-----------------------------------------------------*/
    bool Save();
    bool Load();

    // Save specific values (auto-saves)
    void SetStartupProfile(const std::string& profile);
    void SetAutostart(bool enabled);
    void SetLastColor(uint8_t r, uint8_t g, uint8_t b);
    void SetLastBrightness(uint8_t brightness);
    void RememberLastProfile(const std::string& profile);

private:
    ConfigManager();
    ~ConfigManager();

    std::string config_directory;
    AppSettings settings;

    // JSON helpers
    json SettingsToJson() const;
    void JsonToSettings(const json& j);
};

} // namespace OneClickRGB
