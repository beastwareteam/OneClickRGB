/*---------------------------------------------------------*\
| ProfileManager.cpp                                        |
|                                                           |
| JSON-based profile storage implementation                 |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "ProfileManager.h"
#include "DeviceManager.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace OneClickRGB {

/*---------------------------------------------------------*\
| JSON Serialization Helpers                                |
\*---------------------------------------------------------*/
void to_json(json& j, const RGBColor& c)
{
    j = json{{"r", c.r}, {"g", c.g}, {"b", c.b}};
}

void from_json(const json& j, RGBColor& c)
{
    j.at("r").get_to(c.r);
    j.at("g").get_to(c.g);
    j.at("b").get_to(c.b);
}

void to_json(json& j, const DeviceProfileState& state)
{
    j = json{
        {"hardware_id", state.hardware_id},
        {"device_name", state.device_name},
        {"active_mode", state.active_mode},
        {"brightness", state.brightness},
        {"colors", state.colors}
    };
}

void from_json(const json& j, DeviceProfileState& state)
{
    j.at("hardware_id").get_to(state.hardware_id);
    j.at("device_name").get_to(state.device_name);
    j.at("active_mode").get_to(state.active_mode);
    j.at("brightness").get_to(state.brightness);
    j.at("colors").get_to(state.colors);
}

void to_json(json& j, const Profile& p)
{
    j = json{
        {"name", p.name},
        {"description", p.description},
        {"device_states", p.device_states},
        {"created_timestamp", p.created_timestamp},
        {"modified_timestamp", p.modified_timestamp},
        {"is_favorite", p.is_favorite},
        {"hotkey", p.hotkey}
    };
}

void from_json(const json& j, Profile& p)
{
    j.at("name").get_to(p.name);
    j.at("description").get_to(p.description);
    j.at("device_states").get_to(p.device_states);
    j.at("created_timestamp").get_to(p.created_timestamp);
    j.at("modified_timestamp").get_to(p.modified_timestamp);

    if (j.contains("is_favorite"))
        j.at("is_favorite").get_to(p.is_favorite);
    if (j.contains("hotkey"))
        j.at("hotkey").get_to(p.hotkey);
}

/*---------------------------------------------------------*\
| ProfileManager Implementation                             |
\*---------------------------------------------------------*/
ProfileManager::ProfileManager()
{
    // Default profile directory
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata)
    {
        profile_directory = std::string(appdata) + "\\OneClickRGB\\profiles";
    }
    else
    {
        profile_directory = "profiles";
    }
#else
    const char* home = std::getenv("HOME");
    if (home)
    {
        profile_directory = std::string(home) + "/.config/OneClickRGB/profiles";
    }
    else
    {
        profile_directory = "profiles";
    }
#endif

    // Create directory if it doesn't exist
    fs::create_directories(profile_directory);

    // Load existing profiles
    RefreshProfileList();
}

ProfileManager::~ProfileManager()
{
}

void ProfileManager::SetProfileDirectory(const std::string& path)
{
    profile_directory = path;
    fs::create_directories(profile_directory);
    RefreshProfileList();
}

std::string ProfileManager::GetProfileFilePath(const std::string& name) const
{
    return profile_directory + "/" + name + ".json";
}

bool ProfileManager::SaveProfile(const std::string& name,
                                  const std::string& description)
{
    Profile profile = CaptureCurrentState(name, description);

    if (SaveProfileToFile(profile))
    {
        profiles[name] = profile;
        return true;
    }

    return false;
}

Profile ProfileManager::CaptureCurrentState(const std::string& name,
                                             const std::string& description)
{
    Profile profile;
    profile.name = name;
    profile.description = description;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    profile.created_timestamp = timestamp;
    profile.modified_timestamp = timestamp;

    // Capture state from all devices
    auto& manager = DeviceManager::Get();
    auto devices = manager.GetDevices();

    for (auto* device : devices)
    {
        DeviceProfileState state;
        state.hardware_id = device->GetHardwareId();
        state.device_name = device->GetName();
        state.active_mode = device->GetActiveMode();
        state.brightness = device->GetBrightness();
        state.colors = device->GetAllColors();

        profile.device_states.push_back(state);
    }

    return profile;
}

bool ProfileManager::SaveProfileToFile(const Profile& profile)
{
    try
    {
        json j = profile;

        std::ofstream file(GetProfileFilePath(profile.name));
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

bool ProfileManager::LoadProfile(const std::string& name)
{
    auto it = profiles.find(name);
    if (it == profiles.end())
    {
        // Try loading from file
        Profile profile;
        if (!LoadProfileFromFile(GetProfileFilePath(name), profile))
        {
            return false;
        }
        profiles[name] = profile;
        it = profiles.find(name);
    }

    return ApplyProfile(it->second);
}

bool ProfileManager::LoadProfileFromFile(const std::string& filepath,
                                          Profile& out_profile)
{
    try
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            return false;
        }

        json j;
        file >> j;
        out_profile = j.get<Profile>();
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ProfileManager::ApplyProfile(const Profile& profile)
{
    auto& manager = DeviceManager::Get();

    for (const auto& state : profile.device_states)
    {
        auto* device = manager.GetDeviceByHardwareId(state.hardware_id);
        if (device == nullptr)
        {
            continue;  // Device not connected, skip
        }

        // Initialize if needed
        if (!device->IsInitialized())
        {
            device->Initialize();
        }

        // Apply state
        device->SetMode(state.active_mode);
        device->SetBrightness(state.brightness);

        // Apply colors
        for (size_t i = 0; i < state.colors.size(); i++)
        {
            device->SetLED(static_cast<uint32_t>(i), state.colors[i]);
        }

        device->UpdateLEDs();
    }

    return true;
}

bool ProfileManager::DeleteProfile(const std::string& name)
{
    profiles.erase(name);

    try
    {
        fs::remove(GetProfileFilePath(name));
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ProfileManager::RenameProfile(const std::string& old_name,
                                    const std::string& new_name)
{
    auto it = profiles.find(old_name);
    if (it == profiles.end())
    {
        return false;
    }

    Profile profile = it->second;
    profile.name = new_name;

    if (!SaveProfileToFile(profile))
    {
        return false;
    }

    DeleteProfile(old_name);
    profiles[new_name] = profile;

    return true;
}

bool ProfileManager::ProfileExists(const std::string& name) const
{
    return profiles.find(name) != profiles.end() ||
           fs::exists(GetProfileFilePath(name));
}

std::vector<std::string> ProfileManager::GetProfileNames() const
{
    std::vector<std::string> names;
    names.reserve(profiles.size());

    for (const auto& [name, _] : profiles)
    {
        names.push_back(name);
    }

    return names;
}

std::vector<Profile> ProfileManager::GetAllProfiles() const
{
    std::vector<Profile> result;
    result.reserve(profiles.size());

    for (const auto& [_, profile] : profiles)
    {
        result.push_back(profile);
    }

    return result;
}

Profile* ProfileManager::GetProfile(const std::string& name)
{
    auto it = profiles.find(name);
    if (it != profiles.end())
    {
        return &it->second;
    }
    return nullptr;
}

void ProfileManager::RefreshProfileList()
{
    profiles.clear();

    try
    {
        for (const auto& entry : fs::directory_iterator(profile_directory))
        {
            if (entry.path().extension() == ".json")
            {
                Profile profile;
                if (LoadProfileFromFile(entry.path().string(), profile))
                {
                    profiles[profile.name] = profile;
                }
            }
        }
    }
    catch (const std::exception&)
    {
        // Directory might not exist yet
    }
}

bool ProfileManager::ApplyProfileByIndex(size_t index)
{
    auto names = GetProfileNames();
    if (index >= names.size())
    {
        return false;
    }

    return LoadProfile(names[index]);
}

std::vector<std::string> ProfileManager::GetFavoriteProfiles() const
{
    std::vector<std::string> favorites;

    for (const auto& [name, profile] : profiles)
    {
        if (profile.is_favorite)
        {
            favorites.push_back(name);
        }
    }

    return favorites;
}

void ProfileManager::SetFavorite(const std::string& name, bool is_favorite)
{
    auto it = profiles.find(name);
    if (it != profiles.end())
    {
        it->second.is_favorite = is_favorite;
        SaveProfileToFile(it->second);
    }
}

void ProfileManager::SetStartupProfile(const std::string& name)
{
    startup_profile = name;
    // TODO: Save to config file
}

bool ProfileManager::LoadStartupProfile()
{
    if (startup_profile.empty())
    {
        return false;
    }

    return LoadProfile(startup_profile);
}

bool ProfileManager::ExportProfile(const std::string& name,
                                    const std::string& export_path)
{
    auto it = profiles.find(name);
    if (it == profiles.end())
    {
        return false;
    }

    try
    {
        json j = it->second;
        std::ofstream file(export_path);
        file << j.dump(4);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ProfileManager::ImportProfile(const std::string& import_path)
{
    Profile profile;
    if (!LoadProfileFromFile(import_path, profile))
    {
        return false;
    }

    profiles[profile.name] = profile;
    return SaveProfileToFile(profile);
}

} // namespace OneClickRGB
