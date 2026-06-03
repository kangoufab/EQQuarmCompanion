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
WizardSmallImageFile=..\resources\app_icon.ico

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

{ Lance l'installation silencieuse du MSI MariaDB téléchargé.
  Le mot de passe root est celui saisi dans CredPage (peut être vide). }
function DoInstallMariaDB(): Boolean;
var
  MsiPath: String;
  Password: String;
  Params: String;
  ResultCode: Integer;
begin
  Result := False;
  MsiPath  := ExpandConstant('{tmp}\mariadb-setup.msi');
  Password := CredPage.Values[3];

  Params :=
    '/i "' + MsiPath + '"' +
    ' /quiet' +
    ' SERVICENAME=MariaDB' +
    ' ROOTPASSWORD="' + Password + '"' +
    ' ADDLOCAL=ALL';

  if not Exec(
      ExpandConstant('{sys}\msiexec.exe'),
      Params,
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
    MsgBox(
      'Impossible de lancer msiexec.' + #13#10 +
      SysErrorMessage(ResultCode),
      mbError, MB_OK);
    Exit;
  end;

  if ResultCode <> 0 then begin
    MsgBox(
      'L''installation de MariaDB a échoué (code ' +
      IntToStr(ResultCode) + ').' + #13#10 +
      'Consultez le journal %TEMP%\MariaDB_install.log.',
      mbError, MB_OK);
    Exit;
  end;

  Result := True;
end;

{ Localise mysql.exe sur le système.
  Cherche dans les répertoires courants de MariaDB et MySQL. }
function FindMysqlExe(): String;
var
  Candidates: TArrayOfString;
  I: Integer;
begin
  Result := '';
  SetArrayLength(Candidates, 8);
  Candidates[0] := ExpandConstant('{pf}\MariaDB 11.4\bin\mysql.exe');
  Candidates[1] := ExpandConstant('{pf}\MariaDB 10.11\bin\mysql.exe');
  Candidates[2] := ExpandConstant('{pf}\MariaDB 10.6\bin\mysql.exe');
  Candidates[3] := ExpandConstant('{pf}\MariaDB\bin\mysql.exe');
  Candidates[4] := ExpandConstant('{pf}\MySQL\MySQL Server 8.0\bin\mysql.exe');
  Candidates[5] := ExpandConstant('{pf}\MySQL\MySQL Server 5.7\bin\mysql.exe');
  Candidates[6] := 'C:\mariadb\bin\mysql.exe';
  Candidates[7] := 'C:\mysql\bin\mysql.exe';

  for I := 0 to GetArrayLength(Candidates) - 1 do begin
    if FileExists(Candidates[I]) then begin
      Result := Candidates[I];
      Exit;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  MysqlExe: String;
  Host, Port, User, Pass, PassPart: String;
  DumpArchive, DumpDir, DumpSql, BatPath: String;
  Params: String;
  ResultCode: Integer;
  PSFile: String;
  PSScript: String;
  RawDumpSql: AnsiString;
begin
  if CurStep = ssInstall then begin

    { --- Installation MariaDB --- }
    if WillInstallMariaDB then begin
      WizardForm.StatusLabel.Caption := 'Installation de MariaDB...';
      if not DoInstallMariaDB() then
        Exit;
    end;

    { --- Import de la BDD Quarm --- }
    if WillImportDB then begin
      MysqlExe := FindMysqlExe();
      if MysqlExe = '' then begin
        MsgBox(
          'mysql.exe introuvable.' + #13#10 +
          'Ajoutez le répertoire bin de MariaDB/MySQL au PATH' + #13#10 +
          'et relancez l''import manuellement.',
          mbError, MB_OK);
        Exit;
      end;

      Host        := CredPage.Values[0];
      Port        := CredPage.Values[1];
      User        := CredPage.Values[2];
      Pass        := CredPage.Values[3];
      DumpArchive := ExpandConstant('{tmp}\quarm_dump.tar.gz');
      DumpDir     := ExpandConstant('{tmp}\quarm_dump_extract');

      if Pass <> '' then PassPart := ' -p' + Pass
      else PassPart := '';

      { Extraire l'archive .tar.gz via PowerShell (tar natif Windows 10/11) }
      WizardForm.StatusLabel.Caption := 'Extraction du dump Quarm...';
      PSFile   := ExpandConstant('{tmp}\extract_dump.ps1');
      PSScript :=
        '$archive = "' + DumpArchive + '"' + #13#10 +
        '$dest = "' + DumpDir + '"' + #13#10 +
        'if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }' + #13#10 +
        'New-Item -ItemType Directory -Path $dest -Force | Out-Null' + #13#10 +
        'tar -xzf $archive -C $dest' + #13#10 +
        '# Trouver le premier fichier .sql dans l''arborescence extraite' + #13#10 +
        '$sql = Get-ChildItem $dest -Recurse -Filter "*.sql" | Select-Object -First 1' + #13#10 +
        'if ($sql) { $sql.FullName | Out-File "' + DumpDir + '\sql_path.txt" -Encoding utf8 -NoNewline }' + #13#10 +
        'else { exit 1 }';
      SaveStringToFile(PSFile, PSScript, False);
      Exec('powershell.exe',
        '-NoProfile -ExecutionPolicy Bypass -File "' + PSFile + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      DeleteFile(PSFile);

      if ResultCode <> 0 then begin
        MsgBox(
          'Échec de l''extraction du dump Quarm.' + #13#10 +
          'Vérifiez que l''archive a été correctement téléchargée.',
          mbError, MB_OK);
        Exit;
      end;

      { Lire le chemin du fichier SQL extrait }
      if not LoadStringFromFile(DumpDir + '\sql_path.txt', RawDumpSql) then begin
        MsgBox(
          'Impossible de localiser le fichier .sql dans l''archive.',
          mbError, MB_OK);
        Exit;
      end;
      DumpSql := Trim(String(RawDumpSql));

      { Créer la base }
      WizardForm.StatusLabel.Caption := 'Création de la base quarm...';
      Params :=
        '-h ' + Host + ' -P ' + Port + ' -u ' + User + PassPart +
        ' -e "CREATE DATABASE IF NOT EXISTS quarm CHARACTER SET utf8mb4"';
      if not Exec(MysqlExe, Params, '', SW_HIDE,
          ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then begin
        MsgBox(
          'Échec de la création de la base quarm (code ' +
          IntToStr(ResultCode) + ').' + #13#10 +
          'Vérifiez les credentials et l''état du serveur.',
          mbError, MB_OK);
        Exit;
      end;

      { Import dump via .bat temporaire (nécessaire pour la redirection <) }
      WizardForm.StatusLabel.Caption :=
        'Import du dump Quarm (peut prendre plusieurs minutes)...';
      BatPath := ExpandConstant('{tmp}\import_quarm.bat');
      SaveStringToFile(BatPath,
        '@echo off' + #13#10 +
        '"' + MysqlExe + '"' +
        ' -h ' + Host + ' -P ' + Port +
        ' -u ' + User + PassPart +
        ' quarm < "' + DumpSql + '"',
        False);
      Exec(ExpandConstant('{sys}\cmd.exe'),
        '/c "' + BatPath + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      if ResultCode <> 0 then
        MsgBox(
          'Import échoué (code ' + IntToStr(ResultCode) + ').' + #13#10 +
          'La base quarm a été créée mais est vide.' + #13#10 +
          'Réessayez manuellement : mysql -u root quarm < fichier.sql',
          mbError, MB_OK);
      DeleteFile(BatPath);

      { Nettoyage du répertoire d'extraction }
      if DirExists(DumpDir) then
        DelTree(DumpDir, True, True, True);
    end;

  end;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then begin
    if WillImportDB then
      WizardForm.FinishedLabel.Caption :=
        'EqQuarmCompanion a été installé.' + #13#10 + #13#10 +
        'La base de données quarm a été importée.' + #13#10 +
        'Configurez la connexion (host/port/user/password)' + #13#10 +
        'dans l''onglet Infos de l''application.';
  end;
end;

procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption :=
    'Cet assistant va installer EqQuarmCompanion sur votre ordinateur.' + #13#10 + #13#10 +
    'EqQuarmCompanion est un outil d''analyse d''items et de combat' + #13#10 +
    'pour le serveur EverQuest Project Quarm.' + #13#10 + #13#10 +
    'Cliquez sur Suivant pour continuer.';

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
