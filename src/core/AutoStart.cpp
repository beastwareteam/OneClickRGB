/*---------------------------------------------------------*\
| AutoStart.cpp                                             |
|                                                           |
| System autostart implementation                           |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#include "AutoStart.h"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace OneClickRGB {

std::string AutoStart::startup_arguments;

/*---------------------------------------------------------*\
| Cross-Platform Interface                                  |
\*---------------------------------------------------------*/
bool AutoStart::Enable(const std::string& profile_name)
{
#ifdef _WIN32
    return EnableWindows(profile_name);
#else
    return EnableLinux(profile_name);
#endif
}

bool AutoStart::Disable()
{
#ifdef _WIN32
    return DisableWindows();
#else
    return DisableLinux();
#endif
}

bool AutoStart::IsEnabled()
{
#ifdef _WIN32
    return IsEnabledWindows();
#else
    return IsEnabledLinux();
#endif
}

std::string AutoStart::GetStartupCommand()
{
    std::string cmd;

#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    cmd = std::string(path);
#else
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        cmd = std::string(path);
    }
#endif

    if (!startup_arguments.empty())
    {
        cmd += " " + startup_arguments;
    }

    return cmd;
}

void AutoStart::SetStartupArguments(const std::string& args)
{
    startup_arguments = args;
}

std::string AutoStart::GetStartupProfile()
{
    // Parse from startup_arguments
    // Format: "profile <name>" or "--profile=<name>"
    size_t pos = startup_arguments.find("profile ");
    if (pos != std::string::npos)
    {
        return startup_arguments.substr(pos + 8);
    }
    return "";
}

/*---------------------------------------------------------*\
| Windows Implementation                                    |
\*---------------------------------------------------------*/
#ifdef _WIN32

bool AutoStart::EnableWindows(const std::string& profile_name)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    // Build command with profile if specified
    std::string cmd = GetStartupCommand();
    if (!profile_name.empty())
    {
        cmd += " profile \"" + profile_name + "\"";
    }
    else
    {
        // Default: apply last saved profile
        cmd += " --autostart";
    }

    result = RegSetValueExA(
        hKey,
        "OneClickRGB",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(cmd.c_str()),
        static_cast<DWORD>(cmd.length() + 1)
    );

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool AutoStart::DisableWindows()
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    result = RegDeleteValueA(hKey, "OneClickRGB");
    RegCloseKey(hKey);

    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool AutoStart::IsEnabledWindows()
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    result = RegQueryValueExA(hKey, "OneClickRGB", NULL, NULL, NULL, NULL);
    RegCloseKey(hKey);

    return result == ERROR_SUCCESS;
}

#else
/*---------------------------------------------------------*\
| Linux Implementation                                      |
\*---------------------------------------------------------*/

bool AutoStart::EnableLinux(const std::string& profile_name)
{
    const char* home = std::getenv("HOME");
    if (!home)
    {
        return false;
    }

    std::string autostart_dir = std::string(home) + "/.config/autostart";
    fs::create_directories(autostart_dir);

    std::string desktop_file = autostart_dir + "/oneclickrgb.desktop";

    // Build command
    std::string cmd = GetStartupCommand();
    if (!profile_name.empty())
    {
        cmd += " profile \"" + profile_name + "\"";
    }
    else
    {
        cmd += " --autostart";
    }

    // Create .desktop file
    std::ofstream file(desktop_file);
    if (!file.is_open())
    {
        return false;
    }

    file << "[Desktop Entry]\n";
    file << "Type=Application\n";
    file << "Name=OneClickRGB\n";
    file << "Comment=RGB Lighting Control\n";
    file << "Exec=" << cmd << "\n";
    file << "Icon=oneclickrgb\n";
    file << "Terminal=false\n";
    file << "Categories=Utility;\n";
    file << "X-GNOME-Autostart-enabled=true\n";

    file.close();
    return true;
}

bool AutoStart::DisableLinux()
{
    const char* home = std::getenv("HOME");
    if (!home)
    {
        return false;
    }

    std::string desktop_file = std::string(home) + "/.config/autostart/oneclickrgb.desktop";

    try
    {
        fs::remove(desktop_file);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool AutoStart::IsEnabledLinux()
{
    const char* home = std::getenv("HOME");
    if (!home)
    {
        return false;
    }

    std::string desktop_file = std::string(home) + "/.config/autostart/oneclickrgb.desktop";
    return fs::exists(desktop_file);
}

#endif

} // namespace OneClickRGB
