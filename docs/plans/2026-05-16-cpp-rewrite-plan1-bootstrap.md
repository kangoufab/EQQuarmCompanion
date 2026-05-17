# EqQuarmCompanion — Plan 1 : Bootstrap

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Créer le dépôt C++/Qt6 `EqQuarmCompanion`, le scaffolding CMake+vcpkg, le module `Config` avec tests GTest, et une fenêtre Qt vide qui compile et se lance.

**Architecture:** Core C++ pur (nlohmann/json, std::), pas de Qt dans `src/core/`. Couche DB et UI ajoutées dans les plans suivants. Le module Config est le seul composant core de ce plan.

**Tech Stack:** C++17, Qt 6.x Widgets, CMake 3.25+, vcpkg (nlohmann-json, gtest), MSVC 2022 x64.

**Référence Python :** `D:\Games\quarm\source\eq-item-evaluator` — consulter `core/config.py` pour la logique exacte.

---

### Task 1 : Initialisation du dépôt et structure des répertoires

**Files:**
- Create: `D:\Games\quarm\source\EqQuarmCompanion\` (nouveau dépôt git)
- Create: `.gitignore`
- Create: `src/core/` `src/db/` `src/ui/` `tests/` `resources/`

- [ ] **Step 1 : Créer le répertoire et initialiser git**

```powershell
New-Item -ItemType Directory -Path "D:\Games\quarm\source\EqQuarmCompanion"
cd "D:\Games\quarm\source\EqQuarmCompanion"
git init
git remote add origin https://github.com/kangoufab/EqQuarmCompanion.git
```

- [ ] **Step 2 : Créer `.gitignore`**

Fichier `D:\Games\quarm\source\EqQuarmCompanion\.gitignore` :
```
build/
out/
.cmake/
CMakeUserPresets.json
*.user
.vs/
*.suo
x64/
```

- [ ] **Step 3 : Créer l'arborescence des sources**

```powershell
foreach ($d in @("src\core","src\db","src\ui","tests","resources")) {
    New-Item -ItemType Directory -Path $d -Force
}
```

- [ ] **Step 4 : Copier `config_defaults.json`**

```powershell
Copy-Item "D:\Games\quarm\source\eq-item-evaluator\config_defaults.json" `
          "resources\config_defaults.json"
```

Contenu attendu dans `resources/config_defaults.json` (vérifier après copie) :
```json
{
  "eq_files_dir": "",
  "current_expansion": "Classic",
  "db": { "host": "localhost", "port": 3306, "user": "root",
          "password": "", "database": "quarm" },
  "characters": {},
  "class_weights": { "Warrior": {"ac":4,"hp":3,...}, ... }
}
```

- [ ] **Step 5 : Premier commit**

```powershell
git add .gitignore resources/config_defaults.json
git commit -m "chore: init repo + structure + config_defaults.json"
```

---

### Task 2 : vcpkg.json + CMakePresets.json

**Files:**
- Create: `vcpkg.json`
- Create: `CMakePresets.json`

- [ ] **Step 1 : Créer `vcpkg.json`**

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

- [ ] **Step 2 : Créer `CMakePresets.json`**

Adapter `CMAKE_PREFIX_PATH` au chemin Qt installé sur la machine (Qt installer → typiquement `C:/Qt/6.x.x/msvc2022_64`).

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows-x64-debug",
      "displayName": "Windows x64 Debug",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.9.0/msvc2022_64",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      }
    },
    {
      "name": "windows-x64-release",
      "displayName": "Windows x64 Release",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.9.0/msvc2022_64",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows"
      }
    }
  ],
  "buildPresets": [
    { "name": "windows-x64-debug",   "configurePreset": "windows-x64-debug"   },
    { "name": "windows-x64-release", "configurePreset": "windows-x64-release" }
  ],
  "testPresets": [
    { "name": "windows-x64-debug", "configurePreset": "windows-x64-debug",
      "output": { "outputOnFailure": true } }
  ]
}
```

- [ ] **Step 3 : Commit**

```powershell
git add vcpkg.json CMakePresets.json
git commit -m "chore: vcpkg manifest + CMake presets"
```

---

### Task 3 : CMakeLists.txt principal

**Files:**
- Create: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`

- [ ] **Step 1 : Créer `CMakeLists.txt` à la racine**

```cmake
cmake_minimum_required(VERSION 3.25)
project(EqQuarmCompanion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ── Packages ─────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Concurrent)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest        CONFIG REQUIRED)

# ── Core library (no Qt) ─────────────────────────────────────────────
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
target_include_directories(eq_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(eq_core PRIVATE nlohmann_json::nlohmann_json)

# ── DB library (Qt SQL + Network) ────────────────────────────────────
add_library(eq_db STATIC
    src/db/db_connection.cpp
    src/db/item_database.cpp
    src/db/npc_database.cpp
    src/db/bis_scraper.cpp
)
target_include_directories(eq_db PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(eq_db PUBLIC Qt6::Sql Qt6::Network eq_core)

# ── Executable ───────────────────────────────────────────────────────
add_executable(EqQuarmCompanion WIN32
    src/main.cpp
    src/ui/main_window.cpp
    src/ui/character_tab.cpp
    src/ui/fight_tab.cpp
    src/ui/spells_tab.cpp
    src/ui/infos_tab.cpp
    src/ui/widgets.cpp
    src/ui/settings_dialog.cpp
)
target_link_libraries(EqQuarmCompanion PRIVATE
    Qt6::Widgets Qt6::Concurrent eq_core eq_db
)

# Copy config_defaults.json next to exe
add_custom_command(TARGET EqQuarmCompanion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/config_defaults.json"
        "$<TARGET_FILE_DIR:EqQuarmCompanion>/config_defaults.json"
    COMMENT "Copying config_defaults.json"
)

# windeployqt (Release seulement)
find_program(WINDEPLOYQT windeployqt
    HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET EqQuarmCompanion POST_BUILD
        COMMAND "${WINDEPLOYQT}" --no-translations
            "$<TARGET_FILE:EqQuarmCompanion>"
        COMMENT "windeployqt"
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 2 : Créer `tests/CMakeLists.txt`**

```cmake
add_executable(eq_tests
    test_config.cpp
    test_npc_analysis.cpp
    test_stats_calculator.cpp
    test_spell_stats.cpp
    test_spell_stacking.cpp
)
target_link_libraries(eq_tests PRIVATE eq_core GTest::gtest_main)
target_include_directories(eq_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
include(GoogleTest)
gtest_discover_tests(eq_tests)
```

- [ ] **Step 3 : Créer les stubs vides pour que CMake compile**

Chaque fichier listé dans `add_library(eq_core ...)`, `add_library(eq_db ...)` et `add_executable(...)` doit exister. Créer des stubs vides :

```powershell
# core stubs
foreach ($f in @(
  "src/core/config.cpp","src/core/character_parser.cpp",
  "src/core/stats_calculator.cpp","src/core/npc_analysis.cpp",
  "src/core/spell_stats.cpp","src/core/spell_stacking.cpp",
  "src/core/derived_metrics.cpp","src/core/equipped_effects.cpp"
)) { New-Item -ItemType File -Path $f -Force }

# db stubs
foreach ($f in @(
  "src/db/db_connection.cpp","src/db/item_database.cpp",
  "src/db/npc_database.cpp","src/db/bis_scraper.cpp"
)) { New-Item -ItemType File -Path $f -Force }

# ui stubs
foreach ($f in @(
  "src/ui/main_window.cpp","src/ui/character_tab.cpp",
  "src/ui/fight_tab.cpp","src/ui/spells_tab.cpp",
  "src/ui/infos_tab.cpp","src/ui/widgets.cpp",
  "src/ui/settings_dialog.cpp"
)) { New-Item -ItemType File -Path $f -Force }

# test stubs
foreach ($f in @(
  "tests/test_config.cpp","tests/test_npc_analysis.cpp",
  "tests/test_stats_calculator.cpp","tests/test_spell_stats.cpp",
  "tests/test_spell_stacking.cpp"
)) { New-Item -ItemType File -Path $f -Force }
```

- [ ] **Step 4 : Commit**

```powershell
git add CMakeLists.txt tests/CMakeLists.txt src/ tests/
git commit -m "chore: CMakeLists + stubs vides pour tous les modules"
```

---

### Task 4 : `src/core/types.h` — tous les structs partagés

**Files:**
- Create: `src/core/types.h`

- [ ] **Step 1 : Écrire `src/core/types.h`**

```cpp
#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ── Personnage ────────────────────────────────────────────────────────

struct CharacterInfo {
    std::string name;
    std::string class_name;
    int         level{};
};

// ── NPC ───────────────────────────────────────────────────────────────

struct NpcData {
    int  id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int  min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int  mr{}, fr{}, cr{}, dr{}, pr{};
    int  npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    std::string name, special_abilities, zone_long_name;
};

// ── Stats joueur ──────────────────────────────────────────────────────

struct StatRange { int base{}, capped{}; };

struct PlayerTotals {
    StatRange hp, mana;
    int atk{}, mitigation{};
    int str_v{}, sta{}, dex{}, agi{}, int_v{}, wis{}, cha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int haste{}, hp_regen{}, mana_regen{};
};

// ── Résultats analyse combat ──────────────────────────────────────────

struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, mitigation_pct{}, disc_mult{1.f};
    int   npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};

enum class Rating        { GOOD, MEDIUM, LOW  };
enum class OffenseRating { EASY, MEDIUM, HARD };

struct ResistRating  { int value{}; float pct{}; Rating rating{}; };
struct ResistRatings { ResistRating mr, fr, cr, dr, pr; };

struct MeleeOffense  { int player_atk{}, npc_ac{}; OffenseRating rating{}; };
struct SpellOffense  { int npc_resist{};            OffenseRating rating{}; };
struct OffenseRatings {
    MeleeOffense melee;
    std::optional<SpellOffense> mr, fr, cr, dr, pr;
};

struct SpecialAbility { std::string tag, severity; }; // severity: "red"|"orange"|"grey"

// ── Sorts ─────────────────────────────────────────────────────────────

struct SpellData {
    int id{}, resist_type{}, spell_type{}, recast_delay{}, targettype{};
    std::string name;
    std::array<int, 12> effect_base_value{};
    std::array<int, 12> effect_limit_value{};
    std::array<int, 12> effect_formula{};
    std::array<int, 12> spa{};
    std::array<int, 16> classes{};
    int cast_time{};
    // Champs pour slow/debuff analysis
    int buffduration{}, buffdurationformula{};
};

// ── Items ─────────────────────────────────────────────────────────────

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

- [ ] **Step 2 : Commit**

```powershell
git add src/core/types.h
git commit -m "feat(core): types.h — tous les structs partagés"
```

---

### Task 5 : Module `Config` avec tests GTest

**Files:**
- Create: `src/core/config.h`
- Modify: `src/core/config.cpp`
- Modify: `tests/test_config.cpp`

La logique est identique à `D:\Games\quarm\source\eq-item-evaluator\core\config.py`.

- [ ] **Step 1 : Écrire le test en premier (`tests/test_config.cpp`)**

```cpp
#include <gtest/gtest.h>
#include "core/config.h"
#include <filesystem>
#include <fstream>

namespace {
    // Minimal defaults JSON for tests
    const char* kDefaults = R"({
        "eq_files_dir": "",
        "current_expansion": "Classic",
        "db": {"host":"localhost","port":3306,"user":"root","password":"","database":"quarm"},
        "characters": {},
        "class_weights": {}
    })";

    std::filesystem::path writeTempDefaults() {
        auto p = std::filesystem::temp_directory_path() / "eq_test_defaults.json";
        std::ofstream f(p); f << kDefaults;
        return p;
    }
}

TEST(Config, GetReturnsDefaultValue) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_nonexistent.json";
    Config c(cfg, defaults);
    EXPECT_EQ(c.get("current_expansion"), "Classic");
}

TEST(Config, SetAndSaveRoundtrip) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_save.json";
    {
        Config c(cfg, defaults);
        c.set("current_expansion", nlohmann::json("Luclin"));
        c.save();
    }
    {
        Config c(cfg, defaults);
        EXPECT_EQ(c.get("current_expansion"), "Luclin");
    }
    std::filesystem::remove(cfg);
}

TEST(Config, GetDbConfig) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_db.json";
    Config c(cfg, defaults);
    auto db = c.getDbConfig();
    EXPECT_EQ(db.host, "localhost");
    EXPECT_EQ(db.port, 3306);
    EXPECT_EQ(db.database, "quarm");
}

TEST(Config, GetClassWeights) {
    const char* withWeights = R"({
        "eq_files_dir":"","current_expansion":"Classic",
        "db":{"host":"localhost","port":3306,"user":"root","password":"","database":"quarm"},
        "characters":{},
        "class_weights":{"Warrior":{"ac":4,"hp":3}}
    })";
    auto p = std::filesystem::temp_directory_path() / "eq_test_defs_w.json";
    { std::ofstream f(p); f << withWeights; }
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_w.json";
    Config c(cfg, p);
    auto w = c.getClassWeights("Warrior");
    EXPECT_FLOAT_EQ(w.at("ac"), 4.f);
    EXPECT_FLOAT_EQ(w.at("hp"), 3.f);
    std::filesystem::remove(p);
}
```

- [ ] **Step 2 : Vérifier que le test échoue à la compilation (Config pas encore écrit)**

```powershell
cmake --preset windows-x64-debug
cmake --build build/debug --target eq_tests
```

Attendu : erreur `'Config': identifier not found` (normal).

- [ ] **Step 3 : Écrire `src/core/config.h`**

```cpp
#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

struct DbConfig {
    std::string host, user, password, database;
    int port{3306};
};

class Config {
public:
    Config(std::filesystem::path configPath,
           std::filesystem::path defaultsPath);

    [[nodiscard]] std::string get(std::string_view key) const;
    void set(std::string_view key, nlohmann::json value);
    void save() const;

    [[nodiscard]] DbConfig getDbConfig() const;
    [[nodiscard]] std::map<std::string, float> getClassWeights(
        std::string_view className) const;
    void setClassWeights(std::string_view className,
                         const std::map<std::string, float>&);
    [[nodiscard]] std::string getCharacterClass(std::string_view charName) const;
    void setCharacterClass(std::string_view charName, std::string_view className);

private:
    std::filesystem::path _configPath;
    nlohmann::json        _data;
};
```

- [ ] **Step 4 : Écrire `src/core/config.cpp`**

```cpp
#include "core/config.h"
#include <fstream>
#include <stdexcept>

Config::Config(std::filesystem::path configPath,
               std::filesystem::path defaultsPath)
    : _configPath(std::move(configPath))
{
    // Charger les defaults
    std::ifstream df(defaultsPath);
    if (!df.is_open())
        throw std::runtime_error("config_defaults.json introuvable : "
                                 + defaultsPath.string());
    _data = nlohmann::json::parse(df);

    // Merge config utilisateur par-dessus les defaults
    if (std::filesystem::exists(_configPath)) {
        try {
            std::ifstream cf(_configPath);
            auto user = nlohmann::json::parse(cf);
            _data.merge_patch(user);
        } catch (const std::exception&) {
            // config.json illisible → defaults utilisés
        }
    }
}

std::string Config::get(std::string_view key) const {
    auto it = _data.find(key);
    if (it == _data.end()) return {};
    return it->is_string() ? it->get<std::string>() : it->dump();
}

void Config::set(std::string_view key, nlohmann::json value) {
    _data[std::string(key)] = std::move(value);
}

void Config::save() const {
    std::ofstream f(_configPath);
    f << _data.dump(2);
}

DbConfig Config::getDbConfig() const {
    DbConfig d;
    if (auto it = _data.find("db"); it != _data.end()) {
        d.host     = it->value("host",     "localhost");
        d.port     = it->value("port",     3306);
        d.user     = it->value("user",     "root");
        d.password = it->value("password", "");
        d.database = it->value("database", "quarm");
    }
    return d;
}

std::map<std::string, float> Config::getClassWeights(
    std::string_view className) const
{
    std::map<std::string, float> weights;
    auto cw = _data.find("class_weights");
    if (cw == _data.end()) return weights;
    auto cls = cw->find(std::string(className));
    if (cls == cw->end()) return weights;
    for (auto& [k, v] : cls->items())
        weights[k] = v.get<float>();
    return weights;
}

void Config::setClassWeights(std::string_view className,
                              const std::map<std::string, float>& w) {
    auto& cw = _data["class_weights"][std::string(className)];
    for (auto& [k, v] : w) cw[k] = v;
}

std::string Config::getCharacterClass(std::string_view charName) const {
    try {
        return _data.at("characters").at(std::string(charName))
                    .at("class").get<std::string>();
    } catch (...) { return {}; }
}

void Config::setCharacterClass(std::string_view charName,
                                std::string_view className) {
    _data["characters"][std::string(charName)]["class"] = std::string(className);
}
```

- [ ] **Step 5 : Compiler et lancer les tests**

```powershell
cmake --preset windows-x64-debug
cmake --build build/debug --target eq_tests
cd build/debug
ctest -C Debug --output-on-failure
```

Attendu :
```
[==========] Running 4 tests from 1 test suite.
[----------] 4 tests from Config
[ RUN      ] Config.GetReturnsDefaultValue
[       OK ] Config.GetReturnsDefaultValue
[ RUN      ] Config.SetAndSaveRoundtrip
[       OK ] Config.SetAndSaveRoundtrip
[ RUN      ] Config.GetDbConfig
[       OK ] Config.GetDbConfig
[ RUN      ] Config.GetClassWeights
[       OK ] Config.GetClassWeights
[==========] 4 tests passed.
```

- [ ] **Step 6 : Commit**

```powershell
git add src/core/config.h src/core/config.cpp tests/test_config.cpp
git commit -m "feat(core): Config — nlohmann/json, load/save, getDbConfig, getClassWeights"
```

---

### Task 6 : `src/main.cpp` — fenêtre Qt vide

**Files:**
- Create: `src/main.cpp`
- Create: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`

- [ ] **Step 1 : Écrire `src/ui/main_window.h` (stub)**

```cpp
#pragma once
#include <QMainWindow>

class Config;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, QWidget* parent = nullptr);
};
```

- [ ] **Step 2 : Écrire `src/ui/main_window.cpp` (stub)**

```cpp
#include "ui/main_window.h"
#include "core/config.h"
#include <QLabel>

MainWindow::MainWindow(Config* config, QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("EQ Quarm Companion");
    resize(1280, 800);
    auto* lbl = new QLabel("EQ Quarm Companion — chargement en cours…", this);
    lbl->setAlignment(Qt::AlignCenter);
    setCentralWidget(lbl);
}
```

- [ ] **Step 3 : Écrire `src/main.cpp`**

```cpp
#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <filesystem>
#include "core/config.h"
#include "ui/main_window.h"

static QSplashScreen* makeSplash() {
    QPixmap pix(420, 180);
    pix.fill(QColor("#1a1a2e"));
    QPainter p(&pix);
    p.setPen(QColor("#64b5f6"));
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(pix.rect().adjusted(0, -30, 0, 0),
               Qt::AlignCenter, "EQ Quarm Companion");
    p.setPen(QColor("#888888"));
    p.setFont(QFont("Arial", 10));
    p.drawText(pix.rect().adjusted(0, 50, 0, 0),
               Qt::AlignCenter, "Chargement en cours…");
    p.end();
    auto* s = new QSplashScreen(pix);
    s->setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    return s;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EQ Quarm Companion");

    auto* splash = makeSplash();
    splash->show();
    app.processEvents();

    auto exeDir   = std::filesystem::path(
                        QCoreApplication::applicationDirPath().toStdWString());
    auto cfgPath  = exeDir / "config.json";
    auto defsPath = exeDir / "config_defaults.json";

    Config config(cfgPath, defsPath);

    MainWindow window(&config);
    window.show();
    splash->finish(&window);
    delete splash;

    return app.exec();
}
```

- [ ] **Step 4 : Ajouter les stubs de headers manquants pour les UI Qt (requis par `CMAKE_AUTOMOC`)**

Créer un header stub pour chaque classe UI listée dans CMakeLists.txt :

`src/ui/character_tab.h` :
```cpp
#pragma once
#include <QWidget>
class CharacterTab : public QWidget {
    Q_OBJECT
public: explicit CharacterTab(QWidget* p=nullptr):QWidget(p){}
};
```

`src/ui/fight_tab.h` :
```cpp
#pragma once
#include <QWidget>
class FightTab : public QWidget {
    Q_OBJECT
public: explicit FightTab(QWidget* p=nullptr):QWidget(p){}
};
```

`src/ui/spells_tab.h` :
```cpp
#pragma once
#include <QWidget>
class SpellsTab : public QWidget {
    Q_OBJECT
public: explicit SpellsTab(QWidget* p=nullptr):QWidget(p){}
};
```

`src/ui/infos_tab.h` :
```cpp
#pragma once
#include <QWidget>
class InfosTab : public QWidget {
    Q_OBJECT
public: explicit InfosTab(QWidget* p=nullptr):QWidget(p){}
};
```

`src/ui/widgets.h` :
```cpp
#pragma once
#include <QComboBox>
class SearchComboBox : public QComboBox {
    Q_OBJECT
public: explicit SearchComboBox(QWidget* p=nullptr):QComboBox(p){}
signals: void popup_requested();
};
```

`src/ui/settings_dialog.h` :
```cpp
#pragma once
#include <QDialog>
class Config;
class SettingsDialog : public QDialog {
    Q_OBJECT
public: explicit SettingsDialog(Config*, QWidget* p=nullptr):QDialog(p){}
};
```

- [ ] **Step 5 : Compiler l'exécutable**

```powershell
cmake --preset windows-x64-debug
cmake --build build/debug --target EqQuarmCompanion
```

Attendu : compilation sans erreur, `build/debug/bin/EqQuarmCompanion.exe` créé.

- [ ] **Step 6 : Lancer l'exécutable et vérifier la fenêtre**

```powershell
.\build\debug\bin\EqQuarmCompanion.exe
```

Attendu : splash screen bleu foncé s'affiche 1-2s, puis fenêtre 1280×800 "EQ Quarm Companion" avec label centré. Pas de crash.

- [ ] **Step 7 : Commit**

```powershell
git add src/main.cpp src/ui/main_window.h src/ui/main_window.cpp
git add src/ui/character_tab.h src/ui/fight_tab.h src/ui/spells_tab.h
git add src/ui/infos_tab.h src/ui/widgets.h src/ui/settings_dialog.h
git commit -m "feat: fenetre Qt vide compilable avec splash screen"
```

---

### Vérification finale du Plan 1

- [ ] `cmake --preset windows-x64-debug && cmake --build build/debug` → succès sans warning
- [ ] `ctest --preset windows-x64-debug` → 4 tests Config passent
- [ ] `.\build\debug\bin\EqQuarmCompanion.exe` → fenêtre s'ouvre, pas de crash
- [ ] `git log --oneline` → 5 commits propres

**Prêt pour Plan 2 (modules `core/`) quand les 4 vérifications passent.**
