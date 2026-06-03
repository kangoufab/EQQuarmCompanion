# EqQuarmCompanion Installer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produire `installer/output/EqQuarmCompanion-Setup.exe` — un installeur Inno Setup 6 qui installe l'app, détecte MySQL/MariaDB, propose d'installer MariaDB si absent, et importe le dernier dump SQL Quarm depuis GitHub.

**Architecture:** Un unique script `installer/eqquarmcompanion.iss` construit de façon incrémentale, sans plugin externe (utilise le `TDownloadWizardPage` natif d'Inno Setup 6). Les appels API (MariaDB downloads, GitHub) sont délégués à des scripts PowerShell temporaires écrits sur disque et exécutés via `Exec`.

**Tech Stack:** Inno Setup 6 (Pascal `[Code]`), PowerShell 7, Inno Setup built-in downloader (TDownloadWizardPage), msiexec, mysql CLI.

---

## File Structure

| Fichier | Rôle |
|---------|------|
| `installer/eqquarmcompanion.iss` | Script principal — créé ici, enrichi tâche par tâche |
| `installer/build_installer.ps1` | Compile le `.iss` avec `iscc.exe` |

Prérequis : **Inno Setup 6** installé (`C:\Program Files (x86)\Inno Setup 6\iscc.exe`). Build release à jour dans `build/release/bin/`.

---

## Task 1 : Prérequis — vérifier Inno Setup + créer build script

**Files:**
- Create: `installer/build_installer.ps1`

- [ ] **Étape 1 : Vérifier qu'Inno Setup 6 est installé**

```powershell
Test-Path "C:\Program Files (x86)\Inno Setup 6\iscc.exe"
```

Expected: `True`. Si `False`, télécharger depuis https://jrsoftware.org/isdl.php et installer.

- [ ] **Étape 2 : Vérifier que le build release est à jour**

```powershell
Test-Path "D:\Games\quarm\source\EqQuarmCompanion\build\release\bin\EqQuarmCompanion.exe"
```

Expected: `True`. Si `False`, faire le build release d'abord (cf. CLAUDE.md section Build).

- [ ] **Étape 3 : Créer `installer/build_installer.ps1`**

```powershell
# installer/build_installer.ps1
$iscc = "C:\Program Files (x86)\Inno Setup 6\iscc.exe"
$script = Join-Path $PSScriptRoot "eqquarmcompanion.iss"

if (-not (Test-Path $iscc)) {
    Write-Error "Inno Setup 6 introuvable : $iscc"
    exit 1
}
if (-not (Test-Path $script)) {
    Write-Error "Script .iss introuvable : $script"
    exit 1
}

& $iscc $script
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation échouée (code $LASTEXITCODE)"
    exit $LASTEXITCODE
}
Write-Host "Installeur généré dans installer/output/"
```

- [ ] **Étape 4 : Commit**

```
git add installer/build_installer.ps1
git commit -m "feat(installer): build script Inno Setup"
```

---

## Task 2 : Scaffold de base — installer l'app uniquement

**Files:**
- Create: `installer/eqquarmcompanion.iss`

- [ ] **Étape 1 : Créer `installer/eqquarmcompanion.iss`**

```pascal
; installer/eqquarmcompanion.iss
#define AppName "EqQuarmCompanion"
#define AppVersion "1.0.0"
#define BinDir "..\build\release\bin"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
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
Source: "{#BinDir}\*"; DestDir: "{app}"; \
  Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"

[Run]
Filename: "{app}\EqQuarmCompanion.exe"; \
  Description: "Lancer {#AppName}"; \
  Flags: nowait postinstall skipifsilent
```

> **Note :** Remplacer `{{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}` par un vrai GUID (générer via PowerShell : `[System.Guid]::NewGuid().ToString().ToUpper()`).

- [ ] **Étape 2 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t2.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)` + fichier `installer/output/EqQuarmCompanion-Setup.exe` créé.

- [ ] **Étape 3 : Tester l'installeur de base**

Lancer `installer/output/EqQuarmCompanion-Setup.exe`. Vérifier :
- wizard s'ouvre, page Welcome → choix répertoire → progression → finish
- `EqQuarmCompanion.exe` présent dans le répertoire cible
- icône sur le bureau créée
- `Lancer EqQuarmCompanion` sur la page Finish fonctionne

- [ ] **Étape 4 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): scaffold de base — copie des fichiers app"
```

---

## Task 3 : Détection MySQL/MariaDB

**Files:**
- Modify: `installer/eqquarmcompanion.iss` — ajouter section `[Code]`

- [ ] **Étape 1 : Ajouter la section `[Code]` avec `DetectMySQL()`**

Ajouter à la fin de `eqquarmcompanion.iss` :

```pascal
[Code]
var
  MySQLDetected: Boolean;

function DetectMySQL(): Boolean;
var
  Dummy: String;
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
end;
```

- [ ] **Étape 2 : Compiler et vérifier**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t3.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 3 : Vérifier la détection**

Lancer l'installeur. Si MariaDB est installé sur la machine de dev, ajouter temporairement un `MsgBox(BoolToStr(MySQLDetected), mbInformation, MB_OK)` dans `InitializeWizard` pour vérifier que la détection retourne `True`.

- [ ] **Étape 4 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): détection MySQL/MariaDB"
```

---

## Task 4 : Pages wizard — Composants et Credentials

**Files:**
- Modify: `installer/eqquarmcompanion.iss`

- [ ] **Étape 1 : Déclarer les variables de pages et les flags globaux**

Dans la section `[Code]`, remplacer la déclaration `var` existante par :

```pascal
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
```

- [ ] **Étape 2 : Construire les pages dans `InitializeWizard`**

Remplacer la procédure `InitializeWizard` par :

```pascal
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
```

- [ ] **Étape 3 : Contrôler la visibilité de la page Credentials**

Ajouter la fonction `ShouldSkipPage` dans `[Code]` :

```pascal
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
```

- [ ] **Étape 4 : Lire les choix dans `NextButtonClick`**

Ajouter la fonction `NextButtonClick` :

```pascal
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = CompPage.ID then begin
    WillImportDB := (IdxImportDB >= 0) and CompPage.Values[IdxImportDB];
    WillInstallMariaDB :=
      (IdxInstallMariaDB >= 0) and CompPage.Values[IdxInstallMariaDB];
  end;
end;
```

- [ ] **Étape 5 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t4.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 6 : Vérifier le wizard**

Lancer l'installeur. Vérifier :
- Page "Composants" apparaît après le choix de répertoire
- Si MariaDB non détecté : deux cases cochées
- Si MariaDB détecté : une seule case (import DB)
- Décocher "Importer la BDD" → page Credentials sautée
- Cocher "Importer la BDD" → page Credentials visible avec valeurs par défaut
- Champ Password masqué

- [ ] **Étape 7 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): pages Composants + Credentials"
```

---

## Task 5 : Résolution des URLs via PowerShell

**Files:**
- Modify: `installer/eqquarmcompanion.iss` — ajouter fonctions `GetMariaDBUrl` et `GetLatestDumpUrl`

- [ ] **Étape 1 : Ajouter les variables d'URL globales**

Dans le bloc `var` de `[Code]`, ajouter :

```pascal
  MariaDBDownloadUrl: String;
  DumpDownloadUrl: String;
```

- [ ] **Étape 2 : Ajouter `GetMariaDBUrl()`**

Ajouter la fonction dans `[Code]` :

```pascal
{ Appelle l'API MariaDB pour obtenir l'URL du MSI Windows x64 le plus récent.
  Retourne '' en cas d'erreur. }
function GetMariaDBUrl(): String;
var
  PSFile, UrlFile: String;
  Script: String;
  ResultCode: Integer;
begin
  Result := '';
  PSFile  := ExpandConstant('{tmp}\get_mariadb_url.ps1');
  UrlFile := ExpandConstant('{tmp}\mariadb_url.txt');

  Script :=
    '$ErrorActionPreference = "Stop"' + #13#10 +
    'try {' + #13#10 +
    '  $r = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/"' + #13#10 +
    '  $latest = ($r.major_releases | Where-Object { $_.release_status -eq "Stable" } | Sort-Object release_id -Descending)[0]' + #13#10 +
    '  $files = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/$($latest.release_id)/"' + #13#10 +
    '  $msi = $files.files.file | Where-Object { $_.file_name -match "winx64\.msi$" } | Select-Object -First 1' + #13#10 +
    '  $msi.url | Out-File "' + UrlFile + '" -Encoding utf8 -NoNewline' + #13#10 +
    '} catch { exit 1 }';

  SaveStringToFile(PSFile, Script, False);
  try
    if Exec('powershell.exe',
        '-NoProfile -ExecutionPolicy Bypass -File "' + PSFile + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then begin
      if (ResultCode = 0) and FileExists(UrlFile) then
        LoadStringFromFile(UrlFile, Result);
    end;
  finally
    DeleteFile(PSFile);
    DeleteFile(UrlFile);
  end;
  Result := Trim(Result);
end;
```

- [ ] **Étape 3 : Ajouter `GetLatestDumpUrl()`**

```pascal
{ Appelle l'API GitHub pour obtenir l'URL du dernier fichier SQL dans
  utils/sql/database_full. Retourne '' en cas d'erreur. }
function GetLatestDumpUrl(): String;
var
  PSFile, UrlFile: String;
  Script: String;
  ResultCode: Integer;
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
      if (ResultCode = 0) and FileExists(UrlFile) then
        LoadStringFromFile(UrlFile, Result);
    end;
  finally
    DeleteFile(PSFile);
    DeleteFile(UrlFile);
  end;
  Result := Trim(Result);
end;
```

- [ ] **Étape 4 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t5.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 5 : Tester les fonctions PowerShell indépendamment**

Ouvrir PowerShell et exécuter manuellement les scripts inline pour vérifier que les URLs sont bien récupérées :

```powershell
# Test MariaDB API
$r = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/"
$latest = ($r.major_releases | Where-Object { $_.release_status -eq "Stable" } | Sort-Object release_id -Descending)[0]
Write-Host "Latest: $($latest.release_id)"
$files = Invoke-RestMethod "https://downloads.mariadb.org/rest-api/mariadb/$($latest.release_id)/"
$msi = $files.files.file | Where-Object { $_.file_name -match "winx64\.msi$" } | Select-Object -First 1
Write-Host "MSI URL: $($msi.url)"
```

```powershell
# Test GitHub API
$h = @{ "User-Agent" = "EqQuarmCompanion-Installer" }
$r = Invoke-RestMethod "https://api.github.com/repos/SecretsOTheP/EQMacEmu/contents/utils/sql/database_full" -Headers $h
$f = $r | Where-Object { $_.type -eq "file" } | Sort-Object name -Descending | Select-Object -First 1
Write-Host "Dump: $($f.name) -> $($f.download_url)"
```

Si la structure JSON est différente de ce qui est attendu (propriétés manquantes), ajuster les chemins dans les scripts en conséquence dans `eqquarmcompanion.iss`.

- [ ] **Étape 6 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): résolution URL MariaDB + dump GitHub via PS"
```

---

## Task 6 : Infrastructure de téléchargement (TDownloadWizardPage)

**Files:**
- Modify: `installer/eqquarmcompanion.iss`

- [ ] **Étape 1 : Déclarer la DownloadPage**

Dans le bloc `var` de `[Code]`, ajouter :

```pascal
  DownloadPage: TDownloadWizardPage;
```

- [ ] **Étape 2 : Créer la DownloadPage dans `InitializeWizard`**

À la fin de `InitializeWizard`, ajouter :

```pascal
  DownloadPage := CreateDownloadPage(
    'Téléchargement',
    'Téléchargement des composants sélectionnés...',
    nil   { pas de callback de progression personnalisé }
  );
```

- [ ] **Étape 3 : Remplir et déclencher les téléchargements dans `NextButtonClick`**

Modifier `NextButtonClick` pour y ajouter le déclenchement à la page `wpReady` :

```pascal
function NextButtonClick(CurPageID: Integer): Boolean;
var
  Msg: String;
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
      DownloadPage.Add(DumpDownloadUrl, 'quarm_dump.sql', '');
    end;

    if DownloadPage.FilesCount > 0 then begin
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
```

- [ ] **Étape 4 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t6.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 5 : Tester les téléchargements**

Lancer l'installeur, cocher les deux options, cliquer Suivant sur la page Récapitulatif. Vérifier :
- Page de téléchargement apparaît avec barre de progression
- Les fichiers sont téléchargés (pas d'erreur)
- Après téléchargement, l'installation continue

- [ ] **Étape 6 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): téléchargements MariaDB + dump via TDownloadWizardPage"
```

---

## Task 7 : Installation silencieuse de MariaDB

**Files:**
- Modify: `installer/eqquarmcompanion.iss`

- [ ] **Étape 1 : Ajouter `InstallMariaDB()`**

Ajouter dans `[Code]` :

```pascal
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
```

- [ ] **Étape 2 : Ajouter `FindMysqlExe()`**

```pascal
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
```

- [ ] **Étape 3 : Appeler `DoInstallMariaDB` dans `CurStepChanged`**

Ajouter la procédure `CurStepChanged`. Tous les locaux pour l'import DB sont déclarés ici pour être complétés en Task 8 :

```pascal
procedure CurStepChanged(CurStep: TSetupStep);
var
  MysqlExe: String;
  Host, Port, User, Pass, PassPart: String;
  DumpPath, BatPath: String;
  Params: String;
  ResultCode: Integer;
begin
  if CurStep = ssInstall then begin

    { --- Installation MariaDB --- }
    if WillInstallMariaDB then begin
      WizardForm.StatusLabel.Caption := 'Installation de MariaDB...';
      if not DoInstallMariaDB() then
        Exit;  { erreur déjà affichée dans DoInstallMariaDB }
    end;

    { L'import DB sera ajouté ici en Task 8 }
  end;
end;
```

- [ ] **Étape 4 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t7.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 5 : Tester l'installation MariaDB**

Sur une machine sans MariaDB (ou VM), cocher "Installer MariaDB", lancer l'installeur. Vérifier :
- Message "Installation de MariaDB..." visible dans la barre de statut
- MariaDB s'installe (service `MariaDB` visible dans services.msc après)
- Le mot de passe root correspond à celui saisi page Credentials

- [ ] **Étape 6 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): installation silencieuse MariaDB"
```

---

## Task 8 : Import du dump Quarm

**Files:**
- Modify: `installer/eqquarmcompanion.iss`

- [ ] **Étape 1 : Ajouter l'import DB dans `CurStepChanged`**

Remplacer le corps de `CurStepChanged` par la version complète (MariaDB install + import DB). `PassPart` est déclaré dans le bloc `var` en tête de procédure (Task 7 Étape 3 l'a déjà ajouté) :

```pascal
procedure CurStepChanged(CurStep: TSetupStep);
var
  MysqlExe: String;
  Host, Port, User, Pass, PassPart: String;
  DumpPath, BatPath: String;
  Params: String;
  ResultCode: Integer;
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

      Host     := CredPage.Values[0];
      Port     := CredPage.Values[1];
      User     := CredPage.Values[2];
      Pass     := CredPage.Values[3];
      DumpPath := ExpandConstant('{tmp}\quarm_dump.sql');

      if Pass <> '' then PassPart := ' -p' + Pass
      else PassPart := '';

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
        ' quarm < "' + DumpPath + '"',
        False);
      Exec(ExpandConstant('{sys}\cmd.exe'),
        '/c "' + BatPath + '"',
        '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
      if ResultCode <> 0 then
        MsgBox(
          'Import échoué (code ' + IntToStr(ResultCode) + ').' + #13#10 +
          'La base quarm a été créée mais est vide.' + #13#10 +
          'Réessayez manuellement : mysql -u root quarm < quarm_dump.sql',
          mbError, MB_OK);
      DeleteFile(BatPath);
    end;

  end;
end;
```

- [ ] **Étape 2 : Compiler**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t8.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Expected: `Successful compile (0 errors)`.

- [ ] **Étape 3 : Tester l'import**

Lancer l'installeur avec "Importer la BDD Quarm" coché (MariaDB déjà installé). Vérifier :
- Message "Import dump Quarm" visible
- Après installation : `mysql -u root -e "SHOW DATABASES;"` liste `quarm`
- `mysql -u root quarm -e "SHOW TABLES;"` liste des tables non vides

- [ ] **Étape 4 : Commit**

```
git add installer/eqquarmcompanion.iss
git commit -m "feat(installer): import dump Quarm via mysql CLI"
```

---

## Task 9 : Intégration finale + ajout au .gitignore

**Files:**
- Modify: `installer/eqquarmcompanion.iss` — page Welcome + message Finish
- Modify: `.gitignore`

- [ ] **Étape 1 : Enrichir la page Welcome**

Dans `[Setup]`, ajouter :

```ini
WizardSmallImageFile=..\resources\app_icon.ico
```

Dans `[Code]`, ajouter dans `InitializeWizard` :

```pascal
  WizardForm.WelcomeLabel2.Caption :=
    'Cet assistant va installer EqQuarmCompanion sur votre ordinateur.' + #13#10 + #13#10 +
    'EqQuarmCompanion est un outil d''analyse d''items et de combat' + #13#10 +
    'pour le serveur EverQuest Project Quarm.' + #13#10 + #13#10 +
    'Cliquez sur Suivant pour continuer.';
```

- [ ] **Étape 2 : Ajouter un message de fin conditionnel**

Ajouter dans `[Code]` :

```pascal
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
```

- [ ] **Étape 3 : Ajouter `installer/output/` au .gitignore**

Dans `.gitignore`, ajouter :

```
installer/output/
```

- [ ] **Étape 4 : Compiler et test complet end-to-end**

```powershell
$s = "C:\Users\fabri\AppData\Local\Temp\build_t9.ps1"
'& "C:\Program Files (x86)\Inno Setup 6\iscc.exe" "D:\Games\quarm\source\EqQuarmCompanion\installer\eqquarmcompanion.iss"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Scénario de test complet :
1. Lancer `EqQuarmCompanion-Setup.exe`
2. Page Welcome → texte descriptif visible
3. Page Répertoire → choisir `C:\Games\EqQuarmCompanion`
4. Page Composants → deux cases cochées (si MariaDB absent)
5. Page Credentials → valeurs par défaut correctes
6. Page Récapitulatif → Suivant → téléchargements visibles
7. Installation → messages de statut visibles
8. Page Finish → message "base de données quarm importée" visible
9. Case "Lancer EqQuarmCompanion" cochée → app se lance

- [ ] **Étape 5 : Commit final**

```
git add installer/eqquarmcompanion.iss .gitignore
git commit -m "feat(installer): page Welcome + message Finish conditionnel"
```

---

## Task 10 : Mise à jour CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Étape 1 : Ajouter une section "Installer" dans CLAUDE.md**

Dans la section `## Build` de `CLAUDE.md`, ajouter après la section Release :

```markdown
### Installer

Prérequis : Inno Setup 6 installé + build release à jour.

```powershell
$s = "$env:TEMP\build_installer.ps1"
'Set-Location "D:\Games\quarm\source\EqQuarmCompanion"; & ".\installer\build_installer.ps1"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Génère `installer/output/EqQuarmCompanion-Setup.exe`.
```

- [ ] **Étape 2 : Commit**

```
git add CLAUDE.md
git commit -m "docs: instructions build installeur dans CLAUDE.md"
```

---

## Récapitulatif des fichiers produits

| Fichier | Taille estimée |
|---------|----------------|
| `installer/eqquarmcompanion.iss` | ~350 lignes |
| `installer/build_installer.ps1` | ~20 lignes |
| `installer/output/EqQuarmCompanion-Setup.exe` | ~50–80 Mo (dépend du dump) |
