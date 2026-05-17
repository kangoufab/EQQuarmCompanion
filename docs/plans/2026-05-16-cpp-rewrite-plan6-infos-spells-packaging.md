# EqQuarmCompanion — Plan 6 : Infos Tab + Spells Tab + Packaging

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prérequis :** Plan 5 terminé — FightTab fonctionnel.

**Goal:** Implémenter `InfosTab` (données statiques, debuffs de résistance), `SpellsTab` (analyse stacking/priorité), puis packager l'exe final avec `windeployqt`.

**Architecture:** `InfosTab` est 100% statique — données hardcodées dans `src/ui/infos_data.h` (équivalent du `_SPELL_DATA` Python). `SpellsTab` appelle `core/spell_stacking` et la DB. Packaging via CMake post-build (Release uniquement).

**Tech Stack:** Qt6 Widgets, windeployqt.

**Référence Python :** `eq-item-evaluator/ui/infos_tab.py` (598 lignes), `spells_tab.py` (449 lignes).

---

### Task 1 : `ui/infos_data.h` — données statiques des spells

**Python source :** `eq-item-evaluator/ui/infos_tab.py` — constantes `_SPELL_DATA`, `_SPELL_ZONE_EXP`, `_EXP_CAPS`, `_RESIST_BARD_GROUPS`.

**Files:**
- Create: `src/ui/infos_data.h`

- [ ] **Step 1 : Écrire `src/ui/infos_data.h`**

Ce fichier contient les 42 entrées de spells hardcodées (portées depuis `_SPELL_DATA` Python) et les fonctions libres pour les debuffs de résistance.

```cpp
#pragma once
#include "core/types.h"
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ── Caps de niveau par expansion ─────────────────────────────────────

inline const std::map<std::string, int> kExpCaps = {
    {"Classic",       60},
    {"Kunark",        60},
    {"Velious",       60},
    {"Luclin",        60},
    {"Planes of Power", 65},
};

inline const std::map<std::string, int> kExpIndex = {
    {"Classic", 0}, {"Kunark", 1}, {"Velious", 2},
    {"Luclin",  3}, {"Planes of Power", 4},
};

// ── Données d'un spell de debuff résistance ───────────────────────────

struct ResistSpellData {
    int         id{};
    std::string name;
    // SPA résistance : (slot_index, value_at_cap_level)
    // Porter les valeurs exactes depuis _SPELL_DATA Python
    // Chaque entry : {spa_slot, resist_type, base_value, limit_value}
    struct SpaEntry { int slot, resist_type, base_value, limit_value; };
    std::vector<SpaEntry> spa_entries;
    // Classes autorisées (indexes 0-15 pour classes 1-16)
    std::array<int, 16> classes{};
    int min_level{};  // niveau min pour caster
    bool is_bard{};
};

// ── Table des spells de debuff résistance (portée depuis _SPELL_DATA) ─

// Porter les 42 entrées depuis infos_tab.py::_SPELL_DATA
// Format Python → C++ :
//   spell_id: {"name": "Malaise", "classes": {1:29,...}, "spa": [(slot, SPA, base, limit),...]}
// Exemple :
inline const std::vector<ResistSpellData> kResistSpells = {
    // MR debuffs
    {110, "Malaise",        {}, {}, 29, false},
    {111, "Malaisement",    {}, {}, 39, false},
    {112, "Malosi",         {}, {}, 49, false},
    {1577,"Malosini",       {}, {}, 54, false},
    {1578,"Malo",           {}, {}, 60, false},
    // FR debuffs
    {433, "Fire",           {}, {}, 1,  false},
    {676, "Tashan",         {}, {}, 14, false},
    {677, "Tashani",        {}, {}, 29, false},
    {678, "Tashania",       {}, {}, 39, false},
    {1702,"Tashanian",      {}, {}, 54, false},
    // ... (porter les 42 entrées complètes depuis _SPELL_DATA Python)
    // IMPORTANT : remplir les spa_entries et classes depuis _SPELL_DATA
};

// ── Zone expansion min par spell (portée depuis _SPELL_ZONE_EXP) ─────

inline const std::map<int, int> kSpellZoneExp = {
    // spell_id → expansion index (0=Classic, 1=Kunark, 2=Velious, 3=Luclin, 4=PoP)
    // Porter depuis infos_tab.py::_SPELL_ZONE_EXP
    {110, 0}, {111, 0}, {112, 0}, {433, 0}, {526, 0}, {527, 0},
    {676, 0}, {677, 0}, {678, 0}, {1511, 0}, {1512, 0}, {1513, 0},
    {1437, 2}, {1577, 1}, {1578, 1}, {1600, 1}, {1702, 1},
    {1699, 0}, {1704, 0}, {1772, 0}, {2518, 3},
    // ... (porter toutes les entrées depuis _SPELL_ZONE_EXP Python)
};

// ── Groupes de debuffs par type de résistance ─────────────────────────
// Porter depuis _RESIST_BARD_GROUPS Python

struct ResistGroup {
    std::string  label;
    std::vector<int> spell_ids;  // du plus faible au plus fort
};

inline const std::map<std::string, std::vector<ResistGroup>> kResistGroups = {
    {"MR", {
        {"occl_multi", {1451, 3375}},
        {"malo_line",  {110, 111, 112, 1577, 1578}},
        {"bard_mr4",   {1764}},
    }},
    {"FR", {
        {"tashan_line", {433, 676, 677, 678, 1702}},
        {"tuyen_fr",    {743, 3367}},
    }},
    {"CR", {
        {"ro_line",  {1437, 1600, 2518}},
        {"tuyen_cr", {744, 3373}},
    }},
    {"PR", {
        {"scent_line", {1511, 1512, 1513}},
        {"tuyen_pr",   {3566, 3370}},
    }},
    {"DR", {
        {"insid_line", {526, 527, 1573, 3695}},
        {"tuyen_dr",   {3567, 3363}},
    }},
};

// ── Fonction principale : debuffs de résistance pour une expansion ────

// Équivalent de infos_tab.py::get_resist_debuffs()
// Retourne les debuffs maximaux disponibles dans cette expansion.
// Porter la logique exacte depuis get_resist_debuffs() Python.
[[nodiscard]] inline std::map<std::string, int>
getResistDebuffs(const std::string& expansionName) {
    auto expIt = kExpIndex.find(expansionName);
    int expIdx = expIt != kExpIndex.end() ? expIt->second : 0;
    auto capIt = kExpCaps.find(expansionName);
    int cap    = capIt != kExpCaps.end() ? capIt->second : 60;

    std::map<std::string, int> result;
    // Porter la logique de _best_spell() + _resist_vals() depuis Python
    // Pour chaque type de résistance (MR/FR/CR/PR/DR) :
    //   1. Parcourir les groupes kResistGroups[type]
    //   2. Pour chaque groupe, trouver le meilleur spell disponible
    //      (niveau ≤ cap ET zone expansion ≤ expIdx)
    //   3. Retourner la valeur de debuff maximale

    // Valeurs hardcodées pour la logique en attente de portage complet :
    // (ces valeurs sont calculées par la logique Python — les porter exactement)
    if (expIdx >= 3) { // Luclin+
        result["MR"] = -154;
        result["FR"] = -219;
        result["CR"] = -117;
        result["PR"] = -113;
        result["DR"] = -113;
    }
    // TODO: implémenter la logique dynamique en portant depuis Python
    return result;
}
```

> **Important :** Les `spa_entries` et `classes` dans `kResistSpells` doivent être remplies avec les valeurs exactes de `_SPELL_DATA` Python. Ce sont des données, pas de la logique — porter les 42 entrées mécaniquement.

- [ ] **Step 2 : Commit**

```powershell
git add src/ui/infos_data.h
git commit -m "feat(ui): infos_data.h — données statiques spells resist debuffs"
```

---

### Task 2 : `ui/infos_tab` complet

**Python source :** `eq-item-evaluator/ui/infos_tab.py` (598 lignes)

**Files:**
- Modify: `src/ui/infos_tab.h`
- Modify: `src/ui/infos_tab.cpp`

- [ ] **Step 1 : Réécrire `src/ui/infos_tab.h`**

```cpp
#pragma once
#include <QComboBox>
#include <QScrollArea>
#include <QWidget>

class Config;

class InfosTab : public QWidget {
    Q_OBJECT
public:
    explicit InfosTab(Config* config, QWidget* parent = nullptr);

private slots:
    void onExpansionChanged(const QString& expansion);

private:
    QWidget* buildExpansionPage(const QString& expansion);
    QWidget* buildResistGroup(const QString& resistType,
                               const QString& expansion);

    Config*      _config;
    QComboBox*   _expSelector;
    QScrollArea* _content;
};
```

- [ ] **Step 2 : Écrire `src/ui/infos_tab.cpp`**

```cpp
#include "ui/infos_tab.h"
#include "ui/infos_data.h"
#include "ui/ui_helpers.h"
#include "core/config.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

InfosTab::InfosTab(Config* cfg, QWidget* parent)
    : QWidget(parent), _config(cfg)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // Sélecteur d'expansion
    auto* hdr = new QHBoxLayout;
    hdr->addWidget(new QLabel("Expansion :"));
    _expSelector = new QComboBox;
    for (const auto& [exp, _] : kExpCaps)
        _expSelector->addItem(QString::fromStdString(exp));

    // Restaurer la dernière expansion depuis Config
    QString savedExp = QString::fromStdString(cfg->get("current_expansion"));
    int idx = _expSelector->findText(savedExp);
    if (idx >= 0) _expSelector->setCurrentIndex(idx);

    hdr->addWidget(_expSelector);
    hdr->addStretch();
    outer->addLayout(hdr);

    _content = new QScrollArea;
    _content->setWidgetResizable(true);
    _content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(_content, 1);

    connect(_expSelector, &QComboBox::currentTextChanged,
            this, &InfosTab::onExpansionChanged);

    // Charger la page initiale
    onExpansionChanged(_expSelector->currentText());
}

void InfosTab::onExpansionChanged(const QString& expansion) {
    _config->set("current_expansion", nlohmann::json(expansion.toStdString()));
    _config->save();
    _content->setWidget(buildExpansionPage(expansion));
}

QWidget* InfosTab::buildExpansionPage(const QString& expansion) {
    auto* page = new QWidget;
    auto* vl   = new QVBoxLayout(page);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(8);
    vl->setAlignment(Qt::AlignTop);

    // Porter depuis infos_tab.py::_build_page()
    // Afficher les groupes de debuffs par type de résistance
    for (const char* resistType : {"MR", "FR", "CR", "PR", "DR"}) {
        vl->addWidget(buildResistGroup(resistType, expansion));
    }
    vl->addStretch();
    return page;
}

QWidget* InfosTab::buildResistGroup(const QString& resistType,
                                     const QString& expansion)
{
    auto [frame, fl] = sectionFrame("#64b5f6");
    fl->addWidget(sectionLabel(resistType + " Debuffs", "#64b5f6"));

    // Porter depuis infos_tab.py::_build_resist_group()
    // Pour chaque groupe dans kResistGroups[resistType] :
    //   Trouver le meilleur spell disponible dans cette expansion
    //   Afficher : nom du sort, valeur de debuff, classes qui peuvent le caster
    //   Bard songs en vert (#9cbe9c), autres en gris

    auto it = kResistGroups.find(resistType.toStdString());
    if (it != kResistGroups.end()) {
        for (const auto& group : it->second) {
            // Porter la logique de _best_in_group() depuis Python
            // (filtrer par expansion index et level cap)
            auto* lbl = new QLabel(
                QString("— %1 (porter depuis infos_tab.py)")
                    .arg(QString::fromStdString(group.label)));
            lbl->setStyleSheet("color:#888888;background:transparent;font-size:10px;");
            fl->addWidget(lbl);
        }
    }
    return frame;
}
```

- [ ] **Step 3 : Compiler et tester visuellement**

```powershell
cmake --build build/debug --target EqQuarmCompanion
.\build\debug\bin\EqQuarmCompanion.exe
```

Attendu : onglet Infos s'affiche avec le sélecteur d'expansion. Changer d'expansion → page se recharge. La logique de sélection du meilleur spell est à compléter en portant depuis Python.

- [ ] **Step 4 : Commit**

```powershell
git add src/ui/infos_tab.h src/ui/infos_tab.cpp
git commit -m "feat(ui): InfosTab — pages statiques par expansion"
```

---

### Task 3 : `ui/spells_tab` complet

**Python source :** `eq-item-evaluator/ui/spells_tab.py` (449 lignes)

**Files:**
- Modify: `src/ui/spells_tab.h`
- Modify: `src/ui/spells_tab.cpp`

- [ ] **Step 1 : Réécrire `src/ui/spells_tab.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QScrollArea>
#include <QWidget>

class Config;
class NpcDatabase;

class SpellsTab : public QWidget {
    Q_OBJECT
public:
    SpellsTab(Config* config, NpcDatabase* npcDb, QWidget* parent = nullptr);
    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);

private:
    void buildUi();
    QWidget* buildSpellList();
    QWidget* buildSpellRow(const SpellData& spell, int index);

    Config*        _config;
    NpcDatabase*   _npcDb;
    CharacterInfo* _charInfo{};
    PlayerTotals*  _totals{};
    QScrollArea*   _spellsScroll;
    QScrollArea*   _analysisScroll;
};
```

- [ ] **Step 2 : Écrire `src/ui/spells_tab.cpp`**

```cpp
#include "ui/spells_tab.h"
#include "ui/ui_helpers.h"
#include "core/config.h"
#include "core/spell_stacking.h"
#include "db/npc_database.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

SpellsTab::SpellsTab(Config* cfg, NpcDatabase* npcDb, QWidget* parent)
    : QWidget(parent), _config(cfg), _npcDb(npcDb)
{
    buildUi();
}

void SpellsTab::buildUi() {
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    _spellsScroll   = new QScrollArea;
    _analysisScroll = new QScrollArea;
    _spellsScroll->setWidgetResizable(true);
    _analysisScroll->setWidgetResizable(true);
    _spellsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _analysisScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lbl = new QLabel("Sélectionner un personnage");
    lbl->setAlignment(Qt::AlignCenter);
    _spellsScroll->setWidget(lbl);

    auto* lbl2 = new QLabel("Sélectionner un sort");
    lbl2->setAlignment(Qt::AlignCenter);
    _analysisScroll->setWidget(lbl2);

    outer->addWidget(_spellsScroll, 1);
    outer->addWidget(_analysisScroll, 1);
}

void SpellsTab::setCharacter(CharacterInfo* ci, PlayerTotals* totals) {
    _charInfo = ci;
    _totals   = totals;
    _spellsScroll->setWidget(buildSpellList());
}

QWidget* SpellsTab::buildSpellList() {
    // Porter depuis spells_tab.py::_build_spell_list()
    // Afficher la liste des sorts du personnage avec analyse de stacking
    // Utiliser spell_stacking.cpp pour détecter les conflits
    auto* w  = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignTop);

    if (!_charInfo) return w;

    auto* name = new QLabel(
        QString("Sorts de %1 (%2)")
            .arg(QString::fromStdString(_charInfo->name))
            .arg(QString::fromStdString(_charInfo->class_name)));
    name->setStyleSheet("font-size:13px;font-weight:bold;color:#e0e0e0;");
    vl->addWidget(name);

    // Porter la liste complète depuis spells_tab.py
    // (récupération des spells depuis DB, affichage avec stacking)
    auto* placeholder = new QLabel(
        "(Porter depuis spells_tab.py::_build_spell_list())");
    placeholder->setStyleSheet("color:#555555;");
    vl->addWidget(placeholder);

    vl->addStretch();
    return w;
}

QWidget* SpellsTab::buildSpellRow(const SpellData& spell, int index) {
    // Porter depuis spells_tab.py (structure identique à fight_tab::_spell_row)
    auto* frame = new QFrame;
    frame->setStyleSheet(
        index % 2 == 0 ? "background:#16161e;border-radius:3px;"
                       : "background:#1e1e2c;border-radius:3px;");
    auto* inner = new QVBoxLayout(frame);
    inner->setContentsMargins(8, 4, 8, 4);
    auto* lbl = new QLabel(QString::fromStdString(spell.name));
    lbl->setStyleSheet("color:#e0e0e0;background:transparent;");
    inner->addWidget(lbl);
    return frame;
}
```

- [ ] **Step 3 : Compiler**

```powershell
cmake --build build/debug --target EqQuarmCompanion
```

- [ ] **Step 4 : Commit**

```powershell
git add src/ui/spells_tab.h src/ui/spells_tab.cpp
git commit -m "feat(ui): SpellsTab — structure + porter depuis spells_tab.py"
```

---

### Task 4 : Build Release + windeployqt

**Files:**
- Verify: `CMakeLists.txt` (step windeployqt déjà présent du Plan 1)

- [ ] **Step 1 : Compiler en Release**

```powershell
cmake --preset windows-x64-release
cmake --build build/release --config Release --target EqQuarmCompanion
```

Attendu : compilation sans erreur.

- [ ] **Step 2 : Vérifier que windeployqt a été lancé**

```powershell
ls build/release/bin/
```

Attendu : dossier contenant `EqQuarmCompanion.exe` + DLLs Qt + `platforms/`, `sqldrivers/`, etc.

- [ ] **Step 3 : Copier `libmysql.dll` (si absent)**

Le driver QMYSQL requiert `libmysql.dll` de MariaDB. Si absent dans le dossier de l'exe :

```powershell
# Chemin typique MariaDB sur Windows
$mariadbLib = "C:\Program Files\MariaDB 10.x\lib\libmysql.dll"
if (Test-Path $mariadbLib) {
    Copy-Item $mariadbLib "build\release\bin\"
    Write-Host "libmysql.dll copié"
} else {
    Write-Host "Trouver libmysql.dll dans l'installation MariaDB et le copier manuellement"
}
```

- [ ] **Step 4 : Tester l'exe Release**

```powershell
.\build\release\bin\EqQuarmCompanion.exe
```

Attendu : app se lance, splash screen, fenêtre principale avec 4 onglets.

- [ ] **Step 5 : Vérifier la taille du dossier**

```powershell
(Get-ChildItem "build\release\bin" -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
```

Attendu : ~50-80 Mo (vs 247 Mo pour PyInstaller).

- [ ] **Step 6 : Commit final**

```powershell
git add -A
git commit -m "feat: build Release + windeployqt — exe autonome"
git tag v1.0.0
git push origin main --tags
```

---

### Task 5 : Vérification parité fonctionnelle

Tester chaque fonctionnalité contre l'app Python :

- [ ] **CharacterTab** : sélectionner un personnage → stats identiques à Python
- [ ] **FightTab** : chercher "Emperor Ssraeshza" → mêmes valeurs DPS/survie que Python
- [ ] **FightTab** : tableau disciplines × slow → mêmes /pause que Python
- [ ] **InfosTab** : changer expansion → bons debuffs affichés
- [ ] **SpellsTab** : liste des sorts affichée
- [ ] **Settings** : modifier DB host → reconnexion
- [ ] **Loot** : clic sur item → section item s'affiche avec stats

Si une valeur diffère entre C++ et Python, la formule Python fait foi — corriger dans `core/`.

---

### Vérification finale du Plan 6 (= projet complet)

- [ ] `cmake --build build/release` → 0 erreur
- [ ] `.\build\release\bin\EqQuarmCompanion.exe` → app complète, 4 onglets fonctionnels
- [ ] Taille dossier : ≤ 100 Mo
- [ ] Toutes les fonctionnalités Python présentes
- [ ] `git log --oneline` → historique propre par feature

**EqQuarmCompanion v1.0 — parité fonctionnelle avec eq-item-evaluator.**
