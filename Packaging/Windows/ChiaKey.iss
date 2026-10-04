; The Windows installer. Built by .github/workflows/release-windows.yml; locally:
;   ISCC /DVersion=0.1.0 /DX64Dir=<x64 Release> /DX86Dir=<x86 Release> /DIconFile=<app.ico>
;        /DOutputDir=<dir> Packaging\Windows\ChiaKey.iss

#ifndef Version
  #error Version is required, e.g. /DVersion=0.1.0
#endif
#ifndef NumericVersion
  #define NumericVersion Version
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif
#define LexiconSize FileSize(AddBackslash(X64Dir) + "ChiaKeySource.db")

[Setup]
; never change: it is how an upgrade finds the installed copy
AppId={{AF41550D-9D4A-47FF-9D69-C9248A374258}
AppName=千秋輸入法
AppVersion={#Version}
AppVerName=千秋輸入法 {#Version}
AppPublisher=Chiaki.C
AppPublisherURL=https://chiaki.ch/works/chiakey
AppSupportURL=https://github.com/chiakich/ChiaKey/issues
AppUpdatesURL=https://github.com/chiakich/ChiaKey/releases
VersionInfoVersion={#NumericVersion}
VersionInfoTextVersion={#Version}
VersionInfoProductName=ChiaKey
VersionInfoProductVersion={#NumericVersion}
VersionInfoProductTextVersion={#Version}
VersionInfoCompany=ChiaKey
VersionInfoDescription=ChiaKey installer
DefaultDirName={autopf}\ChiaKey
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
; 64-bit Windows gets both DLLs, so 32-bit apps can type too; ARM64 is not built yet
ArchitecturesAllowed=x64os x86os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0
; every app that types has the DLL loaded; a new version goes into its own folder instead
CloseApplications=no
RestartApplications=no
LicenseFile=..\..\LICENSE
SetupIconFile={#IconFile}
UninstallDisplayIcon={app}\{#Version}\ChiaKeySettings.exe
UninstallDisplayName=千秋輸入法 {#Version}
WizardStyle=modern
OutputDir={#OutputDir}
OutputBaseFilename=ChiaKey-Windows-{#Version}-Setup
Compression=lzma2/max
SolidCompression=yes

[Languages]
Name: "zh_TW"; MessagesFile: "ChineseTraditional.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
zh_TW.FinishedLabel=千秋輸入法已經安裝完成。%n%n已經開著的程式要重新開啟，才會載入新版。如果輸入法清單裡沒有千秋輸入法，請到 Windows 設定的「時間與語言 > 語言與地區」加入「中文 (繁體，台灣)」。
en.FinishedLabel=ChiaKey has been installed.%n%nApps that are already open need to be restarted to load it. If ChiaKey is not in the input method list, add "Chinese (Traditional, Taiwan)" under Settings > Time & language > Language & region.

[Files]
; the 32-bit DLL registers first, so the profile's icon ends up pointing at the 64-bit one
Source: "{#X86Dir}\ChiaKeyTsf.dll"; DestDir: "{app}\{#Version}\x86"; Flags: ignoreversion regserver 32bit uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X64Dir}\ChiaKeyTsf.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion regserver 64bit uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X86Dir}\ChiaKeyTsf.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion regserver 32bit uninsrestartdelete; Check: not Is64BitInstallMode
Source: "{#X64Dir}\ChiaKeySource.db"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: LexiconNeeded
Source: "{#X64Dir}\ChiaKeySettings.exe"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X86Dir}\ChiaKeySettings.exe"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: not Is64BitInstallMode
Source: "{#X64Dir}\Microsoft.Web.WebView2.Core.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X64Dir}\Microsoft.Web.WebView2.WinForms.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X64Dir}\WebView2Loader.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: Is64BitInstallMode
Source: "{#X86Dir}\Microsoft.Web.WebView2.Core.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: not Is64BitInstallMode
Source: "{#X86Dir}\Microsoft.Web.WebView2.WinForms.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: not Is64BitInstallMode
Source: "{#X86Dir}\WebView2Loader.dll"; DestDir: "{app}\{#Version}"; Flags: ignoreversion uninsrestartdelete; Check: not Is64BitInstallMode
Source: "{#X64Dir}\WebView2-LICENSE.txt"; DestDir: "{app}\{#Version}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}\{#Version}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\千秋輸入法\千秋輸入法設定"; Filename: "{app}\{#Version}\ChiaKeySettings.exe"
Name: "{autoprograms}\千秋輸入法\千秋輸入法詞彙編輯器"; Filename: "{app}\{#Version}\ChiaKeySettings.exe"; Parameters: "/phrases"

Name: "{autoprograms}\千秋輸入法\千秋輸入法字典"; Filename: "{app}\{#Version}\ChiaKeySettings.exe"; Parameters: "/dictionary"

[Run]
; Preserve update preferences and refresh the per-user startup path after an upgrade. The helper
; must run as the original desktop user, never with the installer's elevated token.
Filename: "{app}\{#Version}\ChiaKeySettings.exe"; Parameters: "/update-register"; Flags: runasoriginaluser nowait runhidden

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "ChiaKeyUpdates"; Flags: uninsdeletevalue

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
// A folder an app still has a DLL loaded from refuses to go; the next upgrade
// or a restart after uninstalling takes care of it.
procedure RemoveOlderVersions();
var
  Found: TFindRec;
  Root: String;
begin
  Root := ExpandConstant('{app}');
  if FindFirst(Root + '\*', Found) then
  try
    repeat
      // only version folders: anything else in there is not ours to remove
      if ((Found.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and
         (Length(Found.Name) > 0) and (Found.Name[1] >= '0') and (Found.Name[1] <= '9') and
         (Found.Name <> '{#Version}') then
        DelTree(Root + '\' + Found.Name, True, True, True);
    until not FindNext(Found);
  finally
    FindClose(Found);
  end;
end;

// Reinstalling a version: apps that type have its DLL loaded, which cannot be
// overwritten but can be renamed out of the way and deleted at the next restart.
procedure MoveAsideLoaded(const Path: String);
var
  Aside: String;
begin
  if not FileExists(Path) or DeleteFile(Path) then
    Exit;
  Aside := Path + '.' + GetDateTimeString('yyyymmddhhnnss', #0, #0) + '.old';
  if RenameFile(Path, Aside) then
    RestartReplace(Aside, '');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Folder: String;
begin
  Folder := ExpandConstant('{app}\{#Version}');
  MoveAsideLoaded(Folder + '\ChiaKeyTsf.dll');
  MoveAsideLoaded(Folder + '\x86\ChiaKeyTsf.dll');
  MoveAsideLoaded(Folder + '\ChiaKeySettings.exe');
  Result := '';
end;

// SQLite keeps the lexicon open without delete sharing, so it cannot be moved
// aside; a version's lexicon is the same file every time anyway.
function LexiconNeeded(): Boolean;
var
  Size: Integer;
begin
  Result := not FileSize(ExpandConstant('{app}\{#Version}\ChiaKeySource.db'), Size) or
            (Size <> {#LexiconSize});
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    RemoveOlderVersions();
end;
