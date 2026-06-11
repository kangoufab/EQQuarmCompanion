# EqQuarmCompanion — CLAUDE.md

Companion app for Project Quarm (EverQuest emulator). Evaluates item upgrades and NPC combat analysis. C++17/Qt6 rewrite of the Python app at `D:\Games\quarm\source\eq-item-evaluator`.

## Environment

- **Qt**: 6.11.1 — MinGW only (`C:\Qt\6.11.1\mingw_64`). **No MSVC install.**
- **Compiler**: MinGW 13.1 (`C:\Qt\Tools\mingw1310_64\bin\g++.exe`)
- **CMake**: `C:\Qt\Tools\CMake_64\bin\cmake.exe`
- **Ninja**: `C:\Qt\Tools\Ninja\ninja.exe`
- **vcpkg**: `C:\vcpkg` — triplet `x64-mingw-dynamic`
- **windeployqt**: `C:\Qt\6.11.1\mingw_64\bin\windeployqt.exe`

## PowerShell

**Toujours créer un fichier `.ps1` temporaire et l'exécuter, plutôt que de passer des commandes inline au PowerShell tool.** Cela vaut pour toutes les commandes PowerShell sans exception, pas seulement les longues. Écrire le script dans un fichier (ex. `$env:TEMP\run_xxx.ps1`), l'exécuter, puis le supprimer.

## Build

Always use `.ps1` scripts to avoid the Windows 8191-char command-line limit. Never pass long cmake commands inline in PowerShell.

### Debug (with tests)

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
& $cmake --preset windows-x64-debug
& $cmake --build build/debug
```

### Release

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
& $cmake --preset windows-x64-release
& $cmake --build build/release --target EqQuarmCompanion
```

Preset names are defined in `CMakePresets.json`: `windows-x64-debug` and `windows-x64-release`.

**Note:** `VCPKG_APPLOCAL_DEPS=OFF` is set in both presets to disable vcpkg's `applocal.ps1` post-build step. This avoids breakage when PowerShell is updated (the step hard-codes the installed PS version in a INTERNAL cache variable). Safe because nlohmann-json is header-only, gtest is test-only, and Qt is installed separately. If the INTERNAL cache var `Z_VCPKG_PWSH_PATH` becomes stale, clear it with `cmake -U "Z_VCPKG_PWSH_PATH" -U "Z_VCPKG_POWERSHELL_PATH" --preset windows-x64-debug`.

### Tests

```powershell
cd build/debug
ctest --output-on-failure
```

36 tests across 6 suites: config, character_parser, stats_calculator, npc_analysis, spell_stats, spell_stacking. Les tests `character_parser` (dans `tests/test_character_parser.cpp`) couvrent le happy-path mais aussi les comportements documentés : `bag_item_ids` depuis `GeneralN-SlotM`, section AAIndex, expansion de slots Ear/Wrist/Fingers, base stats/race, classe inconnue, pattern `*-Quarmy.txt`.

### Installer

Prérequis : Inno Setup 6 installé + build release à jour.

```powershell
$s = "$env:TEMP\build_installer.ps1"
'Set-Location "D:\Games\quarm\source\EqQuarmCompanion"; & ".\installer\build_installer.ps1"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Génère `installer/output/EqQuarmCompanion-Setup.exe`.

**Règle :** toujours regénérer l'installer après un build release.

## Qualité & CI

- **CI** : `.github/workflows/ci.yml` build + `ctest` sur `push`/PR vers `main` (runner `windows-latest`) — **vert**. MinGW 13.1 + Ninja + Qt via `install-qt-action`, vcpkg préinstallé du runner, presets `ci-debug`/`ci-*` (dans `CMakePresets.json`) qui lisent l'environnement (`QT_ROOT_DIR`, `VCPKG_INSTALLATION_ROOT`) au lieu des chemins absolus locaux. Ne touche pas aux presets `windows-x64-*` de dev. **Deux pièges résolus, ne pas régresser :**
  - **Qt 6.8.3 LTS côté CI** (pas la 6.11.1 du poste de dev) : le dépôt Qt 6.11.x a un layout par compilateur (`qt6_6111_mingw`, `qt6_6111_llvm_mingw`) que `aqtinstall 3.3.0` — dernière version, installée par l'action — ne sait pas résoudre (cherche l'ancien `qt6_6111/qt6_6111/Updates.xml`). La 6.8 LTS a le layout classique. Les APIs Qt6 utilisées sont stables sur tout 6.x, donc le build CI valide bien le code.
  - **DLL vcpkg sur le PATH** : `VCPKG_APPLOCAL_DEPS=OFF` ne copie pas `libgtest.dll` à côté de l'exe ; les étapes Build et Test prependent `build/debug/vcpkg_installed/x64-mingw-dynamic/debug/bin` au PATH, sinon `gtest_discover_tests` échoue (« Error running test executable »).
- **`.clang-format`** : style calé sur le code existant (4 espaces, ~100 col, pointeurs collés au type). **Non appliqué rétroactivement** — à adopter au fil des fichiers modifiés, pour éviter un diff massif.
- **`.clang-tidy`** : analyse statique conservatrice (bugprone/performance/quelques modernize). Usage : configurer avec `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (déjà dans `ci-debug`) puis `clang-tidy -p build/debug src/core/<fichier>.cpp`.

## Architecture

```
src/
  core/       # Pure C++17 — no Qt. All game logic, formulas, parsers.
  db/         # Qt SQL + Network. Database access (MySQL via ODBC or direct).
  ui/         # Qt6 Widgets. 4 tabs: Stuff / Fight / Buffs / Infos.
  main.cpp
resources/    # config_defaults.json, icons
tests/        # GTest unit tests (core/ only)
docs/
  specs/      # Functional specs for each feature
  plans/      # Implementation plans (6 C++ rewrite plans + 1 evolution plan)
  functional-documentation.md  # EQ game formulas + combat analysis reference
```

## Critical Architecture Decisions

### `item_slots` not `slots`
Qt defines `#define slots` as a keyword for signal/slot syntax. All structs in `src/core/types.h` use `item_slots` instead of `slots` to avoid the macro conflict. This affects `ItemData`, `LootItem`, and all code that reads these structs. **Never rename back to `slots`.**

### `#undef slots` in db/ files
Some db/ `.cpp` files use `#undef slots` before including project headers as an alternative approach. Both patterns coexist.

### nlohmann_json linked PUBLIC
`eq_core` links `nlohmann_json` with `target_link_libraries(eq_core PUBLIC nlohmann_json::nlohmann_json)` because `config.h` (a public header) includes `nlohmann/json.hpp`. If changed to PRIVATE, consumers of eq_core will get missing-include errors.

### AUTOMOC requires headers in source lists
Qt's AUTOMOC scans files listed in `add_library()`/`add_executable()`. Any QObject subclass header **must** appear in the CMakeLists source list, not just the `.cpp`. Missing this causes vtable link errors.

### CharacterTab — implémentation répartie sur plusieurs `.cpp`
`character_tab.cpp` était un god-file (>1480 l). L'implémentation de la classe `CharacterTab` est désormais scindée en deux unités de compilation : `character_tab.cpp` (UI, inventaire, comparaison, slots) et `character_tab_tooltip.cpp` (`buildStatTooltip`). C++ autorise de définir des méthodes membres d'une même classe dans des `.cpp` différents — le **header `character_tab.h` et l'API publique sont inchangés**, donc AUTOMOC n'est pas impacté (seul le header avec `Q_OBJECT` doit rester listé, ce qui est le cas). Les helpers libres partagés par les deux TU (`getItemStat`, `isAttrStat`, `isResistStat`) vivent dans `character_tab_internal.h` en `inline` (définition unique conforme à l'ODR), **pas** en `static` (sinon redéfinition par TU). Pour scinder à nouveau, ajouter le nouveau `.cpp` à la liste de sources de `EqQuarmCompanion` dans `CMakeLists.txt`.

### QtConcurrent for async NPC search
`FightTab` dispatches NPC database searches via `QtConcurrent::run()` + `QFutureWatcher` to avoid blocking the UI thread. Pattern established in `src/ui/fight_tab.cpp`.

### `_buffedTotals` — buffed stats in Fight tab
`MainWindow` holds two `PlayerTotals`: `_playerTotals` (base+items+AAs) and `_buffedTotals` (adds active buffs from SpellsTab). `FightTab` receives a pointer to `_buffedTotals` via `setCharacter()`. When buffs change (`onBuffStatsChanged`), `_buffedTotals` is updated and `FightTab::refreshStats()` rebuilds the right panel so it reflects the buffed resists and stats shown in the global stats bar.

### Clickies in Buffs tab
"Clickies" is a synthetic class entry in the Buffs tab that collects click-effect spells from: (1) equipped items (already in `_equippedItems`), and (2) items in personal bag slots only (`General1-8`, not `Bank`/`SharedBank`) loaded via `ItemDatabase::getItemClickeffects()`. Deduplication by spell_id keeps the first item encountered. `CharacterInfo::bag_item_ids` stores the IDs from bag slots parsed in `character_parser.cpp`.

### Tooltips de sorts — `formatSpellTooltip` et couleur accent
`formatSpellTooltip` (dans `spell_tooltip.h/cpp`) génère du HTML `<table>` avec deux sections : **EFFETS** (SPAs 0-133) et **CONDITIONS** (SPAs 134-144 — limites focus). Le paramètre `accentColor` colore le header du sort et les valeurs d'effets ; les labels utilisent `kHtmlLabel` (`#aaaaaa`) et les conditions `kHtmlCondLabel`/`kHtmlCondValue`/`kHtmlCondHeader`. Les appelants passent leur couleur existante (`col` dans `item_card.cpp`, `color` dans `stats_bar.cpp`). L'onglet Buffs passe `kAccentBuff` (buff), `kAccentPurple` (clicky), `kAccentBlocked` (bloqué).

Dans l'onglet Buffs, les tooltips enrichis (sections CONFLIT + SORT avec durée/cast) sont construits par `spellExtraRows()` et injectés via `appendTooltipRows()` — directement dans `spells_tab.cpp`, sans modifier `formatSpellTooltip`. Les couleurs de ces sections utilisent `kHtmlConflictHeader`, `kHtmlConflictLabel`, `kHtmlSortHeader`, `kHtmlSortLabel`, `kHtmlSortValue`.

### Strikeout dans l'onglet Buffs — `QFont::setStrikeOut()` pas CSS
Le strikeout des sorts bloqués est appliqué via `QFont::setStrikeOut(true)` sur le widget (label/checkbox), **pas** via `text-decoration: line-through` dans le stylesheet. Raison : Qt propage l'attribut `strikeOut` du QFont du widget vers son rendu de tooltip, causant un texte barré dans le tooltip. Le `QToolTip` utilise `QToolTip::font()` comme base (indépendant du widget), donc `QFont::setStrikeOut()` n'affecte que le label, pas le tooltip.

Les checkboxes bloquées ont aussi un override explicite `QCheckBox:disabled { color: kAccentBlocked; }` dans leur stylesheet. Sans cela, `setEnabled(false)` fait utiliser à Qt la couleur système `grayText` qui peut tomber sous 4.5:1 sur `kBgCard`, écrasant la couleur définie par le sélecteur `QCheckBox { color: ... }`. L'override `:disabled` prend priorité et préserve `kAccentBlocked` (#cc6666) quelle que soit la palette système.

### Token system — palette.h comme source unique de couleurs
Toutes les couleurs UI sont déclarées dans `src/ui/palette.h` (`inline const char*`). Les règles :
- Ne jamais hardcoder une couleur déjà présente comme token. Utiliser `kBgCard`, `kGreen`, `kAccentBlue`, etc.
- Tout nouveau fichier `.cpp` dans `src/ui/` doit inclure `ui/palette.h` (direct) ou `ui/ui_helpers.h` / `ui/stat_categories.h` (transitif). `spell_tooltip.cpp` l'inclut directement.
- Tout nouveau token s'ajoute dans `palette.h` **avant** d'être utilisé. Ne pas créer de variables locales `static const char* color = "#..."`.
- `stat_categories.h` : ne jamais redéfinir `CatColors`, `CAT_COLORS`, `CAT_LABELS`, `CLASS_CATEGORIES`, `STAT_LABELS`, `STAT_SUFFIX` dans un fichier local — importer le header. Partagé par `character_tab.cpp`, `stats_bar.cpp`, **et** `spells_tab.cpp`.
- Tokens `kAccentWorn`, `kAccentFocus`, `kAccentProc`, `kAccentBuff` dans `palette.h` — utilisés par `item_card.cpp`, `spell_tooltip.cpp` et `spells_tab.cpp`. Ne pas re-hardcoder `#8888ff`, `#88cc88`, `#ffaa44`, `#80b0e0`.
- Familles de tokens supplémentaires dans `palette.h` :
  - `kAccentGoldHover` — variante hover/focus du gold BIS (`character_tab.cpp`).
  - `kBgTint*` / `kBorderTint*` — teintes de fond par couleur sémantique, utilisées par `sectionTheme()` dans `ui_helpers.h`. Ne pas hardcoder `"#2a241a"` etc. — utiliser `kBgTintOrange`, `kBgTintGreen`, `kBgTintRed`, `kBgTintPurple`.
  - `kHtmlLabel`, `kHtmlCondHeader/Label/Value`, `kHtmlConflictHeader/Label`, `kHtmlSortHeader/Label/Value` — couleurs pour les strings HTML de tooltip (impossible d'utiliser des variables QSS dans les attributs `style=`). Ces tokens garantissent la cohérence entre `spell_tooltip.cpp`, `item_tooltip.cpp`, `stats_bar.cpp` et `spells_tab.cpp`.

### Stuff tab — layout 3 colonnes
L'onglet Stuff a un layout 3 colonnes via `QSplitter` :
- **Inventaire** (colonne gauche, redimensionnable, min 260px, défaut 260px) : items équipés par slot + items des sacs groupés par numéro de bag (`Bag 1`, `Bag 2`…). Cliquer un item le charge dans la zone de comparaison. Le minimum (260px) est calé sur la largeur intrinsèque de la grille d'items équipés (5 colonnes × 42px + spacing/marges ≈ 238px) pour éviter qu'elle soit clippée — le défilement horizontal y est désactivé.
- **Recherche** (colonne du milieu) : filtre slot + SearchCombo + Clear — comportement inchangé.
- **Comparaison** (colonne droite) : cartes item, score UPGRADE/DOWNGRADE, boutons Équiper et Source.

`CharacterInfo::bag_item_ids` est `std::vector<std::pair<int,int>>` — `{bag_number, item_id}` — le numéro de bag est extrait depuis `"GeneralN-SlotM"` dans `character_parser.cpp`. La fenêtre par défaut est 1280×760 (minimum 900×600).

### Grille d'items équipés — icônes via `ItemData::icon`, pas `id`
Le bloc « Équipé » de l'inventaire affiche une grille graphique façon paperdoll (armure/bijoux en grille 5 colonnes + rangée d'armes séparée Primary/Secondary/Range/Ammo), construite par `makeEquipCell()` dans `rebuildInventoryPanel()`. Chaque cellule est un `QPushButton` avec icône, tooltip enrichi (`formatItemTooltip`) et clic → `showComparison`.

Les fichiers PNG sont nommés `item_<icon>.png` où `<icon>` est la valeur de la colonne **`icon`** de la table `items` (graphique partagé entre items similaires), **pas** l'`id` de l'item — plusieurs items peuvent partager la même icône. `ItemData::icon` est peuplé par `rowToItemData()` dans `item_database.cpp`. `loadItemIcon()` (statique, dans `character_tab.cpp`) résout le chemin via `QCoreApplication::applicationDirPath() + "/imgs/items/item_<icon>.png"` et met en cache les pixmaps redimensionnés (clé `{iconId, size}`) pour éviter de relire/rescaler le PNG depuis le disque à chaque `rebuildInventoryPanel()` (déclenché par `setCharacter()` à chaque changement de personnage et rafraîchissement du file watcher).

Déploiement : `resources/imgs/items/` (uniquement les fichiers `item_*.png`, pas les `.gif` numériques inutilisés) est copié à côté de l'exécutable par CMake (`copy_directory_if_different`, voir `CMakeLists.txt`), et l'installeur Inno Setup le récupère automatiquement via `{#BinDir}\*`.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/core/types.h` | All shared structs (ItemData, LootItem, NpcData, CharacterInfo, PlayerTotals, PlayerTotalsExtra, SpellData…); `CharacterInfo::bag_item_ids` = `vector<pair<int,int>>` {bag_number, item_id} |
| `src/core/config.h/cpp` | JSON config read/write, DbConfig, getClassWeights, setClassWeights, getCharacterClass |
| `src/core/character_parser.h/cpp` | Parse EQ TSV character files; extrait `{bag_number, item_id}` depuis `GeneralN-SlotM` → `bag_item_ids` |
| `src/core/stats_calculator.h/cpp` | HP/Mana/ATK/AC caps, `applyWornStats`, `calculateTotalsWithSpells` (inclut AAs) |
| `src/core/npc_analysis.h/cpp` | Incoming damage, resist ratings, slow land %, special abilities. Triple attack (SA 6) = DA chance (pas DA×0.135). Defensive disc : DB inchangé, seul DI/2 — réduction ~33%, pas 50%. Vérifiés sur log AoW lv70. |
| `src/core/spell_stats.h/cpp` | spellValue, spellIncomingDps |
| `src/core/spell_stacking.h/cpp` | spellsStack() — bard vs non-bard logic |
| `src/core/derived_metrics.h` | `offensivePower()`, `defensiveScore()` — scores offensifs/défensifs joueur pour le système d'upgrade |
| `src/core/equipped_effects.h` | `getActiveEffects()` — effets worn/proc actifs depuis les items équipés |
| `src/db/db_connection.h` | `DbConnection` singleton — gestion de `QSqlDatabase`; `connect()`, `testConnection()` |
| `src/db/item_database.h/cpp` | Item DB queries; `getItemClickeffects(QList<int>)` → `QList<QPair<QString,int>>` (item_name, spell_id) |
| `src/db/npc_database.h` | `NpcDatabase` — `searchNpcs()`, `getNpcById()`, `getNpcSpells()`, `getNpcLoot()` |
| `src/db/bis_scraper.h` | `BisScaper` — scraper async HTML (QNetworkAccessManager) pour Best-in-Slot |
| `src/ui/main_window.h/cpp` | App shell; `_playerTotals` (base+items+AAs), `_buffedTotals` (+ active buffs); file watcher |
| `src/ui/item_card.h/cpp` | Widget item unifié (carte compacte Option B) — utilisé par Stuff et Fight |
| `src/ui/character_tab.h/cpp` | Stuff tab — layout 3 colonnes (QSplitter) : inventaire équipé+sacs / recherche / comparaison; `rebuildInventoryPanel()`. **L'implémentation de la classe est répartie sur 2 `.cpp`** (voir décision d'archi ci-dessous) |
| `src/ui/character_tab_tooltip.cpp` | Définit `CharacterTab::buildStatTooltip()` (~290 l, décomposition HTML BASE/ITEMS/FORMULE d'un stat), extrait de `character_tab.cpp` |
| `src/ui/character_tab_internal.h` | Header interne (pas de QObject) : helpers `inline` `getItemStat`/`isAttrStat`/`isResistStat` partagés entre les 2 TU de CharacterTab |
| `src/ui/fight_tab.h/cpp` | 2D DPS×slow table, loot, NPC tags, resists; `refreshStats()` rebuilds right panel with buffed totals. CH rotation: `safeP = floor(hp×0.70×10/maxDPS)` = pause max (dixièmes) pour HP ≥ 30% ; affiché pour N min clerics tel que `ceil(100/N) ≤ safeP`. |
| `src/ui/infos_tab.h/cpp` | Expansion selector + resist debuff groups |
| `src/ui/spells_tab.h/cpp` | Buffs tab — class list, checkboxes, stacking, sets save/load, search bar, Clickies. `_conflicts` = `map<int,int>` loser→winner. Helpers fichier : `spellExtraRows()` (sections CONFLIT+SORT dans les tooltips), `appendTooltipRows()` (injection dans table existante). |
| `src/ui/infos_data.h` | `kExpCaps` (cap joueur par extension : Luclin=60, PoP=65), `kDebuffsByExp`, `getResistDebuffs()` |
| `src/ui/infos_spell_data.h` | Données statiques sorts debuff; `getInfoGroups()`, `getCrossConflicts()`, `getResistGroupOrder()`, `getResistBardGroups()`, `bestInGroup()`, `spellResistVal()` |
| `src/ui/settings_dialog.h` | `SettingsDialog` — dialog 3 onglets : DB / Fichiers / Poids (sliders stat par classe) |
| `src/ui/spell_tooltip.h/cpp` | `formatSpellTooltip(spell, level, spellNames, accentColor)` — tooltip HTML en `<table>` 2 colonnes : section EFFETS (SPAs 0-133) + CONDITIONS (SPAs 134-144). `accentColor` colore header + valeurs (Worn `kAccentWorn`, Focus `kAccentFocus`, Proc `kAccentProc`, Click `kAccentPurple`, Buff `kAccentBuff`). |
| `src/ui/stats_bar.h` | `makePlayerStatsBar()` — bandeau de stats avec tooltips par source |
| `src/ui/ui_helpers.h` | Helpers UI inline : `sectionFrame()`, `sectionLabel()`, `gridWidget()`, `kComboStyle` — couleurs depuis `palette.h` |
| `src/ui/palette.h` | Token colors système : `kBg*`, `kText*`, `kGreen/kOrange/kRed`, `kAccent*`, `kAccentGold`, `kAccentWorn/Focus/Proc/Buff`, `kAccentGoldHover`, `kBgTint*/kBorderTint*` (teintes section), `kHtml*` (couleurs tooltip HTML) — voir ce fichier pour tous les tokens |
| `src/ui/stat_categories.h` | `CatColors` struct + `STAT_CATEGORIES`, `CAT_LABELS`, `CAT_COLORS`, `CLASS_CATEGORIES`, `STAT_LABELS`, `STAT_SUFFIX` — partagé par `character_tab.cpp`, `stats_bar.cpp`, `spells_tab.cpp` |
| `src/ui/widgets.h` | `SearchComboBox` — QComboBox avec signal `popup_requested()` |

## Config File

At runtime, the app stores `config.json` (et `eqquarm_debug.log`) dans **`%APPDATA%\EqQuarmCompanion\`**, pas à côté de l'exe — choix fait dans `main.cpp` pour éviter la virtualisation UAC (VirtualStore) quand l'app est installée dans `Program Files`. `config_defaults.json` reste une ressource en lecture seule livrée à côté de l'exe ; `config.json` est créé au premier lancement à partir de ce fichier. Si un `config.json` hérité existe à côté de l'exe (ancien emplacement), il est **migré** (copié) vers `%APPDATA%` au démarrage tant que la cible n'existe pas encore. Si `%APPDATA%` n'est pas défini, l'app retombe sur le dossier de l'exe. Le `eq_files_dir` key points to the EverQuest character file directory.

## Database

MySQL database named `quarm` (Project Quarm server DB). Connection params in `config.json` under `"db"`. Default: `localhost:3306` user `root` password `rooteq`. Tables used: `npc_types`, `spells_new`, `loottable`, `lootdrop`, `items`, `races`, `global_loot`.

**Index FULLTEXT — actuellement débranché (régression).** `npc_database.cpp` et `item_database.cpp` contiennent des helpers `hasNpcsFulltextIndex`/`hasItemsFulltextIndex` (qui font `ALTER TABLE … ADD FULLTEXT INDEX ft_name` si l'index manque) + la migration `docs/sql_migrations/add_fulltext_indexes.sql`. Le branchement FULLTEXT était bien câblé dans `searchItems`/`searchNpcs` (commit `5366365`) : `if (hasXFulltextIndex(db)) { … WHERE MATCH(Name) AGAINST(:t IN BOOLEAN MODE) … } else { … LIKE … }`. Le refactor QCompleter (`d47f79d`) a **supprimé la branche FULLTEXT** des deux recherches, laissant uniquement le `LIKE '%term%'` (full-scan) et les helpers comme code mort. L'index `ft_name` n'est donc plus ni créé ni exploité. Le bouton « Source » (`getNpcSources`) filtre par `item_id` et n'a jamais utilisé le FULLTEXT. Pour restaurer : remettre la branche `MATCH … AGAINST` (cf. `git show 5366365:src/db/item_database.cpp`).

