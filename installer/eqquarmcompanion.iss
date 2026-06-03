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
  MariaDBDownloadUrl: String;
  DumpDownloadUrl: String;
  DownloadPage: TDownloadWizardPage;

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

// Appelle l'API MariaDB pour obtenir l'URL du MSI Windows x64 le plus recent.
// Retourne '' en cas d'erreur.
// Structure API reelle :
//   GET /rest-api/mariadb/         -> major_releases[]
//   GET /rest-api/mariadb/VER/     -> releases.VER.files[]
// Chaque file a : file_name, file_download_url
function GetMariaDBUrl(): String;
var
  PSFile, UrlFile: String;
  Script: String;
  ResultCode: Integer;
  RawContent: AnsiString;
begin
  Result := '';
  PSFile  := ExpandConstant('{tmp}\get_mariadb_url.ps1');
  UrlFile := ExpandConstant('{tmp}\mariadb_url.txt');

  Script :=
    '$ErrorActionPreference = "Stop"' + #13#10 +
    'try {' + #13#10 +
    '  $r = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/"' + #13#10 +
    '  $stable = ($r.major_releases | Where-Object { $_.release_status -eq "Stable" } | Sort-Object release_id -Descending)[0]' + #13#10 +
    '  $resp = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/$($stable.release_id)/"' + #13#10 +
    '  $latestKey = ($resp.releases.PSObject.Properties.Name | Sort-Object { [Version]($_ -replace ''-.*'','''''') } -Descending)[0]' + #13#10 +
    '  $files = $resp.releases.$latestKey.files' + #13#10 +
    '  $msi = $files | Where-Object { $_.file_name -match "winx64\.msi$" } | Select-Object -First 1' + #13#10 +
    '  $msi.file_download_url | Out-File "' + UrlFile + '" -Encoding utf8 -NoNewline' + #13#10 +
    '} catch { exit 1 }';

  SaveStringToFile(PSFile, Script, False);
  try
    if Exec('powershell.exe',
        '-NoProfile -ExecutionPolicy Bypass -File "' + PSFile + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      if (ResultCode = 0) and FileExists(UrlFile) then begin
        LoadStringFromFile(UrlFile, RawContent);
        Result := Trim(String(RawContent));
      end;
    end;
  finally
    DeleteFile(PSFile);
    DeleteFile(UrlFile);
  end;
end;

{ Appelle l'API GitHub pour obtenir l'URL du dernier fichier dans
  utils/sql/database_full. Retourne '' en cas d'erreur. }
function GetLatestDumpUrl(): String;
var
  PSFile, UrlFile: String;
  Script: String;
  ResultCode: Integer;
  RawContent: AnsiString;
begin
  Result := '';
  PSFile  := ExpandConstant('{tmp}\get_dump_url.ps1');
  UrlFile := ExpandConstant('{tmp}\dump_url.txt');

  Script :=
    '$ErrorActionPreference = "Stop"' + #13#10 +
    'try {' + #13#10 +
    '  $h = @{ "User-Agent" = "EqQuarmCompanion-Installer" }' + #13#10 +
    '  $r = Invoke-RestMethod "https://api.github.com/repos/SecretsOTheP/EQMacEmu/contents/utils/sql/database_full" -Headers $h' + #13#10 +
    '  $f = $r | Where-Object { $_.type -eq "file" } | Sort-Object name -Descending | Select-Object -First 1' + #13#10 +
    '  $f.download_url | Out-File "' + UrlFile + '" -Encoding utf8 -NoNewline' + #13#10 +
    '} catch { exit 1 }';

  SaveStringToFile(PSFile, Script, False);
  try
    if Exec('powershell.exe',
        '-NoProfile -ExecutionPolicy Bypass -File "' + PSFile + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      if (ResultCode = 0) and FileExists(UrlFile) then begin
        LoadStringFromFile(UrlFile, RawContent);
        Result := Trim(String(RawContent));
      end;
    end;
  finally
    DeleteFile(PSFile);
    DeleteFile(UrlFile);
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

  DownloadPage := CreateDownloadPage(
    'Téléchargement',
    'Téléchargement des composants sélectionnés...',
    nil
  );
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
var
  Msg: String;
  DownloadCount: Integer;
begin
  Result := True;

  { Lire les choix de la page Composants }
  if CurPageID = CompPage.ID then begin
    WillImportDB := (IdxImportDB >= 0) and CompPage.Values[IdxImportDB];
    WillInstallMariaDB :=
      (IdxInstallMariaDB >= 0) and CompPage.Values[IdxInstallMariaDB];
  end;

  { Déclencher les téléchargements depuis la page Récapitulatif }
  if CurPageID = wpReady then begin
    DownloadPage.Clear;
    DownloadCount := 0;

    if WillInstallMariaDB then begin
      Msg := 'Résolution de l''URL MariaDB...';
      WizardForm.StatusLabel.Caption := Msg;
      MariaDBDownloadUrl := GetMariaDBUrl();
      if MariaDBDownloadUrl = '' then begin
        MsgBox(
          'Impossible de récupérer l''URL de MariaDB.' + #13#10 +
          'Vérifiez votre connexion internet.',
          mbError, MB_OK);
        Result := False;
        Exit;
      end;
      DownloadPage.Add(MariaDBDownloadUrl, 'mariadb-setup.msi', '');
      DownloadCount := DownloadCount + 1;
    end;

    if WillImportDB then begin
      WizardForm.StatusLabel.Caption :=
        'Résolution de l''URL du dump Quarm...';
      DumpDownloadUrl := GetLatestDumpUrl();
      if DumpDownloadUrl = '' then begin
        MsgBox(
          'Impossible de récupérer l''URL du dump Quarm sur GitHub.' + #13#10 +
          'Vérifiez votre connexion internet.',
          mbError, MB_OK);
        Result := False;
        Exit;
      end;
      DownloadPage.Add(DumpDownloadUrl, 'quarm_dump.tar.gz', '');
      DownloadCount := DownloadCount + 1;
    end;

    if DownloadCount > 0 then begin
      DownloadPage.Show;
      try
        try
          DownloadPage.Download;
        except
          MsgBox(
            'Erreur lors du téléchargement :' + #13#10 +
            GetExceptionMessage,
            mbError, MB_OK);
          Result := False;
        end;
      finally
        DownloadPage.Hide;
      end;
    end;
  end;
end;
