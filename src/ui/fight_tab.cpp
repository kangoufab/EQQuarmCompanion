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
#include <QLineEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

static const std::vector<std::tuple<QString,int,int>> kSlowSpells = {
    {"Turgur", 1, 25},
    {"Plague", 5, 75},
};
static const int   kChMaxClr  = 6;
static const float kChHpFloor = 0.30f;

// ── Constructor ───────────────────────────────────────────────────────────────

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

    _searchCombo = new SearchComboBox;
    _searchCombo->setFixedWidth(420);
    _searchCombo->lineEdit()->setPlaceholderText("Rechercher un NPC...");
    connect(_searchCombo, &SearchComboBox::popup_requested, this, &FightTab::doSearch);
    connect(_searchCombo, qOverload<int>(&QComboBox::activated),
            this, &FightTab::onNpcSelected);

    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(_searchCombo);
    searchRow->addStretch();
    outer->addLayout(searchRow);

    auto* cols = new QHBoxLayout;
    cols->setSpacing(10);
    _leftScroll  = new QScrollArea;
    _rightScroll = new QScrollArea;
    _leftScroll->setWidgetResizable(true);
    _rightScroll->setWidgetResizable(true);
    _leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lbl  = new QLabel("Recherchez un NPC pour commencer");
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
    _charInfo = ci; _totals = totals;
    if (_hasNpc) {
        _leftScroll->setWidget(buildLeftPanel(_currentNpc));
        _rightScroll->setWidget(buildRightPanel(_currentNpc));
    }
}

// ── Search ────────────────────────────────────────────────────────────────────

void FightTab::doSearch() {
    QString query = _searchCombo->lineEdit()->text().trimmed();
    if (query.length() < 2) return;
    auto future = QtConcurrent::run([this, query]() -> std::vector<NpcData> {
        auto results = _npcDb->searchNpcs(QString(query).replace(' ', '_'));
        return std::vector<NpcData>(results.begin(), results.end());
    });
    _searchWatcher->setFuture(future);
}

void FightTab::onSearchDone() {
    _searchResults = _searchWatcher->result();
    _searchCombo->blockSignals(true);
    QString text = _searchCombo->lineEdit()->text();
    _searchCombo->clear();
    for (const auto& npc : _searchResults)
        _searchCombo->addItem(
            QString("%1 (%2)")
                .arg(QString::fromStdString(npc.name).replace('_', ' '))
                .arg(QString::fromStdString(npc.zone_long_name)));
    _searchCombo->lineEdit()->setText(text);
    _searchCombo->blockSignals(false);
    if (!_searchResults.empty()) static_cast<QComboBox*>(_searchCombo)->showPopup();
}

void FightTab::onNpcSelected(int index) {
    if (index < 0 || index >= static_cast<int>(_searchResults.size())) return;
    _currentNpc = _searchResults[index];
    _hasNpc = true;
    _leftScroll->setWidget(buildLeftPanel(_currentNpc));
    _rightScroll->setWidget(buildRightPanel(_currentNpc));
}

void FightTab::toggleLootSort() {
    _lootSort = (_lootSort == "chance") ? "score" : "chance";
    if (_hasNpc) _leftScroll->setWidget(buildLeftPanel(_currentNpc));
}

// ── Left panel ────────────────────────────────────────────────────────────────

QWidget* FightTab::buildLeftPanel(const NpcData& npc) {
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop);

    auto* name = new QLabel(QString::fromStdString(npc.name).replace('_', ' '));
    name->setStyleSheet("font-size:14px;font-weight:bold;color:#e0e0e0;");
    layout->addWidget(name);
    auto* sub = new QLabel(
        QString::fromStdString(npc.zone_long_name)
        + QString("  *  Race %1  *  Class %2").arg(npc.race).arg(npc.npc_class));
    sub->setStyleSheet("color:#888888;font-size:11px;");
    layout->addWidget(sub);

    auto [fCombat, flCombat] = sectionFrame("#64b5f6");
    flCombat->addWidget(sectionLabel("Combat", "#64b5f6"));
    flCombat->addWidget(gridWidget({
        {"Level",    QString::number(npc.level)},
        {"HP",       QString::number(npc.hp)},
        {"AC",       QString::number(npc.ac)},
        {"ATK",      QString::number(npc.atk)},
        {"Accuracy", QString::number(npc.accuracy)},
        {"Min hit",  QString::number(npc.min_hit)},
        {"Max hit",  QString::number(npc.max_hit)},
        {"Delay",    QString("%1s").arg(npc.attack_delay / 10.0, 0, 'f', 1)},
        {"Hits/rnd", QString::number(std::max(1, npc.attack_count))},
    }, 1));
    layout->addWidget(fCombat);

    auto [fRes, flRes] = sectionFrame("#64b5f6");
    flRes->addWidget(sectionLabel("Resists", "#64b5f6"));
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

    auto [fAb, flAb] = sectionFrame("#ffb74d");
    flAb->addWidget(sectionLabel("Special Abilities", "#ffb74d"));
    auto abilities = decodeSpecialAbilities(npc.special_abilities);
    if (abilities.empty()) {
        auto* none = new QLabel("(none)"); none->setStyleSheet("color:#555555;background:transparent;");
        flAb->addWidget(none);
    } else {
        for (const auto& ab : abilities) {
            const char* c = ab.severity == "red" ? kRed : ab.severity == "orange" ? kOrange : "#888888";
            auto* lbl = new QLabel(QString::fromStdString(ab.tag));
            lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(c));
            flAb->addWidget(lbl);
        }
    }
    layout->addWidget(fAb);

    auto [fLoot, flLoot] = sectionFrame("#81c784");
    flLoot->addWidget(sectionLabel("Loot", "#81c784"));
    if (npc.loottable_id > 0) {
        auto loot = _npcDb->getNpcLoot(npc.loottable_id);
        if (loot.isEmpty()) {
            flLoot->addWidget(new QLabel("(no loot)"));
        } else {
            for (const auto& item : loot) {
                bool equippable = item.item_slots > 0;
                const char* nameColor = equippable ? "#e0e0e0" : "#555555";
                auto* btn = new QPushButton(
                    QString("%1  %2%").arg(QString::fromStdString(item.name))
                                      .arg(item.chance, 0, 'f', 0));
                btn->setFlat(true);
                btn->setStyleSheet(
                    QString("QPushButton{text-align:left;color:%1;background:transparent;border:none;padding:0;}"
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

// ── Right panel + DPS table (stubs for Task 3) ───────────────────────────────

QWidget* FightTab::buildRightPanel(const NpcData&) {
    auto* w = new QLabel("Sélectionner un personnage d'abord");
    w->setAlignment(Qt::AlignCenter);
    return w;
}

QWidget* FightTab::buildDpsSlowTable(
    const std::vector<std::pair<QString,IncomingDamageResult>>&,
    const std::vector<std::tuple<QString,std::optional<float>,float>>&,
    float, int)
{
    return new QWidget;
}

void FightTab::onLootClicked(int) {}
