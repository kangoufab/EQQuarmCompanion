#include "ui/fight_tab.h"
#include "ui/item_card.h"
#include "ui/ui_helpers.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "core/npc_analysis.h"
#include "core/spell_stats.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include <QtConcurrent/QtConcurrent>
#include <QComboBox>
#include <QCompleter>
#include <QFrame>
#include <QLineEdit>
#include <QStringListModel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include "ui/infos_data.h"
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>

static const std::vector<std::tuple<QString,int,int>> kSlowSpells = {
    {"Turgur", 1, 25},
    {"Plague", 5, 75},
};
static const int   kChMaxClr  = 12;
static const float kChHpFloor = 0.30f;

static const std::vector<std::pair<const char*, int>> kSlotBits = {
    {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
    {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
    {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
    {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
    {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
    {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
};

static QString decodeSlots(int bits) {
    QStringList out;
    for (auto& [n, b] : kSlotBits) if (bits & b) out << n;
    return out.join(", ");
}


static bool charCanEquip(const LootItem& item, const CharacterInfo* ci) {
    if (!ci || item.item_slots == 0) return false;
    static const std::map<std::string, int> CLASS_BIT = {
        {"Warrior",1},{"Cleric",2},{"Paladin",4},{"Ranger",8},
        {"Shadowknight",16},{"Druid",32},{"Monk",64},{"Bard",128},
        {"Rogue",256},{"Shaman",512},{"Necromancer",1024},{"Wizard",2048},
        {"Magician",4096},{"Enchanter",8192},{"Beastlord",16384},{"Berserker",32768},
    };
    static const std::map<int, int> RACE_BIT = {
        {1,1},{2,2},{3,4},{4,8},{5,16},{6,32},{7,64},{8,128},
        {9,256},{10,512},{11,1024},{12,2048},{128,4096},{130,8192},{330,16384},{331,16384},
    };
    auto cit = CLASS_BIT.find(ci->class_name);
    if (cit == CLASS_BIT.end()) return false;
    if ((item.classes & 32767) != 32767 && !(item.classes & cit->second)) return false;
    auto rit = RACE_BIT.find(ci->race);
    if (rit != RACE_BIT.end() && item.races != 65535 && item.races != 0
        && !(item.races & rit->second)) return false;
    if (item.reqlevel > 0 && item.reqlevel > ci->level) return false;
    return true;
}

// Index dans SpellData::classes (0-based, ordre EQ standard) ; 255 = classe ne peut pas l'utiliser
static const std::map<std::string, int> SPELL_CLASS_INDEX = {
    {"Warrior",0},{"Cleric",1},{"Paladin",2},{"Ranger",3},
    {"Shadowknight",4},{"Druid",5},{"Monk",6},{"Bard",7},
    {"Rogue",8},{"Shaman",9},{"Necromancer",10},{"Wizard",11},
    {"Magician",12},{"Enchanter",13},{"Beastlord",14},{"Berserker",15},
};

static bool charCanUseSpell(const SpellData& spell, const CharacterInfo* ci) {
    if (!ci) return false;
    auto it = SPELL_CLASS_INDEX.find(ci->class_name);
    return it != SPELL_CLASS_INDEX.end() && spell.classes[it->second] != 255;
}


static QString valueDelta(int v, int r) {
    if (!r) return QString::number(v);
    int d = v - r;
    if (!d) return QString::number(v);
    const char* c = d > 0 ? kGreen : kRed;
    return QString("%1  <span style='color:%2'>%3%4</span>")
           .arg(v).arg(c).arg(d > 0 ? "+" : "").arg(d);
}

static QString valueDeltaInvert(int v, int r) {
    if (!r) return QString::number(v);
    int d = v - r;
    if (!d) return QString::number(v);
    const char* c = d < 0 ? kGreen : kRed;
    return QString("%1  <span style='color:%2'>%3%4</span>")
           .arg(v).arg(c).arg(d > 0 ? "+" : "").arg(d);
}

static QWidget* tileGrid(const std::vector<std::pair<QString,QString>>& tiles, int cols = 4) {
    static const std::map<std::string, std::pair<const char*,const char*>> tileAccents = {
        {"HP",  {kBgTileNoLimit, kGreen}},
        {"ATK", {"#2a1e1e",     kRed}},
    };
    auto* w = new QWidget; w->setStyleSheet("background:transparent;");
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0,0,0,0); g->setSpacing(4);
    for (int i = 0; i < static_cast<int>(tiles.size()); ++i) {
        int row = i / cols, col = i % cols;
        const auto& [label, value] = tiles[i];
        auto it = tileAccents.find(label.toStdString());
        const char* bg = kBgTile, *tc = kTextBase;
        if (it != tileAccents.end()) { bg = it->second.first; tc = it->second.second; }
        auto* frame = new QFrame;
        frame->setStyleSheet(QString("background:%1;border-radius:3px;").arg(bg));
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(6,3,6,3); fl->setSpacing(1);
        auto* nl = new QLabel(label);
        nl->setStyleSheet(QString("font-size:13px;color:%1;background:transparent;").arg(kTextTileKey));
        auto* vl = new QLabel(value);
        vl->setTextFormat(Qt::RichText);
        vl->setStyleSheet(QString("font-weight:bold;color:%1;background:transparent;").arg(tc));
        fl->addWidget(nl); fl->addWidget(vl);
        g->addWidget(frame, row, col);
    }
    return w;
}

static const char* spellTypeColor(const std::string& type) {
    static const std::set<std::string> danger  = {"charm","mez","fear","slow","stun"};
    static const std::set<std::string> harmful = {"dot","nuke","root","snare","lifetap","dispel"};
    static const std::set<std::string> helpful = {"buff","heal","pet","rune","cure","rez","combat buff"};
    if (danger.count(type))  return kRed;
    if (harmful.count(type)) return kOrange;
    if (helpful.count(type)) return kGreen;
    return kTextSecondary;
}

static std::vector<std::tuple<QString,std::optional<float>,float>>
buildSlowScenarios(const NpcData& npc, int playerLevel,
                   const std::map<std::string,int>& debuffs)
{
    std::vector<std::tuple<QString,std::optional<float>,float>> scenarios = {
        {"No slow", std::nullopt, 100.f}
    };
    for (const auto& [name, resistType, atkSpeed] : kSlowSpells) {
        if (!slowLandPct(npc, playerLevel, resistType))  // immune
            continue;
        NpcData npcD = npc;
        if (resistType == 1) {
            auto it = debuffs.find("MR");
            if (it != debuffs.end()) npcD.mr = std::max(0, npc.mr + it->second);
        } else if (resistType == 5) {
            auto it = debuffs.find("DR");
            if (it != debuffs.end()) npcD.dr = std::max(0, npc.dr + it->second);
        }
        auto pct = slowLandPct(npcD, playerLevel, resistType);
        if (!pct || *pct < 10.f) continue;
        scenarios.push_back({name, *pct, static_cast<float>(atkSpeed)});
    }
    return scenarios;
}

static int itemStatVal(const ItemData& item, const std::string& k) {
    if (k=="hp")         return item.hp;
    if (k=="mana")       return item.mana;
    if (k=="ac")         return item.ac;
    if (k=="atk")        return item.atk;
    if (k=="astr")       return item.astr;
    if (k=="asta")       return item.asta;
    if (k=="adex")       return item.adex;
    if (k=="aagi")       return item.aagi;
    if (k=="aint")       return item.aint;
    if (k=="awis")       return item.awis;
    if (k=="acha")       return item.acha;
    if (k=="mr")         return item.mr;
    if (k=="fr")         return item.fr;
    if (k=="cr")         return item.cr;
    if (k=="dr")         return item.dr;
    if (k=="pr")         return item.pr;
    if (k=="haste")      return item.haste;
    if (k=="hp_regen")   return item.hp_regen;
    if (k=="mana_regen") return item.mana_regen;
    if (k=="damage")     return item.damage;
    return 0;
}

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
    _searchCombo->setMinimumWidth(280);
    _searchCombo->setMaximumWidth(500);
    _searchCombo->setAccessibleName("Rechercher un NPC");
    _searchCombo->lineEdit()->setPlaceholderText("Rechercher un NPC...");

    _searchModel = new QStringListModel(this);
    auto* completer = new QCompleter(_searchModel, this);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    _searchCombo->setCompleter(completer);

    connect(_searchCombo, &SearchComboBox::popup_requested, this, &FightTab::doSearch);
    connect(completer, QOverload<const QString&>::of(&QCompleter::activated),
            this, [this](const QString& text) {
        for (int i = 0; i < static_cast<int>(_searchResults.size()); ++i) {
            QString display = QString("%1 (%2)")
                .arg(QString::fromStdString(_searchResults[i].name).replace('_', ' '))
                .arg(QString::fromStdString(_searchResults[i].zone_long_name));
            if (display == text) { onNpcSelected(i); return; }
        }
    });

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

void FightTab::refreshStats() {
    if (_hasNpc && _charInfo && _totals)
        _rightScroll->setWidget(buildRightPanel(_currentNpc));
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
    QStringList names;
    for (const auto& npc : _searchResults)
        names << QString("%1 (%2)")
                     .arg(QString::fromStdString(npc.name).replace('_', ' '))
                     .arg(QString::fromStdString(npc.zone_long_name));
    _searchModel->setStringList(names);
    if (!_searchResults.empty())
        _searchCombo->completer()->complete();
}

void FightTab::onNpcSelected(int index) {
    if (index < 0 || index >= static_cast<int>(_searchResults.size())) return;
    const NpcData& partial = _searchResults[index];
    auto full = _npcDb->getNpcById(partial.id);
    if (!full) return;
    full->zone_long_name = partial.zone_long_name;  // preserve zone from search
    _currentNpc = std::move(*full);
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
    name->setStyleSheet(QString("font-size:14px;font-weight:bold;color:%1;").arg(kTextPrimary));
    layout->addWidget(name);

    // Tags: RAID / QUEST / ENCOUNTER (E)
    if (npc.raid_target || npc.is_quest || npc.encounter) {
        auto* tagsW = new QWidget; tagsW->setStyleSheet("background:transparent;");
        auto* tagsL = new QHBoxLayout(tagsW);
        tagsL->setSpacing(4); tagsL->setContentsMargins(0,0,0,0);
        auto addTag = [&](const char* label, const char* color) {
            auto* t = new QLabel(label);
            t->setStyleSheet(
                QString("color:%1;font-size:12px;font-weight:bold;border:1px solid %1;"
                        "border-radius:3px;padding:1px 5px;background:transparent;").arg(color));
            tagsL->addWidget(t);
        };
        if (npc.raid_target) addTag("RAID",      "#ef5350");
        if (npc.is_quest)    addTag("QUEST",     "#ffb74d");
        if (npc.encounter)   addTag("ENCOUNTER", "#ba68c8");
        tagsL->addStretch();
        layout->addWidget(tagsW);
    }

    // Zone type tag (F)
    static const std::map<int, const char*> ZONE_TYPE_LABELS = {
        {0, "Donjon"}, {1, "Ext\xc3\xa9rieur"}, {2, "Ville"},
    };
    QString zoneTxt = QString::fromStdString(npc.zone_long_name);
    if (npc.zone_type >= 0) {
        auto zt = ZONE_TYPE_LABELS.find(npc.zone_type);
        if (zt != ZONE_TYPE_LABELS.end())
            zoneTxt += QString("  <span style='color:#555555;font-size:12px;'>[%1]</span>")
                        .arg(QString::fromUtf8(zt->second));
    }
    auto* sub = new QLabel(
        zoneTxt + QString("  *  %1  *  %2")
            .arg(QString::fromStdString(npc.race_name.empty()
                ? "Race " + std::to_string(npc.race) : npc.race_name))
            .arg(QString::fromStdString(npc.class_name.empty()
                ? "Classe " + std::to_string(npc.npc_class) : npc.class_name)));
    sub->setTextFormat(Qt::RichText);
    sub->setStyleSheet(QString("color:%1;font-size:13px;").arg(kTextSecondary));
    layout->addWidget(sub);

    auto [fCombat, flCombat] = sectionFrame("#64b5f6");
    flCombat->addWidget(sectionLabel("Combat", "#64b5f6"));
    flCombat->addWidget(tileGrid({
        {"Level",    QString::number(npc.level)},
        {"HP",       QLocale(QLocale::English).toString(npc.hp)},
        {"AC",       QString::number(npc.ac)},
        {"ATK",      QString::number(npc.atk)},
        {"Accuracy", QString::number(npc.accuracy)},
        {"Min hit",  QString::number(npc.min_hit)},
        {"Max hit",  QString::number(npc.max_hit)},
        {"Delay",    QString("%1s").arg(npc.attack_delay / 10.0, 0, 'f', 1)},
        {"Hits/rnd", QString::number(std::max(1, npc.attack_count))},
        {"Avoidance",  npc.avoidance  > 0 ? QString("+%1").arg(npc.avoidance)           : QString("—")},
        {"Slow Mit.",  npc.slow_mitigation > 0 ? QString("%1%").arg(npc.slow_mitigation) : QString("—")},
    }, 4));
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
    {
        auto* hdrRow = new QHBoxLayout;
        hdrRow->setContentsMargins(0,0,0,0);
        hdrRow->addWidget(sectionLabel("Loot", "#81c784"));
        hdrRow->addStretch();
        auto* sortBtn = new QPushButton(_lootSort == "chance" ? "Score ↓" : "% ↓");
        sortBtn->setFlat(true);
        sortBtn->setStyleSheet(
            "QPushButton{color:#555555;font-size:13px;background:transparent;border:none;}"
            "QPushButton:hover{color:#81c784;}");
        connect(sortBtn, &QPushButton::clicked, this,
                [this]{ QTimer::singleShot(0, this, &FightTab::toggleLootSort); });
        hdrRow->addWidget(sortBtn);
        auto* hdrW = new QWidget; hdrW->setStyleSheet("background:transparent;");
        hdrW->setLayout(hdrRow);
        flLoot->addWidget(hdrW);
    }
    if (npc.loottable_id > 0) {
        auto loot = _npcDb->getNpcLoot(npc.loottable_id);
        if (_lootSort == "score" && _charInfo) {
            auto weights = _config->getClassWeights(_charInfo->class_name);
            if (!weights.empty()) {
                QList<int> lootIds;
                for (const auto& li : loot) lootIds.append(li.item_id);
                auto itemMap = _itemDb->getItemsByIds(lootIds);
                std::vector<float> scores(loot.size(), 0.f);
                for (int i = 0; i < (int)loot.size(); ++i) {
                    auto it = itemMap.find(loot[i].item_id);
                    if (it != itemMap.end())
                        for (auto& [k, w] : weights)
                            scores[i] += itemStatVal(*it, k) * w;
                }
                std::vector<int> idx(loot.size());
                std::iota(idx.begin(), idx.end(), 0);
                std::sort(idx.begin(), idx.end(), [&](int a, int b){ return scores[a] > scores[b]; });
                QList<LootItem> sorted;
                for (int i : idx) sorted.push_back(loot[i]);
                loot = sorted;
            }
        }
        if (loot.isEmpty()) {
            flLoot->addWidget(new QLabel("(no loot)"));
        } else {
            for (const auto& item : loot) {
                const char* nameColor;
                if (item.scrolleffect > 0) {
                    // Spell scroll: usability depends on the granted spell's class list,
                    // not on item_slots (scrolls have no equip slots) — same 2-tier
                    // graying as equippable items (usable vs. not-usable-by-class).
                    auto spell = _itemDb->getSpellById(item.scrolleffect);
                    bool usable = spell && charCanUseSpell(*spell, _charInfo);
                    nameColor = usable ? kTextPrimary : kTextSecondary;
                } else {
                    bool equippable = item.item_slots > 0;
                    bool usable = charCanEquip(item, _charInfo);
                    // 3 levels: usable by char = bright, has slots but wrong class/race/level = dim, no slots = very dim
                    nameColor = usable ? kTextPrimary : (equippable ? kTextSecondary : kTextDim);
                }

                auto* row = new QWidget; row->setStyleSheet("background:transparent;");
                auto* rowL = new QHBoxLayout(row);
                rowL->setContentsMargins(0,0,0,0); rowL->setSpacing(4);

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
                rowL->addWidget(btn);

                if (item.nodrop) {
                    auto* ndLbl = new QLabel("NO DROP");
                    ndLbl->setStyleSheet(
                        "color:#ef5350;font-size:11px;font-weight:bold;"
                        "background:transparent;border:none;");
                    rowL->addWidget(ndLbl);
                }
                rowL->addStretch();
                flLoot->addWidget(row);
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
    hdr->setStyleSheet(QString("font-size:14px;font-weight:bold;color:%1;").arg(kTextPrimary));
    layout->addWidget(hdr);
    auto* sub = new QLabel(
        QString::fromStdString(_charInfo->class_name)
        + QString(" — niveau %1").arg(_charInfo->level));
    sub->setStyleSheet(QString("color:%1;font-size:13px;").arg(kTextSecondary));
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
    flDmg->addWidget(sectionLabelPrimary("Incoming Damage", kRed));
    auto dmg = incomingDamage(npc, *_totals, _charInfo->class_name,
                               _charInfo->level, "none");
    std::vector<std::pair<QString,QString>> dmgRows = {
        {"Avg hit",   QString("~%1  (%2-%3)").arg(dmg.avg_hit,0,'f',0).arg(npc.min_hit).arg(npc.max_hit)},
        {"NPC Power", QString::number(dmg.npc_offense)},
        {"Your Mit.", QString("%1  (d20->%2/20)").arg(dmg.player_mit).arg(dmg.exp_roll)},
        {"Mit. down", QString("~%1%").arg(dmg.mitigation_pct,0,'f',0)},
    };
    if (spDps > 0.f)
        dmgRows.push_back({"Spell DPS",
            QString("<span style='color:#ba68c8'>~%1/s</span>").arg(spDps,0,'f',0)});
    flDmg->addWidget(gridWidget(dmgRows, 1));

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

    std::string expansion = _config->get("current_expansion");
    if (expansion.empty()) expansion = "Luclin";
    auto slowScenarios = buildSlowScenarios(npc, _charInfo->level, getResistDebuffs(expansion));

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
        return QString("%1  <span style='color:%2'>%3 ≈%4%</span>")
               .arg(r.value).arg(c).arg(l).arg(r.pct, 0, 'f', 0);
    };
    flRes->addWidget(gridWidget({
        {"MR", resEntry(ratings.mr)}, {"FR", resEntry(ratings.fr)},
        {"CR", resEntry(ratings.cr)}, {"DR", resEntry(ratings.dr)},
        {"PR", resEntry(ratings.pr)},
    }, 1));

    // Resist recommendation — what's needed to reach GOOD
    // Formula: pct = val/(val + npc.level*2) >= 0.65  →  val >= ceil(26*level/7)
    {
        int threshold = (26 * npc.level + 6) / 7;
        QStringList needs;
        auto check = [&](const char* label, int cur, const ResistRating& r) {
            if (r.rating != Rating::GOOD && cur < threshold)
                needs << QString("+%1 %2").arg(threshold - cur).arg(label);
        };
        check("MR", _totals->mr, ratings.mr);
        check("FR", _totals->fr, ratings.fr);
        check("CR", _totals->cr, ratings.cr);
        check("DR", _totals->dr, ratings.dr);
        check("PR", _totals->pr, ratings.pr);
        if (!needs.isEmpty()) {
            auto* lbl = new QLabel(
                QString("Pour GOOD (%1+) : %2")
                    .arg(threshold).arg(needs.join("  ·  ")));
            lbl->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;").arg(kTextMuted));
            lbl->setWordWrap(true);
            flRes->addWidget(lbl);
        }
    }

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
            std::string stStr = effectiveSpellType(sp);
            const char* typeColor = spellTypeColor(stStr);
            QString tt = QString::fromStdString(targetTypeLabel(sp.targettype));

            QString resistBadge;
            auto pct = spellResistPct(sp, *_totals, _charInfo->level, npc.level);
            if (pct) {
                const char* c = *pct >= 65.f ? kGreen : (*pct >= 35.f ? kOrange : kRed);
                resistBadge = QString(" <span style='color:%1'>%2%</span>").arg(c).arg(*pct,0,'f',0);
            }

            auto* lbl1 = new QLabel(
                QString("<b>%1</b>  <span style='color:%7'>(%2%3)</span>"
                        "  <span style='color:%4'>%5</span>"
                        "  <span style='color:%8'>[%6]</span>")
                    .arg(QString::fromStdString(sp.name)).arg(rt).arg(resistBadge)
                    .arg(typeColor).arg(QString::fromStdString(stStr)).arg(tt)
                    .arg(kTextSecondary).arg(kTextDim));
            lbl1->setTextFormat(Qt::RichText); lbl1->setWordWrap(true);
            lbl1->setStyleSheet("background:transparent;");
            inner->addWidget(lbl1);

            QString summary = QString::fromStdString(formatSpellSummary(sp, npc.level));
            if (!summary.isEmpty()) {
                auto* lbl2 = new QLabel(summary);
                lbl2->setStyleSheet(QString("color:%1;font-size:14px;background:transparent;").arg(kTextMuted));
                lbl2->setWordWrap(true);
                inner->addWidget(lbl2);
            }

            flSpells->addWidget(frame);
            ++i;
        }
    }
    layout->addWidget(fSpells);

    // Your Offense
    auto [fOff, flOff] = sectionFrame("#ffb74d");
    flOff->addWidget(sectionLabelPrimary("Your Offense (vs NPC resists)", kOrange));
    auto off = offenseRatings(npc, *_totals, _charInfo->class_name);
    const char* mc = off.melee.rating == OffenseRating::EASY   ? kGreen
                   : off.melee.rating == OffenseRating::MEDIUM ? kOrange : kRed;
    const char* ml = off.melee.rating == OffenseRating::EASY   ? "EASY"
                   : off.melee.rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
    QString acStr = off.melee.npc_avoidance > 0
        ? QString("AC %1 +%2av").arg(off.melee.npc_ac).arg(off.melee.npc_avoidance)
        : QString("AC %1").arg(off.melee.npc_ac);
    std::vector<std::pair<QString,QString>> offPairs = {
        {"ATK", QString("%1  <span style='color:%2'>%3 vs %4</span>")
                .arg(off.melee.player_atk).arg(mc).arg(ml).arg(acStr)},
    };
    auto addSpellOff = [&](const char* label, const char* key, const std::optional<SpellOffense>& so) {
        if (!so) return;
        const char* c = so->rating == OffenseRating::EASY   ? kGreen
                      : so->rating == OffenseRating::MEDIUM ? kOrange : kRed;
        const char* l = so->rating == OffenseRating::EASY   ? "EASY"
                      : so->rating == OffenseRating::MEDIUM ? "MEDIUM" : "HARD";
        offPairs.push_back({label,
            QString("<span style='color:%1'>%2 — NPC %3 %4</span>")
                .arg(c).arg(l).arg(key).arg(so->npc_resist)});
    };
    addSpellOff("Magic",   "MR", off.mr); addSpellOff("Fire",    "FR", off.fr);
    addSpellOff("Cold",    "CR", off.cr); addSpellOff("Disease", "DR", off.dr);
    addSpellOff("Poison",  "PR", off.pr);
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
            hhtml = QString("<span style='color:%1;font-size:13px;font-weight:bold'>%2</span>"
                            "<br><span style='color:%3;font-size:14px'>%4% land</span>")
                    .arg(kOrange).arg(label).arg(pc).arg(*landPct, 0, 'f', 0);
        } else {
            hhtml = QString("<span style='color:%1;font-size:13px;font-weight:bold'>%2</span>")
                    .arg(kOrange).arg(label);
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
            lhtml = QString("<span style='color:%1;font-size:14px'>%2</span>"
                            "<br><span style='color:%3;font-size:14px'>-%4% %5</span>")
                    .arg(kTextSecondary).arg(discLabel).arg(kTextMuted).arg(red, 0, 'f', 0).arg(sfx);
        } else {
            lhtml = QString("<span style='color:%1;font-size:14px'>%2</span>")
                    .arg(kTextSecondary).arg(discLabel);
        }
        auto* rowLbl = new QLabel(lhtml);
        rowLbl->setTextFormat(Qt::RichText); rowLbl->setStyleSheet("background:transparent;");
        g->addWidget(rowLbl, ri + 1, 0);

        for (int ci = 0; ci < static_cast<int>(slowScenarios.size()); ++ci) {
            const auto& [_label, landPct, atkSpeed] = slowScenarios[ci];
            float minTotal = dmg.min_dps * (atkSpeed / 100.f) + spDps;
            float maxTotal = dmg.max_dps * (atkSpeed / 100.f) + spDps;

            float meleeSlowed = dmg.est_dps * (atkSpeed / 100.f);
            float total = meleeSlowed + spDps;
            float surv  = (total > 0.f && hp > 0) ? static_cast<float>(hp) / total : 0.f;

            const char* sc = surv >= 120.f ? kGreen : (surv >= 60.f ? kOrange : kRed);
            QString survStr = surv > 0.f ? QString("~%1s").arg(surv, 0, 'f', 0) : "?";

            // Plage survie min–max (affichée si écart > 10%)
            {
                float survMin = (maxTotal > 0.f && hp > 0)
                                ? static_cast<float>(hp) / maxTotal : 0.f;
                float survMax = (minTotal > 0.f && hp > 0)
                                ? static_cast<float>(hp) / minTotal : 0.f;
                if (survMin > 0.f && survMax > survMin * 1.10f) {
                    QString capStr = (survMax >= 999.f)
                        ? "&gt;999"
                        : QString::number(static_cast<int>(survMax));
                    survStr += QString(" <span style='color:%1;font-size:12px'>"
                                       "[%2\xe2\x80\x93%3s]</span>").arg(kTextMuted)
                               .arg(static_cast<int>(survMin)).arg(capStr);
                }
            }

            const char* dpsC = !landPct ? kTextSecondary : kTextPrimary;

            QString chLine;
            if (hp > 0 && maxTotal > 0.f) {
                // safeP = floor of max gap (tenths) before HP hits 30% at maxTotal DPS
                int safeP = static_cast<int>(hp * (1.f - kChHpFloor) * 10.f / maxTotal);
                int n = -1;
                for (int nn = 1; nn <= kChMaxClr; ++nn) {
                    int minP = static_cast<int>(std::ceil(100.f / nn));
                    if (minP <= safeP) { n = nn; break; }
                }
                if (n > 0) {
                    const char* chC = !landPct ? kBorderAccent : kAccentBlue;
                    chLine = QString("<br><span style='color:%1;font-size:13px'>/pause %2 · %3clr</span>")
                             .arg(chC).arg(safeP).arg(n);
                } else {
                    chLine = QString("<br><span style='color:%1;font-size:13px'>&gt;%2clr</span>")
                             .arg(kRed).arg(kChMaxClr);
                }
            }
            QString rangeStr = (minTotal > 0.f && maxTotal > minTotal)
                ? QString("<br><span style='color:%1;font-size:12px'>[%2–%3/s]</span>")
                    .arg(kTextMuted).arg(minTotal, 0, 'f', 0).arg(maxTotal, 0, 'f', 0)
                : QString();

            auto* cell = new QLabel(
                QString("<span style='color:%1'>~%2/s</span><span style='color:%3'> · %4</span>%5%6")
                .arg(dpsC).arg(total, 0, 'f', 0).arg(sc).arg(survStr).arg(chLine).arg(rangeStr));
            cell->setTextFormat(Qt::RichText); cell->setAlignment(Qt::AlignCenter);
            cell->setStyleSheet("background:transparent;font-size:14px;");
            g->addWidget(cell, ri + 1, ci + 1);
        }
    }

    return w;
}

void FightTab::onLootClicked(int itemId) {
    auto item = _itemDb->getItemById(itemId);
    if (!item) return;
    _lootItem = *item;

    // Detect slots — prioritise slots that have something equipped
    _lootSlots.clear();
    if (_charInfo && item->item_slots) {
        for (const auto& [slotName, bit] : kSlotBits) {
            if (!(item->item_slots & bit)) continue;
            for (const auto& [eqSlot, eqId] : _charInfo->equipped)
                if (eqSlot == slotName) { _lootSlots.push_back(QString::fromStdString(slotName)); break; }
        }
    }
    if (_lootSlots.empty() && item->item_slots) {
        for (const auto& [slotName, bit] : kSlotBits)
            if (item->item_slots & bit)
                _lootSlots.push_back(QString::fromStdString(slotName));
    }

    showLootForSlot(_lootSlots.empty() ? "" : _lootSlots[0]);
    if (item->item_slots > 0)
        emit itemSelected(QString::fromStdString(item->name));
}

void FightTab::showLootForSlot(const QString& slot) {
    while (_itemSectionLayout->count()) {
        auto* child = _itemSectionLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
    }

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{color:#333333;border:none;background:#333333;max-height:1px;}");
    _itemSectionLayout->addWidget(sep);

    // Slot buttons (only when multiple slots)
    if (_lootSlots.size() > 1) {
        auto* slotW = new QWidget; slotW->setStyleSheet("background:transparent;");
        auto* slotL = new QHBoxLayout(slotW);
        slotL->setContentsMargins(0, 2, 0, 4); slotL->setSpacing(4);
        auto* lbl = new QLabel("Slot :");
        lbl->setStyleSheet("color:#888;font-size:12px;background:transparent;");
        slotL->addWidget(lbl);
        for (const auto& s : _lootSlots) {
            auto* btn = new QPushButton(s);
            bool cur = (s == slot);
            btn->setEnabled(!cur);
            btn->setStyleSheet(cur
                ? "QPushButton{background:#2a3a5a;border:1px solid #64b5f6;border-radius:3px;"
                  "color:#64b5f6;padding:1px 8px;font-size:12px;}"
                : "QPushButton{background:#1a2236;border:1px solid #3a4a6a;border-radius:3px;"
                  "color:#c0c0c0;padding:1px 8px;font-size:12px;}"
                  "QPushButton:hover{border-color:#64b5f6;color:#64b5f6;}");
            connect(btn, &QPushButton::clicked, [this, s]() { showLootForSlot(s); });
            slotL->addWidget(btn);
        }
        slotL->addStretch();
        _itemSectionLayout->addWidget(slotW);
    }

    // Equipped item in this slot for delta comparison
    std::optional<ItemData> equippedItem;
    if (_charInfo && !slot.isEmpty()) {
        for (const auto& [eqSlot, eqId] : _charInfo->equipped) {
            if (QString::fromStdString(eqSlot) == slot) {
                equippedItem = _itemDb->getItemById(eqId);
                break;
            }
        }
    }

    std::map<int,SpellData> spells;
    auto loadSpell = [&](int id) {
        if (id > 0 && !spells.count(id)) {
            auto s = _itemDb->getSpellById(id);
            if (s) spells[id] = std::move(*s);
        }
    };
    loadSpell(_lootItem.worneffect);
    loadSpell(_lootItem.focuseffect);
    loadSpell(_lootItem.proceffect);
    loadSpell(_lootItem.clickeffect);
    loadSpell(_lootItem.scrolleffect);

    // Build spell name lookup for SE_LimitSpell (SPA 139) in tooltips
    std::map<int,QString> limitNames;
    for (auto& [sid_key, sp] : spells) {
        for (int i = 0; i < 12; ++i) {
            if (sp.spa[i] == 254) break;
            if (sp.spa[i] == 139) {
                int sid = std::abs(sp.effect_base_value[i]);
                if (sid > 0 && !limitNames.count(sid)) {
                    auto ref = _itemDb->getSpellById(sid);
                    if (ref) limitNames[sid] = QString::fromStdString(ref->name);
                }
            }
        }
    }

    _itemSectionLayout->addWidget(
        makeItemCard(&_lootItem, equippedItem ? &*equippedItem : nullptr,
                     spells.empty() ? nullptr : &spells,
                     {},
                     limitNames.empty() ? nullptr : &limitNames));
    _itemSection->setVisible(true);
}
