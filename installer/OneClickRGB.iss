; OneClickRGB Installer Script for Inno Setup
; https://jrsoftware.org/isinfo.php

#define MyAppName "OneClickRGB"
#define MyAppVersion "1.0.0"
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

; Qt DLLs (adjust paths based on your Qt installation)
Source: "..\build\Release\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\Release\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\Release\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\Release\Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\Release\Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\Release\Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt platforms plugin
Source: "..\build\Release\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; Qt styles plugin
Source: "..\build\Release\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; HIDAPI
Source: "..\dependencies\hidapi\hidapi.dll"; DestDir: "{app}"; Flags: ignoreversion

; PawnIO (for SMBus/RAM control)
Source: "..\dependencies\PawnIO\PawnIOLib.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dependencies\PawnIO\modules\*.bin"; DestDir: "{app}\modules"; Flags: ignoreversion

; Configuration files
Source: "..\config\devices.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\controller_database.json"; DestDir: "{app}\config"; Flags: ignoreversion skipifsourcedoesntexist

; Default profiles
Source: "..\config\profiles\*"; DestDir: "{app}\config\profiles"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; VC++ Runtime (if not using static linking)
Source: "..\redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; Autostart entry
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "OneClickRGB"; ValueData: """{app}\{#MyAppExeName}"" --minimized"; Flags: uninsdeletevalue; Tasks: autostart

[Run]
; Install VC++ Runtime if needed
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Visual C++ Runtime..."; Flags: waituntilterminated skipifdoesntexist

; Launch app after install
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

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
