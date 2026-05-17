# EqQuarmCompanion — Réécriture C++/Qt6

> **For agentic workers:** Use superpowers:writing-plans then superpowers:subagent-driven-development to implement this spec task-by-task.

**Date :** 2026-05-16  
**Répertoire cible :** `D:\Games\quarm\source\EqQuarmCompanion`  
**Origine :** Réécriture à parité fonctionnelle exacte de `eq-item-evaluator` (PyQt6 ~5 700 LoC)

---

## Objectif

Produire un exécutable Windows autonome (~60 Mo) sans runtime Python, en C++17/Qt6, avec exactement les mêmes 4 onglets et fonctionnalités que l'app Python.

---

## Stack technique

| Composant | Choix | Raison |
|---|---|---|
| Build | CMake 3.25+ + vcpkg manifest | Standard industrie, multiplateforme |
| UI | Qt6 Widgets | Même framework que PyQt6, API identique |
| SQL | Qt6 Sql (QMYSQL driver) | Natif Qt, libmysqlclient déjà présente (MariaDB) |
| Config | nlohmann/json (vcpkg) | Parsing config.json sans Qt |
| Tests | googletest (vcpkg) | Tests unitaires core sans QApplication |
| HTTP | Qt6 Network (QNetworkAccessManager) | Natif Qt, async |
| Packaging | windeployqt (post-build) | Déploiement automatique des DLLs Qt |

---

## Structure du projet

```
EqQuarmCompanion/
├── CMakeLists.txt
├── vcpkg.json                  # manifest mode — nlohmann-json, gtest
├── CMakePresets.json           # presets windows-x64-debug / windows-x64-release
├── src/
│   ├── core/                   # C++ pur — zéro #include <Qt*>
│   │   ├── types.h             # Tous les structs partagés
│   │   ├── config.{h,cpp}      # nlohmann/json
│   │   ├── character_parser.{h,cpp}
│   │   ├── stats_calculator.{h,cpp}
│   │   ├── npc_analysis.{h,cpp}
│   │   ├── spell_stats.{h,cpp}
│   │   ├── spell_stacking.{h,cpp}
│   │   ├── derived_metrics.{h,cpp}
│   │   └── equipped_effects.{h,cpp}
│   ├── db/                     # Qt SQL + Qt Network — retourne des types de core/types.h
│   │   ├── db_connection.{h,cpp}
│   │   ├── item_database.{h,cpp}
│   │   ├── npc_database.{h,cpp}
│   │   └── bis_scraper.{h,cpp} # QNetworkAccessManager — interdit dans core/
│   ├── ui/                     # Qt6 Widgets uniquement
│   │   ├── main_window.{h,cpp}
│   │   ├── character_tab.{h,cpp}
│   │   ├── fight_tab.{h,cpp}
│   │   ├── spells_tab.{h,cpp}
│   │   ├── infos_tab.{h,cpp}
│   │   ├── infos_data.h        # données statiques spells (équiv. _SPELL_DATA Python)
│   │   ├── widgets.{h,cpp}
│   │   └── settings_dialog.{h,cpp}
│   └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_config.cpp
│   ├── test_stats_calculator.cpp
│   ├── test_npc_analysis.cpp
│   ├── test_spell_stats.cpp
│   └── test_spell_stacking.cpp
└── resources/
    └── config_defaults.json
```

**Règle de séparation stricte :**
- `core/` — aucun `#include <Qt*>`. Types `std::`, nlohmann/json. Testable avec GTest sans QApplication.
- `db/` — QSqlDatabase/QSqlQuery uniquement. Retourne des types `core/types.h`. IO non testé en unitaire.
- `ui/` — Qt6 exclusivement. Appelle `db/` et `core/`.

---

## CMakeLists.txt (structure)

```cmake
cmake_minimum_required(VERSION 3.25)
project(EqQuarmCompanion LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)
find_package(nlohmann_json REQUIRED)    # via vcpkg

add_library(eq_core STATIC
    src/core/config.cpp
    src/core/character_parser.cpp
    src/core/stats_calculator.cpp
    src/core/npc_analysis.cpp
    src/core/spell_stats.cpp
    src/core/spell_stacking.cpp
    src/core/derived_metrics.cpp
    src/core/equipped_effects.cpp
)
target_link_libraries(eq_core PRIVATE nlohmann_json::nlohmann_json)

add_library(eq_db STATIC
    src/db/db_connection.cpp
    src/db/item_database.cpp
    src/db/npc_database.cpp
    src/db/bis_scraper.cpp      # QNetworkAccessManager
)
target_link_libraries(eq_db PRIVATE Qt6::Sql Qt6::Network eq_core)

add_executable(EqQuarmCompanion WIN32 src/main.cpp <ui_sources>)
target_link_libraries(EqQuarmCompanion PRIVATE Qt6::Widgets Qt6::Network eq_core eq_db)

# Post-build: windeployqt
add_custom_command(TARGET EqQuarmCompanion POST_BUILD
    COMMAND windeployqt --no-translations $<TARGET_FILE:EqQuarmCompanion>
)

# Tests
enable_testing()
add_subdirectory(tests)
```

**`vcpkg.json` :**
```json
{
  "name": "eq-quarm-companion",
  "version": "1.0.0",
  "dependencies": [
    "nlohmann-json",
    "gtest"
  ]
}
```

---

## Types partagés — `core/types.h`

```cpp
#pragma once
#include <string>
#include <optional>
#include <vector>
#include <map>

struct CharacterInfo {
    std::string name, class_name;
    int level{};
};

struct NpcData {
    int id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    std::string name, special_abilities, zone_long_name;
};

struct StatRange { int base{}, capped{}; };

struct PlayerTotals {
    StatRange hp, mana;
    int atk{}, mitigation{};
    int str_v{}, sta{}, dex{}, agi{}, int_v{}, wis{}, cha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int haste{}, hp_regen{}, mana_regen{};
};

struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, mitigation_pct{}, disc_mult{1.f};
    int npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};

enum class Rating        { GOOD, MEDIUM, LOW };
enum class OffenseRating { EASY, MEDIUM, HARD };

struct ResistRating  { int value{}; float pct{}; Rating rating{}; };
struct ResistRatings { ResistRating mr, fr, cr, dr, pr; };

struct MeleeOffense  { int player_atk{}, npc_ac{}; OffenseRating rating{}; };
struct SpellOffense  { int npc_resist{}; OffenseRating rating{}; };
struct OffenseRatings {
    MeleeOffense melee;
    std::optional<SpellOffense> mr, fr, cr, dr, pr;
};

struct SpecialAbility { std::string tag; std::string severity; }; // "red"|"orange"|"grey"

struct SpellData {
    int id{}, resist_type{}, spell_type{}, recast_delay{};
    std::string name;
    // champs SPA (effect_base_value, effect_limit_value, effect_formula, etc.)
    std::array<int, 12> effect_base_value{};
    std::array<int, 12> effect_limit_value{};
    std::array<int, 12> effect_formula{};
    std::array<int, 12> spa{};
    int targettype{};
    // champs classes (classes1..classes16)
    std::array<int, 16> classes{};
};

struct ItemData {
    int id{}, hp{}, mana{}, ac{}, atk{};
    int astr{}, asta{}, adex{}, aagi{}, aint{}, awis{}, acha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int damage{}, delay{}, itemtype{};
    int haste{}, hp_regen{}, mana_regen{};
    int slots{}, reqlevel{};
    int worneffect{}, focuseffect{}, proceffect{};
    std::string name, lore;
    std::string worneffect_name, focuseffect_name, proceffect_name;
};

struct LootItem { int item_id{}, slots{}; float chance{}; std::string name; };
```

---

## Core — interfaces des modules

### `core/npc_analysis.h`

Traduction directe de `npc_analysis.py` :

```cpp
IncomingDamageResult incomingDamage(const NpcData&, const PlayerTotals&,
                                     std::string_view className, int level,
                                     std::string_view discipline);

ResistRatings    resistRatings(const NpcData&, const PlayerTotals&, int level);
OffenseRatings   offenseRatings(const NpcData&, const PlayerTotals&,
                                 std::string_view className);

std::optional<float> slowLandPct(const NpcData&, int playerLevel,
                                  int resistType, int resistDiff = 0);
std::optional<float> spellResistPct(const SpellData&, const PlayerTotals&,
                                     int playerLevel, int npcLevel);

std::vector<SpecialAbility> decodeSpecialAbilities(std::string_view raw);

std::string resistLabel(const SpellData&);
std::string effectiveSpellType(const SpellData&);
std::string targetTypeLabel(int targettype);
std::string formatSpellSummary(const SpellData&, int npcLevel);
```

### `core/stats_calculator.h`

```cpp
PlayerTotals calculateTotals(const CharacterInfo&,
                              const std::vector<ItemData>& equipped);
void applyWornStats(ItemData&, int level);  // équiv. apply_worn_stats()
```

### `core/spell_stats.h`

```cpp
float spellValue(const SpellData&, int level);
float spellIncomingDps(const std::vector<SpellData>& npcSpells,
                        const PlayerTotals&, int playerLevel, int npcLevel);
```

### `core/config.h`

```cpp
class Config {
public:
    explicit Config(std::filesystem::path configPath);
    std::string           get(std::string_view key) const;
    void                  set(std::string_view key, nlohmann::json value);
    void                  save() const;
    std::map<std::string,float> getClassWeights(std::string_view className) const;
    void setClassWeights(std::string_view className,
                          const std::map<std::string,float>&);
    std::string getCharacterClass(std::string_view charName) const;
    void        setCharacterClass(std::string_view charName,
                                   std::string_view className);
private:
    std::filesystem::path _configPath;
    nlohmann::json        _data;
};
```

### `core/character_parser.h`

```cpp
std::optional<CharacterInfo> parseCharacterFile(std::filesystem::path);
```

---

## Couche DB — `db/`

```cpp
// db/db_connection.h
class DbConnection {
public:
    static DbConnection& instance();
    bool connect(const QString& host, int port,
                 const QString& db, const QString& user, const QString& pass);
    bool isConnected() const;
};

// db/npc_database.h
class NpcDatabase : public QObject {
    Q_OBJECT
public:
    QList<NpcData>              searchNpcs(const QString& nameFragment);
    std::optional<NpcData>      getNpcById(int id);
    QList<SpellData>            getNpcSpells(int npcSpellsId);
    QList<LootItem>             getNpcLoot(int loottableId);
};

// db/item_database.h
class ItemDatabase : public QObject {
    Q_OBJECT
public:
    std::optional<ItemData>     getItemById(int id);
};
```

Tous les types de retour sont des types `core/types.h` — aucun `QVariant` ne traverse vers `core/`.

---

## UI — architecture des onglets

### Pattern uniforme

Chaque onglet reçoit à la construction :
```cpp
TabWidget(CharacterInfo* charInfo, PlayerTotals* playerTotals,
          Config* config, NpcDatabase* npcDb, ItemDatabase* itemDb,
          QWidget* parent = nullptr)
```

Et expose :
```cpp
void refresh();  // appelé par MainWindow quand le personnage change
```

### `ui/main_window.h`

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, QWidget* parent = nullptr);
private:
    QTabWidget*    _tabs;
    QComboBox*     _charSelector;
    CharacterTab*  _charTab;
    FightTab*      _fightTab;
    SpellsTab*     _spellsTab;
    InfosTab*      _infosTab;
    void onCharacterChanged(int index);
};
```

### `ui/fight_tab.h`

```cpp
class FightTab : public QWidget {
    Q_OBJECT
signals:
    void itemSelected(const QString& itemName);
private:
    QWidget* buildLeftPanel(const NpcData&);
    QWidget* buildRightPanel(const NpcData&);
    // Tableau 2D disciplines x slow
    QWidget* buildDpsSlowTable(const QList<DiscRow>&,
                                const QList<SlowScenario>&,
                                float spDps, int hp);
    void doSearch();                   // QtConcurrent::run → searchNpcs
    void onNpcSelected(int index);
};
```

**Async DB :** recherche NPC via `QtConcurrent::run()` + `QFutureWatcher` pour ne pas geler l'UI.

### `ui/infos_tab.h`

Données statiques — aucune DB, aucun réseau :
```cpp
// ui/infos_data.h — équivalent de _SPELL_DATA + _SPELL_ZONE_EXP Python
// Fonctions libres :
std::map<std::string,int> getResistDebuffs(std::string_view expansionName);
```

```cpp
class InfosTab : public QWidget {
    Q_OBJECT
    // QComboBox expansion → filtre les données statiques de infos_data.h
    // Sauvegarde "current_expansion" dans Config
};
```

---

## Tests

`tests/` contient des tests GTest sur `core/` uniquement (pas de Qt, pas de DB) :

```cpp
// test_npc_analysis.cpp
TEST(NpcAnalysis, IncomingDamageWarriorDefensive) {
    NpcData npc; npc.min_hit = 100; npc.max_hit = 200; /* … */
    PlayerTotals pt; pt.mitigation = 800;
    auto r = incomingDamage(npc, pt, "Warrior", 65, "defensive");
    EXPECT_NEAR(r.disc_mult, 0.3f, 0.01f);
}
```

---

## Packaging

Dossier `Release/EqQuarmCompanion/` généré automatiquement par `windeployqt` en post-build. Taille estimée ~60 Mo (vs 247 Mo pour le PyInstaller). Distribution : zip du dossier.

`config.json` est lu/écrit depuis `QCoreApplication::applicationDirPath()` (même logique que `sys.executable` dans le Python).

---

## Hors scope

- Support macOS/Linux
- Dark mode natif OS (les stylesheets Qt sont portées telles quelles)
- Mise à jour automatique
- Authentification DB autre que MySQL/MariaDB
