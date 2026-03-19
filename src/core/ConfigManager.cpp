/*---------------------------------------------------------*\
| ConfigManager.cpp                                         |
|                                                           |
| Persistent configuration implementation                   |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "ConfigManager.h"
#include "AutoStart.h"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Singleton Instance                                        |
\*---------------------------------------------------------*/
ConfigManager& ConfigManager::Get()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
{
    // Set default config directory
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        config_directory = std::string(path) + "\\OneClickRGB";
    }
    else
    {
        config_directory = "config";
    }
#else
    const char* home = std::getenv("HOME");
    if (home)
    {
        config_directory = std::string(home) + "/.config/OneClickRGB";
    }
    else
    {
        config_directory = "config";
    }
#endif

    // Create directory if needed
    fs::create_directories(config_directory);

    // Load existing config
    Load();
}

ConfigManager::~ConfigManager()
{
    Save();
}

void ConfigManager::SetConfigDirectory(const std::string& path)
{
    config_directory = path;
    fs::create_directories(config_directory);
    Load();
}

std::string ConfigManager::GetConfigFilePath() const
{
    return config_directory + "/settings.json";
}

/*---------------------------------------------------------*\
| Save/Load                                                 |
\*---------------------------------------------------------*/
bool ConfigManager::Save()
{
    try
    {
        json j = SettingsToJson();

        std::ofstream file(GetConfigFilePath());
        if (!file.is_open())
        {
            return false;
        }

        file << j.dump(4);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ConfigManager::Load()
{
    try
    {
        std::ifstream file(GetConfigFilePath());
        if (!file.is_open())
        {
            return false;
        }

        json j;
        file >> j;
        JsonToSettings(j);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

/*---------------------------------------------------------*\
| Quick Setters (auto-save)                                 |
\*---------------------------------------------------------*/
void ConfigManager::SetStartupProfile(const std::string& profile)
{
    settings.startup_profile = profile;
    Save();
}

void ConfigManager::SetAutostart(bool enabled)
{
    settings.autostart_enabled = enabled;

    if (enabled)
    {
        AutoStart::Enable(settings.startup_profile);
    }
    else
    {
        AutoStart::Disable();
    }

    Save();
}

void ConfigManager::SetLastColor(uint8_t r, uint8_t g, uint8_t b)
{
    settings.last_color = (b << 16) | (g << 8) | r;
    Save();
}

void ConfigManager::SetLastBrightness(uint8_t brightness)
{
    settings.last_brightness = brightness;
    Save();
}

void ConfigManager::RememberLastProfile(const std::string& profile)
{
    settings.last_profile = profile;
    Save();
}

/*---------------------------------------------------------*\
| JSON Serialization                                        |
\*---------------------------------------------------------*/
json ConfigManager::SettingsToJson() const
{
    return json{
        {"autostart_enabled", settings.autostart_enabled},
        {"startup_profile", settings.startup_profile},
        {"apply_on_startup", settings.apply_on_startup},
        {"minimize_to_tray", settings.minimize_to_tray},
        {"start_minimized", settings.start_minimized},
        {"show_notifications", settings.show_notifications},
        {"lazy_load", settings.lazy_load},
        {"scan_interval_ms", settings.scan_interval_ms},
        {"last_profile", settings.last_profile},
        {"last_color", settings.last_color},
        {"last_brightness", settings.last_brightness},
        {"window", {
            {"x", settings.window_x},
            {"y", settings.window_y},
            {"width", settings.window_width},
            {"height", settings.window_height}
        }}
    };
}

void ConfigManager::JsonToSettings(const json& j)
{
    if (j.contains("autostart_enabled"))
        settings.autostart_enabled = j["autostart_enabled"];
    if (j.contains("startup_profile"))
        settings.startup_profile = j["startup_profile"];
    if (j.contains("apply_on_startup"))
        settings.apply_on_startup = j["apply_on_startup"];
    if (j.contains("minimize_to_tray"))
        settings.minimize_to_tray = j["minimize_to_tray"];
    if (j.contains("start_minimized"))
        settings.start_minimized = j["start_minimized"];
    if (j.contains("show_notifications"))
        settings.show_notifications = j["show_notifications"];
    if (j.contains("lazy_load"))
        settings.lazy_load = j["lazy_load"];
    if (j.contains("scan_interval_ms"))
        settings.scan_interval_ms = j["scan_interval_ms"];
    if (j.contains("last_profile"))
        settings.last_profile = j["last_profile"];
    if (j.contains("last_color"))
        settings.last_color = j["last_color"];
    if (j.contains("last_brightness"))
        settings.last_brightness = j["last_brightness"];

    if (j.contains("window"))
    {
        auto& w = j["window"];
        if (w.contains("x")) settings.window_x = w["x"];
        if (w.contains("y")) settings.window_y = w["y"];
        if (w.contains("width")) settings.window_width = w["width"];
        if (w.contains("height")) settings.window_height = w["height"];
    }
}

} // namespace OneClickRGB
