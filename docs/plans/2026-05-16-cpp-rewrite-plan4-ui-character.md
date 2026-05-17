# EqQuarmCompanion — Plan 4 : UI Skeleton + Character Tab

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prérequis :** Plan 3 terminé — couche DB compilée, connexion MySQL fonctionnelle.

**Goal:** Implémenter `MainWindow` (4 onglets, sélecteur personnage), `SettingsDialog`, `SearchComboBox`, et l'onglet `CharacterTab` complet.

**Architecture:** Pattern uniforme pour chaque onglet : reçoit `(CharacterInfo*, PlayerTotals*, Config*, NpcDatabase*, ItemDatabase*)` + expose `void refresh()`. La `MainWindow` orchestre le chargement des fichiers perso et appelle `refresh()` sur tous les onglets.

**Tech Stack:** Qt6 Widgets (QMainWindow, QTabWidget, QScrollArea, QComboBox, QGridLayout).

**Référence Python :** `eq-item-evaluator/ui/main_window.py`, `character_tab.py`, `widgets.py`, `settings_dialog.py`.

---

### Task 1 : `ui/widgets` — `SearchComboBox`

**Python source :** `eq-item-evaluator/ui/widgets.py`

**Files:**
- Modify: `src/ui/widgets.h`
- Modify: `src/ui/widgets.cpp`

- [ ] **Step 1 : Réécrire `src/ui/widgets.h`**

```cpp
#pragma once
#include <QComboBox>
#include <QKeyEvent>

// QComboBox qui émet popup_requested() quand l'utilisateur tape
// (pour déclencher la recherche NPC/item sans ouvrir la popup Qt).
class SearchComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit SearchComboBox(QWidget* parent = nullptr);

signals:
    void popup_requested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showPopup() override;
};
```

- [ ] **Step 2 : Écrire `src/ui/widgets.cpp`**

```cpp
#include "ui/widgets.h"

SearchComboBox::SearchComboBox(QWidget* parent) : QComboBox(parent) {
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
}

void SearchComboBox::keyPressEvent(QKeyEvent* event) {
    QComboBox::keyPressEvent(event);
    // Émet popup_requested() à chaque frappe pour déclencher la recherche
    if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Escape)
        emit popup_requested();
}

void SearchComboBox::showPopup() {
    emit popup_requested();
    QComboBox::showPopup();
}
```

- [ ] **Step 3 : Commit**

```powershell
git add src/ui/widgets.h src/ui/widgets.cpp
git commit -m "feat(ui): SearchComboBox — emit popup_requested on keypress"
```

---

### Task 2 : `ui/settings_dialog`

**Python source :** `eq-item-evaluator/ui/settings_dialog.py`

**Files:**
- Modify: `src/ui/settings_dialog.h`
- Modify: `src/ui/settings_dialog.cpp`

- [ ] **Step 1 : Réécrire `src/ui/settings_dialog.h`**

```cpp
#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccepted();

private:
    Config*    _config;
    QLineEdit* _eqFilesDir;
    QLineEdit* _dbHost;
    QSpinBox*  _dbPort;
    QLineEdit* _dbUser;
    QLineEdit* _dbPassword;
    QLineEdit* _dbName;
};
```

- [ ] **Step 2 : Écrire `src/ui/settings_dialog.cpp`**

```cpp
#include "ui/settings_dialog.h"
#include "core/config.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(Config* cfg, QWidget* parent)
    : QDialog(parent), _config(cfg)
{
    setWindowTitle("Paramètres");
    setMinimumWidth(420);

    auto* form = new QFormLayout;
    _eqFilesDir = new QLineEdit(QString::fromStdString(cfg->get("eq_files_dir")));
    auto* browseBtn = new QPushButton("…");
    browseBtn->setFixedWidth(32);
    connect(browseBtn, &QPushButton::clicked, [this]{
        auto dir = QFileDialog::getExistingDirectory(this, "Dossier fichiers EQ");
        if (!dir.isEmpty()) _eqFilesDir->setText(dir);
    });
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(_eqFilesDir);
    dirRow->addWidget(browseBtn);
    auto* dirW = new QWidget; dirW->setLayout(dirRow);
    form->addRow("Dossier EQ :", dirW);

    auto dbCfg = cfg->getDbConfig();
    _dbHost     = new QLineEdit(QString::fromStdString(dbCfg.host));
    _dbPort     = new QSpinBox; _dbPort->setRange(1, 65535); _dbPort->setValue(dbCfg.port);
    _dbUser     = new QLineEdit(QString::fromStdString(dbCfg.user));
    _dbPassword = new QLineEdit(QString::fromStdString(dbCfg.password));
    _dbPassword->setEchoMode(QLineEdit::Password);
    _dbName     = new QLineEdit(QString::fromStdString(dbCfg.database));
    form->addRow("DB Host :",     _dbHost);
    form->addRow("DB Port :",     _dbPort);
    form->addRow("DB User :",     _dbUser);
    form->addRow("DB Password :", _dbPassword);
    form->addRow("DB Name :",     _dbName);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* vl = new QVBoxLayout(this);
    vl->addLayout(form);
    vl->addWidget(btns);
}

void SettingsDialog::onAccepted() {
    _config->set("eq_files_dir", nlohmann::json(_eqFilesDir->text().toStdString()));
    nlohmann::json db;
    db["host"]     = _dbHost->text().toStdString();
    db["port"]     = _dbPort->value();
    db["user"]     = _dbUser->text().toStdString();
    db["password"] = _dbPassword->text().toStdString();
    db["database"] = _dbName->text().toStdString();
    _config->set("db", db);
    _config->save();
    accept();
}
```

- [ ] **Step 3 : Commit**

```powershell
git add src/ui/settings_dialog.h src/ui/settings_dialog.cpp
git commit -m "feat(ui): SettingsDialog — eq_files_dir + DB params"
```

---

### Task 3 : `ui/main_window`

**Python source :** `eq-item-evaluator/ui/main_window.py` (105 lignes)

**Files:**
- Modify: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`

- [ ] **Step 1 : Réécrire `src/ui/main_window.h`**

```cpp
#pragma once
#include "core/character_parser.h"
#include "core/stats_calculator.h"
#include "core/types.h"
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QTabWidget>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;
class CharacterTab;
class FightTab;
class SpellsTab;
class InfosTab;
class SettingsDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, NpcDatabase* npcDb,
                        ItemDatabase* itemDb, QWidget* parent = nullptr);

private slots:
    void onCharacterChanged(int index);
    void openSettings();

private:
    void loadCharacterFiles();
    void refreshAllTabs();

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;

    QComboBox*    _charSelector;
    QTabWidget*   _tabs;
    CharacterTab* _charTab;
    FightTab*     _fightTab;
    SpellsTab*    _spellsTab;
    InfosTab*     _infosTab;

    std::vector<CharacterInfo>  _characters;
    CharacterInfo               _currentChar;
    PlayerTotals                _playerTotals;
};
```

- [ ] **Step 2 : Écrire `src/ui/main_window.cpp`**

```cpp
#include "ui/main_window.h"
#include "ui/character_tab.h"
#include "ui/fight_tab.h"
#include "ui/spells_tab.h"
#include "ui/infos_tab.h"
#include "ui/settings_dialog.h"
#include "core/config.h"
#include <QHBoxLayout>
#include <QMenuBar>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(Config* config, NpcDatabase* npcDb,
                        ItemDatabase* itemDb, QWidget* parent)
    : QMainWindow(parent), _config(config), _npcDb(npcDb), _itemDb(itemDb)
{
    setWindowTitle("EQ Quarm Companion");
    resize(1280, 800);

    // ── Toolbar ──────────────────────────────────────────────────────
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    _charSelector = new QComboBox;
    _charSelector->setMinimumWidth(200);
    toolbar->addWidget(new QLabel("  Personnage : "));
    toolbar->addWidget(_charSelector);
    toolbar->addSeparator();
    auto* settingsBtn = new QPushButton("⚙");
    settingsBtn->setFlat(true);
    toolbar->addWidget(settingsBtn);

    connect(_charSelector, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCharacterChanged);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    // ── Tabs ──────────────────────────────────────────────────────────
    _tabs    = new QTabWidget;
    _charTab  = new CharacterTab(config, npcDb, itemDb);
    _fightTab = new FightTab(config, npcDb, itemDb);
    _spellsTab = new SpellsTab(config, npcDb);
    _infosTab  = new InfosTab(config);

    _tabs->addTab(_charTab,   "Stuff");
    _tabs->addTab(_fightTab,  "Fight");
    _tabs->addTab(_spellsTab, "Spells");
    _tabs->addTab(_infosTab,  "Infos");

    setCentralWidget(_tabs);

    // ── Charger les personnages ────────────────────────────────────────
    loadCharacterFiles();
}

void MainWindow::loadCharacterFiles() {
    auto eqDir = std::filesystem::path(
        QString::fromStdString(_config->get("eq_files_dir")).toStdWString());
    auto files = findCharacterFiles(eqDir);

    _charSelector->blockSignals(true);
    _charSelector->clear();
    _characters.clear();

    for (auto& f : files) {
        auto ci = parseCharacterFile(f);
        if (!ci) continue;
        _characters.push_back(*ci);
        _charSelector->addItem(
            QString::fromStdString(ci->name + " (" + ci->class_name + ")"));
    }
    _charSelector->blockSignals(false);

    if (!_characters.empty()) onCharacterChanged(0);
}

void MainWindow::onCharacterChanged(int index) {
    if (index < 0 || index >= static_cast<int>(_characters.size())) return;
    _currentChar = _characters[index];
    _playerTotals = calculateTotals(_currentChar, {});
    refreshAllTabs();
}

void MainWindow::refreshAllTabs() {
    _charTab->setCharacter(&_currentChar, &_playerTotals);
    _fightTab->setCharacter(&_currentChar, &_playerTotals);
    _spellsTab->setCharacter(&_currentChar, &_playerTotals);
}

void MainWindow::openSettings() {
    SettingsDialog dlg(_config, this);
    if (dlg.exec() == QDialog::Accepted)
        loadCharacterFiles();
}
```

- [ ] **Step 3 : Mettre à jour `src/main.cpp`** — passer `npcDb` et `itemDb` à `MainWindow`

```cpp
// Remplacer "MainWindow window(&config);" par :
NpcDatabase  npcDb;
ItemDatabase itemDb;
MainWindow window(&config, &npcDb, &itemDb);
```

Ajouter les includes :
```cpp
#include "db/npc_database.h"
#include "db/item_database.h"
```

- [ ] **Step 4 : Mettre à jour les stubs UI** pour accepter les nouveaux constructeurs

Chaque onglet stub doit accepter les paramètres (sans les utiliser encore). Mettre à jour `character_tab.h`, `fight_tab.h`, `spells_tab.h`, `infos_tab.h` :

```cpp
// character_tab.h
#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase; class ItemDatabase;
class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p=nullptr)
        : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};

// fight_tab.h
#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase; class ItemDatabase;
class FightTab : public QWidget {
    Q_OBJECT
public:
    FightTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p=nullptr)
        : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};

// spells_tab.h
#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase;
class SpellsTab : public QWidget {
    Q_OBJECT
public:
    SpellsTab(Config*, NpcDatabase*, QWidget* p=nullptr) : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};

// infos_tab.h
#pragma once
#include <QWidget>
class Config;
class InfosTab : public QWidget {
    Q_OBJECT
public:
    InfosTab(Config*, QWidget* p=nullptr) : QWidget(p) {}
};
```

- [ ] **Step 5 : Compiler et lancer**

```powershell
cmake --build build/debug --target EqQuarmCompanion
.\build\debug\bin\EqQuarmCompanion.exe
```

Attendu : fenêtre avec toolbar (sélecteur personnage + bouton ⚙) et 4 onglets vides (Stuff / Fight / Spells / Infos). Le dialog Paramètres s'ouvre depuis ⚙.

- [ ] **Step 6 : Commit**

```powershell
git add src/ui/main_window.h src/ui/main_window.cpp src/main.cpp
git add src/ui/character_tab.h src/ui/fight_tab.h src/ui/spells_tab.h src/ui/infos_tab.h
git commit -m "feat(ui): MainWindow — 4 onglets + sélecteur personnage + Settings"
```

---

### Task 4 : `ui/character_tab` complet

**Python source :** `eq-item-evaluator/ui/character_tab.py` (468 lignes)

**Files:**
- Modify: `src/ui/character_tab.h`
- Modify: `src/ui/character_tab.cpp`

- [ ] **Step 1 : Réécrire `src/ui/character_tab.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class Config;
class NpcDatabase;
class ItemDatabase;

class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config* config, NpcDatabase* npcDb,
                 ItemDatabase* itemDb, QWidget* parent = nullptr);

    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);

signals:
    void equippedItemChanged(const QString& slotName, int itemId);

private:
    void buildUi();
    QWidget* buildStatsPanel();
    QWidget* buildEquipPanel();

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;
    CharacterInfo* _charInfo{};
    PlayerTotals*  _totals{};
    QScrollArea*  _statsScroll;
    QScrollArea*  _equipScroll;
};
```

- [ ] **Step 2 : Écrire `src/ui/character_tab.cpp`**

Porter depuis `character_tab.py` (468 lignes). La structure est deux panneaux côte à côte dans un `QHBoxLayout` :
- **Panneau gauche** : stats du personnage (QGridLayout avec tuiles)
- **Panneau droit** : slots d'équipement avec SearchComboBox par slot

```cpp
#include "ui/character_tab.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "db/item_database.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

CharacterTab::CharacterTab(Config* cfg, NpcDatabase* npcDb,
                            ItemDatabase* itemDb, QWidget* parent)
    : QWidget(parent), _config(cfg), _npcDb(npcDb), _itemDb(itemDb)
{
    buildUi();
}

void CharacterTab::buildUi() {
    auto* outer = new QHBoxLayout(this);
    outer->setSpacing(8);

    _statsScroll = new QScrollArea;
    _statsScroll->setWidgetResizable(true);
    _statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    _equipScroll = new QScrollArea;
    _equipScroll->setWidgetResizable(true);
    _equipScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* emptyStats = new QLabel("Sélectionner un personnage");
    emptyStats->setAlignment(Qt::AlignCenter);
    _statsScroll->setWidget(emptyStats);

    auto* emptyEquip = new QLabel("Sélectionner un personnage");
    emptyEquip->setAlignment(Qt::AlignCenter);
    _equipScroll->setWidget(emptyEquip);

    outer->addWidget(_statsScroll, 1);
    outer->addWidget(_equipScroll, 1);
}

void CharacterTab::setCharacter(CharacterInfo* ci, PlayerTotals* totals) {
    _charInfo = ci;
    _totals   = totals;
    _statsScroll->setWidget(buildStatsPanel());
    _equipScroll->setWidget(buildEquipPanel());
}

QWidget* CharacterTab::buildStatsPanel() {
    // Porter depuis character_tab.py::_build_stats_panel()
    // Sections : Combat (HP/Mana/ATK/AC), Stats (STR/STA/…), Resists (MR/FR/…)
    // Utiliser les mêmes helpers _section_frame() / _grid_widget() que fight_tab
    // (les définir dans un fichier partagé ui/ui_helpers.h dans le Plan 5)
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignTop);
    if (!_charInfo) return w;

    auto* name = new QLabel(QString::fromStdString(_charInfo->name));
    name->setStyleSheet("font-size:14px;font-weight:bold;color:#e0e0e0;");
    vl->addWidget(name);
    auto* sub = new QLabel(
        QString::fromStdString(_charInfo->class_name + " — niveau " +
                               std::to_string(_charInfo->level)));
    sub->setStyleSheet("color:#888888;font-size:11px;");
    vl->addWidget(sub);

    // Porter les sections Stats depuis character_tab.py
    // (HP capped, Mana capped, ATK, AC, STR, STA, DEX, AGI, INT, WIS, CHA, resists)

    vl->addStretch();
    return w;
}

QWidget* CharacterTab::buildEquipPanel() {
    // Porter depuis character_tab.py::_build_equip_panel()
    // Un SearchComboBox par slot EQ (Head, Chest, Legs, …)
    // Chaque SearchComboBox → ItemDatabase::searchItems() → emit equippedItemChanged()
    auto* w = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignTop);
    // Porter les 22 slots depuis character_tab.py
    vl->addStretch();
    return w;
}
```

> **Porter depuis Python :** Les sections de stats et les 22 slots d'équipement sont définis dans `character_tab.py`. La logique de calcul (totaux, caps, class weights) est déjà dans `core/stats_calculator`.

- [ ] **Step 3 : Compiler**

```powershell
cmake --build build/debug --target EqQuarmCompanion
```

- [ ] **Step 4 : Commit**

```powershell
git add src/ui/character_tab.h src/ui/character_tab.cpp
git commit -m "feat(ui): CharacterTab — stats + equip slots"
```

---

### Vérification finale du Plan 4

- [ ] `cmake --build build/debug` → 0 erreur
- [ ] App lancée → toolbar + 4 onglets visibles
- [ ] ⚙ ouvre le dialog Paramètres
- [ ] Onglet "Stuff" affiche les stats du personnage sélectionné

**Prêt pour Plan 5 (Fight tab) quand les vérifications passent.**
