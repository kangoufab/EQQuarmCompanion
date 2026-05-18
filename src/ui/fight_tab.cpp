#include "ui/fight_tab.h"
#include "ui/stats_bar.h"
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

    // Bandeau stats en haut
    {
        auto* holder = new QWidget;
        holder->setStyleSheet("background: transparent;");
        _statsLayout = new QVBoxLayout(holder);
        _statsLayout->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(holder);
    }

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

    // Reconstruire le bandeau stats
    while (_statsLayout->count()) {
        auto* child = _statsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (_totals && _charInfo && _charInfo->level > 0) {
        std::string exp = _config->get("current_expansion");
        _statsLayout->addWidget(
            makePlayerStatsBar(*_totals, _charInfo->class_name, exp));
    }

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

// ── Right panel + DPS table ───────────────────────────────────────────────────

QWidget* FightTab::buildRightPanel(const NpcData& npc) {
    if (!_charInfo || !_totals) {
        auto* w = new QLabel("Selectionnez un personnage");
        w->setAlignment(Qt::AlignCenter);
        return w;
    }

    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop);

    auto* hdr = new QLabel(
        QString("Analyse vs %1").arg(QString::fromStdString(_charInfo->name)));
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#e0e0e0;");
    layout->addWidget(hdr);
    auto* sub = new QLabel(
        QString::fromStdString(_charInfo->class_name)
        + QString(" — niveau %1").arg(_charInfo->level));
    sub->setStyleSheet("color:#888888;font-size:11px;");
    layout->addWidget(sub);

    // Sorts NPC
    QList<SpellData> spells;
    if (npc.npc_spells_id > 0)
        spells = _npcDb->getNpcSpells(npc.npc_spells_id);
    std::vector<SpellData> spellsVec(spells.begin(), spells.end());

    float spDps = spellIncomingDps(spellsVec, *_totals, _charInfo->level, npc.level);
    int hp = _totals->hp.capped;

    // Incoming Damage
    auto [fDmg, flDmg] = sectionFrame("#ef5350");
    flDmg->addWidget(sectionLabel("Incoming Damage", "#ef5350"));
    auto dmg = incomingDamage(npc, *_totals, _charInfo->class_name,
                               _charInfo->level, "none");
    flDmg->addWidget(gridWidget({
        {"Avg hit",   QString("~%1  (%2-%3)").arg(dmg.avg_hit,0,'f',0).arg(npc.min_hit).arg(npc.max_hit)},
        {"NPC Power", QString::number(dmg.npc_offense)},
        {"Your Mit.", QString("%1  (d20->%2/20)").arg(dmg.player_mit).arg(dmg.exp_roll)},
        {"Mit. down", QString("~%1%").arg(dmg.mitigation_pct,0,'f',0)},
    }, 1));

    using DiscRow = std::pair<QString, IncomingDamageResult>;
    std::vector<DiscRow> disciplines = {{"—", dmg}};
    if (_charInfo->class_name == "Warrior") {
        if (_charInfo->level >= 52)
            disciplines.push_back({"Evasive",
                incomingDamage(npc, *_totals, "Warrior", _charInfo->level, "evasive")});
        if (_charInfo->level >= 55)
            disciplines.push_back({"Def.",
                incomingDamage(npc, *_totals, "Warrior", _charInfo->level, "defensive")});
    }

    using SlowScenario = std::tuple<QString, std::optional<float>, float>;
    std::vector<SlowScenario> slowScenarios = {{"No slow", std::nullopt, 100.f}};
    for (const auto& [name, resistType, atkSpeed] : kSlowSpells) {
        auto pct = slowLandPct(npc, _charInfo->level, resistType, 0);
        if (!pct || *pct < 10.f) continue;
        slowScenarios.push_back({name, *pct, static_cast<float>(atkSpeed)});
    }

    flDmg->addWidget(buildDpsSlowTable(disciplines, slowScenarios, spDps, hp));
    layout->addWidget(fDmg);

    // Your Resists
    auto [fRes, flRes] = sectionFrame("#64b5f6");
    flRes->addWidget(sectionLabel(
        QString("Your Resists (vs lv.%1 spells)").arg(npc.level), "#64b5f6"));
    auto ratings = resistRatings(npc, *_totals, _charInfo->level);
    auto resEntry = [](const ResistRating& r) -> QString {
        const char* c = r.rating == Rating::GOOD   ? kGreen
                      : r.rating == Rating::MEDIUM ? kOrange : kRed;
        const char* l = r.rating == Rating::GOOD   ? "GOOD"
                      : r.rating == Rating::MEDIUM ? "MEDIUM" : "LOW";
        return QString("%1  <span style='color:%2'>%3 ~%4%</span>")
               .arg(r.value).arg(c).arg(l).arg(r.pct, 0, 'f', 0);
    };
    flRes->addWidget(gridWidget({
        {"MR", resEntry(ratings.mr)}, {"FR", resEntry(ratings.fr)},
        {"CR", resEntry(ratings.cr)}, {"DR", resEntry(ratings.dr)},
        {"PR", resEntry(ratings.pr)},
    }, 1));
    layout->addWidget(fRes);

    // NPC Spells
    auto [fSpells, flSpells] = sectionFrame("#ba68c8");
    flSpells->addWidget(sectionLabel("NPC Spells", "#ba68c8"));
    if (spells.isEmpty()) {
        auto* nl = new QLabel("(no spells)"); nl->setStyleSheet("color:#555555;background:transparent;");
        flSpells->addWidget(nl);
    } else {
        int i = 0;
        for (const auto& sp : spells) {
            auto* frame = new QFrame;
            frame->setStyleSheet(i%2==0 ? "background:#16161e;border-radius:3px;"
                                        : "background:#1e1e2c;border-radius:3px;");
            auto* inner = new QVBoxLayout(frame);
            inner->setContentsMargins(8, 4, 8, 4); inner->setSpacing(2);

            QString rt = QString::fromStdString(resistLabel(sp));
            QString st = QString::fromStdString(effectiveSpellType(sp));
            QString tt = QString::fromStdString(targetTypeLabel(sp.targettype));

            QString resistBadge;
            auto pct = spellResistPct(sp, *_totals, _charInfo->level, npc.level);
            if (pct) {
                const char* c = *pct >= 65.f ? kGreen : (*pct >= 35.f ? kOrange : kRed);
                resistBadge = QString(" <span style='color:%1'>%2%</span>").arg(c).arg(*pct,0,'f',0);
            }

            auto* lbl1 = new QLabel(
                QString("<b>%1</b>  <span style='color:#888888'>(%2%3)</span>  %4"
                        "  <span style='color:#555555'>[%5]</span>")
                    .arg(QString::fromStdString(sp.name)).arg(rt).arg(resistBadge).arg(st).arg(tt));
            lbl1->setTextFormat(Qt::RichText); lbl1->setWordWrap(true);
            lbl1->setStyleSheet("background:transparent;");
            inner->addWidget(lbl1);
            flSpells->addWidget(frame);
            ++i;
        }
    }
    layout->addWidget(fSpells);

    // Your Offense
    auto [fOff, flOff] = sectionFrame("#ffb74d");
    flOff->addWidget(sectionLabel("Your Offense (vs NPC resists)", "#ffb74d"));
    auto off = offenseRatings(npc, *_totals, _charInfo->class_name);
    const char* mc = off.melee.rating == OffenseRating::EASY   ? kGreen
                   : off.melee.rating == OffenseRating::MEDIUM ? kOrange : kRed;
    const char* ml = off.melee.rating == OffenseRating::EASY   ? "EASY"
                   : off.melee.rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
    std::vector<std::pair<QString,QString>> offPairs = {
        {"ATK", QString("%1  <span style='color:%2'>%3 vs AC %4</span>")
                .arg(off.melee.player_atk).arg(mc).arg(ml).arg(off.melee.npc_ac)},
    };
    auto addSpellOff = [&](const char* key, const std::optional<SpellOffense>& so) {
        if (!so) return;
        const char* c = so->rating == OffenseRating::EASY   ? kGreen
                      : so->rating == OffenseRating::MEDIUM ? kOrange : kRed;
        const char* l = so->rating == OffenseRating::EASY   ? "EASY"
                      : so->rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
        offPairs.push_back({key,
            QString("<span style='color:%1'>%2 - NPC %3 %4</span>")
                .arg(c).arg(l).arg(key).arg(so->npc_resist)});
    };
    addSpellOff("MR", off.mr); addSpellOff("FR", off.fr);
    addSpellOff("CR", off.cr); addSpellOff("DR", off.dr);
    addSpellOff("PR", off.pr);
    flOff->addWidget(gridWidget(offPairs, 1));
    layout->addWidget(fOff);

    // Item detail section
    _itemSection = new QWidget; _itemSection->setVisible(false);
    _itemSectionLayout = new QVBoxLayout(_itemSection);
    _itemSectionLayout->setContentsMargins(0, 4, 0, 0);
    layout->addWidget(_itemSection);

    layout->addStretch();
    return container;
}

QWidget* FightTab::buildDpsSlowTable(
    const std::vector<std::pair<QString,IncomingDamageResult>>& disciplines,
    const std::vector<std::tuple<QString,std::optional<float>,float>>& slowScenarios,
    float spDps, int hp)
{
    auto* w = new QWidget; w->setStyleSheet("background:transparent;");
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 6, 0, 0);
    g->setHorizontalSpacing(10); g->setVerticalSpacing(4);

    for (int ci = 0; ci < static_cast<int>(slowScenarios.size()); ++ci) {
        const auto& [label, landPct, _atk] = slowScenarios[ci];
        QString hhtml;
        if (landPct) {
            const char* pc = *landPct >= 65.f ? kGreen : *landPct >= 35.f ? kOrange : kRed;
            hhtml = QString("<span style='color:#ffb74d;font-size:9px;font-weight:bold'>%1</span>"
                            "<br><span style='color:%2;font-size:8px'>%3% land</span>")
                    .arg(label).arg(pc).arg(*landPct, 0, 'f', 0);
        } else {
            hhtml = QString("<span style='color:#ffb74d;font-size:9px;font-weight:bold'>%1</span>").arg(label);
        }
        auto* hdr = new QLabel(hhtml);
        hdr->setTextFormat(Qt::RichText); hdr->setAlignment(Qt::AlignCenter);
        hdr->setStyleSheet("background:transparent;");
        g->addWidget(hdr, 0, ci + 1);
    }

    for (int ri = 0; ri < static_cast<int>(disciplines.size()); ++ri) {
        const auto& [discLabel, dmg] = disciplines[ri];

        QString lhtml;
        if (!dmg.disc_note.empty() && dmg.disc_mult < 1.f) {
            float red = (1.f - dmg.disc_mult) * 100.f;
            QString sfx = dmg.disc_note == "defensive" ? "dmg" : "hits";
            lhtml = QString("<span style='color:#888888;font-size:10px'>%1</span>"
                            "<br><span style='color:#555555;font-size:8px'>-%2% %3</span>")
                    .arg(discLabel).arg(red, 0, 'f', 0).arg(sfx);
        } else {
            lhtml = QString("<span style='color:#888888;font-size:10px'>%1</span>").arg(discLabel);
        }
        auto* rowLbl = new QLabel(lhtml);
        rowLbl->setTextFormat(Qt::RichText); rowLbl->setStyleSheet("background:transparent;");
        g->addWidget(rowLbl, ri + 1, 0);

        for (int ci = 0; ci < static_cast<int>(slowScenarios.size()); ++ci) {
            const auto& [_label, landPct, atkSpeed] = slowScenarios[ci];
            float meleeSlowed = dmg.est_dps * (atkSpeed / 100.f);
            float total = meleeSlowed + spDps;
            float surv  = (total > 0.f && hp > 0) ? hp / total : 0.f;

            const char* sc = surv >= 120.f ? kGreen : (surv >= 60.f ? kOrange : kRed);
            QString survStr = surv > 0.f ? QString("~%1s").arg(surv, 0, 'f', 0) : "?";
            const char* dpsC = !landPct ? "#888888" : "#e0e0e0";

            QString chLine;
            if (hp > 0 && total > 0.f) {
                float safePause = hp * (1.f - kChHpFloor) * 10.f / total;
                int n = -1, pause = 0;
                for (int nn = 1; nn <= kChMaxClr; ++nn) {
                    int p = static_cast<int>(std::round(100.f / nn));
                    if (p <= safePause) { n = nn; pause = p; break; }
                }
                if (n > 0) {
                    const char* chC = !landPct ? "#4a7aa8" : "#64b5f6";
                    chLine = QString("<br><span style='color:%1;font-size:9px'>/pause %2 · %3clr</span>")
                             .arg(chC).arg(pause).arg(n);
                } else {
                    chLine = QString("<br><span style='color:%1;font-size:9px'>&gt;%2clr</span>")
                             .arg(kRed).arg(kChMaxClr);
                }
            }

            auto* cell = new QLabel(
                QString("<span style='color:%1'>~%2/s</span><span style='color:%3'> · %4</span>%5")
                .arg(dpsC).arg(total, 0, 'f', 0).arg(sc).arg(survStr).arg(chLine));
            cell->setTextFormat(Qt::RichText); cell->setAlignment(Qt::AlignCenter);
            cell->setStyleSheet("background:transparent;font-size:10px;");
            g->addWidget(cell, ri + 1, ci + 1);
        }
    }

    if (spDps > 0.f) {
        auto* note = new QLabel(
            QString("<span style='color:#555555;font-size:9px'>+ ~%1/s spell DPS (constant)</span>")
            .arg(spDps, 0, 'f', 0));
        note->setTextFormat(Qt::RichText); note->setStyleSheet("background:transparent;");
        g->addWidget(note, static_cast<int>(disciplines.size())+1, 0,
                     1, static_cast<int>(slowScenarios.size())+1);
    }
    return w;
}

void FightTab::onLootClicked(int itemId) {
    auto item = _itemDb->getItemById(itemId);
    if (!item) return;

    while (_itemSectionLayout->count()) {
        auto* child = _itemSectionLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
    }

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{color:#333333;border:none;background:#333333;max-height:1px;}");
    _itemSectionLayout->addWidget(sep);

    auto* itemW = new QWidget;
    auto* vl = new QVBoxLayout(itemW);
    auto* nameL = new QLabel(QString::fromStdString(item->name));
    nameL->setStyleSheet("font-size:13px;font-weight:bold;color:#e0e0e0;");
    vl->addWidget(nameL);

    // Stats
    std::vector<std::pair<QString,QString>> stats;
    if (item->hp)   stats.push_back({"HP",   QString::number(item->hp)});
    if (item->mana) stats.push_back({"Mana", QString::number(item->mana)});
    if (item->ac)   stats.push_back({"AC",   QString::number(item->ac)});
    if (item->atk)  stats.push_back({"ATK",  QString::number(item->atk)});
    if (!stats.empty()) {
        auto [f, fl] = sectionFrame("#ef5350");
        fl->addWidget(sectionLabel("Combat", "#ef5350"));
        fl->addWidget(gridWidget(stats, 2));
        vl->addWidget(f);
    }

    _itemSectionLayout->addWidget(itemW);
    _itemSection->setVisible(true);
    if (item->item_slots > 0)  // <-- item_slots not slots
        emit itemSelected(QString::fromStdString(item->name));
}
