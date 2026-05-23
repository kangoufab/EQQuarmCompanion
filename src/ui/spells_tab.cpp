#include "ui/spells_tab.h"
#include "ui/stats_bar.h"
#include "core/config.h"
#include "core/spell_stacking.h"
#include "core/spell_stats.h"
#include "core/stats_calculator.h"
#include "db/item_database.h"

#undef slots  // Qt macro conflicts

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdlib>
#include <set>

// ── Constantes ────────────────────────────────────────────────────────────

static const std::vector<std::string> BUFF_CASTER_CLASSES = {
    "Cleric","Paladin","Ranger","Druid","Shaman","Bard",
    "Necromancer","Wizard","Magician","Enchanter","Beastlord","Shadowknight",
    "Clickies",
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
                              const std::map<std::string, ItemData>& equipped,
                              const AaStats& aaStats)
{
    _charInfo      = charInfo;
    _baseTotals    = baseTotals;
    _aaStats       = aaStats;
    _equippedItems = equipped;

    // Reset state
    _activeBuffs.clear();
    _conflicts.clear();
    _currentClassSpells.clear();
    _currentClass.clear();

    loadClickies();
    rebuildClassList();   // vide le panneau gauche et remet le compteur à 0
    refreshStats();       // réémet les stats sans buffs pour le nouveau perso

    refreshSetsCombo();

    if (_classList && _classList->count() > 0) {
        int row = std::max(0, _classList->currentRow());
        // setCurrentRow n'émet pas currentRowChanged si la row n'a pas changé,
        // donc on appelle directement onClassSelected pour forcer le rechargement.
        _classList->setCurrentRow(row);
        onClassSelected(row);
    } else {
        refreshStats();
    }
}

// ── buildUi ───────────────────────────────────────────────────────────────

void SpellsTab::buildUi()
{
    setStyleSheet("background: #0f1624;");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // ── Barre de gestion des sets ─────────────────────────────────────────
    {
        auto* setsFrame = new QFrame;
        setsFrame->setStyleSheet(
            "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
        auto* sh = new QHBoxLayout(setsFrame);
        sh->setContentsMargins(8, 4, 8, 4);
        sh->setSpacing(6);

        auto* lbl = new QLabel(QString::fromUtf8("Sets :"));
        lbl->setStyleSheet("font-size: 14px; color: #888; border: none; background: transparent;");
        sh->addWidget(lbl);

        _setsCombo = new QComboBox;
        _setsCombo->setMinimumWidth(160);
        _setsCombo->setStyleSheet(
            "QComboBox { background: #141428; border: 1px solid #3a4a6a; "
            "color: #c0c0c0; font-size: 14px; padding: 2px 6px; border-radius: 3px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: #1a1a2e; color: #c0c0c0; "
            "selection-background-color: #3a4a6a; }");
        sh->addWidget(_setsCombo);

        auto makeBtn = [&](const char* text, const char* color) {
            auto* btn = new QPushButton(QString::fromUtf8(text));
            btn->setStyleSheet(
                QString("QPushButton { background: #1e2a3e; border: 1px solid %1; "
                        "color: %1; font-size: 14px; padding: 2px 10px; border-radius: 3px; }"
                        "QPushButton:hover { background: %1; color: #0f1624; }"
                        "QPushButton:disabled { border-color: #444; color: #444; }").arg(color));
            return btn;
        };

        _loadBtn = makeBtn("Charger", "#64b5f6");
        sh->addWidget(_loadBtn);

        auto* saveBtn = makeBtn("Sauvegarder", "#81c784");
        sh->addWidget(saveBtn);

        _deleteBtn = makeBtn("Supprimer", "#e57373");
        sh->addWidget(_deleteBtn);

        sh->addStretch();
        outer->addWidget(setsFrame);

        connect(_loadBtn,  &QPushButton::clicked, this, &SpellsTab::onLoadSet);
        connect(saveBtn,   &QPushButton::clicked, this, &SpellsTab::onSaveSet);
        connect(_deleteBtn,&QPushButton::clicked, this, &SpellsTab::onDeleteSet);
        connect(_setsCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this](int i) {
                    bool has = i >= 0 && _setsCombo->count() > 0;
                    _loadBtn->setEnabled(has);
                    _deleteBtn->setEnabled(has);
                });

        refreshSetsCombo();
    }

    // ── Split principal : gauche (buffs actifs) + droite (browser) ─────────
    auto* splitH = new QHBoxLayout;
    splitH->setSpacing(6);
    outer->addLayout(splitH, 1);

    // ── GAUCHE : buffs actifs ─────────────────────────────────────────────
    {
        auto* leftFrame = new QFrame;
        leftFrame->setFixedWidth(220);
        leftFrame->setStyleSheet(
            "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
        auto* leftL = new QVBoxLayout(leftFrame);
        leftL->setContentsMargins(6, 6, 6, 6);
        leftL->setSpacing(4);

        _headerLabel = new QLabel(
            QString::fromUtf8("Buffs actifs (0/%1)").arg(MAX_BUFF_SLOTS));
        _headerLabel->setStyleSheet(
            "font-size: 14px; color: #64b5f6; font-variant: small-caps; font-weight: bold;"
            "border: none; background: transparent;");
        leftL->addWidget(_headerLabel);

        auto* buffsScroll = new QScrollArea;
        buffsScroll->setWidgetResizable(true);
        buffsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        buffsScroll->setStyleSheet(
            "QScrollArea { border: 1px solid #2a3a5a; background: #141428; }");
        _activeBuffsInner = new QWidget;
        _activeBuffsInner->setStyleSheet("background: transparent;");
        _activeBuffsLayout = new QVBoxLayout(_activeBuffsInner);
        _activeBuffsLayout->setContentsMargins(4, 4, 4, 4);
        _activeBuffsLayout->setSpacing(2);
        _activeBuffsLayout->addStretch();
        buffsScroll->setWidget(_activeBuffsInner);
        leftL->addWidget(buffsScroll);

        splitH->addWidget(leftFrame);
    }

    // ── DROITE : browser de sorts ─────────────────────────────────────────
    {
        auto* rightFrame = new QFrame;
        rightFrame->setStyleSheet(
            "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
        auto* rightL = new QVBoxLayout(rightFrame);
        rightL->setContentsMargins(6, 6, 6, 6);
        rightL->setSpacing(4);

        auto* sortsTitre = new QLabel(QString::fromUtf8("Sorts b\xc3\xa9n\xc3\xa9" "fiques"));
        sortsTitre->setStyleSheet(
            "font-size: 14px; color: #888; font-variant: small-caps;"
            "border: none; background: transparent;");
        rightL->addWidget(sortsTitre);

        // Search bar for filtering spells
        _spellSearch = new QLineEdit;
        _spellSearch->setPlaceholderText(QString::fromUtf8("Filtrer les sorts\xe2\x80\xa6"));
        _spellSearch->setClearButtonEnabled(true);
        _spellSearch->setStyleSheet(
            "QLineEdit { background: #141428; border: 1px solid #3a4a6a; "
            "border-radius: 3px; color: #c0c0c0; padding: 3px 6px; font-size: 13px; }"
            "QLineEdit:focus { border-color: #64b5f6; }");
        rightL->addWidget(_spellSearch);
        connect(_spellSearch, &QLineEdit::textChanged,
                this, [this](const QString&) { rebuildRightPanel(); });

        auto* panels = new QHBoxLayout;
        panels->setSpacing(6);

        // Liste des classes
        _classList = new QListWidget;
        _classList->setFixedWidth(130);
        _classList->setStyleSheet(
            "QListWidget { background: #1a1a2e; border: 1px solid #3a4a6a; font-size: 14px; }"
            "QListWidget::item { color: #888; padding: 3px 6px; }"
            "QListWidget::item:selected { background: #3a4a6a; color: #64b5f6; }");
        for (const auto& cls : BUFF_CASTER_CLASSES)
            _classList->addItem(QString::fromStdString(cls));
        panels->addWidget(_classList);

        // Panel sorts (checkboxes)
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

        rightL->addLayout(panels);
        splitH->addWidget(rightFrame, 1);
    }

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
    if (_currentClass == "Clickies") {
        _currentClassSpells.clear();
        for (auto& e : _clickieSpells)
            _currentClassSpells.push_back(e.spell);
        rebuildRightPanel();
    } else {
        loadSpellsForClass(_currentClass);
    }
}

// ── loadSpellsForClass ────────────────────────────────────────────────────

void SpellsTab::loadSpellsForClass(const std::string& className) {
    _currentClassSpells.clear();
    if (!_itemDb) { qDebug() << "[SpellsTab] itemDb null"; rebuildRightPanel(); return; }

    int maxLvl = expansionMaxLevel();
    qDebug() << "[SpellsTab] loadSpellsForClass" << QString::fromStdString(className) << "maxLvl=" << maxLvl;
    auto spells = _itemDb->getBeneficialSpellsByClass(
        QString::fromStdString(className), maxLvl);

    qDebug() << "[SpellsTab] got" << spells.size() << "spells from DB";

    for (auto& sd : spells) {
        // Exclure les sorts self-only (targettype==6) sauf si classe == classe perso
        bool isSelfOnly = (sd.targettype == 6);
        if (isSelfOnly && className != (_charInfo ? _charInfo->class_name : ""))
            continue;
        _currentClassSpells.push_back(sd);
    }

    qDebug() << "[SpellsTab] after filter:" << _currentClassSpells.size() << "spells";
    rebuildRightPanel();
}

// ── loadClickies ──────────────────────────────────────────────────────────

void SpellsTab::loadClickies() {
    _clickieSpells.clear();
    _clickieItemNames.clear();
    if (!_charInfo || !_itemDb) return;

    // Paires (nom_item, spell_id) : d'abord les items équipés (déjà en mémoire)
    QList<QPair<QString,int>> allPairs;
    for (auto& [slot, item] : _equippedItems)
        if (item.clickeffect > 0)
            allPairs.append({QString::fromStdString(item.name), item.clickeffect});

    // Puis les items des sacs (General1-8), requête DB batch
    QList<int> bagIds;
    bagIds.reserve((int)_charInfo->bag_item_ids.size());
    for (int id : _charInfo->bag_item_ids) bagIds.append(id);
    allPairs.append(_itemDb->getItemClickeffects(bagIds));

    // Dédoublonner par spell_id, récupérer le SpellData
    std::set<int> seen;
    for (auto& [itemName, spellId] : allPairs) {
        if (seen.count(spellId)) continue;
        seen.insert(spellId);
        auto sd = _itemDb->getSpellById(spellId);
        if (!sd) continue;
        _clickieSpells.push_back({itemName.toStdString(), *sd});
        _clickieItemNames[spellId] = itemName.toStdString();
    }
}

// ── rebuildRightPanel ─────────────────────────────────────────────────────

void SpellsTab::rebuildRightPanel()
{
    while (_rightLayout->count()) {
        auto* child = _rightLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    QString searchTxt = _spellSearch ? _spellSearch->text().trimmed() : QString();

    int charLevel    = _charInfo ? _charInfo->level : 0;
    int tooltipLevel = charLevel > 0 ? charLevel : expansionMaxLevel();
    int atCap        = (int)_activeBuffs.size() >= MAX_BUFF_SLOTS;

    std::set<int> selectedIds;
    for (auto& b : _activeBuffs) selectedIds.insert(b.spell.id);

    std::map<int, QString> winnerNames;
    for (auto& [loserId, winnerId] : _conflicts)
        for (auto& b : _activeBuffs)
            if (b.spell.id == winnerId) { winnerNames[loserId] = QString::fromStdString(b.spell.name); break; }

    std::vector<SpellData> activePool;
    for (auto& b : _activeBuffs) activePool.push_back(b.spell);

    bool isClickies = (_currentClass == "Clickies");
    auto it = SPELL_CLASS_ID_UI.find(_currentClass);

    // ── Groupement par catégorie (STAT_CATS) ──────────────────────────────
    // Détermine la catégorie primaire d'un sort selon les stats qu'il affecte.
    auto spellCategory = [&](const SpellData& sp) -> std::string {
        auto sd = spellToStatDict(sp, tooltipLevel);
        for (auto& [catName, keys] : STAT_CATS)
            for (auto& k : keys)
                if (sd.count(k) && sd.at(k) != 0) return catName;
        return "Autres";
    };

    // Grouper les sorts
    std::map<std::string, std::vector<const SpellData*>> grouped;
    static const std::vector<std::string> CAT_ORDER = {"Melee","Range","Defense","Sorts","Autres"};
    for (auto& sp : _currentClassSpells) {
        if (!searchTxt.isEmpty() &&
            !QString::fromStdString(sp.name).contains(searchTxt, Qt::CaseInsensitive))
            continue;
        grouped[spellCategory(sp)].push_back(&sp);
    }

    // ── Créer une checkbox pour un sort ──────────────────────────────────
    auto makeCheckbox = [&](const SpellData& spell) {
        int minLvl = (it != SPELL_CLASS_ID_UI.end() && it->second <= 15)
                     ? spell.classes[it->second - 1] : 0;
        bool isSelected = selectedIds.count(spell.id);
        bool isBlocked  = _conflicts.count(spell.id);

        QString label;
        if (isClickies) {
            auto iit = _clickieItemNames.find(spell.id);
            QString itemName = iit != _clickieItemNames.end()
                ? QString::fromStdString(iit->second) : "?";
            label = QString::fromStdString(spell.name) + " (" + itemName + ")";
        } else {
            label = QString::fromStdString(spell.name) + QString(" (Niv. %1)").arg(minLvl);
        }
        auto* cb = new QCheckBox(label);
        cb->setChecked(isSelected);
        cb->setToolTip(formatSpellTooltip(spell, tooltipLevel));

        if (isBlocked) {
            QString wname = winnerNames.count(spell.id) ? winnerNames[spell.id] : "un autre sort";
            cb->setStyleSheet("font-size: 14px; color: #aa4444; text-decoration: line-through;");
            cb->setToolTip(QString("Bloqu\xc3\xa9 par %1").arg(wname));
            cb->setEnabled(false);
        } else if (!isSelected) {
            auto conflict = findConflictInPool(spell, activePool, charLevel);
            if (conflict) {
                QString wname;
                for (auto& b : _activeBuffs)
                    if (b.spell.id == *conflict) { wname = QString::fromStdString(b.spell.name); break; }
                cb->setStyleSheet("font-size: 14px; color: #aa4444; text-decoration: line-through;");
                cb->setToolTip(QString("Bloqu\xc3\xa9 par %1").arg(wname));
                cb->setEnabled(false);
            } else {
                cb->setStyleSheet("font-size: 14px; color: #c0c0c0;");
                cb->setEnabled(!atCap);
            }
        } else {
            cb->setStyleSheet("font-size: 14px; color: #c0c0c0;");
            cb->setEnabled(true);
        }

        SpellData spellCopy = spell;
        std::string cls = _currentClass;
        connect(cb, &QCheckBox::toggled, this, [this, spellCopy, cls, minLvl](bool checked) {
            if (checked) {
                if ((int)_activeBuffs.size() >= MAX_BUFF_SLOTS) return;
                if (std::any_of(_activeBuffs.begin(), _activeBuffs.end(),
                                [&](const ActiveBuff& b) { return b.spell.id == spellCopy.id; }))
                    return;
                _activeBuffs.push_back({spellCopy, cls, minLvl});
            } else {
                _activeBuffs.erase(
                    std::remove_if(_activeBuffs.begin(), _activeBuffs.end(),
                                   [&](const ActiveBuff& b){ return b.spell.id == spellCopy.id; }),
                    _activeBuffs.end());
            }
            std::vector<SpellData> allSpells;
            for (auto& b : _activeBuffs) allSpells.push_back(b.spell);
            auto sr = checkStacking(allSpells, _charInfo ? _charInfo->level : 0);
            _conflicts = sr.conflicts;
            rebuildClassList();
            rebuildRightPanel();
            refreshStats();
        });
        return cb;
    };

    // ── Affichage par catégorie ───────────────────────────────────────────
    for (const auto& catName : CAT_ORDER) {
        auto git = grouped.find(catName);
        if (git == grouped.end() || git->second.empty()) continue;

        const char* catLabel = catName.c_str();
        auto lblIt = CAT_LABELS_UI.find(catName);
        if (lblIt != CAT_LABELS_UI.end()) catLabel = lblIt->second;

        // Couleur de l'en-tête selon la catégorie
        const char* accent = "#888888";
        auto colIt = CAT_COLORS_UI.find(catName);
        if (colIt != CAT_COLORS_UI.end()) accent = colIt->second.accent;

        auto* hdr = new QLabel(QString::fromUtf8(catLabel));
        hdr->setStyleSheet(
            QString("font-size:11px; color:%1; font-variant:small-caps; font-weight:bold;"
                    " background:#1a1a2e; border:none; padding:2px 4px; margin-top:4px;")
                .arg(accent));
        _rightLayout->addWidget(hdr);

        for (const auto* sp : git->second)
            _rightLayout->addWidget(makeCheckbox(*sp));
    }

    _rightLayout->addStretch();
}

// ── rebuildClassList ──────────────────────────────────────────────────────

void SpellsTab::rebuildClassList()
{
    _headerLabel->setText(
        QString::fromUtf8("Buffs actifs (%1/%2)")
            .arg((int)_activeBuffs.size()).arg(MAX_BUFF_SLOTS));

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

    rebuildActiveBuffsList();
}

// ── rebuildActiveBuffsList ────────────────────────────────────────────────

void SpellsTab::rebuildActiveBuffsList()
{
    if (!_activeBuffsLayout) return;

    // Vider (sauf le stretch final)
    while (_activeBuffsLayout->count() > 1) {
        auto* child = _activeBuffsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    // Tri : ordre classe dans BUFF_CASTER_CLASSES, puis level DESC, puis nom ASC
    auto classOrder = [](const std::string& cls) {
        for (int i = 0; i < (int)BUFF_CASTER_CLASSES.size(); ++i)
            if (BUFF_CASTER_CLASSES[i] == cls) return i;
        return (int)BUFF_CASTER_CLASSES.size();
    };
    std::vector<const ActiveBuff*> sorted;
    sorted.reserve(_activeBuffs.size());
    for (auto& b : _activeBuffs) sorted.push_back(&b);
    std::sort(sorted.begin(), sorted.end(), [&](const ActiveBuff* a, const ActiveBuff* b) {
        int oa = classOrder(a->buffClass), ob = classOrder(b->buffClass);
        if (oa != ob) return oa < ob;
        if (a->minLevel != b->minLevel) return a->minLevel > b->minLevel; // DESC
        return a->spell.name < b->spell.name;
    });

    for (auto* bp : sorted) {
        auto& b = *bp;
        bool blocked = _conflicts.count(b.spell.id) > 0;

        auto* row = new QWidget;
        row->setStyleSheet("background: transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(2, 1, 2, 1);
        rl->setSpacing(4);

        // Nom du sort
        auto* nameLbl = new QLabel(QString::fromStdString(b.spell.name));
        nameLbl->setStyleSheet(
            blocked
            ? "font-size: 13px; color: #aa4444; text-decoration: line-through; "
              "border: none; background: transparent;"
            : "font-size: 13px; color: #c0c0c0; border: none; background: transparent;");
        nameLbl->setToolTip(formatSpellTooltip(b.spell,
            _charInfo ? _charInfo->level : expansionMaxLevel()));
        rl->addWidget(nameLbl, 1);

        // Classe source (petit, grisé)
        QString shortCls = (b.buffClass == "Clickies")
            ? "Clk" : QString::fromStdString(b.buffClass).left(3);
        auto* clsLbl = new QLabel(shortCls);
        clsLbl->setStyleSheet(
            "font-size: 14px; color: #555; border: none; background: transparent;");
        rl->addWidget(clsLbl);

        // Bouton ×
        auto* removeBtn = new QPushButton("\xc3\x97");
        removeBtn->setFixedSize(16, 16);
        removeBtn->setStyleSheet(
            "QPushButton { background: #2a1a1a; border: 1px solid #5a3a3a; "
            "color: #e57373; font-size: 13px; border-radius: 3px; padding: 0; }"
            "QPushButton:hover { background: #5a3a3a; }");
        SpellData spellCopy = b.spell;
        connect(removeBtn, &QPushButton::clicked, this, [this, spellCopy] {
            _activeBuffs.erase(
                std::remove_if(_activeBuffs.begin(), _activeBuffs.end(),
                               [&](const ActiveBuff& ab){ return ab.spell.id == spellCopy.id; }),
                _activeBuffs.end());
            std::vector<SpellData> all;
            for (auto& ab : _activeBuffs) all.push_back(ab.spell);
            auto sr = checkStacking(all, _charInfo ? _charInfo->level : 0);
            _conflicts = sr.conflicts;
            rebuildClassList();
            rebuildRightPanel();
            refreshStats();
        });
        rl->addWidget(removeBtn);

        // Insérer avant le stretch
        _activeBuffsLayout->insertWidget(_activeBuffsLayout->count() - 1, row);
    }
}

// ── refreshStats ──────────────────────────────────────────────────────────

void SpellsTab::refreshStats()
{
    if (!_charInfo || _charInfo->level <= 0) return;

    // Sorts effectifs (non bloqués)
    std::vector<SpellData> effective;
    for (auto& b : _activeBuffs)
        if (!_conflicts.count(b.spell.id))
            effective.push_back(b.spell);

    // Sans buffs actifs : utiliser _baseTotals directement.
    // calculateTotalsWithSpells n'inclut pas les AAs — sans buff à afficher,
    // on évite de passer des totaux partiels qui faussent le bandeau.
    if (effective.empty()) {
        if (_baseTotals) emit statsChanged(*_baseTotals, {});
        return;
    }

    int casterLvl = expansionMaxLevel();
    std::vector<std::map<std::string, int>> spellDicts;
    for (auto& sp : effective) {
        int lvl = (sp.targettype == 6) ? _charInfo->level : casterLvl;
        spellDicts.push_back(spellToStatDict(sp, lvl));
    }

    std::vector<ItemData> items;
    int primaryItemtype = 0;
    for (auto& [slot, item] : _equippedItems) {
        items.push_back(item);
        if (slot == "Primary") primaryItemtype = item.itemtype;
    }

    PlayerTotals totals = calculateTotalsWithSpells(
        *_charInfo, items, spellDicts, primaryItemtype, nullptr, &_aaStats);

    // Construire l'extra pour les tooltips : spell_sources par stat
    PlayerTotalsExtra spellExtra;
    for (int i = 0; i < (int)effective.size(); ++i) {
        for (auto& [k, v] : spellDicts[i]) {
            if (v != 0)
                spellExtra.stats[k].spell_sources.emplace_back(effective[i].name, v);
        }
    }

    emit statsChanged(totals, spellExtra);
}

// ── Gestion des sets de buffs ─────────────────────────────────────────────

void SpellsTab::refreshSetsCombo()
{
    if (!_setsCombo) return;
    _setsCombo->blockSignals(true);
    _setsCombo->clear();

    if (_charInfo && !_charInfo->name.empty()) {
        auto sets = _config->getCharacterJson(_charInfo->name, "buff_sets");
        if (sets.is_array()) {
            for (auto& s : sets)
                _setsCombo->addItem(QString::fromStdString(s.value("name", "?")));
        }
    }

    bool has = _setsCombo->count() > 0;
    if (_loadBtn)   _loadBtn->setEnabled(has);
    if (_deleteBtn) _deleteBtn->setEnabled(has);
    _setsCombo->blockSignals(false);
}

void SpellsTab::onSaveSet()
{
    if (!_charInfo || _charInfo->name.empty()) return;

    bool ok;
    QString defaultName = _setsCombo->currentText();
    QString name = QInputDialog::getText(
        this, QString::fromUtf8("Sauvegarder le set"),
        QString::fromUtf8("Nom du set :"),
        QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();

    // Sérialiser les buffs actifs
    nlohmann::json buffsJson = nlohmann::json::array();
    for (auto& b : _activeBuffs)
        buffsJson.push_back({{"spell_id",  b.spell.id},
                             {"class",     b.buffClass},
                             {"min_level", b.minLevel}});

    // Récupérer ou créer le tableau pour CE personnage
    auto sets = _config->getCharacterJson(_charInfo->name, "buff_sets");
    if (!sets.is_array()) sets = nlohmann::json::array();

    std::string sname = name.toStdString();
    bool found = false;
    for (auto& s : sets) {
        if (s.value("name", "") == sname) {
            s["buffs"] = buffsJson;
            found = true; break;
        }
    }
    if (!found)
        sets.push_back({{"name", sname}, {"buffs", buffsJson}});

    _config->setCharacterJson(_charInfo->name, "buff_sets", sets);
    _config->save();
    refreshSetsCombo();

    int idx = _setsCombo->findText(name);
    if (idx >= 0) _setsCombo->setCurrentIndex(idx);
}

void SpellsTab::onLoadSet()
{
    if (!_charInfo || _charInfo->name.empty()) return;
    int idx = _setsCombo->currentIndex();
    if (idx < 0) return;

    auto sets = _config->getCharacterJson(_charInfo->name, "buff_sets");
    if (!sets.is_array() || idx >= (int)sets.size()) return;
    auto& set = sets[idx];
    if (!set.contains("buffs") || !set["buffs"].is_array()) return;

    _activeBuffs.clear();
    for (auto& entry : set["buffs"]) {
        int spellId   = entry.value("spell_id",  0);
        std::string cls = entry.value("class",   "");
        int minLvl    = entry.value("min_level", 0);
        if (spellId <= 0) continue;

        auto sd = _itemDb->getSpellById(spellId);
        if (!sd) continue;

        _activeBuffs.push_back({*sd, cls, minLvl});
    }

    // Recalculer conflits
    std::vector<SpellData> allSpells;
    for (auto& b : _activeBuffs) allSpells.push_back(b.spell);
    auto sr = checkStacking(allSpells, _charInfo ? _charInfo->level : 0);
    _conflicts = sr.conflicts;

    rebuildClassList();
    rebuildRightPanel();
    refreshStats();
}

void SpellsTab::onDeleteSet()
{
    if (!_charInfo || _charInfo->name.empty()) return;
    int idx = _setsCombo->currentIndex();
    if (idx < 0) return;

    auto sets = _config->getCharacterJson(_charInfo->name, "buff_sets");
    if (!sets.is_array() || idx >= (int)sets.size()) return;

    QString name = QString::fromStdString(sets[idx].value("name", "?"));
    auto ret = QMessageBox::question(
        this, QString::fromUtf8("Supprimer"),
        QString::fromUtf8("Supprimer le set « %1 » ?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    sets.erase(idx);
    _config->setCharacterJson(_charInfo->name, "buff_sets", sets);
    _config->save();
    refreshSetsCombo();
}

