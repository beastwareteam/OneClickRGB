/*---------------------------------------------------------*\
| AutoStart.h                                               |
|                                                           |
| System autostart integration for One-Click ready          |
|                                                           |
| This file is part of the OneClickRGB project              |
\*---------------------------------------------------------*/

#pragma once

#include <string>

namespace OneClickRGB {

/*---------------------------------------------------------*\
| AutoStart Class                                           |
| Manages system startup integration                        |
\*---------------------------------------------------------*/
class AutoStart
{
public:
    /*-----------------------------------------------------*\
    | Enable/Disable Autostart                              |
    \*-----------------------------------------------------*/

    // Enable autostart with optional profile to load
    static bool Enable(const std::string& profile_name = "");

    // Disable autostart
    static bool Disable();

    // Check if autostart is enabled
    static bool IsEnabled();

    /*-----------------------------------------------------*\
    | Autostart Configuration                               |
    \*-----------------------------------------------------*/

    // Get the command that will be run at startup
    static std::string GetStartupCommand();

    // Set custom startup arguments
    static void SetStartupArguments(const std::string& args);

    // Get current startup profile
    static std::string GetStartupProfile();

private:
    static std::string startup_arguments;

#ifdef _WIN32
    static bool EnableWindows(const std::string& profile_name);
    static bool DisableWindows();
    static bool IsEnabledWindows();
#else
    static bool EnableLinux(const std::string& profile_name);
    static bool DisableLinux();
    static bool IsEnabledLinux();
#endif
};

} // namespace OneClickRGB
