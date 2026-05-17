# EqQuarmCompanion — Plan 5 : Fight Tab

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prérequis :** Plan 4 terminé — MainWindow + CharacterTab fonctionnels.

**Goal:** Implémenter `FightTab` complet : recherche NPC, panneau gauche (combat/résists/abilities/loot), panneau droit (tableau DPS 2D disciplines×slow, CH rotation, résists joueur, sorts NPC, offense).

**Architecture:** Même design que le Python — deux `QScrollArea` côte à côte. Recherche NPC async via `QtConcurrent::run()`. Tableau DPS en `QGridLayout`. Porter exactement depuis `eq-item-evaluator/ui/fight_tab.py`.

**Tech Stack:** Qt6 Widgets, QtConcurrent, QGridLayout, QFutureWatcher.

**Référence Python :** `eq-item-evaluator/ui/fight_tab.py` (903 lignes). C'est le fichier de référence principal pour ce plan.

---

### Task 1 : Helpers UI partagés (`ui/ui_helpers.h`)

Ces helpers sont utilisés par `FightTab` et `CharacterTab`. Les extraire dans un header partagé.

**Files:**
- Create: `src/ui/ui_helpers.h`

- [ ] **Step 1 : Écrire `src/ui/ui_helpers.h`**

```cpp
#pragma once
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>
#include <vector>

// Couleurs palette (identiques à fight_tab.py)
inline const char* kGreen  = "#81c784";
inline const char* kOrange = "#ffb74d";
inline const char* kRed    = "#ef5350";

// Thèmes de section par couleur accent
inline std::pair<const char*, const char*> sectionTheme(const char* accent) {
    // {fond, bordure}
    if (QLatin1String(accent) == "#64b5f6") return {"#1a2236", "#3a4a6a"};
    if (QLatin1String(accent) == "#ffb74d") return {"#2a241a", "#5a4a3a"};
    if (QLatin1String(accent) == "#81c784") return {"#1a2a1e", "#3a5a4a"};
    if (QLatin1String(accent) == "#ef5350") return {"#2a1a1a", "#5a3a3a"};
    if (QLatin1String(accent) == "#ba68c8") return {"#241a2a", "#4a3a5a"};
    return {"#1a1a2e", "#3a3a5a"};
}

// QLabel header de section (small-caps)
inline QLabel* sectionLabel(const QString& text, const char* accent = "#888888") {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
        "font-size:10px;color:%1;font-variant:small-caps;"
        "font-weight:bold;border:none;background:transparent;").arg(accent));
    return lbl;
}

// QFrame de section avec fond coloré
inline std::pair<QFrame*, QVBoxLayout*> sectionFrame(const char* accent = "#64b5f6") {
    auto [bg, border] = sectionTheme(accent);
    auto* frame = new QFrame;
    frame->setStyleSheet(
        QString("QFrame{background:%1;border-radius:4px;border:1px solid %2;}")
            .arg(bg).arg(border));
    auto* inner = new QVBoxLayout(frame);
    inner->setContentsMargins(8, 6, 8, 6);
    inner->setSpacing(4);
    return {frame, inner};
}

// Grid de paires clé/valeur
inline QWidget* gridWidget(
    const std::vector<std::pair<QString,QString>>& pairs, int cols = 2)
{
    auto* w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(4);
    for (int i = 0; i < static_cast<int>(pairs.size()); ++i) {
        int row = i / cols, col = i % cols;
        auto* kl = new QLabel(QString("<span style='color:#888888'>%1</span>")
                              .arg(pairs[i].first));
        kl->setTextFormat(Qt::RichText);
        kl->setStyleSheet("background:transparent;");
        auto* vl2 = new QLabel(QString("<b>%1</b>").arg(pairs[i].second));
        vl2->setTextFormat(Qt::RichText);
        vl2->setStyleSheet("background:transparent;");
        g->addWidget(kl,  row, col*2);
        g->addWidget(vl2, row, col*2+1);
    }
    return w;
}
```

- [ ] **Step 2 : Commit**

```powershell
git add src/ui/ui_helpers.h
git commit -m "feat(ui): ui_helpers.h — sectionFrame, sectionLabel, gridWidget"
```

---

### Task 2 : `FightTab` — structure et panneau gauche

**Files:**
- Modify: `src/ui/fight_tab.h`
- Modify: `src/ui/fight_tab.cpp`

- [ ] **Step 1 : Réécrire `src/ui/fight_tab.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QFutureWatcher>
#include <QScrollArea>
#include <QWidget>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;

class FightTab : public QWidget {
    Q_OBJECT
public:
    FightTab(Config* config, NpcDatabase* npcDb,
             ItemDatabase* itemDb, QWidget* parent = nullptr);

    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);

signals:
    void itemSelected(const QString& itemName);

private slots:
    void doSearch();
    void onNpcSelected(int index);
    void onSearchDone();
    void onLootClicked(int itemId);
    void toggleLootSort();

private:
    void buildUi();
    QWidget* buildLeftPanel(const NpcData&);
    QWidget* buildRightPanel(const NpcData&);
    QWidget* buildDpsSlowTable(
        const std::vector<std::pair<QString,IncomingDamageResult>>& disciplines,
        const std::vector<std::tuple<QString,std::optional<float>,float>>& slowScenarios,
        float spDps, int hp);

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;
    CharacterInfo* _charInfo{};
    PlayerTotals*  _totals{};

    class SearchComboBox* _searchCombo;
    QScrollArea*          _leftScroll;
    QScrollArea*          _rightScroll;
    QWidget*              _itemSection{};
    QVBoxLayout*          _itemSectionLayout{};

    NpcData              _currentNpc;
    bool                 _hasNpc{false};
    std::vector<NpcData> _searchResults;
    QString              _lootSort{"chance"};  // "chance" | "score"

    QFutureWatcher<std::vector<NpcData>>* _searchWatcher;
};
```

- [ ] **Step 2 : Écrire la structure de `src/ui/fight_tab.cpp`**

```cpp
#include "ui/fight_tab.h"
#include "ui/ui_helpers.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "core/npc_analysis.h"
#include "core/spell_stats.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include <QtConcurrent/QtConcurrent>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

// ── Slow spells : (label, resist_type, atk_speed_pct) ─────────────────
static const std::vector<std::tuple<QString,int,int>> kSlowSpells = {
    {"Turgur", 1, 25},   // MR, atk_speed → 25 (75% slow)
    {"Plague", 5, 75},   // DR, atk_speed → 75 (25% slow)
};

static const int kChMaxClr  = 6;
static const float kChHpFloor = 0.30f;

FightTab::FightTab(Config* cfg, NpcDatabase* npcDb,
                    ItemDatabase* itemDb, QWidget* parent)
    : QWidget(parent), _config(cfg), _npcDb(npcDb), _itemDb(itemDb)
{
    _searchWatcher = new QFutureWatcher<std::vector<NpcData>>(this);
    connect(_searchWatcher, &QFutureWatcher<std::vector<NpcData>>::finished,
            this, &FightTab::onSearchDone);
    buildUi();
}

void FightTab::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // Barre de recherche
    _searchCombo = new SearchComboBox;
    _searchCombo->setFixedWidth(420);
    _searchCombo->lineEdit()->setPlaceholderText("Rechercher un NPC…");
    connect(_searchCombo, &SearchComboBox::popup_requested,
            this, &FightTab::doSearch);
    connect(_searchCombo, qOverload<int>(&QComboBox::activated),
            this, &FightTab::onNpcSelected);

    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(_searchCombo);
    searchRow->addStretch();
    outer->addLayout(searchRow);

    // Deux panneaux
    auto* cols = new QHBoxLayout;
    cols->setSpacing(10);
    _leftScroll  = new QScrollArea;
    _rightScroll = new QScrollArea;
    _leftScroll->setWidgetResizable(true);
    _rightScroll->setWidgetResizable(true);
    _leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lbl = new QLabel("Recherchez un NPC pour commencer");
    lbl->setAlignment(Qt::AlignCenter);
    _leftScroll->setWidget(lbl);
    auto* lbl2 = new QLabel("Recherchez un NPC pour commencer");
    lbl2->setAlignment(Qt::AlignCenter);
    _rightScroll->setWidget(lbl2);

    cols->addWidget(_leftScroll, 1);
    cols->addWidget(_rightScroll, 1);
    outer->addLayout(cols, 1);
}

void FightTab::setCharacter(CharacterInfo* ci, PlayerTotals* totals) {
    _charInfo = ci;
    _totals   = totals;
    if (_hasNpc) {
        _leftScroll->setWidget(buildLeftPanel(_currentNpc));
        _rightScroll->setWidget(buildRightPanel(_currentNpc));
    }
}
```

- [ ] **Step 3 : Implémenter `doSearch()` et `onSearchDone()` (async)**

```cpp
void FightTab::doSearch() {
    QString query = _searchCombo->lineEdit()->text().trimmed();
    if (query.length() < 2) return;

    // Async search via QtConcurrent
    auto future = QtConcurrent::run([this, query]() -> std::vector<NpcData> {
        auto results = _npcDb->searchNpcs(query.replace(' ', '_'));
        return std::vector<NpcData>(results.begin(), results.end());
    });
    _searchWatcher->setFuture(future);
}

void FightTab::onSearchDone() {
    _searchResults = _searchWatcher->result();
    _searchCombo->blockSignals(true);
    QString text = _searchCombo->lineEdit()->text();
    _searchCombo->clear();
    for (const auto& npc : _searchResults) {
        QString zone = QString::fromStdString(npc.zone_long_name);
        QString name = QString::fromStdString(npc.name).replace('_', ' ');
        _searchCombo->addItem(QString("%1 (%2)").arg(name).arg(zone));
    }
    _searchCombo->lineEdit()->setText(text);
    _searchCombo->blockSignals(false);
    if (!_searchResults.empty()) _searchCombo->showPopup();
}

void FightTab::onNpcSelected(int index) {
    if (index < 0 || index >= static_cast<int>(_searchResults.size())) return;
    _currentNpc = _searchResults[index];
    _hasNpc = true;
    _leftScroll->setWidget(buildLeftPanel(_currentNpc));
    _rightScroll->setWidget(buildRightPanel(_currentNpc));
}
```

- [ ] **Step 4 : Implémenter `buildLeftPanel()`**

```cpp
QWidget* FightTab::buildLeftPanel(const NpcData& npc) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop);

    // Header
    auto* name = new QLabel(QString::fromStdString(npc.name).replace('_', ' '));
    name->setStyleSheet("font-size:14px;font-weight:bold;color:#e0e0e0;");
    layout->addWidget(name);
    auto* sub = new QLabel(
        QString::fromStdString(npc.zone_long_name)
        + QString("  ·  Race %1  ·  Class %2").arg(npc.race).arg(npc.npc_class));
    sub->setStyleSheet("color:#888888;font-size:11px;");
    layout->addWidget(sub);

    // Section Combat
    auto [fCombat, flCombat] = sectionFrame("#64b5f6");
    flCombat->addWidget(sectionLabel("Combat", "#64b5f6"));
    flCombat->addWidget(gridWidget({
        {"Level",    QString::number(npc.level)},
        {"HP",       QString("%L1").arg(npc.hp)},
        {"AC",       QString::number(npc.ac)},
        {"ATK",      QString::number(npc.atk)},
        {"Accuracy", QString::number(npc.accuracy)},
        {"Min hit",  QString::number(npc.min_hit)},
        {"Max hit",  QString::number(npc.max_hit)},
        {"Delay",    QString("%1s").arg(npc.attack_delay / 10.0, 0, 'f', 1)},
        {"Hits/rnd", QString::number(std::max(1, npc.attack_count))},
    }, 1));
    layout->addWidget(fCombat);

    // Section Résists
    auto [fRes, flRes] = sectionFrame("#64b5f6");
    flRes->addWidget(sectionLabel("Résists", "#64b5f6"));
    auto npcResColor = [](int v) -> QString {
        const char* c = v >= 200 ? kRed : (v >= 100 ? kOrange : kGreen);
        return QString("<span style='color:%1'>%2</span>").arg(c).arg(v);
    };
    flRes->addWidget(gridWidget({
        {"MR", npcResColor(npc.mr)}, {"FR", npcResColor(npc.fr)},
        {"CR", npcResColor(npc.cr)}, {"DR", npcResColor(npc.dr)},
        {"PR", npcResColor(npc.pr)},
    }, 5));
    layout->addWidget(fRes);

    // Section Special Abilities
    auto [fAb, flAb] = sectionFrame("#ffb74d");
    flAb->addWidget(sectionLabel("Special Abilities", "#ffb74d"));
    auto abilities = decodeSpecialAbilities(npc.special_abilities);
    if (abilities.empty()) {
        auto* none = new QLabel("(none)");
        none->setStyleSheet("color:#555555;background:transparent;");
        flAb->addWidget(none);
    } else {
        for (const auto& ab : abilities) {
            const char* c = ab.severity == "red" ? kRed
                          : ab.severity == "orange" ? kOrange : "#888888";
            auto* lbl = new QLabel(QString::fromStdString(ab.tag));
            lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(c));
            flAb->addWidget(lbl);
        }
    }
    layout->addWidget(fAb);

    // Section Loot
    auto [fLoot, flLoot] = sectionFrame("#81c784");
    flLoot->addWidget(sectionLabel("Loot", "#81c784"));
    if (npc.loottable_id > 0) {
        auto loot = _npcDb->getNpcLoot(npc.loottable_id);
        if (loot.isEmpty()) {
            flLoot->addWidget(new QLabel("(no loot)"));
        } else {
            for (const auto& item : loot) {
                bool equippable = item.slots > 0;
                const char* nameColor = equippable ? "#e0e0e0" : "#555555";
                auto* btn = new QPushButton(
                    QString("%1  %2%").arg(
                        QString::fromStdString(item.name))
                        .arg(item.chance, 0, 'f', 0));
                btn->setFlat(true);
                btn->setStyleSheet(
                    QString("QPushButton{text-align:left;color:%1;"
                            "background:transparent;border:none;padding:0;}"
                            "QPushButton:hover{color:#81c784;}").arg(nameColor));
                if (item.item_id > 0)
                    connect(btn, &QPushButton::clicked,
                            [this, id=item.item_id]{ onLootClicked(id); });
                flLoot->addWidget(btn);
            }
        }
    } else {
        flLoot->addWidget(new QLabel("(no loot)"));
    }
    layout->addWidget(fLoot);

    layout->addStretch();
    return container;
}
```

- [ ] **Step 5 : Commit**

```powershell
git add src/ui/fight_tab.h src/ui/fight_tab.cpp src/ui/ui_helpers.h
git commit -m "feat(ui): FightTab — recherche NPC async + panneau gauche"
```

---

### Task 3 : `FightTab` — panneau droit (tableau DPS + CH rotation)

**Files:**
- Modify: `src/ui/fight_tab.cpp`

- [ ] **Step 1 : Implémenter `buildRightPanel()`**

```cpp
QWidget* FightTab::buildRightPanel(const NpcData& npc) {
    if (!_charInfo || !_totals) return new QLabel("Pas de personnage sélectionné");

    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop);

    // Header
    auto* hdr = new QLabel(
        QString("Analyse vs %1").arg(QString::fromStdString(_charInfo->name)));
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#e0e0e0;");
    layout->addWidget(hdr);
    auto* sub = new QLabel(
        QString::fromStdString(_charInfo->class_name)
        + QString(" — niveau %1").arg(_charInfo->level));
    sub->setStyleSheet("color:#888888;font-size:11px;");
    layout->addWidget(sub);

    // Récupérer les sorts NPC
    QList<SpellData> spells;
    if (npc.npc_spells_id > 0)
        spells = _npcDb->getNpcSpells(npc.npc_spells_id);
    std::vector<SpellData> spellsVec(spells.begin(), spells.end());

    float spDps = spellIncomingDps(spellsVec, *_totals,
                                    _charInfo->level, npc.level);
    int hp = _totals->hp.capped;

    // ── Section Incoming Damage ──────────────────────────────────────
    auto [fDmg, flDmg] = sectionFrame("#ef5350");
    flDmg->addWidget(sectionLabel("Incoming Damage", "#ef5350"));

    auto dmg = incomingDamage(npc, *_totals, _charInfo->class_name,
                               _charInfo->level, "none");
    flDmg->addWidget(gridWidget({
        {"Avg hit",   QString("~%1  (%2–%3)")
                        .arg(dmg.avg_hit, 0, 'f', 0)
                        .arg(npc.min_hit).arg(npc.max_hit)},
        {"NPC Power", QString::number(dmg.npc_offense)},
        {"Your Mit.", QString("%1  (d20→%2/20)").arg(dmg.player_mit).arg(dmg.exp_roll)},
        {"Mit. ↓",    QString("~%1%").arg(dmg.mitigation_pct, 0, 'f', 0)},
    }, 1));

    // Disciplines (Warrior seulement)
    using DiscRow = std::pair<QString, IncomingDamageResult>;
    std::vector<DiscRow> disciplines = {{"—", dmg}};
    if (_charInfo->class_name == "Warrior") {
        if (_charInfo->level >= 52) {
            disciplines.push_back({"Evasive",
                incomingDamage(npc, *_totals, "Warrior", _charInfo->level, "evasive")});
        }
        if (_charInfo->level >= 55) {
            disciplines.push_back({"Def.",
                incomingDamage(npc, *_totals, "Warrior", _charInfo->level, "defensive")});
        }
    }

    // Scénarios slow
    // Porter depuis fight_tab.py::_build_slow_scenarios()
    // (filtrer <10% land chance, intégrer debuffs depuis InfosTab::getResistDebuffs)
    using SlowScenario = std::tuple<QString, std::optional<float>, float>;
    std::vector<SlowScenario> slowScenarios = {{"No slow", std::nullopt, 100.f}};
    // TODO: porter _build_slow_scenarios() en C++ (même logique que Python)

    flDmg->addWidget(buildDpsSlowTable(disciplines, slowScenarios, spDps, hp));
    layout->addWidget(fDmg);

    // ── Section Your Resists ─────────────────────────────────────────
    auto [fRes, flRes] = sectionFrame("#64b5f6");
    flRes->addWidget(sectionLabel(
        QString("Your Resists (vs lv.%1 spells)").arg(npc.level), "#64b5f6"));
    auto ratings = resistRatings(npc, *_totals, _charInfo->level);
    auto resEntry = [](const ResistRating& r) -> QString {
        const char* c = r.rating == Rating::GOOD   ? kGreen
                      : r.rating == Rating::MEDIUM ? kOrange : kRed;
        const char* label = r.rating == Rating::GOOD   ? "GOOD"
                          : r.rating == Rating::MEDIUM ? "MEDIUM" : "LOW";
        return QString("%1  <span style='color:%2'>%3 ≈%4%</span>")
               .arg(r.value).arg(c).arg(label).arg(r.pct, 0, 'f', 0);
    };
    flRes->addWidget(gridWidget({
        {"MR", resEntry(ratings.mr)}, {"FR", resEntry(ratings.fr)},
        {"CR", resEntry(ratings.cr)}, {"DR", resEntry(ratings.dr)},
        {"PR", resEntry(ratings.pr)},
    }, 1));
    layout->addWidget(fRes);

    // ── Section NPC Spells ───────────────────────────────────────────
    auto [fSpells, flSpells] = sectionFrame("#ba68c8");
    flSpells->addWidget(sectionLabel("NPC Spells", "#ba68c8"));
    if (spells.isEmpty()) {
        flSpells->addWidget(new QLabel("(no spells)"));
    } else {
        int i = 0;
        for (const auto& sp : spells) {
            // Porter depuis fight_tab.py::_spell_row()
            auto* frame = new QFrame;
            frame->setStyleSheet(
                i % 2 == 0 ? "background:#16161e;border-radius:3px;"
                           : "background:#1e1e2c;border-radius:3px;");
            auto* inner = new QVBoxLayout(frame);
            inner->setContentsMargins(8, 4, 8, 4);
            inner->setSpacing(2);

            QString rt = QString::fromStdString(resistLabel(sp));
            QString st = QString::fromStdString(effectiveSpellType(sp));
            QString tt = QString::fromStdString(targetTypeLabel(sp.targettype));

            auto pct = spellResistPct(sp, *_totals, _charInfo->level, npc.level);
            QString resistBadge;
            if (pct) {
                const char* c = *pct >= 65.f ? kGreen : (*pct >= 35.f ? kOrange : kRed);
                resistBadge = QString(" <span style='color:%1'>%2%</span>")
                              .arg(c).arg(*pct, 0, 'f', 0);
            }

            auto* lbl1 = new QLabel(
                QString("<b>%1</b>  <span style='color:#888888'>(%2%3)</span>  %4"
                        "  <span style='color:#555555'>[%5]</span>")
                    .arg(QString::fromStdString(sp.name)).arg(rt).arg(resistBadge).arg(st).arg(tt));
            lbl1->setTextFormat(Qt::RichText);
            lbl1->setWordWrap(true);
            lbl1->setStyleSheet("background:transparent;");
            inner->addWidget(lbl1);

            QString summary = QString::fromStdString(formatSpellSummary(sp, npc.level));
            if (!summary.isEmpty()) {
                auto* lbl2 = new QLabel(summary);
                lbl2->setStyleSheet("color:#666666;font-size:10px;background:transparent;");
                lbl2->setWordWrap(true);
                inner->addWidget(lbl2);
            }
            flSpells->addWidget(frame);
            ++i;
        }
    }
    layout->addWidget(fSpells);

    // ── Section Your Offense ─────────────────────────────────────────
    auto [fOff, flOff] = sectionFrame("#ffb74d");
    flOff->addWidget(sectionLabel(
        QString("Your Offense (vs NPC resists)"), "#ffb74d"));
    auto off = offenseRatings(npc, *_totals, _charInfo->class_name);
    const char* mc = off.melee.rating == OffenseRating::EASY   ? kGreen
                   : off.melee.rating == OffenseRating::MEDIUM ? kOrange : kRed;
    const char* mLabel = off.melee.rating == OffenseRating::EASY   ? "EASY"
                       : off.melee.rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
    std::vector<std::pair<QString,QString>> offPairs = {
        {"ATK", QString("%1  <span style='color:%2'>%3 vs AC %4</span>")
                .arg(off.melee.player_atk).arg(mc).arg(mLabel).arg(off.melee.npc_ac)},
    };
    auto addSpellOff = [&](const char* key, const std::optional<SpellOffense>& so) {
        if (!so) return;
        const char* c = so->rating == OffenseRating::EASY   ? kGreen
                      : so->rating == OffenseRating::MEDIUM ? kOrange : kRed;
        const char* l = so->rating == OffenseRating::EASY   ? "EASY"
                      : so->rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
        offPairs.push_back({key,
            QString("<span style='color:%1'>%2 — NPC %3 %4</span>")
                .arg(c).arg(l).arg(key).arg(so->npc_resist)});
    };
    addSpellOff("MR", off.mr); addSpellOff("FR", off.fr);
    addSpellOff("CR", off.cr); addSpellOff("DR", off.dr);
    addSpellOff("PR", off.pr);
    flOff->addWidget(gridWidget(offPairs, 1));
    layout->addWidget(fOff);

    // Item detail section (peuplé au clic sur loot)
    _itemSection = new QWidget;
    _itemSection->setVisible(false);
    _itemSectionLayout = new QVBoxLayout(_itemSection);
    _itemSectionLayout->setContentsMargins(0, 4, 0, 0);
    layout->addWidget(_itemSection);

    layout->addStretch();
    return container;
}
```

- [ ] **Step 2 : Implémenter `buildDpsSlowTable()`**

```cpp
QWidget* FightTab::buildDpsSlowTable(
    const std::vector<std::pair<QString,IncomingDamageResult>>& disciplines,
    const std::vector<std::tuple<QString,std::optional<float>,float>>& slowScenarios,
    float spDps, int hp)
{
    auto* w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 6, 0, 0);
    g->setHorizontalSpacing(10);
    g->setVerticalSpacing(4);

    // En-têtes colonnes (row 0)
    for (int ci = 0; ci < static_cast<int>(slowScenarios.size()); ++ci) {
        const auto& [label, landPct, _atk] = slowScenarios[ci];
        QString hhtml;
        if (landPct) {
            const char* pc = *landPct >= 65.f ? kGreen
                           : *landPct >= 35.f ? kOrange : kRed;
            hhtml = QString("<span style='color:#ffb74d;font-size:9px;font-weight:bold'>"
                            "%1</span><br><span style='color:%2;font-size:8px'>"
                            "%3% land</span>").arg(label).arg(pc).arg(*landPct, 0, 'f', 0);
        } else {
            hhtml = QString("<span style='color:#ffb74d;font-size:9px;font-weight:bold'>"
                            "%1</span>").arg(label);
        }
        auto* hdr = new QLabel(hhtml);
        hdr->setTextFormat(Qt::RichText);
        hdr->setAlignment(Qt::AlignCenter);
        hdr->setStyleSheet("background:transparent;");
        g->addWidget(hdr, 0, ci + 1);
    }

    // Lignes disciplines
    for (int ri = 0; ri < static_cast<int>(disciplines.size()); ++ri) {
        const auto& [discLabel, dmg] = disciplines[ri];

        // Label de ligne
        QString lhtml;
        if (!dmg.disc_note.empty() && dmg.disc_mult < 1.f) {
            float red = (1.f - dmg.disc_mult) * 100.f;
            QString sfx = dmg.disc_note == "defensive" ? "dmg" : "hits";
            lhtml = QString("<span style='color:#888888;font-size:10px'>%1</span>"
                            "<br><span style='color:#555555;font-size:8px'>−%2% %3</span>")
                    .arg(discLabel).arg(red, 0, 'f', 0).arg(sfx);
        } else {
            lhtml = QString("<span style='color:#888888;font-size:10px'>%1</span>")
                    .arg(discLabel);
        }
        auto* rowLbl = new QLabel(lhtml);
        rowLbl->setTextFormat(Qt::RichText);
        rowLbl->setStyleSheet("background:transparent;");
        g->addWidget(rowLbl, ri + 1, 0);

        // Cellules
        for (int ci = 0; ci < static_cast<int>(slowScenarios.size()); ++ci) {
            const auto& [_label, landPct, atkSpeed] = slowScenarios[ci];
            float meleeSlowed = dmg.est_dps * (atkSpeed / 100.f);
            float total = meleeSlowed + spDps;
            float surv  = (total > 0.f && hp > 0) ? hp / total : 0.f;

            const char* sc = surv >= 120.f ? kGreen : (surv >= 60.f ? kOrange : kRed);
            QString survStr = surv > 0.f ? QString("~%1s").arg(surv, 0, 'f', 0) : "?";
            const char* dpsC = !landPct ? "#888888" : "#e0e0e0";

            // CH /pause
            QString chLine;
            if (hp > 0 && total > 0.f) {
                float safePause = hp * (1.f - kChHpFloor) * 10.f / total;
                int n = -1; int pause = 0;
                for (int nn = 1; nn <= kChMaxClr; ++nn) {
                    int p = static_cast<int>(std::round(100.f / nn));
                    if (p <= safePause) { n = nn; pause = p; break; }
                }
                if (n > 0) {
                    const char* chC = !landPct ? "#4a7aa8" : "#64b5f6";
                    chLine = QString("<br><span style='color:%1;font-size:9px'>"
                                     "/pause %2 · %3clr</span>")
                             .arg(chC).arg(pause).arg(n);
                } else {
                    chLine = QString("<br><span style='color:%1;font-size:9px'>"
                                     "&gt;%2clr</span>").arg(kRed).arg(kChMaxClr);
                }
            }

            auto* cell = new QLabel(
                QString("<span style='color:%1'>~%2/s</span>"
                        "<span style='color:%3'> · %4</span>%5")
                .arg(dpsC).arg(total, 0, 'f', 0).arg(sc).arg(survStr).arg(chLine));
            cell->setTextFormat(Qt::RichText);
            cell->setAlignment(Qt::AlignCenter);
            cell->setStyleSheet("background:transparent;font-size:10px;");
            g->addWidget(cell, ri + 1, ci + 1);
        }
    }

    // Note spell DPS
    if (spDps > 0.f) {
        auto* note = new QLabel(
            QString("<span style='color:#555555;font-size:9px'>"
                    "+ ~%1/s spell DPS (constant, non affecté par slow)</span>")
            .arg(spDps, 0, 'f', 0));
        note->setTextFormat(Qt::RichText);
        note->setStyleSheet("background:transparent;");
        g->addWidget(note,
                     static_cast<int>(disciplines.size()) + 1,
                     0, 1, static_cast<int>(slowScenarios.size()) + 1);
    }
    return w;
}
```

- [ ] **Step 3 : Implémenter `onLootClicked()`**

```cpp
void FightTab::onLootClicked(int itemId) {
    auto item = _itemDb->getItemById(itemId);
    if (!item) return;

    // Vider la section item
    while (_itemSectionLayout->count()) {
        auto* child = _itemSectionLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
    }

    // Porter buildItemContent() depuis fight_tab.py::_build_item_content()
    // Sections : Combat (HP/Mana/AC/ATK), Stats, Resists, Weapon, Effects
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{color:#333333;border:none;background:#333333;max-height:1px;}");
    _itemSectionLayout->addWidget(sep);

    auto* itemW = new QWidget;
    auto* vl = new QVBoxLayout(itemW);
    auto* nameL = new QLabel(QString::fromStdString(item->name));
    nameL->setStyleSheet("font-size:13px;font-weight:bold;color:#e0e0e0;");
    vl->addWidget(nameL);
    // Porter le reste depuis fight_tab.py::_build_item_content()

    _itemSectionLayout->addWidget(itemW);
    _itemSection->setVisible(true);
    if (item->slots > 0)
        emit itemSelected(QString::fromStdString(item->name));
}
```

- [ ] **Step 4 : Compiler et tester visuellement**

```powershell
cmake --build build/debug --target EqQuarmCompanion
.\build\debug\bin\EqQuarmCompanion.exe
```

Attendu :
- Rechercher "Ssraeshza" → dropdown avec résultats
- Cliquer un résultat → panneau gauche avec combat/resists/abilities/loot
- Panneau droit avec tableau DPS + résists + sorts NPC
- Cliquer un item dans Loot → section item apparaît en bas du panneau droit

- [ ] **Step 5 : Commit**

```powershell
git add src/ui/fight_tab.h src/ui/fight_tab.cpp
git commit -m "feat(ui): FightTab complet — tableau DPS 2D, CH rotation, loot, sorts NPC"
```

---

### Vérification finale du Plan 5

- [ ] Recherche NPC → résultats dans dropdown
- [ ] Sélection NPC → panneaux gauche et droit remplis
- [ ] Tableau DPS 2D visible avec colonnes slow et lignes discipline
- [ ] `/pause X · Nclr` visible dans chaque cellule (si HP > 0)
- [ ] Clic loot item → section item apparaît
- [ ] `cmake --build build/debug` → 0 erreur

**Prêt pour Plan 6 (Infos + Spells + packaging) quand les vérifications passent.**
