; Inno Setup 6 — standalone Trimmi installer
; Packages dist\payload (staged by stage_payload.ps1): app + Qt + CRT + FFmpeg
; Target PCs do not need Qt, FFmpeg, vcpkg, or a separate MSVC redistributable.

#define MyAppName "Trimmi"
#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "Trimmi"
#define MyAppExeName "Trimmi.exe"
#define PayloadDir "..\dist\payload"

[Setup]
AppId={{A7C3E9F2-4B1D-4E8A-9C2F-1D6B8A0E5F33}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/
AppSupportURL=https://github.com/
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=TrimmiSetup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
; Per-user install by default (no admin). App + private DLLs live under {app}.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; Close running Trimmi before overwrite
CloseApplications=yes
RestartApplications=no
AllowNoIcons=yes
InfoBeforeFile=
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Entire self-contained payload (exe, Qt plugins, CRT, FFmpeg libs + tools)
Source: "{#PayloadDir}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; \
    Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; \
    Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup(): Boolean;
var
  PayloadExe: String;
begin
  Result := True;
  { src = folder containing this .iss file }
  PayloadExe := ExpandConstant('{src}\..\dist\payload\{#MyAppExeName}');
  if not FileExists(PayloadExe) then
  begin
    MsgBox('Standalone payload not found:'#13#10 + PayloadExe + #13#10#13#10 +
           'Run installer\stage_payload.ps1 (or build_installer.ps1) before compiling.',
           mbError, MB_OK);
    Result := False;
  end;
end;
