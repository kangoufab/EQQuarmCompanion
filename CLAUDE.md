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

28 tests across 5 suites: config, stats_calculator, npc_analysis, spell_stats, spell_stacking.

### Installer

Prérequis : Inno Setup 6 installé + build release à jour.

```powershell
$s = "$env:TEMP\build_installer.ps1"
'Set-Location "D:\Games\quarm\source\EqQuarmCompanion"; & ".\installer\build_installer.ps1"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Génère `installer/output/EqQuarmCompanion-Setup.exe`.

## Architecture

```
src/
  core/       # Pure C++17 — no Qt. All game logic, formulas, parsers.
  db/         # Qt SQL + Network. Database access (MySQL via ODBC or direct).
  ui/         # Qt6 Widgets. 4 tabs: Stuff / Fight / Spells / Infos.
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

### QtConcurrent for async NPC search
`FightTab` dispatches NPC database searches via `QtConcurrent::run()` + `QFutureWatcher` to avoid blocking the UI thread. Pattern established in `src/ui/fight_tab.cpp`.

### `_buffedTotals` — buffed stats in Fight tab
`MainWindow` holds two `PlayerTotals`: `_playerTotals` (base+items+AAs) and `_buffedTotals` (adds active buffs from SpellsTab). `FightTab` receives a pointer to `_buffedTotals` via `setCharacter()`. When buffs change (`onBuffStatsChanged`), `_buffedTotals` is updated and `FightTab::refreshStats()` rebuilds the right panel so it reflects the buffed resists and stats shown in the global stats bar.

### Clickies in Buffs tab
"Clickies" is a synthetic class entry in the Buffs tab that collects click-effect spells from: (1) equipped items (already in `_equippedItems`), and (2) items in personal bag slots only (`General1-8`, not `Bank`/`SharedBank`) loaded via `ItemDatabase::getItemClickeffects()`. Deduplication by spell_id keeps the first item encountered. `CharacterInfo::bag_item_ids` stores the IDs from bag slots parsed in `character_parser.cpp`.

### Stuff tab — layout 3 colonnes
L'onglet Stuff a un layout 3 colonnes via `QSplitter` :
- **Inventaire** (colonne gauche, redimensionnable, min 120px, défaut 250px) : items équipés par slot + items des sacs groupés par numéro de bag (`Bag 1`, `Bag 2`…). Cliquer un item le charge dans la zone de comparaison.
- **Recherche** (colonne du milieu) : filtre slot + SearchCombo + Clear — comportement inchangé.
- **Comparaison** (colonne droite) : cartes item, score UPGRADE/DOWNGRADE, boutons Équiper et Source.

`CharacterInfo::bag_item_ids` est `std::vector<std::pair<int,int>>` — `{bag_number, item_id}` — le numéro de bag est extrait depuis `"GeneralN-SlotM"` dans `character_parser.cpp`. La fenêtre par défaut est 1480×800.

## Key Source Files

| File | Purpose |
|------|---------|
| `src/core/types.h` | All shared structs (ItemData, LootItem, NpcData, CharacterInfo…); `CharacterInfo::bag_item_ids` = `vector<pair<int,int>>` {bag_number, item_id} |
| `src/core/config.h/cpp` | JSON config read/write, DbConfig, getResistDebuffs |
| `src/core/character_parser.h/cpp` | Parse EQ TSV character files; extrait `{bag_number, item_id}` depuis `GeneralN-SlotM` → `bag_item_ids` |
| `src/core/stats_calculator.h/cpp` | HP/Mana/ATK/AC caps, `applyWornStats`, `calculateTotalsWithSpells` (inclut AAs) |
| `src/core/npc_analysis.h/cpp` | Incoming damage, resist ratings, slow land %, special abilities. Triple attack (SA 6) = DA chance (pas DA×0.135). Defensive disc : DB inchangé, seul DI/2 — réduction ~33%, pas 50%. Vérifiés sur log AoW lv70. |
| `src/core/spell_stats.h/cpp` | spellValue, spellIncomingDps |
| `src/core/spell_stacking.h/cpp` | spellsStack() — bard vs non-bard logic |
| `src/db/item_database.h/cpp` | Item DB queries; `getItemClickeffects(QList<int>)` → clickeffect spell IDs for given item IDs |
| `src/ui/main_window.h/cpp` | App shell; `_playerTotals` (base+items+AAs), `_buffedTotals` (+ active buffs); file watcher |
| `src/ui/item_card.h/cpp` | Widget item unifié (carte compacte Option B) — utilisé par Stuff et Fight |
| `src/ui/character_tab.h/cpp` | Stuff tab — layout 3 colonnes (QSplitter) : inventaire équipé+sacs / recherche / comparaison; `rebuildInventoryPanel()` |
| `src/ui/fight_tab.h/cpp` | 2D DPS×slow table, loot, NPC tags, resists; `refreshStats()` rebuilds right panel with buffed totals. CH rotation: `safeP = floor(hp×0.70×10/maxDPS)` = pause max (dixièmes) pour HP ≥ 30% ; affiché pour N min clerics tel que `ceil(100/N) ≤ safeP`. |
| `src/ui/infos_tab.h/cpp` | Expansion selector + resist debuff groups |
| `src/ui/spells_tab.h/cpp` | Buffs tab — class list, checkboxes, stacking, sets save/load, search bar, Clickies (ClickieEntry, loadClickies, _clickieSpells) |
| `src/ui/infos_spell_data.h` | Données statiques sorts debuff + bestInGroup() + spellResistVal() |

## Config File

At runtime, the app loads `config.json` from the same directory as the executable (created on first run from `resources/config_defaults.json`). The `eq_files_dir` key points to the EverQuest character file directory.

## Database

MySQL database named `quarm` (Project Quarm server DB). Connection params in `config.json` under `"db"`. Default: `localhost:3306` user `root` password `rooteq`. Tables used: `npc_types`, `spells_new`, `loottable`, `lootdrop`, `items`, `races`, `global_loot`.

