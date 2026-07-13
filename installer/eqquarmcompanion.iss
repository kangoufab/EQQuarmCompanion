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
; config.json n'est PAS distribué : il peut contenir la config locale du dev
; (personnages). L'app le recrée dans %APPDATA%\EqQuarmCompanion
; à partir de config_defaults.json au premier lancement.
; quarm_data.db (BDD SQLite embarquée) est inclus automatiquement : c'est un
; fichier parmi d'autres dans BinDir, copié par CMake au build (voir CLAUDE.md).
Source: "{#BinDir}\*"; DestDir: "{app}"; Excludes: "config.json"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\EqQuarmCompanion.exe"

[Run]
Filename: "{app}\EqQuarmCompanion.exe"; Description: "Lancer {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption :=
    'Cet assistant va installer EqQuarmCompanion sur votre ordinateur.' + #13#10 + #13#10 +
    'EqQuarmCompanion est un outil d''analyse d''items et de combat' + #13#10 +
    'pour le serveur EverQuest Project Quarm.' + #13#10 + #13#10 +
    'Cliquez sur Suivant pour continuer.';
end;
