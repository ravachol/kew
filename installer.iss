#define MyAppName "kew"
#ifndef MyAppVersion
  #define MyAppVersion "dev"
#endif
#define MyAppExeName "kew.exe"

[Setup]
AppId={{A1B2C3D4-KEW-APP-ID-CHANGE-ME}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Ravachol
AppPublisherURL=https://codeberg.org/ravachol/kew
AppSupportURL=https://codeberg.org/ravachol/kew/issues
AppUpdatesURL=https://codeberg.org/ravachol/kew/releases

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}

OutputDir=Output
OutputBaseFilename=kew-setup

Compression=lzma
SolidCompression=yes
WizardStyle=modern
LZMANumBlockThreads=4

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
; Main binaries
Source: "stage\bin\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

; Shared resources (read-only app data)
Source: "stage\share\*"; DestDir: "{app}\share"; Flags: recursesubdirs createallsubdirs ignoreversion

; Cmd launcher
Source: "stage\bin\kew.cmd"; DestDir: "{app}"; Flags: ignoreversion

; Docs
Source: "stage\docs\*"; DestDir: "{app}\docs"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\kew"; Filename: "{app}\kew.cmd"; IconFilename: "{app}\kew.ico"
Name: "{commondesktop}\kew"; Filename: "{app}\kew.cmd"; IconFilename: "{app}\kew.ico"; Tasks: desktopicon

[Tasks]
Name: desktopicon; Description: "Create desktop icon"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\kew.cmd"; Description: "Launch kew"; Flags: nowait postinstall skipifsilent
