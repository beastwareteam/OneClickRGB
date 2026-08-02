/**
 * Tests for the autostart task definition.
 *
 * The task XML is the fix for the reported problem: a Run-key entry cannot
 * start an app that requests administrator rights, so the RAM never got its
 * colour at logon. These assertions guard the properties that make the task
 * work - they are not observable from inside the running application, which is
 * why the bug went unnoticed.
 */

#include "../src/autostart.h"

#include <string>

#include "test_framework.h"

namespace {

bool Contains(const std::wstring& haystack, const wchar_t* needle) {
    return haystack.find(needle) != std::wstring::npos;
}

std::wstring SampleXml() {
    return autostart::BuildTaskXml(L"C:\\Program Files\\OneClickRGB\\OneClickRGB.exe",
                                   L"--minimized", L"MACHINE\\User");
}

}  // namespace

TEST(autostart_xml, runs_with_the_highest_available_privileges) {
    // Without this the task starts unelevated, PawnIO refuses to open, and the
    // RAM stays dark - the exact symptom this change fixes.
    CHECK_MSG(Contains(SampleXml(), L"<RunLevel>HighestAvailable</RunLevel>"),
              "task would start unelevated");
}

TEST(autostart_xml, triggers_at_logon) {
    const std::wstring xml = SampleXml();
    CHECK(Contains(xml, L"<LogonTrigger>"));
    CHECK(Contains(xml, L"<Enabled>true</Enabled>"));
}

TEST(autostart_xml, runs_as_the_interactive_user) {
    // A tray application registered for SYSTEM would have no desktop session.
    const std::wstring xml = SampleXml();
    CHECK(Contains(xml, L"<LogonType>InteractiveToken</LogonType>"));
    CHECK(Contains(xml, L"MACHINE\\User"));
}

TEST(autostart_xml, has_no_execution_time_limit) {
    // The default limit terminates tasks after three days, which would silently
    // kill a tray application that is meant to stay resident.
    CHECK(Contains(SampleXml(), L"<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"));
}

TEST(autostart_xml, is_not_suppressed_on_battery) {
    const std::wstring xml = SampleXml();
    CHECK(Contains(xml, L"<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"));
    CHECK(Contains(xml, L"<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"));
}

TEST(autostart_xml, delays_the_start_so_drivers_are_ready) {
    // USB enumeration and the PawnIO service are not necessarily finished when
    // the desktop appears.
    CHECK(Contains(SampleXml(), L"<Delay>PT10S</Delay>"));
}

TEST(autostart_xml, carries_the_executable_and_its_arguments) {
    const std::wstring xml = SampleXml();
    CHECK(Contains(xml, L"<Command>C:\\Program Files\\OneClickRGB\\OneClickRGB.exe</Command>"));
    CHECK(Contains(xml, L"<Arguments>--minimized</Arguments>"));
}

TEST(autostart_xml, working_directory_is_the_install_folder) {
    // Relative lookups for PawnIOLib.dll and SmbusI801.bin depend on this.
    CHECK(Contains(SampleXml(),
                   L"<WorkingDirectory>C:\\Program Files\\OneClickRGB</WorkingDirectory>"));
}

TEST(autostart_xml, xml_special_characters_are_escaped) {
    // Machine and account names may legitimately contain '&'.
    const std::wstring xml = autostart::BuildTaskXml(
        L"C:\\A&B\\app.exe", L"--minimized", L"DOM&AIN\\Us<er>");

    CHECK_MSG(Contains(xml, L"A&amp;B"), "path was not escaped");
    CHECK_MSG(Contains(xml, L"DOM&amp;AIN"), "user domain was not escaped");
    CHECK_MSG(Contains(xml, L"Us&lt;er&gt;"), "user name was not escaped");

    // A raw '&' would make the document malformed and schtasks reject it.
    size_t pos = 0;
    while ((pos = xml.find(L'&', pos)) != std::wstring::npos) {
        const std::wstring tail = xml.substr(pos, 6);
        const bool is_entity =
            tail.rfind(L"&amp;", 0) == 0 || tail.rfind(L"&lt;", 0) == 0 ||
            tail.rfind(L"&gt;", 0) == 0 || tail.rfind(L"&quot;", 0) == 0 ||
            tail.rfind(L"&apos;", 0) == 0;
        CHECK_MSG(is_entity, "unescaped '&' in the task XML");
        ++pos;
    }
}

TEST(autostart_xml, declares_utf16_which_is_what_schtasks_requires) {
    CHECK(Contains(SampleXml(), L"encoding=\"UTF-16\""));
}

TEST(autostart_xml, uses_the_task_scheduler_namespace) {
    CHECK(Contains(SampleXml(),
                   L"http://schemas.microsoft.com/windows/2004/02/mit/task"));
}

TEST(autostart_xml, a_second_logon_does_not_start_a_duplicate) {
    CHECK(Contains(SampleXml(),
                   L"<MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"));
}
