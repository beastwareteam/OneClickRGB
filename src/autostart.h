/**
 * autostart.h - Launch at logon, with administrator rights.
 *
 * Why this is not just a Run-key entry:
 *
 * OneClickRGB's manifest requests requireAdministrator, because the G.Skill RAM
 * path needs the PawnIO kernel driver. Windows refuses to auto-elevate anything
 * started from HKCU\...\Run or from the Startup folder - such entries are
 * silently skipped at logon (the shell reports them under "blocked startup
 * programs"). The result was that the app never ran at logon at all, so the RAM
 * kept whatever colour it had and only a manual "run as administrator" worked.
 *
 * A scheduled task with RunLevel=Highest and an AtLogOn trigger is the
 * supported way to get an elevated process at logon without a UAC prompt, so
 * that is what the autostart toggle now creates.
 *
 * Registration goes through schtasks.exe rather than the Task Scheduler COM
 * API: it is a fraction of the code, and creating the task already requires the
 * elevation the app runs with anyway.
 */

#pragma once
#include <string>

namespace autostart {

/// Task name registered under the Task Scheduler root folder.
constexpr const wchar_t* TASK_NAME = L"OneClickRGB Autostart";

/// Legacy locations that earlier versions used and that must be cleaned up,
/// otherwise a blocked Run entry lingers and keeps warning the user.
constexpr const wchar_t* LEGACY_RUN_KEY   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* LEGACY_RUN_VALUE = L"OneClickRGB";

enum class Status {
    Disabled,          ///< no autostart configured
    ScheduledTask,     ///< the working configuration
    LegacyRunKey,      ///< a Run-key entry exists; it will not start elevated
};

/// Current autostart configuration.
Status Query();

/// Builds the XML passed to schtasks /XML. Exposed for tests: getting
/// RunLevel or the trigger wrong is exactly the failure this module exists to
/// prevent, and it is not observable from the app itself.
std::wstring BuildTaskXml(const std::wstring& exe_path,
                          const std::wstring& arguments,
                          const std::wstring& user_id);

/// Creates (or replaces) the scheduled task. Returns false when schtasks
/// failed, e.g. because the process is not elevated.
bool Enable();

/// Removes the scheduled task and any legacy Run-key entry.
bool Disable();

/// Deletes a leftover Run-key entry from earlier versions. Safe to call always.
void RemoveLegacyRunEntry();

/// One-time upgrade: if a legacy Run entry exists, replace it with a task.
/// Returns true when a migration actually happened.
bool MigrateLegacyIfPresent();

/// Human-readable reason the last Enable()/Disable() failed, for the status log.
const wchar_t* LastError();

}  // namespace autostart
