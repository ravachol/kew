#define MyAppName "kew"

#ifndef MyAppVersion
  #define MyAppVersion "dev"
#endif

#define MyAppExeName "kew.exe"

[Setup]
AppId={{9e49d611-0617-4f1a-b8e2-4337e675c875}}

AppName={#MyAppName}
AppVersion={#MyAppVersion}

VersionInfoProductVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany=Ravachol
VersionInfoDescription=kew player
VersionInfoCopyright=Copyright (C) Ravachol

AppPublisher=Ravachol
AppPublisherURL=https://kewplayer.com
AppContact=https://kewplayer.com
AppSupportURL=https://codeberg.org/ravachol/kew/issues
AppUpdatesURL=https://kewplayer.com/download.html

AppMutex=kew
CloseApplications=yes
RestartApplications=no

DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}

OutputDir=Output
OutputBaseFilename=kew-{#MyAppVersion}-windows-x64

Compression=lzma2
SolidCompression=yes
WizardStyle=modern

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName=kew
Uninstallable=yes
LicenseFile=LICENSE
SetupIconFile=stage\kew.ico

[Files]
; stage should contain:
;   kew.exe
;   *.dll
;   share\
;   docs\
;   kew.ico
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Icons]
Name: "{group}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"
Name: "{commondesktop}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch kew"; Flags: nowait postinstall skipifsilent
