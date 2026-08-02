#include "autostart.h"

#include <windows.h>

#include <fstream>
#include <sstream>

namespace autostart {

namespace {

wchar_t g_last_error[256] = L"";

void SetLastErrorText(const wchar_t* text) {
    wcsncpy_s(g_last_error, text, _TRUNCATE);
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path, n);
}

/// DOMAIN\User of the current process - the task must be registered for the
/// logging-on user, not for SYSTEM, or it will not see the desktop session.
std::wstring CurrentUserId() {
    wchar_t name[256];
    DWORD size = 256;
    if (!GetUserNameW(name, &size)) return L"";

    wchar_t domain[256];
    DWORD dsize = 256;
    if (GetComputerNameW(domain, &dsize))
        return std::wstring(domain) + L"\\" + name;
    return std::wstring(name);
}

/// XML 1.0 does not let these through raw, and a machine name or path can
/// legitimately contain them.
std::wstring XmlEscape(const std::wstring& in) {
    std::wstring out;
    out.reserve(in.size());
    for (wchar_t c : in) {
        switch (c) {
            case L'&':  out += L"&amp;";  break;
            case L'<':  out += L"&lt;";   break;
            case L'>':  out += L"&gt;";   break;
            case L'"':  out += L"&quot;"; break;
            case L'\'': out += L"&apos;"; break;
            default:    out += c;         break;
        }
    }
    return out;
}

std::wstring TempXmlPath() {
    wchar_t dir[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, dir);
    if (n == 0 || n >= MAX_PATH) return L"OneClickRGB_task.xml";
    return std::wstring(dir, n) + L"OneClickRGB_task.xml";
}

/// Runs a command without a console window and waits for it.
/// Returns the exit code, or -1 when the process could not be started.
int RunHidden(const std::wstring& command_line) {
    std::wstring mutable_cmd = command_line;

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(NULL, &mutable_cmd[0], NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exit_code);
}

bool WriteFileUtf16(const std::wstring& path, const std::wstring& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    // schtasks /XML expects UTF-16LE with a BOM.
    const unsigned char bom[2] = {0xFF, 0xFE};
    f.write(reinterpret_cast<const char*>(bom), 2);
    f.write(reinterpret_cast<const char*>(content.data()),
            static_cast<std::streamsize>(content.size() * sizeof(wchar_t)));
    return static_cast<bool>(f);
}

bool ScheduledTaskExists() {
    std::wstring cmd = L"schtasks.exe /Query /TN \"";
    cmd += TASK_NAME;
    cmd += L"\"";
    return RunHidden(cmd) == 0;
}

bool LegacyRunEntryExists() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, LEGACY_RUN_KEY, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    const bool exists =
        RegQueryValueExW(key, LEGACY_RUN_VALUE, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegCloseKey(key);
    return exists;
}

}  // namespace

std::wstring BuildTaskXml(const std::wstring& exe_path,
                          const std::wstring& arguments,
                          const std::wstring& user_id) {
    const std::wstring exe  = XmlEscape(exe_path);
    const std::wstring args = XmlEscape(arguments);
    const std::wstring user = XmlEscape(user_id);

    std::wstring working_dir = exe_path;
    const size_t slash = working_dir.find_last_of(L"\\/");
    working_dir = (slash == std::wstring::npos) ? L"" : XmlEscape(working_dir.substr(0, slash));

    std::wostringstream xml;
    xml << L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        << L"<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        << L"  <RegistrationInfo>\n"
        << L"    <Description>Starts OneClickRGB at logon with administrator rights "
           L"so RGB devices that need the SMBus driver are restored.</Description>\n"
        << L"  </RegistrationInfo>\n"
        << L"  <Triggers>\n"
        << L"    <LogonTrigger>\n"
        << L"      <Enabled>true</Enabled>\n"
        << L"      <UserId>" << user << L"</UserId>\n"
        // The SMBus driver and USB enumeration are not necessarily finished the
        // instant the desktop appears; a short delay avoids a first-run failure.
        << L"      <Delay>PT10S</Delay>\n"
        << L"    </LogonTrigger>\n"
        << L"  </Triggers>\n"
        << L"  <Principals>\n"
        << L"    <Principal id=\"Author\">\n"
        << L"      <UserId>" << user << L"</UserId>\n"
        << L"      <LogonType>InteractiveToken</LogonType>\n"
        // This is the whole point of the task: run elevated, no UAC prompt.
        << L"      <RunLevel>HighestAvailable</RunLevel>\n"
        << L"    </Principal>\n"
        << L"  </Principals>\n"
        << L"  <Settings>\n"
        << L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
        << L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
        << L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
        << L"    <AllowHardTerminate>true</AllowHardTerminate>\n"
        << L"    <StartWhenAvailable>true</StartWhenAvailable>\n"
        << L"    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\n"
        << L"    <IdleSettings>\n"
        << L"      <StopOnIdleEnd>false</StopOnIdleEnd>\n"
        << L"      <RestartOnIdle>false</RestartOnIdle>\n"
        << L"    </IdleSettings>\n"
        << L"    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
        << L"    <Enabled>true</Enabled>\n"
        << L"    <Hidden>false</Hidden>\n"
        << L"    <RunOnlyIfIdle>false</RunOnlyIfIdle>\n"
        // A tray app must not be killed after three days of uptime.
        << L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
        << L"    <Priority>7</Priority>\n"
        << L"  </Settings>\n"
        << L"  <Actions Context=\"Author\">\n"
        << L"    <Exec>\n"
        << L"      <Command>" << exe << L"</Command>\n"
        << L"      <Arguments>" << args << L"</Arguments>\n"
        << L"      <WorkingDirectory>" << working_dir << L"</WorkingDirectory>\n"
        << L"    </Exec>\n"
        << L"  </Actions>\n"
        << L"</Task>\n";
    return xml.str();
}

Status Query() {
    if (ScheduledTaskExists()) return Status::ScheduledTask;
    if (LegacyRunEntryExists()) return Status::LegacyRunKey;
    return Status::Disabled;
}

void RemoveLegacyRunEntry() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, LEGACY_RUN_KEY, 0, KEY_WRITE, &key) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(key, LEGACY_RUN_VALUE);
    RegCloseKey(key);
}

bool Enable() {
    SetLastErrorText(L"");

    const std::wstring user = CurrentUserId();
    if (user.empty()) {
        SetLastErrorText(L"Could not determine the current user account");
        return false;
    }

    const std::wstring xml = BuildTaskXml(ExePath(), L"--minimized", user);
    const std::wstring xml_path = TempXmlPath();
    if (!WriteFileUtf16(xml_path, xml)) {
        SetLastErrorText(L"Could not write the temporary task definition");
        return false;
    }

    std::wstring cmd = L"schtasks.exe /Create /F /TN \"";
    cmd += TASK_NAME;
    cmd += L"\" /XML \"" + xml_path + L"\"";
    const int rc = RunHidden(cmd);

    DeleteFileW(xml_path.c_str());

    if (rc != 0) {
        SetLastErrorText(L"schtasks could not create the task - administrator rights required");
        return false;
    }

    // Both mechanisms at once would start the app twice.
    RemoveLegacyRunEntry();
    return true;
}

bool Disable() {
    SetLastErrorText(L"");
    RemoveLegacyRunEntry();

    if (!ScheduledTaskExists()) return true;

    std::wstring cmd = L"schtasks.exe /Delete /F /TN \"";
    cmd += TASK_NAME;
    cmd += L"\"";
    if (RunHidden(cmd) != 0) {
        SetLastErrorText(L"schtasks could not delete the task");
        return false;
    }
    return true;
}

bool MigrateLegacyIfPresent() {
    if (!LegacyRunEntryExists()) return false;
    if (ScheduledTaskExists()) {
        // Task already does the job; the Run entry is a leftover.
        RemoveLegacyRunEntry();
        return true;
    }
    return Enable();
}

const wchar_t* LastError() {
    return g_last_error;
}

}  // namespace autostart
