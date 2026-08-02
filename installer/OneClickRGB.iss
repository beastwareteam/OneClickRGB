; OneClickRGB Installer Script for Inno Setup
; https://jrsoftware.org/isinfo.php

#define MyAppName "OneClickRGB"
; Keep in sync with APP_VERSION in src/oneclick_rgb_complete.cpp,
; project VERSION in CMakeLists.txt and FILEVERSION in src/OneClickRGB.rc.
#define MyAppVersion "3.6.0"
#define MyAppPublisher "OneClickRGB"
#define MyAppURL "https://github.com/yourusername/OneClickRGB"
#define MyAppExeName "OneClickRGB.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=OneClickRGB_Setup_{#MyAppVersion}
; SetupIconFile=..\resources\icons\app.ico  ; Uncomment when .ico is available
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start with Windows"; GroupDescription: "Startup options:"

[Files]
; Main executable
Source: "..\build\Release\OneClickRGB.exe"; DestDir: "{app}"; Flags: ignoreversion

; No Qt files here: OneClickRGB is a native Win32 application and has never
; linked Qt. The entries that used to be in this section referenced DLLs the
; build does not produce.

; Application icon (loaded at runtime for the window and tray)
Source: "..\src\icon.png"; DestDir: "{app}"; Flags: ignoreversion

; HIDAPI
Source: "..\dependencies\hidapi\hidapi.dll"; DestDir: "{app}"; Flags: ignoreversion

; PawnIO (for SMBus/RAM control)
Source: "..\dependencies\PawnIO\PawnIOLib.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dependencies\PawnIO\modules\*.bin"; DestDir: "{app}\modules"; Flags: ignoreversion

; No configuration files are shipped. Settings live in
; %APPDATA%\OneClickRGB\config.json and are created with defaults on first run
; (see RGBConfig in src/app_config.h). The entries previously here pointed at a
; config\ directory that does not exist, which broke the installer build.

; PawnIO driver installer - run once, sets up SMBus access for G.Skill RAM
Source: "..\portable\PawnIO_setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

; VC++ Runtime (if not using static linking)
Source: "..\redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; No Run-key entry here on purpose. OneClickRGB is manifested as
; requireAdministrator (the PawnIO driver for G.Skill RAM needs it), and
; Windows silently skips elevated programs registered under
; HKCU\...\Run or in the Startup folder. Autostart is a scheduled task with
; "run with highest privileges" instead - created in [Run] below.

[Run]
; Install VC++ Runtime if needed
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Visual C++ Runtime..."; Flags: waituntilterminated skipifdoesntexist

; PawnIO driver - required for G.Skill RAM (SMBus). Without it the RAM stays
; dark no matter how the application is started.
Filename: "{tmp}\PawnIO_setup.exe"; Parameters: "/S"; StatusMsg: "Installing PawnIO driver (G.Skill RAM support)..."; Flags: waituntilterminated skipifdoesntexist

; Autostart as an elevated scheduled task
Filename: "schtasks.exe"; Parameters: "/Create /F /TN ""OneClickRGB Autostart"" /TR ""\""{app}\{#MyAppExeName}\"" --minimized"" /SC ONLOGON /RL HIGHEST /DELAY 0000:10"; StatusMsg: "Setting up autostart..."; Flags: runhidden waituntilterminated; Tasks: autostart

; Clean up autostart entries left by versions before 3.6
Filename: "reg.exe"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v OneClickRGB /f"; Flags: runhidden waituntilterminated skipifdoesntexist

; Launch app after install
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Remove the autostart task, otherwise it survives the uninstall and fails at
; every logon with a missing executable.
Filename: "schtasks.exe"; Parameters: "/Delete /F /TN ""OneClickRGB Autostart"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAutostartTask"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  // Add any pre-installation checks here
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    // Post-installation tasks
  end;
end;
