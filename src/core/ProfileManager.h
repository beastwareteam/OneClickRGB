/*---------------------------------------------------------*\
| ProfileManager.h                                          |
|                                                           |
| JSON-based profile storage and loading                    |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include "../devices/RGBDevice.h"

namespace OneClickRGB {

/*---------------------------------------------------------*\
| Device State in Profile                                   |
\*---------------------------------------------------------*/
struct DeviceProfileState
{
    std::string             hardware_id;
    std::string             device_name;
    int                     active_mode = 0;
    uint8_t                 brightness = 100;
    std::vector<RGBColor>   colors;
};

/*---------------------------------------------------------*\
| Profile Definition                                        |
\*---------------------------------------------------------*/
struct Profile
{
    std::string                         name;
    std::string                         description;
    std::vector<DeviceProfileState>     device_states;
    uint64_t                            created_timestamp = 0;
    uint64_t                            modified_timestamp = 0;

    // Quick action flags
    bool                                is_favorite = false;
    std::string                         hotkey;     // e.g., "Ctrl+Shift+1"
};

/*---------------------------------------------------------*\
| ProfileManager Class                                      |
\*---------------------------------------------------------*/
class ProfileManager
{
public:
    ProfileManager();
    ~ProfileManager();

    /*-----------------------------------------------------*\
    | Directory Configuration                               |
    \*-----------------------------------------------------*/
    void SetProfileDirectory(const std::string& path);
    std::string GetProfileDirectory() const { return profile_directory; }

    /*-----------------------------------------------------*\
    | Profile Operations                                    |
    \*-----------------------------------------------------*/

    // Save current device states as profile
    bool SaveProfile(const std::string& name,
                     const std::string& description = "");

    // Load and apply profile
    bool LoadProfile(const std::string& name);

    // Delete profile
    bool DeleteProfile(const std::string& name);

    // Rename profile
    bool RenameProfile(const std::string& old_name,
                       const std::string& new_name);

    // Check if profile exists
    bool ProfileExists(const std::string& name) const;

    /*-----------------------------------------------------*\
    | Profile List                                          |
    \*-----------------------------------------------------*/
    std::vector<std::string> GetProfileNames() const;
    std::vector<Profile> GetAllProfiles() const;
    Profile* GetProfile(const std::string& name);

    // Refresh profile list from disk
    void RefreshProfileList();

    /*-----------------------------------------------------*\
    | Quick Apply                                           |
    \*-----------------------------------------------------*/

    // Apply profile by index (for One-Click)
    bool ApplyProfileByIndex(size_t index);

    // Get favorite profiles for quick access
    std::vector<std::string> GetFavoriteProfiles() const;

    // Set/unset favorite
    void SetFavorite(const std::string& name, bool is_favorite);

    /*-----------------------------------------------------*\
    | Startup Profile                                       |
    \*-----------------------------------------------------*/
    void SetStartupProfile(const std::string& name);
    std::string GetStartupProfile() const { return startup_profile; }
    bool LoadStartupProfile();

    /*-----------------------------------------------------*\
    | Import/Export                                         |
    \*-----------------------------------------------------*/
    bool ExportProfile(const std::string& name,
                       const std::string& export_path);
    bool ImportProfile(const std::string& import_path);

private:
    std::string profile_directory;
    std::string startup_profile;
    std::map<std::string, Profile> profiles;

    // File path helpers
    std::string GetProfileFilePath(const std::string& name) const;

    // JSON serialization
    bool SaveProfileToFile(const Profile& profile);
    bool LoadProfileFromFile(const std::string& filepath, Profile& out_profile);

    // Capture current device states
    Profile CaptureCurrentState(const std::string& name,
                                 const std::string& description);

    // Apply profile to devices
    bool ApplyProfile(const Profile& profile);
};

} // namespace OneClickRGB
