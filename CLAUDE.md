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

### Tests

```powershell
cd build/debug
ctest --output-on-failure
```

28 tests across 5 suites: config, character_parser, stats_calculator, npc_analysis, spell_stats, spell_stacking.

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
  plans/      # Implementation plans (6 plans for the C++ rewrite)
  formules-calcul-stats-et-stacking.md  # EQ formulas reference
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

## Key Source Files

| File | Purpose |
|------|---------|
| `src/core/types.h` | All shared structs (ItemData, LootItem, NpcData, CharacterData…) |
| `src/core/config.h/cpp` | JSON config read/write, DbConfig, getResistDebuffs |
| `src/core/character_parser.h/cpp` | Parse EQ TSV character files |
| `src/core/stats_calculator.h/cpp` | HP/Mana/ATK/AC caps, class-gated mana |
| `src/core/npc_analysis.h/cpp` | Incoming damage, resist ratings, slow land %, special abilities |
| `src/core/spell_stats.h/cpp` | spellValue, spellIncomingDps |
| `src/core/spell_stacking.h/cpp` | spellsStack() — bard vs non-bard logic |
| `src/ui/fight_tab.h/cpp` | 2D DPS×slow table with CH rotation per cell |
| `src/ui/infos_tab.h/cpp` | Expansion selector + resist debuff groups |
| `src/ui/spells_tab.h/cpp` | **Placeholder** — needs full implementation from spec |

## Incomplete Features

- **SpellsTab**: currently shows placeholder text. Full implementation spec at `docs/specs/2026-05-02-spells-tab-design.md`.
- **InfosTab `buildResistGroup`**: shows group labels only. Best-spell selection logic from Python `infos_tab.py` not yet ported.

## Config File

At runtime, the app loads `config.json` from the same directory as the executable (created on first run from `resources/config_defaults.json`). The `eq_files_dir` key points to the EverQuest character file directory.

## Database

MySQL database named `quarm` (Project Quarm server DB). Connection params in `config.json` under `"db"`. Default: `localhost:3306` user `root` no password. Tables used: `npc_types`, `spells_new`, `loottable`, `lootdrop`, `items`.

## Python Reference App

Functional parity target: `D:\Games\quarm\source\eq-item-evaluator`. When the C++ behavior is unclear, check the Python source (`ui/fight_tab.py`, `core/npc_analysis.py`, etc.) as the reference implementation.
