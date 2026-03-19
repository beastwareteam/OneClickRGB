/*---------------------------------------------------------*\
| main.cpp                                                  |
|                                                           |
| OneClickRGB - Simple RGB Control Application              |
| True One-Click: Autostart support included                |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "core/OneClickRGB.h"
#include "core/AutoStart.h"
#include "core/ConfigManager.h"
#include <iostream>
#include <string>

using namespace OneClickRGB;

void PrintUsage()
{
    std::cout << "\n";
    std::cout << "OneClickRGB - Simple RGB Control\n";
    std::cout << "================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  oneclickrgb [command] [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  scan                Scan for RGB devices\n";
    std::cout << "  list                List detected devices\n";
    std::cout << "  color <r> <g> <b>   Set all devices to color\n";
    std::cout << "  red                 Set all devices to red\n";
    std::cout << "  green               Set all devices to green\n";
    std::cout << "  blue                Set all devices to blue\n";
    std::cout << "  white               Set all devices to white\n";
    std::cout << "  off                 Turn off all devices\n";
    std::cout << "  profile <name>      Load a saved profile\n";
    std::cout << "  save <name>         Save current state as profile\n";
    std::cout << "  profiles            List saved profiles\n";
    std::cout << "  autostart [on|off]  Enable/disable autostart\n";
    std::cout << "  startup <profile>   Set startup profile\n";
    std::cout << "  --autostart         Apply startup config (internal)\n";
    std::cout << "  help                Show this help\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  oneclickrgb red              # All devices red\n";
    std::cout << "  oneclickrgb color 255 128 0  # All devices orange\n";
    std::cout << "  oneclickrgb profile Gaming   # Load Gaming profile\n";
    std::cout << "  oneclickrgb autostart on     # Enable autostart\n";
    std::cout << "  oneclickrgb startup Gaming   # Load Gaming on boot\n";
    std::cout << "\n";
}

void PrintDeviceList()
{
    auto& app = Application::Get();
    auto devices = app.Devices().GetDevices();

    std::cout << "\nDetected RGB Devices: " << devices.size() << "\n";
    std::cout << "========================================\n\n";

    if (devices.empty())
    {
        std::cout << "No RGB devices found.\n";
        std::cout << "Make sure your RGB hardware is connected.\n\n";
        return;
    }

    int index = 0;
    for (auto* device : devices)
    {
        std::cout << "[" << index++ << "] " << device->GetName() << "\n";
        std::cout << "    Vendor:   " << device->GetVendor() << "\n";
        std::cout << "    ID:       " << device->GetHardwareId() << "\n";
        std::cout << "    Status:   " << (device->IsInitialized() ? "Ready" : "Not initialized") << "\n";
        std::cout << "\n";
    }
}

void PrintProfiles()
{
    auto& app = Application::Get();
    auto profiles = app.Profiles().GetProfileNames();

    std::cout << "\nSaved Profiles: " << profiles.size() << "\n";
    std::cout << "========================================\n\n";

    if (profiles.empty())
    {
        std::cout << "No profiles saved yet.\n";
        std::cout << "Use 'oneclickrgb save <name>' to save current state.\n\n";
        return;
    }

    auto& config = ConfigManager::Get();
    std::string startup = config.Settings().startup_profile;

    int index = 0;
    for (const auto& name : profiles)
    {
        auto* profile = app.Profiles().GetProfile(name);
        std::cout << "[" << index++ << "] " << name;
        if (profile && profile->is_favorite)
        {
            std::cout << " *";
        }
        if (name == startup)
        {
            std::cout << " [STARTUP]";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

/*---------------------------------------------------------*\
| Handle Autostart Mode                                     |
| Called when app starts via Windows/Linux autostart        |
\*---------------------------------------------------------*/
void HandleAutostart()
{
    auto& config = ConfigManager::Get();
    auto& app = Application::Get();

    // Scan for devices
    app.Devices().QuickScan();

    // Check what to apply
    std::string startup_profile = config.Settings().startup_profile;

    if (!startup_profile.empty())
    {
        // Apply startup profile
        std::cout << "Applying startup profile: " << startup_profile << "\n";
        app.Profiles().LoadProfile(startup_profile);
    }
    else if (config.Settings().last_color != 0)
    {
        // Apply last used color
        RGBColor color = RGBColor::FromUInt32(config.Settings().last_color);
        std::cout << "Applying last color: RGB("
                  << (int)color.r << ", "
                  << (int)color.g << ", "
                  << (int)color.b << ")\n";
        app.OneClickColor(color);
    }

    std::cout << "OneClickRGB autostart complete.\n";
}

int main(int argc, char* argv[])
{
    // Initialize application
    AppConfig appconfig;
    appconfig.lazy_load_controllers = true;
    appconfig.auto_scan_devices = true;

    auto& app = Application::Get();

    if (!app.Initialize(appconfig))
    {
        std::cerr << "Failed to initialize OneClickRGB\n";
        return 1;
    }

    auto& config = ConfigManager::Get();

    // No arguments - show help and device list
    if (argc < 2)
    {
        PrintUsage();
        PrintDeviceList();
        return 0;
    }

    std::string command = argv[1];

    // Process commands
    if (command == "help" || command == "-h" || command == "--help")
    {
        PrintUsage();
    }
    else if (command == "--autostart")
    {
        // Internal: called by system autostart
        HandleAutostart();
    }
    else if (command == "scan")
    {
        std::cout << "Scanning for RGB devices...\n";
        app.Devices().Rescan();
        PrintDeviceList();
    }
    else if (command == "list")
    {
        PrintDeviceList();
    }
    else if (command == "color")
    {
        if (argc < 5)
        {
            std::cerr << "Usage: oneclickrgb color <r> <g> <b>\n";
            return 1;
        }
        uint8_t r = static_cast<uint8_t>(std::stoi(argv[2]));
        uint8_t g = static_cast<uint8_t>(std::stoi(argv[3]));
        uint8_t b = static_cast<uint8_t>(std::stoi(argv[4]));

        std::cout << "Setting color: RGB(" << (int)r << ", " << (int)g << ", " << (int)b << ")\n";
        app.OneClickColor(r, g, b);

        // Remember last color
        config.SetLastColor(r, g, b);

        std::cout << "Done!\n";
    }
    else if (command == "red")
    {
        std::cout << "Setting all devices to RED\n";
        app.OneClickRed();
        config.SetLastColor(255, 0, 0);
        std::cout << "Done!\n";
    }
    else if (command == "green")
    {
        std::cout << "Setting all devices to GREEN\n";
        app.OneClickGreen();
        config.SetLastColor(0, 255, 0);
        std::cout << "Done!\n";
    }
    else if (command == "blue")
    {
        std::cout << "Setting all devices to BLUE\n";
        app.OneClickBlue();
        config.SetLastColor(0, 0, 255);
        std::cout << "Done!\n";
    }
    else if (command == "white")
    {
        std::cout << "Setting all devices to WHITE\n";
        app.OneClickWhite();
        config.SetLastColor(255, 255, 255);
        std::cout << "Done!\n";
    }
    else if (command == "off")
    {
        std::cout << "Turning off all devices\n";
        app.OneClickOff();
        config.SetLastColor(0, 0, 0);
        std::cout << "Done!\n";
    }
    else if (command == "profile")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: oneclickrgb profile <name>\n";
            PrintProfiles();
            return 1;
        }
        std::string profile_name = argv[2];
        std::cout << "Loading profile: " << profile_name << "\n";

        if (app.Profiles().LoadProfile(profile_name))
        {
            config.RememberLastProfile(profile_name);
            std::cout << "Profile loaded successfully!\n";
        }
        else
        {
            std::cerr << "Failed to load profile: " << profile_name << "\n";
            return 1;
        }
    }
    else if (command == "save")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: oneclickrgb save <name>\n";
            return 1;
        }
        std::string profile_name = argv[2];
        std::cout << "Saving profile: " << profile_name << "\n";

        if (app.Profiles().SaveProfile(profile_name))
        {
            std::cout << "Profile saved successfully!\n";
        }
        else
        {
            std::cerr << "Failed to save profile\n";
            return 1;
        }
    }
    else if (command == "profiles")
    {
        PrintProfiles();
    }
    else if (command == "autostart")
    {
        if (argc < 3)
        {
            // Show current status
            bool enabled = AutoStart::IsEnabled();
            std::cout << "Autostart: " << (enabled ? "ENABLED" : "DISABLED") << "\n";
            if (enabled && !config.Settings().startup_profile.empty())
            {
                std::cout << "Startup profile: " << config.Settings().startup_profile << "\n";
            }
            return 0;
        }

        std::string action = argv[2];
        if (action == "on" || action == "enable" || action == "1")
        {
            if (AutoStart::Enable(config.Settings().startup_profile))
            {
                config.SetAutostart(true);
                std::cout << "Autostart ENABLED\n";
                std::cout << "OneClickRGB will start automatically when you log in.\n";
            }
            else
            {
                std::cerr << "Failed to enable autostart\n";
                return 1;
            }
        }
        else if (action == "off" || action == "disable" || action == "0")
        {
            if (AutoStart::Disable())
            {
                config.SetAutostart(false);
                std::cout << "Autostart DISABLED\n";
            }
            else
            {
                std::cerr << "Failed to disable autostart\n";
                return 1;
            }
        }
        else
        {
            std::cerr << "Usage: oneclickrgb autostart [on|off]\n";
            return 1;
        }
    }
    else if (command == "startup")
    {
        if (argc < 3)
        {
            std::string current = config.Settings().startup_profile;
            if (current.empty())
            {
                std::cout << "No startup profile set.\n";
                std::cout << "Usage: oneclickrgb startup <profile_name>\n";
            }
            else
            {
                std::cout << "Current startup profile: " << current << "\n";
            }
            PrintProfiles();
            return 0;
        }

        std::string profile_name = argv[2];

        // Verify profile exists
        if (!app.Profiles().ProfileExists(profile_name))
        {
            std::cerr << "Profile not found: " << profile_name << "\n";
            std::cerr << "Create it first with: oneclickrgb save " << profile_name << "\n";
            return 1;
        }

        config.SetStartupProfile(profile_name);
        std::cout << "Startup profile set to: " << profile_name << "\n";

        // Update autostart if enabled
        if (AutoStart::IsEnabled())
        {
            AutoStart::Enable(profile_name);
            std::cout << "Autostart updated.\n";
        }
        else
        {
            std::cout << "Enable autostart with: oneclickrgb autostart on\n";
        }
    }
    else
    {
        std::cerr << "Unknown command: " << command << "\n";
        PrintUsage();
        return 1;
    }

    return 0;
}
