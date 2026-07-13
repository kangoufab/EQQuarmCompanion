# BDD embarquée SQLite — remplacement de MySQL externe — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remplacer la dépendance MySQL/MariaDB externe d'EqQuarmCompanion par un fichier SQLite embarqué (`quarm_data.db`) livré à côté de l'exécutable, régénérable depuis la BDD Quarm source via un outil dédié.

**Architecture:** `DbConnection` ouvre un fichier SQLite local (driver `QSQLITE`, natif à Qt) au lieu d'un serveur MySQL distant. Un nouvel exécutable `db_export` (dev-only) copie les tables nécessaires depuis MySQL vers ce fichier SQLite. Toute la configuration MySQL (host/port/user/password) disparaît de l'app livrée.

**Tech Stack:** Qt6 Sql (`QSQLITE` + `QMYSQL` côté outil d'export uniquement), CMake, GTest.

**Spec de référence :** `docs/specs/2026-07-13-embedded-sqlite-db-design.md`

## Global Constraints

- Suppression complète de MySQL côté app livrée : pas de mode "connexion externe" conservé. `DbConfig`, l'onglet "Connexion DB" des Settings, et la clé `"db"` de `config.json`/`config_defaults.json` disparaissent entièrement.
- Aucune dégradation de performance par rapport à l'usage actuel (MySQL sur `localhost`) : les colonnes utilisées en jointure/filtre doivent rester indexées côté SQLite.
- L'outil de régénération est un exécutable C++ dans le repo, configuré uniquement par arguments CLI (pas de fichier de config dev-only).
- **`tools/` est dans `.gitignore`** (dossier local contenant le plugin QMYSQL compilé, `tools/mysql_plugin/`) — le nouvel outil d'export ne doit **pas** vivre sous `tools/`, sinon son code source ne sera jamais commité. Il vit à la racine du repo dans `db_export/`, au même niveau que `installer/`.
- Tables copiées telles quelles (toutes colonnes, introspection automatique) : `items`, `spells_new`, `npc_types`, `races`, `spawnentry`, `spawn2`, `zone`, `npc_spells_entries`, `loottable_entries`, `lootdrop_entries`, `altadv_vars`, `aa_effects`.
- Suivre les conventions du projet : commandes PowerShell dans un `.ps1` temporaire (cf. CLAUDE.md), commits fréquents, un message par tâche.

---

### Task 1: Suppression de la config MySQL côté app (config, DbConnection, Settings, main, MainWindow)

**Files:**
- Modify: `src/core/config.h`
- Modify: `src/core/config.cpp`
- Modify: `resources/config_defaults.json`
- Modify: `tests/test_config.cpp`
- Modify: `src/db/db_connection.h`
- Modify: `src/db/db_connection.cpp`
- Modify: `src/ui/settings_dialog.h`
- Modify: `src/ui/settings_dialog.cpp`
- Modify: `src/main.cpp`
- Modify: `src/ui/main_window.cpp`

**Interfaces:**
- Produces: `DbConnection::connect(const QString& sqliteFilePath)` — remplace `connect(const DbConfig&)`. Utilisé par Task 5 (aucun autre changement de signature nécessaire ensuite).
- Produces: `Config` n'expose plus `DbConfig`/`getDbConfig()` — toute réintroduction future d'une config DB devra passer par une nouvelle clé JSON dédiée si jamais nécessaire (hors périmètre ici).

- [ ] **Step 1: `src/core/config.h` — retirer `DbConfig` et `getDbConfig()`**

Remplacer tout le contenu du fichier par :

```cpp
#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

class Config {
public:
    Config(std::filesystem::path configPath,
           std::filesystem::path defaultsPath);

    [[nodiscard]] std::string      get(std::string_view key) const;
    [[nodiscard]] nlohmann::json   getJson(std::string_view key) const;
    void set(std::string_view key, nlohmann::json value);
    void save() const;

    [[nodiscard]] std::map<std::string, float> getClassWeights(
        std::string_view className) const;
    void setClassWeights(std::string_view className,
                         const std::map<std::string, float>&);
    [[nodiscard]] std::string    getCharacterClass(std::string_view charName) const;
    void setCharacterClass(std::string_view charName, std::string_view className);
    [[nodiscard]] nlohmann::json getCharacterJson(std::string_view charName,
                                                  std::string_view key) const;
    void setCharacterJson(std::string_view charName, std::string_view key,
                          nlohmann::json value);

private:
    std::filesystem::path _configPath;
    nlohmann::json        _data;
};
```

- [ ] **Step 2: `src/core/config.cpp` — retirer `Config::getDbConfig()`**

Supprimer entièrement ce bloc (la fonction complète, lignes 49-59 dans le fichier actuel) :

```cpp
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

```

Le reste du fichier (`get`, `getJson`, `set`, `save`, `getClassWeights`, etc.) reste inchangé.

- [ ] **Step 3: `resources/config_defaults.json` — retirer la clé `"db"`**

Remplacer le contenu par :

```json
{
  "eq_files_dir": "",
  "current_expansion": "Luclin",
  "characters": {},
  "class_weights": {
    "Warrior":     {"ac": 4, "hp": 3, "damage": 3, "delay": -2, "astr": 2, "asta": 3, "aagi": 1, "adex": 1, "awis": 0, "aint": 0, "acha": 0, "mana": 0},
    "Cleric":      {"ac": 2, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 5, "aint": 0, "acha": 0, "mana": 4},
    "Paladin":     {"ac": 4, "hp": 3, "damage": 2, "delay": -1, "astr": 2, "asta": 2, "aagi": 1, "adex": 1, "awis": 3, "aint": 0, "acha": 1, "mana": 2},
    "Ranger":      {"ac": 2, "hp": 2, "damage": 3, "delay": -2, "astr": 3, "asta": 2, "aagi": 3, "adex": 3, "awis": 2, "aint": 0, "acha": 0, "mana": 1},
    "Shadowknight":{"ac": 4, "hp": 3, "damage": 3, "delay": -2, "astr": 2, "asta": 2, "aagi": 1, "adex": 1, "awis": 0, "aint": 2, "acha": 0, "mana": 2},
    "Druid":       {"ac": 1, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 5, "aint": 0, "acha": 0, "mana": 4},
    "Monk":        {"ac": 2, "hp": 3, "damage": 3, "delay": -2, "astr": 3, "asta": 3, "aagi": 4, "adex": 3, "awis": 2, "aint": 0, "acha": 0, "mana": 0},
    "Bard":        {"ac": 2, "hp": 2, "damage": 2, "delay": -1, "astr": 2, "asta": 2, "aagi": 2, "adex": 4, "awis": 0, "aint": 0, "acha": 3, "mana": 0},
    "Rogue":       {"ac": 1, "hp": 2, "damage": 4, "delay": -2, "astr": 3, "asta": 2, "aagi": 3, "adex": 4, "awis": 0, "aint": 0, "acha": 0, "mana": 0},
    "Shaman":      {"ac": 2, "hp": 2, "damage": 1, "delay": -1, "astr": 1, "asta": 2, "aagi": 0, "adex": 0, "awis": 5, "aint": 0, "acha": 0, "mana": 4},
    "Necromancer": {"ac": 1, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 0, "aint": 5, "acha": 0, "mana": 5},
    "Wizard":      {"ac": 1, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 0, "aint": 5, "acha": 0, "mana": 5},
    "Magician":    {"ac": 1, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 0, "aint": 5, "acha": 0, "mana": 5},
    "Enchanter":   {"ac": 1, "hp": 2, "damage": 0, "delay":  0, "astr": 0, "asta": 1, "aagi": 0, "adex": 0, "awis": 0, "aint": 4, "acha": 2, "mana": 5},
    "Berserker":   {"ac": 2, "hp": 3, "damage": 4, "delay": -2, "astr": 3, "asta": 2, "aagi": 2, "adex": 3, "awis": 0, "aint": 0, "acha": 0, "mana": 0}
  }
}
```

- [ ] **Step 4: `tests/test_config.cpp` — retirer les références à `"db"` et le test `GetDbConfig`**

Remplacer tout le contenu par :

```cpp
#include <gtest/gtest.h>
#include "core/config.h"
#include <filesystem>
#include <fstream>

namespace {
    const char* kDefaults = R"({
        "eq_files_dir": "",
        "current_expansion": "Classic",
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
    std::filesystem::remove(defaults);
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
    std::filesystem::remove(defaults);
}

TEST(Config, GetClassWeights) {
    const char* withWeights = R"({
        "eq_files_dir":"","current_expansion":"Classic",
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

TEST(Config, CorruptConfigFallsBackToDefaults) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_corrupt.json";
    // Écrire un JSON invalide dans config.json
    { std::ofstream f(cfg); f << "{ invalid json !!!"; }
    // Le constructeur doit utiliser les defaults silencieusement (pas de throw)
    Config c(cfg, defaults);
    EXPECT_EQ(c.get("current_expansion"), "Classic");
    std::filesystem::remove(cfg);
    std::filesystem::remove(defaults);
}

// Les tests CharacterParser vivent désormais dans test_character_parser.cpp.
```

(Ce fichier passe de 5 à 4 `TEST(...)` — le suite `config` compte donc un test de moins.)

- [ ] **Step 5: `src/db/db_connection.h` — nouvelle signature `connect(QString)`**

Remplacer tout le contenu par :

```cpp
#pragma once
#include <QString>
#include <QtSql/QSqlDatabase>

class DbConnection {
public:
    static DbConnection& instance();

    // Ouvre le fichier SQLite embarqué (quarm_data.db) en lecture seule.
    bool connect(const QString& sqliteFilePath);
    void disconnect();

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QSqlDatabase& db();

private:
    DbConnection() = default;
    QSqlDatabase _db;
};
```

- [ ] **Step 6: `src/db/db_connection.cpp` — driver `QSQLITE`, retrait de `testConnection`**

Remplacer tout le contenu par :

```cpp
#include "db/db_connection.h"
#include <QtSql/QSqlError>
#include <QDebug>

DbConnection& DbConnection::instance() {
    static DbConnection inst;
    return inst;
}

bool DbConnection::connect(const QString& sqliteFilePath) {
    if (_db.isOpen()) _db.close();
    if (QSqlDatabase::contains("main"))
        QSqlDatabase::removeDatabase("main");

    _db = QSqlDatabase::addDatabase("QSQLITE", "main");
    _db.setDatabaseName(sqliteFilePath);
    _db.setConnectOptions("QSQLITE_OPEN_READONLY");

    if (!_db.open()) {
        qWarning() << "DB connection failed:" << _db.lastError().text();
        return false;
    }
    return true;
}

void DbConnection::disconnect() {
    if (_db.isOpen()) _db.close();
    QSqlDatabase::removeDatabase("main");
}

bool DbConnection::isConnected() const { return _db.isOpen(); }
QSqlDatabase& DbConnection::db() { return _db; }
```

- [ ] **Step 7: `src/ui/settings_dialog.h` — retirer l'onglet DB**

Remplacer tout le contenu par :

```cpp
#pragma once
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSlider>
class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccepted();

private:
    QWidget* buildFilesTab();
    QWidget* buildWeightsTab();
    QWidget* buildClassWeightsWidget(const QString& className);

    Config*    _config;

    // Onglet Fichiers
    QLineEdit* _eqFilesDir;
    QComboBox* _expansion;

    // Onglet Poids de stats : className → statName → slider
    QMap<QString, QMap<QString, QSlider*>> _weightSliders;
};
```

- [ ] **Step 8: `src/ui/settings_dialog.cpp` — retirer `buildDbTab`, `onTestConnection`, et la clé `"db"`**

Remplacer tout le contenu par :

```cpp
#include "ui/settings_dialog.h"
#include "core/config.h"
#include <nlohmann/json.hpp>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

static const QStringList EXPANSIONS = {
    "Classic", "Kunark", "Velious", "Luclin", "Planes of Power"
};
static const QStringList EQ_CLASSES = {
    "Warrior","Cleric","Paladin","Ranger","Shadowknight",
    "Druid","Monk","Bard","Rogue","Shaman",
    "Necromancer","Wizard","Magician","Enchanter","Beastlord",
};
static const QStringList SCORED_STATS = {
    "ac","hp","mana","damage","delay",
    "astr","asta","aagi","adex","awis","aint","acha"
};

SettingsDialog::SettingsDialog(Config* cfg, QWidget* parent)
    : QDialog(parent), _config(cfg)
{
    setWindowTitle(QString::fromUtf8("Paramètres"));
    setMinimumWidth(520);

    auto* tabs = new QTabWidget;
    tabs->addTab(buildFilesTab(),   "Fichiers EQ");
    tabs->addTab(buildWeightsTab(), "Poids de stats");

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* vl = new QVBoxLayout(this);
    vl->addWidget(tabs);
    vl->addWidget(btns);
}

// ── Onglet Fichiers EQ ────────────────────────────────────────────────────

QWidget* SettingsDialog::buildFilesTab()
{
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    auto* form = new QFormLayout;

    _eqFilesDir = new QLineEdit(QString::fromStdString(_config->get("eq_files_dir")));
    auto* browseBtn = new QPushButton(QString::fromUtf8("Parcourir..."));
    connect(browseBtn, &QPushButton::clicked, [this] {
        auto dir = QFileDialog::getExistingDirectory(
            this, QString::fromUtf8("Répertoire des fichiers EQ"));
        if (!dir.isEmpty()) _eqFilesDir->setText(dir);
    });
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(_eqFilesDir);
    dirRow->addWidget(browseBtn);
    auto* dirW = new QWidget; dirW->setLayout(dirRow);
    form->addRow(QString::fromUtf8("Répertoire EQ :"), dirW);

    _expansion = new QComboBox;
    _expansion->addItems(EXPANSIONS);
    QString curExp = QString::fromStdString(_config->get("current_expansion"));
    int idx = _expansion->findText(curExp);
    if (idx >= 0) _expansion->setCurrentIndex(idx);
    form->addRow(QString::fromUtf8("Extension courante :"), _expansion);

    layout->addLayout(form);
    layout->addStretch();
    return w;
}

// ── Onglet Poids de stats ─────────────────────────────────────────────────

QWidget* SettingsDialog::buildWeightsTab()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);

    auto* classTabs = new QTabWidget;
    for (const auto& cls : EQ_CLASSES)
        classTabs->addTab(buildClassWeightsWidget(cls), cls);

    layout->addWidget(classTabs);
    scroll->setWidget(container);
    return scroll;
}

QWidget* SettingsDialog::buildClassWeightsWidget(const QString& className)
{
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);

    auto weights = _config->getClassWeights(className.toStdString());
    _weightSliders[className] = {};

    for (const auto& stat : SCORED_STATS) {
        auto* row = new QHBoxLayout;

        auto* lbl = new QLabel(stat);
        lbl->setFixedWidth(60);

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(-5, 5);
        float wval = weights.count(stat.toStdString())
                     ? weights.at(stat.toStdString()) : 0.f;
        slider->setValue(static_cast<int>(wval));

        auto* valLbl = new QLabel(QString::number(slider->value()));
        valLbl->setFixedWidth(25);
        connect(slider, &QSlider::valueChanged,
                [valLbl](int v){ valLbl->setText(QString::number(v)); });

        row->addWidget(lbl);
        row->addWidget(slider);
        row->addWidget(valLbl);
        layout->addLayout(row);

        _weightSliders[className][stat] = slider;
    }
    layout->addStretch();
    return w;
}

// ── Slots ─────────────────────────────────────────────────────────────────

void SettingsDialog::onAccepted()
{
    _config->set("eq_files_dir",
                 nlohmann::json(_eqFilesDir->text().toStdString()));
    _config->set("current_expansion",
                 nlohmann::json(_expansion->currentText().toStdString()));

    for (const auto& cls : EQ_CLASSES) {
        std::map<std::string, float> w;
        auto clsIt = _weightSliders.find(cls);
        if (clsIt != _weightSliders.end()) {
            for (const auto& stat : SCORED_STATS) {
                auto slIt = clsIt->find(stat);
                if (slIt != clsIt->end())
                    w[stat.toStdString()] = static_cast<float>(slIt.value()->value());
            }
        }
        _config->setClassWeights(cls.toStdString(), w);
    }

    _config->save();
    accept();
}
```

- [ ] **Step 9: `src/main.cpp` — connexion directe au fichier SQLite embarqué**

Remplacer ce bloc (lignes 96-100 du fichier actuel) :

```cpp
    // Connexion DB (non bloquant si échec — certaines fonctions désactivées)
    auto dbCfg = config.getDbConfig();
    bool dbOk  = DbConnection::instance().connect(dbCfg);
    if (!dbOk)
        qWarning() << "DB non connectée — vérifier les paramètres dans config.json";
```

par :

```cpp
    // Connexion DB embarquée (SQLite bundlé à côté de l'exe — non bloquant si échec)
    auto dbPath = QString::fromStdWString((exeDir / "quarm_data.db").wstring());
    bool dbOk   = DbConnection::instance().connect(dbPath);
    if (!dbOk)
        qWarning() << "DB non connectée — quarm_data.db introuvable à côté de l'exécutable";
```

- [ ] **Step 10: `src/ui/main_window.cpp` — reconnexion basée sur le chemin fixe, pas sur `DbConfig`**

Remplacer ce bloc de `checkDbStatus()` :

```cpp
    if (!alive) {
        auto dbCfg = _config->getDbConfig();
        alive = DbConnection::instance().connect(dbCfg);
        if (alive) loadCharacterFiles();
    }
```

par :

```cpp
    if (!alive) {
        auto dbPath = QCoreApplication::applicationDirPath() + "/quarm_data.db";
        alive = DbConnection::instance().connect(dbPath);
        if (alive) loadCharacterFiles();
    }
```

Ajouter `#include <QCoreApplication>` dans la liste d'includes en haut du fichier (à côté de `#include <QFile>`).

Remplacer aussi le tooltip obsolète dans `updateDbBadge()` :

```cpp
        _dbBadge->setToolTip(QString::fromUtf8(
            "Base de données non connectée\nV\xc3\xa9rifier les param\xc3\xa8tres dans \xe2\x9a\x99"));
```

par :

```cpp
        _dbBadge->setToolTip(QString::fromUtf8(
            "Base de donn\xc3\xa9es non connect\xc3\xa9e\nquarm_data.db introuvable \xc3\xa0 c\xc3\xb4t\xc3\xa9 de l'ex\xc3\xa9cutable"));
```

- [ ] **Step 11: Build + tests**

Run:
```
cmake --preset windows-x64-debug
cmake --build build/debug
```
Expected: build réussi, 0 erreur (le binaire ne pourra pas encore ouvrir de DB — `quarm_data.db` n'existe pas encore, c'est attendu à ce stade).

Run:
```
cd build/debug
ctest --output-on-failure
```
Expected: tous les tests passent (35 tests — un de moins qu'avant, `Config.GetDbConfig` a été retiré).

- [ ] **Step 12: Commit**

```bash
git add src/core/config.h src/core/config.cpp resources/config_defaults.json \
        tests/test_config.cpp src/db/db_connection.h src/db/db_connection.cpp \
        src/ui/settings_dialog.h src/ui/settings_dialog.cpp src/main.cpp src/ui/main_window.cpp
git commit -m "refactor(db): remplace la connexion MySQL par une connexion SQLite au fichier embarqué"
```

---

### Task 2: Adapter le test d'intégration NPC au fixture SQLite

**Files:**
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_integration_npc.cpp`

**Interfaces:**
- Consumes: `DbConnection::connect(const QString&)` (Task 1).
- Produces: rien de nouveau exposé — ce test devient un test de non-régression sur `resources/quarm_data.db`, exploitable une fois ce fichier généré (Task 5).

- [ ] **Step 1: `tests/CMakeLists.txt` — passer le chemin source au binaire de test**

Remplacer le bloc `if(BUILD_INTEGRATION_TESTS)` par :

```cmake
option(BUILD_INTEGRATION_TESTS "Build DB integration tests (requires resources/quarm_data.db, cf. db_export/)" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(eq_integration_tests
        test_integration_npc.cpp)
    target_link_libraries(eq_integration_tests PRIVATE
        eq_core eq_db GTest::gtest
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Concurrent)
    target_include_directories(eq_integration_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src")
    target_compile_definitions(eq_integration_tests PRIVATE
        EQ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # Pas de gtest_discover_tests → n'apparaît pas dans ctest standard
endif()
```

- [ ] **Step 2: `tests/test_integration_npc.cpp` — ouvrir `resources/quarm_data.db` au lieu de MySQL**

Ajouter `#include <filesystem>` dans les includes en haut du fichier (à côté de `#include <QVariant>`).

Remplacer la classe `NpcIntegrationTest` :

```cpp
class NpcIntegrationTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        DbConfig cfg;
        cfg.host     = "localhost";
        cfg.port     = 3306;
        cfg.user     = "root";
        cfg.password = "rooteq";
        cfg.database = "quarm";
        s_dbReady = DbConnection::instance().connect(cfg);
        if (s_dbReady) s_npcDb = new NpcDatabase();
    }
    static void TearDownTestSuite() {
        delete s_npcDb;
        s_npcDb = nullptr;
    }

protected:
    void SetUp() override {
        if (!s_dbReady)
            GTEST_SKIP() << "Quarm DB non accessible (localhost:3306 quarm)";
    }
};
```

par :

```cpp
class NpcIntegrationTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        auto dbPath = std::filesystem::path(EQ_SOURCE_DIR) / "resources" / "quarm_data.db";
        s_dbReady = std::filesystem::exists(dbPath)
                    && DbConnection::instance().connect(
                           QString::fromStdWString(dbPath.wstring()));
        if (s_dbReady) s_npcDb = new NpcDatabase();
    }
    static void TearDownTestSuite() {
        delete s_npcDb;
        s_npcDb = nullptr;
    }

protected:
    void SetUp() override {
        if (!s_dbReady)
            GTEST_SKIP() << "resources/quarm_data.db introuvable ou illisible";
    }
};
```

- [ ] **Step 3: Vérifier que la cible compile (le fixture n'existe pas encore → tests skip, c'est attendu)**

Run:
```
cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=ON
cmake --build build/debug --target eq_integration_tests
```
Expected: build réussi, 0 erreur (le type `DbConfig` a disparu de `core/config.h` depuis Task 1 — ce fichier ne doit plus le référencer).

Run:
```
build\debug\bin\eq_integration_tests.exe
```
Expected: tous les cas affichent `SKIPPED` avec le message `resources/quarm_data.db introuvable ou illisible` (le fichier sera généré en Task 5).

Reconfigurer ensuite sans le flag pour revenir à l'état par défaut :
```
cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=OFF
```

- [ ] **Step 4: Commit**

```bash
git add tests/CMakeLists.txt tests/test_integration_npc.cpp
git commit -m "test(integration): pointe le test NPC sur le fixture SQLite plutôt qu'un MySQL live"
```

---

### Task 3: Nettoyage dialecte SQL et suppression du code mort FULLTEXT

**Files:**
- Modify: `src/db/item_database.cpp`
- Modify: `src/db/npc_database.cpp`

**Interfaces:**
- Consumes: rien de nouveau (requêtes SQL pures, mêmes signatures publiques `ItemDatabase`/`NpcDatabase`).
- Produces: rien de nouveau — comportement de recherche inchangé (`LIKE` full-scan), juste portable SQLite.

- [ ] **Step 1: `src/db/item_database.cpp` — retirer `hasItemsFulltextIndex` (code mort)**

Supprimer entièrement ce bloc, situé juste après `getItemsByIds` et avant `searchItems` :

```cpp
// Détecte une fois si l'index FULLTEXT existe (docs/sql_migrations/add_fulltext_indexes.sql).
// FULLTEXT passe de O(n) full-scan à O(log n) sur la colonne Name.
static bool s_itemsFulltext = false;
static bool s_itemsFulltextChecked = false;

static bool hasItemsFulltextIndex(const QSqlDatabase& db) {
    if (s_itemsFulltextChecked) return s_itemsFulltext;
    QSqlQuery chk(db);
    chk.exec(
        "SELECT 1 FROM information_schema.STATISTICS"
        " WHERE table_schema = DATABASE()"
        "   AND table_name   = 'items'"
        "   AND index_name   = 'ft_name' LIMIT 1"
    );
    s_itemsFulltext = chk.next();
    if (!s_itemsFulltext) {
        QSqlQuery create(db);
        s_itemsFulltext = create.exec("ALTER TABLE items ADD FULLTEXT INDEX ft_name (Name)");
        if (s_itemsFulltext)
            qInfo() << "[ItemDatabase] FULLTEXT index créé sur items.Name";
#ifdef QT_DEBUG
        else
            qDebug() << "[ItemDatabase] FULLTEXT index indisponible:" << create.lastError().text();
#endif
    }
    s_itemsFulltextChecked = true;
    return s_itemsFulltext;
}

```

- [ ] **Step 2: `src/db/item_database.cpp` — `CHAR_LENGTH` → `LENGTH` dans `searchItems`**

Remplacer :

```cpp
        QString("SELECT %1 FROM %2 WHERE i.Name LIKE :name%3"
                " ORDER BY CHAR_LENGTH(i.Name) ASC LIMIT :lim")
```

par :

```cpp
        QString("SELECT %1 FROM %2 WHERE i.Name LIKE :name%3"
                " ORDER BY LENGTH(i.Name) ASC LIMIT :lim")
```

- [ ] **Step 3: `src/db/npc_database.cpp` — retirer `hasNpcsFulltextIndex` (code mort)**

Supprimer entièrement ce bloc, en tête de fichier après `NpcDatabase::NpcDatabase` :

```cpp
static bool s_npcsFulltext = false;
static bool s_npcsFulltextChecked = false;

static bool hasNpcsFulltextIndex(const QSqlDatabase& db) {
    if (s_npcsFulltextChecked) return s_npcsFulltext;
    QSqlQuery chk(db);
    chk.exec(
        "SELECT 1 FROM information_schema.STATISTICS"
        " WHERE table_schema = DATABASE()"
        "   AND table_name   = 'npc_types'"
        "   AND index_name   = 'ft_name' LIMIT 1"
    );
    s_npcsFulltext = chk.next();
    if (!s_npcsFulltext) {
        QSqlQuery create(db);
        s_npcsFulltext = create.exec("ALTER TABLE npc_types ADD FULLTEXT INDEX ft_name (name)");
        if (s_npcsFulltext)
            qInfo() << "[NpcDatabase] FULLTEXT index créé sur npc_types.name";
#ifdef QT_DEBUG
        else
            qDebug() << "[NpcDatabase] FULLTEXT index indisponible:" << create.lastError().text();
#endif
    }
    s_npcsFulltextChecked = true;
    return s_npcsFulltext;
}

```

- [ ] **Step 4: `src/db/npc_database.cpp` — `CHAR_LENGTH` → `LENGTH` dans `searchNpcs`**

Remplacer :

```cpp
        " ORDER BY CHAR_LENGTH(nt.name) ASC LIMIT 50"
```

par :

```cpp
        " ORDER BY LENGTH(nt.name) ASC LIMIT 50"
```

- [ ] **Step 5: `src/db/npc_database.cpp` — `CONCAT` → `||` dans `getNpcById`**

Remplacer :

```cpp
        " COALESCE(r.name, CONCAT('Race ', nt.race)) AS race_name,"
```

par :

```cpp
        " COALESCE(r.name, 'Race ' || nt.race) AS race_name,"
```

- [ ] **Step 6: Build**

Run:
```
cmake --build build/debug
```
Expected: 0 erreur.

- [ ] **Step 7: Commit**

```bash
git add src/db/item_database.cpp src/db/npc_database.cpp
git commit -m "refactor(db): supprime le code mort FULLTEXT MySQL et corrige le dialecte SQL pour SQLite"
```

---

### Task 4: Outil `db_export` — copie MySQL → SQLite

**Files:**
- Create: `db_export/CMakeLists.txt`
- Create: `db_export/schema_introspect.h`
- Create: `db_export/schema_introspect.cpp`
- Create: `db_export/table_copier.h`
- Create: `db_export/table_copier.cpp`
- Create: `db_export/main.cpp`
- Modify: `CMakeLists.txt` (racine)

**Interfaces:**
- Produces: `introspectColumns(QSqlDatabase& mysqlDb, const QString& table) -> std::vector<ColumnDef>` où `struct ColumnDef { QString name; QString sqliteType; }`.
- Produces: `copyTable(QSqlDatabase& mysqlDb, QSqlDatabase& sqliteDb, const QString& table, const QStringList& indexColumns = {}) -> bool`.
- Produces: exécutable `db_export.exe` (CLI, cf. Task 5 pour son usage).

- [ ] **Step 1: `db_export/schema_introspect.h`**

```cpp
#pragma once
#include <QString>
#include <vector>

struct ColumnDef {
    QString name;
    QString sqliteType; // "INTEGER", "REAL", ou "TEXT"
};

class QSqlDatabase;

// Lit information_schema.columns pour `table` sur la connexion MySQL `mysqlDb`
// et retourne les colonnes dans l'ordre du schéma, avec leur type SQLite mappé.
std::vector<ColumnDef> introspectColumns(QSqlDatabase& mysqlDb, const QString& table);
```

- [ ] **Step 2: `db_export/schema_introspect.cpp`**

```cpp
#include "schema_introspect.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

namespace {

QString mapSqliteType(const QString& mysqlType) {
    QString t = mysqlType.toUpper();
    if (t.contains("INT")) return "INTEGER";
    if (t.contains("FLOAT") || t.contains("DOUBLE") || t.contains("DECIMAL")) return "REAL";
    return "TEXT";
}

} // namespace

std::vector<ColumnDef> introspectColumns(QSqlDatabase& mysqlDb, const QString& table) {
    std::vector<ColumnDef> cols;
    QSqlQuery q(mysqlDb);
    q.prepare(
        "SELECT COLUMN_NAME, DATA_TYPE FROM information_schema.columns"
        " WHERE table_schema = DATABASE() AND table_name = :t"
        " ORDER BY ORDINAL_POSITION"
    );
    q.bindValue(":t", table);
    if (!q.exec()) {
        qWarning() << "introspectColumns failed for" << table << ":" << q.lastError().text();
        return cols;
    }
    while (q.next()) {
        ColumnDef c;
        c.name       = q.value(0).toString();
        c.sqliteType = mapSqliteType(q.value(1).toString());
        cols.push_back(c);
    }
    return cols;
}
```

- [ ] **Step 3: `db_export/table_copier.h`**

```cpp
#pragma once
#include <QString>
#include <QStringList>

class QSqlDatabase;

// Copie la table `table` de la connexion MySQL `mysqlDb` vers la connexion
// SQLite `sqliteDb` : introspection du schéma, CREATE TABLE, copie des lignes
// par lots dans une transaction, puis création des index sur `indexColumns`
// (colonnes utilisées en jointure/filtre par item_database.cpp/npc_database.cpp).
bool copyTable(QSqlDatabase& mysqlDb, QSqlDatabase& sqliteDb,
               const QString& table, const QStringList& indexColumns = {});
```

- [ ] **Step 4: `db_export/table_copier.cpp`**

```cpp
#include "table_copier.h"
#include "schema_introspect.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

bool copyTable(QSqlDatabase& mysqlDb, QSqlDatabase& sqliteDb,
               const QString& table, const QStringList& indexColumns) {
    auto cols = introspectColumns(mysqlDb, table);
    if (cols.empty()) {
        qWarning() << "No columns found for table" << table << "- skipping";
        return false;
    }

    QStringList colDefs, colNames, placeholders;
    for (const auto& c : cols) {
        colDefs << QString("`%1` %2").arg(c.name, c.sqliteType);
        colNames << QString("`%1`").arg(c.name);
        placeholders << "?";
    }

    QSqlQuery createQ(sqliteDb);
    createQ.exec(QString("DROP TABLE IF EXISTS `%1`").arg(table));
    if (!createQ.exec(QString("CREATE TABLE `%1` (%2)").arg(table, colDefs.join(", ")))) {
        qWarning() << "CREATE TABLE failed for" << table << ":" << createQ.lastError().text();
        return false;
    }

    QSqlQuery selectQ(mysqlDb);
    if (!selectQ.exec(QString("SELECT %1 FROM `%2`").arg(colNames.join(", "), table))) {
        qWarning() << "SELECT failed for" << table << ":" << selectQ.lastError().text();
        return false;
    }

    sqliteDb.transaction();
    QSqlQuery insertQ(sqliteDb);
    insertQ.prepare(QString("INSERT INTO `%1` (%2) VALUES (%3)")
                     .arg(table, colNames.join(", "), placeholders.join(", ")));

    int rowCount = 0;
    while (selectQ.next()) {
        for (int i = 0; i < cols.size(); ++i)
            insertQ.bindValue(i, selectQ.value(i));
        if (!insertQ.exec()) {
            qWarning() << "INSERT failed for" << table << ":" << insertQ.lastError().text();
            sqliteDb.rollback();
            return false;
        }
        ++rowCount;
    }
    sqliteDb.commit();
    qInfo() << "Copied" << rowCount << "rows into" << table;

    for (const auto& col : indexColumns) {
        QSqlQuery idxQ(sqliteDb);
        QString idxName = QString("idx_%1_%2").arg(table, col);
        if (!idxQ.exec(QString("CREATE INDEX `%1` ON `%2` (`%3`)")
                       .arg(idxName, table, col)))
            qWarning() << "CREATE INDEX failed for" << table << "." << col << ":"
                       << idxQ.lastError().text();
    }
    return true;
}
```

- [ ] **Step 5: `db_export/main.cpp`**

```cpp
#include "schema_introspect.h"
#include "table_copier.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

namespace {

struct ExportArgs {
    QString host = "localhost";
    int     port = 3306;
    QString user = "root";
    QString password;
    QString database = "quarm";
    QString outPath;
};

ExportArgs parseArgs(const QStringList& args) {
    ExportArgs a;
    for (int i = 0; i < args.size(); ++i) {
        auto next = [&]() -> QString { return (i + 1 < args.size()) ? args[++i] : QString(); };
        if      (args[i] == "--host")     a.host = next();
        else if (args[i] == "--port")     a.port = next().toInt();
        else if (args[i] == "--user")     a.user = next();
        else if (args[i] == "--password") a.password = next();
        else if (args[i] == "--database") a.database = next();
        else if (args[i] == "--out")      a.outPath = next();
    }
    return a;
}

// Tables copiées telles quelles + colonnes indexées pour les jointures/filtres
// utilisés par src/db/item_database.cpp et src/db/npc_database.cpp
// (cf. docs/specs/2026-07-13-embedded-sqlite-db-design.md).
const QMap<QString, QStringList> TABLES = {
    {"items",              {"worneffect", "focuseffect", "proceffect", "clickeffect", "scrolleffect"}},
    {"spells_new",         {}},
    {"npc_types",          {"race", "loottable_id", "npc_spells_id"}},
    {"races",              {}},
    {"spawnentry",         {"npcID", "spawngroupID"}},
    {"spawn2",             {"spawngroupID", "zone"}},
    {"zone",               {"short_name"}},
    {"npc_spells_entries", {"npc_spells_id", "spellid"}},
    {"loottable_entries",  {"loottable_id", "lootdrop_id"}},
    {"lootdrop_entries",   {"lootdrop_id", "item_id"}},
    {"altadv_vars",        {"eqmacid", "skill_id"}},
    {"aa_effects",         {"aaid"}},
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    auto args = parseArgs(app.arguments().mid(1));
    if (args.outPath.isEmpty()) {
        qWarning() << "Usage: db_export --host H --port P --user U --password W"
                      " --database quarm --out path/to/quarm_data.db";
        return 1;
    }

    auto mysqlDb = QSqlDatabase::addDatabase("QMYSQL", "export_src");
    mysqlDb.setHostName(args.host);
    mysqlDb.setPort(args.port);
    mysqlDb.setUserName(args.user);
    mysqlDb.setPassword(args.password);
    mysqlDb.setDatabaseName(args.database);
    if (!mysqlDb.open()) {
        qWarning() << "MySQL connection failed:" << mysqlDb.lastError().text();
        return 1;
    }

    QFile::remove(args.outPath);
    auto sqliteDb = QSqlDatabase::addDatabase("QSQLITE", "export_dst");
    sqliteDb.setDatabaseName(args.outPath);
    if (!sqliteDb.open()) {
        qWarning() << "SQLite create failed:" << sqliteDb.lastError().text();
        return 1;
    }

    bool allOk = true;
    for (auto it = TABLES.constBegin(); it != TABLES.constEnd(); ++it) {
        qInfo() << "Exporting" << it.key() << "...";
        if (!copyTable(mysqlDb, sqliteDb, it.key(), it.value()))
            allOk = false;
    }

    sqliteDb.close();
    mysqlDb.close();
    QSqlDatabase::removeDatabase("export_src");
    QSqlDatabase::removeDatabase("export_dst");

    qInfo() << (allOk ? "Export completed successfully." : "Export completed with errors.");
    return allOk ? 0 : 1;
}
```

- [ ] **Step 6: `db_export/CMakeLists.txt`**

```cmake
add_executable(db_export
    main.cpp
    schema_introspect.cpp
    schema_introspect.h
    table_copier.cpp
    table_copier.h
)
target_link_libraries(db_export PRIVATE Qt6::Core Qt6::Sql)
```

- [ ] **Step 7: `CMakeLists.txt` (racine) — inclure le nouveau sous-projet**

Ajouter cette ligne juste après `add_subdirectory(tests)` (fin du fichier) :

```cmake

# ── Outil dev : régénération de la BDD SQLite embarquée (cf. CLAUDE.md) ──
add_subdirectory(db_export)
```

- [ ] **Step 8: Build**

Run:
```
cmake --preset windows-x64-debug
cmake --build build/debug --target db_export
```
Expected: build réussi, produit `build/debug/bin/db_export.exe`.

- [ ] **Step 9: Commit**

```bash
git add db_export CMakeLists.txt
git commit -m "feat(db_export): ajoute l'outil de génération du fichier SQLite embarqué depuis MySQL"
```

---

### Task 5: Génération de `quarm_data.db`, câblage CMake, vérification end-to-end

**Files:**
- Create (binaire, commité) : `resources/quarm_data.db`
- Modify: `CMakeLists.txt` (racine)

**Interfaces:**
- Consumes: `db_export.exe` (Task 4), `DbConnection::connect(QString)` (Task 1).
- Produces: `resources/quarm_data.db` — fichier consommé par `main.cpp` (Task 1), l'installeur (`{#BinDir}\*`), et `tests/test_integration_npc.cpp` (Task 2).

**Prérequis :** une instance MySQL/MariaDB locale avec la BDD `quarm` importée et accessible (host/port/user/password — par défaut `localhost:3306` / `root` / `rooteq`, cf. section Database de CLAUDE.md). C'est une étape dev ponctuelle, pas un prérequis pour les utilisateurs finaux de l'app.

- [ ] **Step 1: Générer le fichier SQLite**

Run (adapter les identifiants si différents de l'environnement par défaut) :
```
build\debug\bin\db_export.exe --host localhost --port 3306 --user root --password rooteq --database quarm --out resources\quarm_data.db
```
Expected: en sortie, une ligne `Exporting <table> ...` puis `Copied N rows into <table>` pour chacune des 12 tables, terminé par `Export completed successfully.` et code de retour 0.

- [ ] **Step 2: Vérifier le fichier produit**

Run:
```
build\debug\bin\db_export.exe --help
```
n'est pas nécessaire — à la place, vérifier simplement la présence et la taille du fichier :
```
Get-Item resources\quarm_data.db | Select-Object Length
```
Expected: fichier présent, taille non nulle (plusieurs dizaines de Mo attendues selon le contenu de la BDD Quarm).

- [ ] **Step 3: `CMakeLists.txt` (racine) — copier `quarm_data.db` à côté de l'exe**

Ajouter ce bloc juste après le bloc "Copy item icons next to exe" existant, avant le bloc `windeployqt` :

```cmake

# Copy embedded SQLite data next to exe (généré par db_export/, voir CLAUDE.md)
add_custom_command(TARGET EqQuarmCompanion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/quarm_data.db"
        "$<TARGET_FILE_DIR:EqQuarmCompanion>/quarm_data.db"
    COMMENT "Copying embedded SQLite data"
)
```

- [ ] **Step 4: Rebuild complet et vérification runtime**

Run:
```
cmake --preset windows-x64-debug
cmake --build build/debug
```
Expected: build réussi, `build/debug/bin/quarm_data.db` présent après le build (copié par la commande CMake ci-dessus).

Lancer l'application :
```
build\debug\bin\EqQuarmCompanion.exe
```
Vérifier ensuite dans `%APPDATA%\EqQuarmCompanion\eqquarm_debug.log` qu'il n'y a **pas** de ligne `DB non connectée`. Dans l'app, ouvrir l'onglet Stuff et effectuer une recherche d'item (ex. taper un nom d'item connu dans la barre de recherche) : des résultats doivent apparaître, confirmant que les requêtes SQLite fonctionnent avec des données réelles.

- [ ] **Step 5: Activer temporairement les tests d'intégration pour confirmer la régression sur données réelles**

Run:
```
cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=ON
cmake --build build/debug --target eq_integration_tests
build\debug\bin\eq_integration_tests.exe
```
Expected: les 9 tests (`A1`-`A5`, `B1`-`B4`) s'exécutent réellement (plus de `SKIPPED`) et passent, confirmant que les données exportées produisent des résultats cohérents avec la logique métier existante.

Reconfigurer ensuite sans le flag :
```
cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=OFF
```

- [ ] **Step 6: Commit**

```bash
git add resources/quarm_data.db CMakeLists.txt
git commit -m "feat(db): embarque quarm_data.db et copie le fichier à côté de l'exécutable au build"
```

---

### Task 6: Mise à jour de CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Section "Architecture" — ajouter `db_export/` et mettre à jour `db/` et `resources/`**

Remplacer :

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

par :

```
src/
  core/       # Pure C++17 — no Qt. All game logic, formulas, parsers.
  db/         # Qt SQL + Network. Database access (SQLite embarqué, quarm_data.db).
  ui/         # Qt6 Widgets. 4 tabs: Stuff / Fight / Buffs / Infos.
  main.cpp
resources/    # config_defaults.json, icons, quarm_data.db (BDD SQLite embarquée)
tests/        # GTest unit tests (core/ only)
db_export/    # Outil dev — régénère resources/quarm_data.db depuis la BDD Quarm MySQL source
docs/
  specs/      # Functional specs for each feature
  plans/      # Implementation plans (6 C++ rewrite plans + 1 evolution plan)
  functional-documentation.md  # EQ game formulas + combat analysis reference
```

- [ ] **Step 2: Section "Tests" — mettre à jour le nombre de tests**

Remplacer :

```
36 tests across 6 suites: config, character_parser, stats_calculator, npc_analysis, spell_stats, spell_stacking. Les tests `character_parser` (dans `tests/test_character_parser.cpp`) couvrent le happy-path mais aussi les comportements documentés : `bag_item_ids` depuis `GeneralN-SlotM`, section AAIndex, expansion de slots Ear/Wrist/Fingers, base stats/race, classe inconnue, pattern `*-Quarmy.txt`.
```

par :

```
35 tests across 6 suites: config, character_parser, stats_calculator, npc_analysis, spell_stats, spell_stacking. Les tests `character_parser` (dans `tests/test_character_parser.cpp`) couvrent le happy-path mais aussi les comportements documentés : `bag_item_ids` depuis `GeneralN-SlotM`, section AAIndex, expansion de slots Ear/Wrist/Fingers, base stats/race, classe inconnue, pattern `*-Quarmy.txt`.

Un 7e binaire optionnel, `eq_integration_tests` (option CMake `BUILD_INTEGRATION_TESTS`, OFF par défaut), exécute des tests de régression NPC contre `resources/quarm_data.db` — pas contre un MySQL live. Il n'apparaît pas dans `ctest` standard (pas de `gtest_discover_tests`).
```

- [ ] **Step 3: Section "Database" — remplacer entièrement le contenu MySQL par SQLite embarqué**

Remplacer tout le contenu de la section `## Database` (de `MySQL database named...` jusqu'à la fin du paragraphe FULLTEXT) par :

```
## Database

Les données de référence (items, sorts, NPCs, races, zones, loot, AA) sont embarquées avec l'application sous forme d'un fichier **SQLite** (`resources/quarm_data.db`, copié à côté de l'exécutable au build par CMake et récupéré automatiquement par l'installeur via `{#BinDir}\*`). `DbConnection::connect()` ouvre ce fichier en lecture seule via le driver `QSQLITE` — natif à Qt, aucune lib cliente externe à installer (contrairement à l'ancien driver `QMYSQL`, qui nécessitait un plugin compilé contre MariaDB). **Aucune installation MySQL/MariaDB n'est nécessaire côté utilisateur final.**

Tables embarquées (copie intégrale, toutes colonnes) : `items`, `spells_new`, `npc_types`, `races`, `spawnentry`, `spawn2`, `zone`, `npc_spells_entries`, `loottable_entries`, `lootdrop_entries`, `altadv_vars`, `aa_effects`.

**Régénération.** Quand la BDD Quarm source (MySQL) évolue, un développeur ayant accès à une instance MySQL/MariaDB locale avec la BDD `quarm` importée relance l'outil `db_export/` :

```powershell
build\debug\bin\db_export.exe --host localhost --port 3306 --user root --password rooteq --database quarm --out resources\quarm_data.db
```

puis rebuild + régénère l'installeur (cf. section Installer ci-dessus). Le fichier `resources/quarm_data.db` généré est commité dans le repo — c'est ce qui permet à un utilisateur final ou à la CI de builder/lancer l'app sans jamais toucher à MySQL.

**Note historique.** L'index FULLTEXT MySQL (`ft_name`) et son branchement dans `searchItems`/`searchNpcs` — déjà débranché depuis le refactor QCompleter `d47f79d` — ont été définitivement supprimés (code mort `hasItemsFulltextIndex`/`hasNpcsFulltextIndex`) lors du passage à SQLite. Les recherches restent en `LIKE` full-scan, sans régression de perf par rapport à l'état MySQL précédent.
```

- [ ] **Step 4: Table "Key Source Files" — mettre à jour les lignes `db_connection.h` et `config.h/cpp`, ajouter `db_export/`**

Remplacer :

```
| `src/db/db_connection.h` | `DbConnection` singleton — gestion de `QSqlDatabase`; `connect()`, `testConnection()` |
```

par :

```
| `src/db/db_connection.h` | `DbConnection` singleton — gestion de `QSqlDatabase` (driver `QSQLITE`, fichier `quarm_data.db` embarqué à côté de l'exe) ; `connect(QString)` |
```

Remplacer :

```
| `src/core/config.h/cpp` | JSON config read/write, DbConfig, getClassWeights, setClassWeights, getCharacterClass |
```

par :

```
| `src/core/config.h/cpp` | JSON config read/write, getClassWeights, setClassWeights, getCharacterClass |
```

Ajouter une nouvelle ligne (par exemple juste après la ligne `src/db/bis_scraper.h`) :

```
| `db_export/main.cpp` | Outil CLI dev-only — copie les tables MySQL Quarm vers `resources/quarm_data.db` (SQLite). Args : `--host --port --user --password --database --out`. |
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: documente l'architecture SQLite embarquée et l'outil db_export"
```

---

## Self-Review Notes

- **Couverture spec :** section 1 (portée données) → Task 4 ; section 2 (outil export) → Task 4 ; section 3 (côté app) → Task 1 + Task 5 ; section 4 (dialecte SQL) → Task 3 ; section 5 (régénération) → Task 6 ; section 6 (tests) → Task 2. Toutes les sections de `docs/specs/2026-07-13-embedded-sqlite-db-design.md` sont couvertes.
- **Écart repéré par rapport au brainstorming initial :** le spec mentionnait un outil sous `tools/db_export`. `.gitignore` liste `tools/` (dossier local pour le plugin QMYSQL compilé) — l'outil vit donc à la racine sous `db_export/`, sibling de `installer/`, pour être effectivement commité. Signalé explicitement dans les Global Constraints.
- **Type consistency :** `DbConnection::connect(const QString&)` a la même signature partout où il est appelé (Task 1 : `main.cpp`, `main_window.cpp` ; Task 2 : `test_integration_npc.cpp`). `copyTable`/`introspectColumns` ont les mêmes signatures entre leur définition (Task 4, Step 1-4) et leur usage (Task 4, Step 5).
