; SPDX-License-Identifier: GPL-3.0-only
#ifndef Payload
  #error Supply an audited runtime package through scripts/Build-Installer.ps1
#endif
[Setup]
AppId=AudioHole
AppName=AudioHole
AppVersion={#AppVersion}
AppPublisher=RLCypher
AppPublisherURL=https://github.com/RLCypher/audiohole
AppSupportURL=https://github.com/RLCypher/audiohole/issues
DefaultDirName={autopf}\AudioHole
DefaultGroupName=AudioHole
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
OutputDir={#Output}
OutputBaseFilename={#OutputName}
SetupArchitecture=x64
MinVersion=10.0
SetupIconFile=..\assets\branding\audiohole.ico
UninstallDisplayIcon={app}\AudioHole.exe
LicenseFile=..\LICENSE
InfoBeforeFile=ReleaseNotes.txt
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
Uninstallable=yes

[Tasks]
Name: desktopicon; Description: "Create a Desktop shortcut"; Flags: unchecked
Name: startmenuicon; Description: "Create a Start menu shortcut"

[Files]
Source: "{#Payload}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\AudioHole"; Filename: "{app}\AudioHole.exe"; Tasks: desktopicon
Name: "{autoprograms}\AudioHole\AudioHole"; Filename: "{app}\AudioHole.exe"; Tasks: startmenuicon
Name: "{autoprograms}\AudioHole\Uninstall AudioHole"; Filename: "{uninstallexe}"; Tasks: startmenuicon

[Run]
Filename: "{app}\AudioHole.exe"; Description: "Launch AudioHole"; Flags: nowait postinstall skipifsilent runasoriginaluser

[Messages]
ButtonFinish=Exit

[Code]
#include "BuildInfo.iss"
var
  ExistingPage: TInputOptionWizardPage;
  OldFiles: TStringList;
  OldVersion, ExistingDirectory: String;
  Existing, KeepData, UninstallSelected: Boolean;

function GetFileAttributesW(Name: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function NotRedirected(Path: String): Boolean;
var Attributes: LongWord;
begin
  Result := True;
  while Length(Path) > 3 do begin
    Attributes := GetFileAttributesW(Path);
    if (Attributes <> $FFFFFFFF) and ((Attributes and $400) <> 0) then begin
      Result := False; exit;
    end;
    Path := ExtractFileDir(Path);
  end;
end;

function ApplicationRunning: Boolean;
var Locator, Services, Processes: Variant;
begin
  // The image can keep routing while hidden in the tray. Detect it by path,
  // rather than treating an absent visible window as permission to replace it.
  Result := True;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Services := Locator.ConnectServer('', 'root\CIMV2');
    Processes := Services.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name="AudioHole.exe"');
    Result := Processes.Count > 0;
  except
    Log('Cannot verify that AudioHole is closed.');
  end;
end;

function RegisteredUninstaller(Directory: String): String;
var Location, Command, Name: String; ClosingQuote, I: Integer;
begin
  Result := '';
  // Inno can allocate unins001.exe (or a later number) while an earlier
  // uninstaller is finishing its self-cleanup. The registration is authoritative.
  if not RegQueryStringValue(HKA, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioHole_is1',
    'InstallLocation', Location) then exit;
  if CompareText(AddBackslash(ExpandFileName(Location)),
    AddBackslash(ExpandFileName(Directory))) <> 0 then exit;
  if not RegQueryStringValue(HKA, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioHole_is1',
    'UninstallString', Command) then exit;
  Command := Trim(Command);
  if Command = '' then exit;
  if Command[1] = '"' then begin
    Delete(Command, 1, 1);
    ClosingQuote := Pos('"', Command);
    if ClosingQuote = 0 then exit;
    Command := Copy(Command, 1, ClosingQuote - 1);
  end;
  Name := Lowercase(ExtractFileName(Command));
  if (Length(Name) <> 12) or (Copy(Name, 1, 5) <> 'unins') or
    (Copy(Name, 9, 4) <> '.exe') then exit;
  for I := 6 to 8 do
    if (Name[I] < '0') or (Name[I] > '9') then exit;
  if CompareText(AddBackslash(ExpandFileName(ExtractFileDir(Command))),
    AddBackslash(ExpandFileName(Directory))) <> 0 then exit;
  if FileExists(Command) and NotRedirected(Command) then Result := Command;
end;

procedure ReadExisting;
var Valid: Boolean; Action: String;
begin
  ExistingDirectory := WizardDirValue;
  Existing := FileExists(AddBackslash(ExistingDirectory) + 'AudioHole.exe');
  OldFiles.Clear; OldVersion := '';
  Valid := ReadBuildInfo(AddBackslash(ExistingDirectory) + 'docs\build-info.json', OldVersion, OldFiles);
  Action := 'Reinstall';
  if Valid and NewerVersion('{#AppVersion}', OldVersion) then Action := 'Update';
  ExistingPage.CheckListBox.ItemCaption[0] := Action + ' AudioHole';
  ExistingPage.SubCaptionLabel.Caption := 'Installed version: ' + OldVersion +
    '. Installer version: {#AppVersion}. Choose an action.';
  if not Valid then ExistingPage.SubCaptionLabel.Caption :=
    'Installed version is unavailable. Reinstall will replace application files and preserve your data.';
  Log('AudioHole installed version: ' + OldVersion + '; action: ' + Action);
  Log('AudioHole registered uninstaller: ' + RegisteredUninstaller(ExistingDirectory));
end;

procedure InitializeWizard;
var Location: String;
begin
  OldFiles := TStringList.Create;
  if RegQueryStringValue(HKA, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\AudioHole_is1',
    'InstallLocation', Location) and DirExists(Location) then WizardForm.DirEdit.Text := Location;
  ExistingPage := CreateInputOptionPage(wpSelectDir, 'AudioHole is already installed',
    'Choose what to do', 'Your saved data is preserved during Update or Reinstall.', True, False);
  ExistingPage.Add('Reinstall AudioHole');
  ExistingPage.Add('Uninstall AudioHole');
  ExistingPage.Add('Cancel');
  ExistingPage.SelectedValueIndex := 0;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if PageID = ExistingPage.ID then begin
    ReadExisting;
    Result := not Existing;
  end;
end;

procedure PurgeUserData;
var DataRoot: String;
begin
  DataRoot := ExpandConstant('{localappdata}\AudioHole');
  if not NotRedirected(DataRoot) then
    MsgBox('AudioHole data was retained because its path is redirected: ' + DataRoot, mbError, MB_OK)
  else if not DelTree(DataRoot, True, True, True) then
    MsgBox('Some AudioHole data could not be removed: ' + DataRoot, mbError, MB_OK);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var Code, I: Integer; Uninstaller, Path: String;
begin
  Result := True;
  if CurPageID <> ExistingPage.ID then exit;
  if ExistingPage.SelectedValueIndex = 2 then begin
    UninstallSelected := True;
    WizardForm.Close; Result := False; exit;
  end;
  if ExistingPage.SelectedValueIndex <> 1 then exit;
  Result := False;
  if ApplicationRunning then begin
    MsgBox('Exit AudioHole, including its tray icon, before uninstalling.', mbInformation, MB_OK); exit;
  end;
  Uninstaller := RegisteredUninstaller(ExistingDirectory);
  if Uninstaller <> '' then begin
    if not Exec(Uninstaller, '', ExistingDirectory, SW_SHOW, ewWaitUntilTerminated, Code) then begin
      MsgBox('The uninstaller could not be started.', mbError, MB_OK); exit;
    end;
    if Code <> 0 then exit;
  end else begin
    // A portable copy has no Inno installation log. Its generated manifest is
    // the only deletion authority; unknown files and user-added content stay.
    if OldFiles.Count = 0 then begin
      MsgBox('This older portable copy has no file manifest. Remove its application files manually; your saved data is separate.', mbInformation, MB_OK); exit;
    end;
    if MsgBox('Remove the AudioHole application files from this portable copy?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) <> IDYES then exit;
    KeepData := MsgBox('Keep profiles, configurations, and settings?', mbConfirmation,
      MB_YESNO) = IDYES;
    for I := 0 to OldFiles.Count - 1 do begin
      Path := AddBackslash(ExistingDirectory) + OldFiles[I];
      if SafeRuntimePath(OldFiles[I]) and NotRedirected(Path) then DeleteFile(Path);
    end;
    if not KeepData then PurgeUserData;
  end;
  UninstallSelected := True;
  WizardForm.Close;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if UninstallSelected then Confirm := False;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if ApplicationRunning then begin
    Result := 'Exit AudioHole, including its tray icon, then retry. Routing must stop before application files are replaced.';
    exit;
  end;
  ReadExisting;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var I: Integer; Path, Startup, Version: String; NewFiles: TStringList;
begin
  if CurStep = ssPostInstall then begin
    // Retire only previously owned files absent from the new payload. Keep
    // user-added files and every data-root file, and never traverse a junction.
    NewFiles := TStringList.Create;
    if ReadBuildInfo(ExpandConstant('{app}\docs\build-info.json'), Version, NewFiles) then
      for I := 0 to OldFiles.Count - 1 do begin
        Path := AddBackslash(ExistingDirectory) + OldFiles[I];
        if (NewFiles.IndexOf(OldFiles[I]) < 0) and SafeRuntimePath(OldFiles[I]) and
          NotRedirected(Path) and not DeleteFile(Path) then
          Log('Could not remove retired application file: ' + Path);
      end;
    NewFiles.Free;
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'AudioHole', Startup) and
      (CompareText(Startup, '"' + AddBackslash(ExistingDirectory) + 'AudioHole.exe"') = 0) then
      RegWriteStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'AudioHole',
        '"' + ExpandConstant('{app}\AudioHole.exe') + '"');
  end;
end;

function InitializeUninstall: Boolean;
begin
  KeepData := True;
  Result := not ApplicationRunning;
  if not Result then begin
    if not UninstallSilent then MsgBox('Exit AudioHole, including its tray icon, before uninstalling.', mbInformation, MB_OK);
    exit;
  end;
  if UninstallSilent then KeepData := ExpandConstant('{param:PURGEDATA|0}') <> '1'
  else KeepData := MsgBox('Keep profiles, configurations, and settings?', mbConfirmation,
    MB_YESNO) = IDYES;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var Startup: String;
begin
  if CurUninstallStep = usPostUninstall then begin
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'AudioHole', Startup) and
      (CompareText(Startup, '"' + ExpandConstant('{app}\AudioHole.exe') + '"') = 0) then
      RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'AudioHole');
    if not KeepData then PurgeUserData;
  end;
end;
