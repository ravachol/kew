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
SetupIconFile=kew.ico

[Files]
; stage should contain:
;   kew.exe
;   *.dll
;   share\
;   docs\
;   kew.ico
Source: "stage\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Tasks]
Name: desktopicon; \
    Description: "Create a desktop shortcut"; \
    Flags: unchecked

Name: fileassoc; \
    Description: "Set kew as the default music player"; \
    Flags: unchecked

[Icons]
Name: "{group}\kew"; \
    Filename: "{app}\{#MyAppExeName}"; \
    IconFilename: "{app}\kew.ico"

Name: "{commondesktop}\kew"; \
    Filename: "{app}\{#MyAppExeName}"; \
    IconFilename: "{app}\kew.ico"; \
    Tasks: desktopicon


[Registry]
; ============================================================
; Application registration
;
; This tells Windows that kew is an application that can open
; files and provides the command used by Open With.
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\Applications\kew.exe"; \
    ValueType: string; \
    ValueName: "FriendlyAppName"; \
    ValueData: "kew"; \
    Flags: uninsdeletekey

Root: HKCU; \
    Subkey: "Software\Classes\Applications\kew.exe"; \
    ValueType: string; \
    ValueName: "SupportedTypes"; \
    ValueData: ".mp3,.flac,.wav,.m4a,.ogg,.opus,.webm"; \
    Flags: uninsdeletevalue

Root: HKCU; \
    Subkey: "Software\Classes\Applications\kew.exe\DefaultIcon"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "{app}\kew.exe,0"

Root: HKCU; \
    Subkey: "Software\Classes\Applications\kew.exe\shell\open\command"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: """{app}\kew.exe"" play ""%1"""

Root: HKCU; \
    Subkey: "Software\Classes\Applications\kew.exe\shell\open"; \
    ValueType: string; \
    ValueName: "MultiSelectModel"; \
    ValueData: "Player"


; ============================================================
; MP3
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.mp3\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.mp3"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.mp3"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.mp3"; \
    Tasks: fileassoc


; ============================================================
; FLAC
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.flac\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.flac"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.flac"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.flac"; \
    Tasks: fileassoc


; ============================================================
; WAV
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.wav\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.wav"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.wav"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.wav"; \
    Tasks: fileassoc


; ============================================================
; M4A
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.m4a\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.m4a"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.m4a"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.m4a"; \
    Tasks: fileassoc


; ============================================================
; OGG
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.ogg\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.ogg"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.ogg"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.ogg"; \
    Tasks: fileassoc


; ============================================================
; OPUS
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.opus\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.opus"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.opus"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.opus"; \
    Tasks: fileassoc


; ============================================================
; WEBM
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\.webm\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.webm"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\.webm"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "kew.webm"; \
    Tasks: fileassoc


; ============================================================
; DIRECTORY
;
; This makes kew available as an Open With application for
; folders without changing the normal default folder action.
; ============================================================

Root: HKCU; \
    Subkey: "Software\Classes\Directory\OpenWithProgids"; \
    ValueType: string; \
    ValueName: "kew.directory"; \
    ValueData: ""

Root: HKCU; \
    Subkey: "Software\Classes\kew.directory"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "Folder"

Root: HKCU; \
    Subkey: "Software\Classes\kew.directory\DefaultIcon"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: "{app}\kew.exe,0"

Root: HKCU; \
    Subkey: "Software\Classes\kew.directory\shell\open\command"; \
    ValueType: string; \
    ValueName: ""; \
    ValueData: """{app}\kew.exe"" play ""%1"""


[Run]
Filename: "{app}\{#MyAppExeName}"; \
    Description: "Launch kew"; \
    Flags: nowait postinstall skipifsilent
