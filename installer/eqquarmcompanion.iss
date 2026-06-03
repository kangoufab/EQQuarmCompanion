; installer/eqquarmcompanion.iss
#define AppName "EqQuarmCompanion"
#define AppVersion "1.0.0"
#define BinDir "..\build\release\bin"

[Setup]
AppId={{F09E8AE6-A2A8-4434-973D-AA8FAF73A09F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=EqQuarmCompanion
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=output
OutputBaseFilename=EqQuarmCompanion-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
SetupIconFile=..\resources\app_icon.ico

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "{#BinDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"

[Run]
Filename: "{app}\EqQuarmCompanion.exe"; Description: "Lancer {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
var
  MySQLDetected: Boolean;
  CompPage: TInputOptionWizardPage;
  CredPage: TInputQueryWizardPage;
  { indices dans CompPage.CheckListBox — -1 si option absente }
  IdxImportDB: Integer;
  IdxInstallMariaDB: Integer;
  { résultats lus depuis les pages }
  WillImportDB: Boolean;
  WillInstallMariaDB: Boolean;

function DetectMySQL(): Boolean;
begin
  Result := False;

  { Vérifier les services Windows via le registre }
  if RegKeyExists(HKLM,
      'SYSTEM\CurrentControlSet\Services\MySQL') then begin
    Result := True; Exit;
  end;
  if RegKeyExists(HKLM,
      'SYSTEM\CurrentControlSet\Services\MariaDB') then begin
    Result := True; Exit;
  end;
  if RegKeyExists(HKLM,
      'SYSTEM\CurrentControlSet\Services\mariadb') then begin
    Result := True; Exit;
  end;

  { Vérifier les clés d'installation }
  if RegKeyExists(HKLM, 'SOFTWARE\MySQL AB') then begin
    Result := True; Exit;
  end;
  if RegKeyExists(HKLM, 'SOFTWARE\MariaDB') then begin
    Result := True; Exit;
  end;
  if RegKeyExists(HKLM,
      'SOFTWARE\WOW6432Node\MariaDB') then begin
    Result := True; Exit;
  end;
  if RegKeyExists(HKLM,
      'SOFTWARE\WOW6432Node\MySQL AB') then begin
    Result := True; Exit;
  end;

  { Vérifier la présence de mysql.exe dans les chemins courants }
  if FileExists(
      ExpandConstant('{pf}\MariaDB 10.6\bin\mysql.exe')) or
     FileExists(
      ExpandConstant('{pf}\MariaDB 10.11\bin\mysql.exe')) or
     FileExists(
      ExpandConstant('{pf}\MariaDB 11.4\bin\mysql.exe')) or
     FileExists(
      ExpandConstant('{pf}\MySQL\MySQL Server 8.0\bin\mysql.exe')) or
     FileExists(
      ExpandConstant('{pf}\MySQL\MySQL Server 5.7\bin\mysql.exe'))
  then begin
    Result := True; Exit;
  end;
end;

procedure InitializeWizard;
begin
  MySQLDetected := DetectMySQL();

  { --- Page Composants --- }
  CompPage := CreateInputOptionPage(
    wpSelectDir,
    'Composants',
    'Sélectionnez les composants à installer',
    '',
    False,   { checkboxes, pas boutons radio }
    False    { checklist style }
  );

  IdxImportDB := CompPage.Add('Importer la base de données Quarm');
  CompPage.Values[IdxImportDB] := True;

  if not MySQLDetected then begin
    IdxInstallMariaDB := CompPage.Add('Installer MariaDB (serveur de base de données)');
    CompPage.Values[IdxInstallMariaDB] := True;
  end else
    IdxInstallMariaDB := -1;

  { --- Page Credentials BDD --- }
  CredPage := CreateInputQueryPage(
    CompPage.ID,
    'Connexion à la base de données',
    'Paramètres utilisés pour créer et importer la base Quarm.',
    'Ces paramètres ne sont PAS écrits dans config.json — vous les saisirez dans l''application après installation.'
  );
  CredPage.Add('Host :', False);
  CredPage.Add('Port :', False);
  CredPage.Add('User :', False);
  CredPage.Add('Password :', True);   { True = masqué }
  CredPage.Values[0] := 'localhost';
  CredPage.Values[1] := '3306';
  CredPage.Values[2] := 'root';
  CredPage.Values[3] := '';
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  { Sauter Credentials si l'import DB n'est pas sélectionné }
  if (PageID = CredPage.ID) then begin
    if IdxImportDB >= 0 then
      Result := not CompPage.Values[IdxImportDB]
    else
      Result := True;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = CompPage.ID then begin
    WillImportDB := (IdxImportDB >= 0) and CompPage.Values[IdxImportDB];
    WillInstallMariaDB :=
      (IdxInstallMariaDB >= 0) and CompPage.Values[IdxInstallMariaDB];
  end;
end;
