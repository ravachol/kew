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
AppPublisherURL=https://kewplayer.com
AppSupportURL=https://codeberg.org/ravachol/kew/issues
AppUpdatesURL=https://kewplayer.com/download.html

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}

OutputDir=Output
OutputBaseFilename=kew-setup

Compression=lzma
SolidCompression=yes
WizardStyle=modern

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
; stage should contain:
;   kew.exe
;   *.dll
;   share\
;   docs\
;   kew.ico
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Tasks]
Name: desktopicon; Description: "Create desktop icon"; GroupDescription: "Additional icons:"

[Icons]
Name: "{group}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"
Name: "{commondesktop}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch kew"; Flags: nowait postinstall skipifsilent
