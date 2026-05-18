#include "ui/spells_tab.h"
#include "core/config.h"
#include "core/spell_stacking.h"
#include "core/spell_stats.h"
#include "core/stats_calculator.h"
#include "db/item_database.h"

#undef slots  // Qt macro conflicts

#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

// ── Constantes ────────────────────────────────────────────────────────────

static const std::vector<std::string> BUFF_CASTER_CLASSES = {
    "Cleric","Paladin","Ranger","Druid","Shaman","Bard",
    "Necromancer","Wizard","Magician","Enchanter","Beastlord","Shadowknight",
};

static const std::map<std::string, int> SPELL_CLASS_ID_UI = {
    {"Warrior",1},{"Cleric",2},{"Paladin",3},{"Ranger",4},
    {"Shadowknight",5},{"Druid",6},{"Monk",7},{"Bard",8},
    {"Rogue",9},{"Shaman",10},{"Necromancer",11},{"Wizard",12},
    {"Magician",13},{"Enchanter",14},{"Beastlord",15},{"Berserker",16},
};

static const int MAX_BUFF_SLOTS = 15;

// Catégories stats pour l'affichage
struct CatColors { const char* bg; const char* border; const char* accent; };
static const std::vector<std::pair<std::string, std::vector<std::string>>> STAT_CATS = {
    {"Melee",   {"astr","adex","atk","haste"}},
    {"Range",   {"adex","atk","haste"}},
    {"Defense", {"asta","aagi","hp","ac","hp_regen","mr","fr","cr","dr","pr"}},
    {"Sorts",   {"aint","awis","acha","mana","mana_regen"}},
};
static const std::map<std::string, const char*> CAT_LABELS_UI = {
    {"Melee",   "M\xc3\xaalée"},
    {"Range",   "Distance"},
    {"Defense", "D\xc3\xa9" "fense"},
    {"Sorts",   "Sorts"},
};
static const std::map<std::string, CatColors> CAT_COLORS_UI = {
    {"Melee",   {"#2a1a1a","#5a3a3a","#e57373"}},
    {"Range",   {"#2a241a","#5a4a3a","#ffb74d"}},
    {"Defense", {"#1a2236","#3a4a6a","#64b5f6"}},
    {"Sorts",   {"#241a2a","#4a3a5a","#ba68c8"}},
};
static const std::map<std::string, std::set<std::string>> CLASS_CATS_UI = {
    {"Warrior",      {"Melee","Defense"}},
    {"Cleric",       {"Defense","Sorts"}},
    {"Paladin",      {"Melee","Defense","Sorts"}},
    {"Ranger",       {"Melee","Range","Defense","Sorts"}},
    {"Shadowknight", {"Melee","Defense","Sorts"}},
    {"Druid",        {"Defense","Sorts"}},
    {"Monk",         {"Melee","Range","Defense"}},
    {"Bard",         {"Melee","Range","Defense","Sorts"}},
    {"Rogue",        {"Melee","Range","Defense"}},
    {"Shaman",       {"Defense","Sorts"}},
    {"Necromancer",  {"Defense","Sorts"}},
    {"Wizard",       {"Defense","Sorts"}},
    {"Magician",     {"Defense","Sorts"}},
    {"Enchanter",    {"Defense","Sorts"}},
    {"Beastlord",    {"Melee","Defense","Sorts"}},
};

static int getStatVal(const std::string& stat, const PlayerTotals& t) {
    if (stat=="astr")      return t.str_v;
    if (stat=="asta")      return t.sta;
    if (stat=="adex")      return t.dex;
    if (stat=="aagi")      return t.agi;
    if (stat=="aint")      return t.int_v;
    if (stat=="awis")      return t.wis;
    if (stat=="acha")      return t.cha;
    if (stat=="hp")        return t.hp.capped;
    if (stat=="mana")      return t.mana.capped;
    if (stat=="ac")        return t.mitigation;
    if (stat=="atk")       return t.atk;
    if (stat=="haste")     return t.haste;
    if (stat=="hp_regen")  return t.hp_regen;
    if (stat=="mana_regen") return t.mana_regen;
    if (stat=="mr")        return t.mr;
    if (stat=="fr")        return t.fr;
    if (stat=="cr")        return t.cr;
    if (stat=="dr")        return t.dr;
    if (stat=="pr")        return t.pr;
    return 0;
}

static bool isAttr(const std::string& s)   {
    return s=="astr"||s=="asta"||s=="adex"||s=="aagi"||s=="awis"||s=="aint"||s=="acha";
}
static bool isResist(const std::string& s) {
    return s=="mr"||s=="fr"||s=="cr"||s=="dr"||s=="pr";
}

// ── SpellsTab constructor ─────────────────────────────────────────────────

SpellsTab::SpellsTab(Config* config, ItemDatabase* itemDb, QWidget* parent)
    : QWidget(parent), _config(config), _itemDb(itemDb)
{
    buildUi();
}

// ── setCharacter ──────────────────────────────────────────────────────────

void SpellsTab::setCharacter(CharacterInfo* charInfo, PlayerTotals* baseTotals,
                              const std::map<std::string, ItemData>& equipped)
{
    _charInfo      = charInfo;
    _baseTotals    = baseTotals;
    _equippedItems = equipped;

    // Reset state
    _activeBuffs.clear();
    _conflicts.clear();
    _currentClassSpells.clear();
    _currentClass.clear();

    if (_classList && _classList->count() > 0)
        _classList->setCurrentRow(0); // triggers onClassSelected
    else
        refreshStats();
}

// ── buildUi ───────────────────────────────────────────────────────────────

void SpellsTab::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    outer->addWidget(scroll);

    auto* container = new QWidget;
    container->setStyleSheet("background: #0f1624;");
    scroll->setWidget(container);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // En-tête
    {
        _headerLabel = new QLabel(QString::fromUtf8("Sorts b\xc3\xa9" "n\xc3\xa9" "fiques (0/%1)").arg(MAX_BUFF_SLOTS));
        _headerLabel->setStyleSheet(
            "font-size: 10px; color: #888; font-variant: small-caps; "
            "border: none; background: transparent;");
    }

    // Section principale : liste classes + panel sorts
    {
        auto* sectionFrame = new QFrame;
        sectionFrame->setStyleSheet(
            "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
        auto* sv = new QVBoxLayout(sectionFrame);
        sv->setContentsMargins(6, 4, 6, 4);
        sv->setSpacing(4);
        sv->addWidget(_headerLabel);

        auto* panels = new QHBoxLayout;
        panels->setSpacing(6);

        // Liste des classes
        _classList = new QListWidget;
        _classList->setFixedWidth(130);
        _classList->setStyleSheet(
            "QListWidget { background: #1a1a2e; border: 1px solid #3a4a6a; font-size: 10px; }"
            "QListWidget::item { color: #888; padding: 3px 6px; }"
            "QListWidget::item:selected { background: #3a4a6a; color: #64b5f6; }");
        for (const auto& cls : BUFF_CASTER_CLASSES)
            _classList->addItem(QString::fromStdString(cls));
        panels->addWidget(_classList);

        // Panel droit (sorts)
        auto* rightScroll = new QScrollArea;
        rightScroll->setWidgetResizable(true);
        rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        rightScroll->setStyleSheet(
            "QScrollArea { border: 1px solid #3a4a6a; background: #141428; }");
        _rightInner = new QWidget;
        _rightInner->setStyleSheet("background: transparent;");
        _rightLayout = new QVBoxLayout(_rightInner);
        _rightLayout->setContentsMargins(4, 4, 4, 4);
        _rightLayout->setSpacing(2);
        rightScroll->setWidget(_rightInner);
        panels->addWidget(rightScroll);

        sv->addLayout(panels);
        layout->addWidget(sectionFrame);
    }

    // Zone stats
    {
        _statsHolder = new QWidget;
        _statsHolder->setStyleSheet("background: transparent;");
        _statsLayout = new QVBoxLayout(_statsHolder);
        _statsLayout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(_statsHolder);
    }

    layout->addStretch();

    // Connexion après construction
    connect(_classList, &QListWidget::currentRowChanged,
            this, &SpellsTab::onClassSelected);
}

// ── Expansion max level ───────────────────────────────────────────────────

int SpellsTab::expansionMaxLevel() const {
    std::string exp = _config->get("current_expansion");
    if (exp == "Classic")         return 50;
    if (exp == "Planes of Power") return 65;
    return 60; // Kunark / Velious / Luclin
}

// ── onClassSelected ───────────────────────────────────────────────────────

void SpellsTab::onClassSelected(int row) {
    if (row < 0 || row >= (int)BUFF_CASTER_CLASSES.size()) return;
    _currentClass = BUFF_CASTER_CLASSES[row];
    loadSpellsForClass(_currentClass);
}

// ── loadSpellsForClass ────────────────────────────────────────────────────

void SpellsTab::loadSpellsForClass(const std::string& className) {
    _currentClassSpells.clear();
    if (!_itemDb) { rebuildRightPanel(); return; }

    int maxLvl = expansionMaxLevel();
    auto spells = _itemDb->getBeneficialSpellsByClass(
        QString::fromStdString(className), maxLvl);

    for (auto& sd : spells) {
        // Exclure les sorts self-only (targettype==6) sauf si classe == classe perso
        bool isSelfOnly = (sd.targettype == 6);
        if (isSelfOnly && className != (_charInfo ? _charInfo->class_name : ""))
            continue;
        _currentClassSpells.push_back(sd);
    }

    rebuildRightPanel();
}

// ── rebuildRightPanel ─────────────────────────────────────────────────────

void SpellsTab::rebuildRightPanel()
{
    // Vider le panel
    while (_rightLayout->count()) {
        auto* child = _rightLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    int charLevel = _charInfo ? _charInfo->level : 0;
    int atCap = (int)_activeBuffs.size() >= MAX_BUFF_SLOTS;

    // IDs des buffs actifs
    std::set<int> selectedIds;
    for (auto& b : _activeBuffs) selectedIds.insert(b.spell.id);

    // Map loser_id → winner name pour le tooltip
    std::map<int, QString> winnerNames;
    for (auto& [loserId, winnerId] : _conflicts) {
        for (auto& b : _activeBuffs)
            if (b.spell.id == winnerId) {
                winnerNames[loserId] = QString::fromStdString(b.spell.name);
                break;
            }
    }

    // Pool de sorts actifs pour findConflictInPool
    std::vector<SpellData> activePool;
    for (auto& b : _activeBuffs) activePool.push_back(b.spell);

    for (const auto& spell : _currentClassSpells) {
        auto it = SPELL_CLASS_ID_UI.find(_currentClass);
        int minLvl = (it != SPELL_CLASS_ID_UI.end() && it->second <= 16)
                     ? spell.classes[it->second - 1] : 0;

        bool isSelected = selectedIds.count(spell.id);
        bool isBlocked  = _conflicts.count(spell.id);

        QString label = QString::fromStdString(spell.name)
                        + QString(" (Niv. %1)").arg(minLvl);

        auto* cb = new QCheckBox(label);
        cb->setChecked(isSelected);

        if (isBlocked) {
            QString wname = winnerNames.count(spell.id) ? winnerNames[spell.id] : "un autre sort";
            cb->setStyleSheet("font-size: 10px; color: #aa4444; text-decoration: line-through;");
            cb->setToolTip(QString("Bloqu\xc3\xa9 par %1").arg(wname));
            cb->setEnabled(false);
        } else if (!isSelected) {
            // Prévisualiser le conflit si on l'ajoute
            auto conflict = findConflictInPool(spell, activePool, charLevel);
            if (conflict) {
                QString wname;
                for (auto& b : _activeBuffs)
                    if (b.spell.id == *conflict) { wname = QString::fromStdString(b.spell.name); break; }
                cb->setStyleSheet("font-size: 10px; color: #aa4444; text-decoration: line-through;");
                cb->setToolTip(QString("Bloqu\xc3\xa9 par %1").arg(wname));
                cb->setEnabled(false);
            } else {
                cb->setStyleSheet("font-size: 10px; color: #c0c0c0;");
                cb->setEnabled(!atCap);
            }
        } else {
            cb->setStyleSheet("font-size: 10px; color: #c0c0c0;");
            cb->setEnabled(true);
        }

        // Capturer les variables par valeur pour les lambdas
        SpellData spellCopy = spell;
        std::string cls = _currentClass;
        int ml = minLvl;
        connect(cb, &QCheckBox::toggled, this, [this, spellCopy, cls, ml](bool checked) {
            if (checked) {
                if ((int)_activeBuffs.size() >= MAX_BUFF_SLOTS) return;
                if (std::any_of(_activeBuffs.begin(), _activeBuffs.end(),
                                [&](const ActiveBuff& b) { return b.spell.id == spellCopy.id; }))
                    return;
                _activeBuffs.push_back({spellCopy, cls, ml});
            } else {
                _activeBuffs.erase(
                    std::remove_if(_activeBuffs.begin(), _activeBuffs.end(),
                                   [&](const ActiveBuff& b){ return b.spell.id == spellCopy.id; }),
                    _activeBuffs.end());
            }
            // Recalculer les conflits globaux
            std::vector<SpellData> allSpells;
            for (auto& b : _activeBuffs) allSpells.push_back(b.spell);
            auto sr = checkStacking(allSpells, _charInfo ? _charInfo->level : 0);
            _conflicts = sr.conflicts;

            rebuildClassList();
            rebuildRightPanel();
            refreshStats();
        });

        _rightLayout->addWidget(cb);
    }

    // Stretch
    _rightLayout->addStretch();
}

// ── rebuildClassList ──────────────────────────────────────────────────────

void SpellsTab::rebuildClassList()
{
    _headerLabel->setText(
        QString::fromUtf8("Sorts b\xc3\xa9" "n\xc3\xa9" "fiques (%1/%2)")
            .arg((int)_activeBuffs.size()).arg(MAX_BUFF_SLOTS));

    // Compte par classe
    std::map<std::string, int> countByClass;
    for (auto& b : _activeBuffs)
        countByClass[b.buffClass]++;

    _classList->blockSignals(true);
    for (int i = 0; i < (int)BUFF_CASTER_CLASSES.size(); ++i) {
        const auto& cls = BUFF_CASTER_CLASSES[i];
        int n = countByClass.count(cls) ? countByClass[cls] : 0;
        auto* item = _classList->item(i);
        if (item)
            item->setText(n > 0
                ? QString::fromStdString(cls) + QString(" (%1)").arg(n)
                : QString::fromStdString(cls));
    }
    _classList->blockSignals(false);
}

// ── refreshStats ──────────────────────────────────────────────────────────

void SpellsTab::refreshStats()
{
    while (_statsLayout->count()) {
        auto* child = _statsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (!_charInfo || _charInfo->level <= 0) return;

    // Sorts effectifs (non bloqués)
    std::vector<SpellData> effective;
    for (auto& b : _activeBuffs)
        if (!_conflicts.count(b.spell.id))
            effective.push_back(b.spell);

    int casterLvl = expansionMaxLevel();
    std::vector<std::map<std::string, int>> spellDicts;
    for (auto& sp : effective) {
        int lvl = (sp.targettype == 6) ? _charInfo->level : casterLvl;
        spellDicts.push_back(spellToStatDict(sp, lvl));
    }

    // Items équipés en vector
    std::vector<ItemData> items;
    int primaryItemtype = 0;
    for (auto& [slot, item] : _equippedItems) {
        items.push_back(item);
        if (slot == "Primary") primaryItemtype = item.itemtype;
    }

    PlayerTotals totals = calculateTotalsWithSpells(
        *_charInfo, items, spellDicts, primaryItemtype);

    QString lbl = _activeBuffs.empty()
        ? "Stats actuelles"
        : QString("Stats avec sorts (%1 sort%2)")
              .arg((int)effective.size())
              .arg(effective.size() > 1 ? "s" : "");

    _statsLayout->addWidget(makeStatsBarsWidget(lbl, totals));
}

// ── makeStatsBarsWidget ───────────────────────────────────────────────────
// Version simplifiée de CharacterTab::makeStatsBar (sans tooltips).

QFrame* SpellsTab::makeStatsBarsWidget(const QString& label, const PlayerTotals& totals)
{
    std::string cls = _charInfo ? _charInfo->class_name : "";

    // Catégories à afficher selon la classe
    std::vector<std::pair<std::string, std::vector<std::string>>> cats;
    auto classIt = CLASS_CATS_UI.find(cls);
    if (classIt != CLASS_CATS_UI.end()) {
        for (auto& [name, stats] : STAT_CATS)
            if (classIt->second.count(name))
                cats.push_back({name, stats});
    }
    if (cats.empty()) cats = STAT_CATS;

    // Expansion caps
    std::string exp = _config->get("current_expansion");
    int attrCap   = (exp == "Classic" || exp == "Kunark" || exp == "Velious") ? 200 : 255;
    int resistCap = (exp == "Classic" || exp == "Kunark" || exp == "Velious") ? 200 : 500;

    auto* frame = new QFrame;
    frame->setStyleSheet("QFrame { background: #0f1624; border: none; }");
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);

    auto* headerLbl = new QLabel(label);
    headerLbl->setStyleSheet(
        "font-size: 10px; color: #888; font-variant: small-caps; "
        "border: none; background: transparent;");
    outer->addWidget(headerLbl);

    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    grid->setContentsMargins(0, 0, 0, 0);

    for (int idx = 0; idx < (int)cats.size(); ++idx) {
        auto& [catName, catStats] = cats[idx];
        int row = idx / 2, col = idx % 2;

        auto colIt = CAT_COLORS_UI.find(catName);
        if (colIt == CAT_COLORS_UI.end()) continue;
        auto& [bg, border, accent] = colIt->second;

        auto* panel = new QFrame;
        panel->setStyleSheet(
            QString("QFrame { background: %1; border-radius: 4px; border: 1px solid %2; }")
            .arg(bg).arg(border));
        auto* panelL = new QVBoxLayout(panel);
        panelL->setContentsMargins(6, 4, 6, 6);
        panelL->setSpacing(3);

        auto lblIt = CAT_LABELS_UI.find(catName);
        const char* catStr = (lblIt != CAT_LABELS_UI.end()) ? lblIt->second : catName.c_str();
        auto* catLbl = new QLabel(QString::fromUtf8(catStr));
        catLbl->setStyleSheet(
            QString("font-size: 10px; color: %1; font-variant: small-caps; "
                    "font-weight: bold; border: none; background: transparent;").arg(accent));
        panelL->addWidget(catLbl);

        auto* tilesW = new QWidget;
        tilesW->setStyleSheet("background: transparent;");
        auto* tilesL = new QHBoxLayout(tilesW);
        tilesL->setSpacing(3);
        tilesL->setContentsMargins(0, 0, 0, 0);

        for (auto& stat : catStats) {
            bool hasCap = isAttr(stat) || isResist(stat);
            int cap = isAttr(stat) ? attrCap : (isResist(stat) ? resistCap : 0);
            int rawVal  = getStatVal(stat, totals);
            int dispVal = hasCap ? std::min(rawVal, cap) : rawVal;
            bool atCap  = hasCap && rawVal >= cap;

            const char *tileBg, *tileFg;
            if (!hasCap)    { tileBg = "#1e2a1e"; tileFg = "#81c784"; }
            else if (atCap) { tileBg = "#1e3a5f"; tileFg = "#4fc3f7"; }
            else             { tileBg = "#252540"; tileFg = "#cccccc"; }

            auto* tile = new QFrame;
            tile->setStyleSheet(
                QString("QFrame { background: %1; border-radius: 3px; "
                        "border: 1px solid rgba(255,255,255,0.06); }").arg(tileBg));
            auto* tileL = new QVBoxLayout(tile);
            tileL->setContentsMargins(4, 3, 4, 3);
            tileL->setSpacing(1);

            static const std::map<std::string, std::string> SLABELS = {
                {"astr","STR"},{"asta","STA"},{"aagi","AGI"},{"adex","DEX"},
                {"awis","WIS"},{"aint","INT"},{"acha","CHA"},
                {"hp","HP"},{"ac","AC"},{"mana","Mana"},{"atk","ATK"},
                {"haste","Haste"},{"hp_regen","HP/tick"},{"mana_regen","Mana/tick"},
                {"mr","Magic"},{"fr","Fire"},{"cr","Cold"},{"dr","Disease"},{"pr","Poison"},
            };
            static const std::map<std::string, std::string> SUFFIXES = {
                {"haste","%"},{"hp_regen","/tick"},{"mana_regen","/tick"},
            };

            auto slbIt  = SLABELS.find(stat);
            auto sufIt  = SUFFIXES.find(stat);
            QString slabel = slbIt != SLABELS.end()
                ? QString::fromStdString(slbIt->second) : QString::fromStdString(stat);
            QString suffix = sufIt != SUFFIXES.end()
                ? QString::fromStdString(sufIt->second) : QString();

            auto* statNameLbl = new QLabel(slabel);
            statNameLbl->setStyleSheet(
                "font-size: 8px; color: #888888; border: none; background: transparent;");
            statNameLbl->setAlignment(Qt::AlignCenter);

            auto* valLbl = new QLabel(QString::number(dispVal) + suffix);
            valLbl->setStyleSheet(
                QString("font-size: 11px; font-weight: bold; color: %1; "
                        "border: none; background: transparent;").arg(tileFg));
            valLbl->setAlignment(Qt::AlignCenter);

            tileL->addWidget(statNameLbl);
            tileL->addWidget(valLbl);
            tilesL->addWidget(tile);
        }

        panelL->addWidget(tilesW);
        grid->addWidget(panel, row, col);
    }

    auto* gridW = new QWidget;
    gridW->setStyleSheet("background: transparent;");
    gridW->setLayout(grid);
    outer->addWidget(gridW);
    return frame;
}
