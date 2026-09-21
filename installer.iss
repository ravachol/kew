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
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked
Name: contextmenu; Description: "Add ""Play with kew"" to the File Explorer context menu"; Flags: checked

[Icons]
Name: "{group}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"
Name: "{commondesktop}\kew"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\kew.ico"; Tasks: desktopicon

[Registry]
; ============================================================
; MP3
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.mp3\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; FLAC
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.flac\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; WAV
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.wav\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; M4A
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.m4a\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; OGG
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.ogg\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; OPUS
; ============================================================

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.opus\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


; ============================================================
; DIRECTORIES
; ============================================================

Root: HKCU; Subkey: "Software\Classes\Directory\shell\PlayWithKew"; \
    ValueType: string; ValueName: ""; ValueData: "Play with kew"; \
    Tasks: contextmenu; Flags: uninsdeletekey

Root: HKCU; Subkey: "Software\Classes\Directory\shell\PlayWithKew"; \
    ValueType: string; ValueName: "Icon"; ValueData: "{app}\kew.exe"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\Directory\shell\PlayWithKew"; \
    ValueType: string; ValueName: "MultiSelectModel"; ValueData: "Player"; \
    Tasks: contextmenu

Root: HKCU; Subkey: "Software\Classes\Directory\shell\PlayWithKew\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\kew.exe"" play ""%1"""; \
    Tasks: contextmenu


[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch kew"; Flags: nowait postinstall skipifsilent
