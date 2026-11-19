; Inno Setup 6 script. Build with ..\build-installer.ps1
#define AppName "Makiglow"
#define AppVersion "1.0.0"
#define RepoUrl "https://github.com/Moliiknis/Makiglow"
; Overridable for tests: ISCC /DPriv=lowest /DPanelRoot=C:\some\dir
#ifndef Priv
  #define Priv "admin"
#endif
#ifndef PanelRoot
  #define PanelRoot "{userappdata}\Adobe\CEP\extensions"
#endif

[Setup]
AppId={{D68F77AF-0749-442D-8444-BEB90AB3555D}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Makimchi
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
UninstallDisplayName={#AppName} for After Effects
LicenseFile=..\LICENSE.md
OutputDir=..\release
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired={#Priv}
DisableProgramGroupPage=yes
UsedUserAreasWarning=no
CloseApplications=yes
ShowLanguageDialog=auto
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DefaultDirName={code:DefaultPluginDir}
AppendDefaultDirName=no
DirExistsWarning=no
UninstallFilesDir={app}\Uninstall\{#AppName}

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Messages]
en.SelectDirLabel3=Choose the Plug-ins folder of your After Effects. Setup found the newest one automatically.
ru.SelectDirLabel3=Выберите папку Plug-ins вашего After Effects. Установщик сам нашёл самую новую версию.
en.FinishedLabelNoIcons=[name] is installed. Restart After Effects and open Effect > Makimchi > Makiglow.
ru.FinishedLabelNoIcons=[name] установлен. Перезапустите After Effects и откройте Эффект → Makimchi → Makiglow.

[Files]
Source: "..\build\Release\Makiglow.aex"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; Older builds of this plug-in placed in their own Plug-ins subfolder
Type: files; Name: "{app}\..\Makiglow\Makiglow.aex"
Type: files; Name: "{app}\..\BloomForge\BloomForge.aex"

[UninstallDelete]
Type: dirifempty; Name: "{app}\Uninstall"
Type: dirifempty; Name: "{app}"

[Code]
function IsJunction(const Path: String): Boolean;
var
  FR: TFindRec;
begin
  Result := False;
  if FindFirst(Path, FR) then
  begin
    Result := (FR.Attributes and $400) <> 0;  { FILE_ATTRIBUTE_REPARSE_POINT }
    FindClose(FR);
  end;
end;

{ A developer link (junction) in place of the panel folder is removed, never its target. }
procedure DropPanelLink(const Path: String);
begin
  if IsJunction(Path) then
    RemoveDir(Path);
end;

function DefaultPluginDir(Param: String): String;
var
  Y: Integer;
  D: String;
begin
  for Y := 2035 downto 2020 do
  begin
    D := ExpandConstant('{commonpf64}') + '\Adobe\Adobe After Effects ' + IntToStr(Y) + '\Support Files\Plug-ins';
    if DirExists(D) then
    begin
      Result := D + '\Makimchi';
      Exit;
    end;
  end;
  Result := ExpandConstant('{commonpf64}') + '\Adobe\Common\Plug-ins\7.0\MediaCore\Makimchi';
end;
