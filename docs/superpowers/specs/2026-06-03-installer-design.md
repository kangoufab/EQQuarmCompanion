# Installer EqQuarmCompanion — Design

**Date :** 2026-06-03  
**Technologie :** Inno Setup 6 + plugin IDP  
**Cible :** Windows x64, lancement manuel par l'utilisateur

---

## Objectif

Produire un installeur `EqQuarmCompanion-Setup.exe` qui :
1. Installe l'application EqQuarmCompanion et toutes ses dépendances (Qt6, MinGW runtime, MariaDB driver, plugins Qt).
2. Propose d'importer la dernière version de la BDD Quarm (dump SQL depuis GitHub).
3. Propose d'installer MariaDB si aucun serveur MySQL/MariaDB n'est détecté localement.

---

## Flux du wizard

| # | Page | Condition d'affichage |
|---|------|-----------------------|
| 1 | Welcome | Toujours |
| 2 | Dossier d'installation (défaut : `C:\Program Files\EqQuarmCompanion`) | Toujours |
| 3 | Composants | Toujours |
| 4 | Credentials BDD | Si "Importer la BDD Quarm" coché |
| 5 | Récapitulatif | Toujours |
| 6 | Progression | Toujours |
| 7 | Finish (case "Lancer EqQuarmCompanion") | Toujours |

### Page 3 — Composants

```
[x] EqQuarmCompanion          (obligatoire, grisé)
[x] Importer la BDD Quarm     (optionnel, coché par défaut)
[ ] Installer MariaDB          (visible et coché par défaut uniquement si
                                aucun MySQL/MariaDB détecté sur la machine)
```

### Page 4 — Credentials BDD

Champs éditables avec valeurs par défaut :
- Host : `localhost`
- Port : `3306`
- User : `root`
- Password : *(vide)*

Ces credentials sont utilisés pour créer la base et importer le dump. L'utilisateur reconfigure la connexion dans l'app après installation.

---

## Détection MySQL/MariaDB

Cherche dans cet ordre via Pascal `[Code]` :

1. **Service Windows** : `OpenSCManager` + `OpenService` sur les noms `mysql`, `mariadb`, `MariaDB`.
2. **Registry** : clés `HKLM\SOFTWARE\MySQL AB`, `HKLM\SOFTWARE\MariaDB`, `HKLM\SOFTWARE\WOW6432Node\MariaDB`.
3. **PATH** : présence de `mysql.exe` ou `mariadb.exe` dans les répertoires du PATH système.

Si au moins une détection réussit → case "Installer MariaDB" cachée.  
Si aucune détection → case "Installer MariaDB" visible et cochée par défaut.

---

## Téléchargements (plugin IDP)

### MariaDB

- Appel `WinHttpRequest` à `https://downloads.mariadb.org/rest-api/mariadb/latest/`
- Parse JSON : extrait l'URL du fichier `.msi` Windows x64 de la dernière version stable.
- Téléchargement via IDP avec barre de progression.
- Installation silencieuse : `msiexec /i mariadb.msi /quiet SERVICENAME=MariaDB ROOTPASSWORD="{password page 4}"`
- Le mot de passe root utilisé est exactement celui saisi page 4 (vide par défaut).

### Dump Quarm

- Appel `WinHttpRequest` à `https://api.github.com/repos/SecretsOTheP/EQMacEmu/contents/utils/sql/database_full`
- Header `User-Agent: EqQuarmCompanion-Installer` (requis par l'API GitHub).
- Parse JSON : liste des fichiers du répertoire, tri lexicographique décroissant sur `name` (les fichiers du repo suivent une convention de nommage avec date ou version), sélection du premier (`download_url`). Si un seul fichier est présent, il est sélectionné directement.
- Téléchargement via IDP.

---

## Import SQL

Séquence exécutée via `Exec` en `[Code]` après téléchargement du dump :

1. **Localisation de `mysql.exe`** : registry `HKLM\SOFTWARE\MariaDB\...\Location` ou scan PATH.
2. **Création de la base** :
   ```
   mysql -h {host} -P {port} -u {user} -p{password}
         -e "CREATE DATABASE IF NOT EXISTS quarm CHARACTER SET utf8mb4"
   ```
3. **Import du dump** : la redirection shell (`< dump.sql`) n'étant pas disponible via `Exec` Inno Setup, l'installeur génère un fichier `.bat` temporaire dans `%TEMP%` contenant la commande complète, l'exécute via `cmd /c`, puis le supprime.

Si le code de retour est non-nul, une `MsgBox` affiche l'erreur et propose de continuer ou d'annuler. L'import du dump peut prendre plusieurs minutes (fichier potentiellement volumineux).

---

## Artefacts produits

| Fichier | Rôle |
|---------|------|
| `installer/eqquarmcompanion.iss` | Script Inno Setup principal |
| `installer/build_installer.ps1` | Compile le `.iss` avec `iscc.exe` |
| `installer/idp.iss` | Include IDP (download plugin) |
| `installer/idp.dll` | DLL runtime IDP |
| `installer/output/EqQuarmCompanion-Setup.exe` | Installeur final |

### Prérequis de build de l'installeur

- Inno Setup 6 installé (`iscc.exe` dans PATH ou chemin fixe).
- Plugin IDP (`idp.dll` + `idp.iss`) dans `installer/`.
- `build/release/bin/` à jour (build release effectué au préalable).

---

## Fichiers embarqués dans l'installeur

Tout le contenu de `build/release/bin/` est inclus, soit :
- `EqQuarmCompanion.exe`
- Qt6 DLLs : `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Network.dll`, `Qt6Sql.dll`, `Qt6Svg.dll`, `Qt6Widgets.dll`
- MinGW runtime : `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- MariaDB driver : `libmariadb.dll`, `sqldrivers/qsqlmysql.dll`, `sqldrivers/libmariadb.dll`
- Plugins Qt : `platforms/`, `imageformats/`, `styles/`, `tls/`, `networkinformation/`, `iconengines/`, `generic/`
- `config_defaults.json`

---

## Hors périmètre

- Configuration automatique de `config.json` (host/port/user/password) : l'utilisateur le fait manuellement dans l'app.
- Mode silencieux / headless de l'installeur.
- Désinstalleur personnalisé (Inno Setup génère un désinstalleur standard automatiquement).
- Mise à jour automatique de l'app ou de la BDD après installation.
