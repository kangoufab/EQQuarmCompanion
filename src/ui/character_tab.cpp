#include "ui/character_tab.h"
#include "ui/ui_helpers.h"
#include "ui/stat_categories.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "core/stats_calculator.h"
#include "db/item_database.h"

#undef slots   // Qt macro conflicts with local variable names

#include <QVBoxLayout>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QCompleter>
#include <QStringListModel>
#include "ui/spell_tooltip.h"
#include "ui/item_tooltip.h"
#include <QSplitter>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QPointer>
#include <QString>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QCoreApplication>
#include <QIcon>
#include <QPixmap>
#include <algorithm>
#include <functional>
#include <set>
#include <utility>

// ── Constantes portées depuis Python stat_caps.py et character_tab.py ─────
// Clés ASCII neutres pour les maps ; labels affichés via CAT_LABELS

static const std::map<int, const char*> SKILL_NAMES = {
    {0,"1H Blunt"},{1,"1H Slash"},{2,"2H Blunt"},{3,"2H Slash"},
    {4,"Abjuration"},{5,"Alteration"},{6,"Apply Poison"},{7,"Archery"},
    {8,"Backstab"},{9,"Bind Wound"},{10,"Bash"},{11,"Block"},
    {12,"Brass Inst."},{13,"Channeling"},{14,"Conjuration"},
    {15,"Defense"},{16,"Disarm"},{17,"Disarm Traps"},{18,"Divination"},
    {19,"Dodge"},{20,"Double Attack"},{21,"Dragon Punch"},{22,"Dual Wield"},
    {23,"Eagle Strike"},{24,"Evocation"},{25,"Feign Death"},
    {27,"Flying Kick"},{29,"Hand to Hand"},{30,"Hide"},{31,"Kick"},
    {32,"Meditate"},{34,"Offense"},{35,"Parry"},{36,"Pick Lock"},
    {37,"Piercing"},{38,"Riposte"},{39,"Round Kick"},{40,"Safe Fall"},
    {42,"Singing"},{43,"Sneak"},{49,"Pick Pockets"},{51,"Swimming"},
    {52,"Throwing"},{53,"Tiger Claw"},{54,"Tracking"},
};

// Slot bitmask (slot_name → bit)
static const std::vector<std::pair<std::string, int>> SLOT_BITMASK = {
    {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
    {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
    {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
    {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
    {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
    {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
};

// Class bitmask (class_name → bit)
static const std::map<std::string, int> CLASS_BITS = {
    {"Warrior",1},{"Cleric",2},{"Paladin",4},{"Ranger",8},
    {"Shadowknight",16},{"Druid",32},{"Monk",64},{"Bard",128},
    {"Rogue",256},{"Shaman",512},{"Necromancer",1024},{"Wizard",2048},
    {"Magician",4096},{"Enchanter",8192},{"Beastlord",16384},{"Berserker",32768},
};

// Stats évaluées pour le score
static const std::vector<std::string> SCORED_STATS = {
    "ac","hp","mana","damage","delay","astr","asta","aagi","adex","awis","aint","acha"
};

// Toutes les stats affichées dans les cartes de comparaison
static const std::vector<std::pair<std::string, std::string>> DISPLAY_STATS = {
    {"hp","HP"},{"mana","Mana"},{"ac","AC"},{"atk","ATK"},
    {"damage","Dmg"},{"delay","Delay"},
    {"astr","STR"},{"asta","STA"},{"adex","DEX"},{"aagi","AGI"},
    {"awis","WIS"},{"aint","INT"},{"acha","CHA"},
    {"mr","MR"},{"fr","FR"},{"cr","CR"},{"dr","DR"},{"pr","PR"},
    {"haste","Haste"},{"hp_regen","HP/tick"},{"mana_regen","Mana/tick"},
};


// ── Helpers statiques ─────────────────────────────────────────────────────


static int getItemStat(const std::string& stat, const ItemData& item) {
    if (stat == "hp")        return item.hp;
    if (stat == "mana")      return item.mana;
    if (stat == "ac")        return item.ac;
    if (stat == "atk")       return item.atk;
    if (stat == "damage")    return item.damage;
    if (stat == "delay")     return item.delay;
    if (stat == "astr")      return item.astr;
    if (stat == "asta")      return item.asta;
    if (stat == "adex")      return item.adex;
    if (stat == "aagi")      return item.aagi;
    if (stat == "awis")      return item.awis;
    if (stat == "aint")      return item.aint;
    if (stat == "acha")      return item.acha;
    if (stat == "mr")        return item.mr;
    if (stat == "fr")        return item.fr;
    if (stat == "cr")        return item.cr;
    if (stat == "dr")        return item.dr;
    if (stat == "pr")        return item.pr;
    if (stat == "haste")     return item.haste;
    if (stat == "hp_regen")  return item.hp_regen;
    if (stat == "mana_regen") return item.mana_regen;
    return 0;
}

static bool isAttrStat(const std::string& s) {
    return s=="astr"||s=="asta"||s=="adex"||s=="aagi"||s=="awis"||s=="aint"||s=="acha";
}
static bool isResistStat(const std::string& s) {
    return s=="mr"||s=="fr"||s=="cr"||s=="dr"||s=="pr";
}

// ── CharacterTab ──────────────────────────────────────────────────────────

CharacterTab::CharacterTab(Config* config, NpcDatabase* npcDb,
                           ItemDatabase* itemDb, QWidget* p)
    : QWidget(p), _config(config), _npcDb(npcDb), _itemDb(itemDb)
{
    buildUi();
}

void CharacterTab::setCharacter(CharacterInfo* charInfo, PlayerTotals* totals,
                                const std::map<std::string, ItemData>& equipped)
{
    _charInfo      = charInfo;
    _totals        = totals;
    _equippedItems = equipped;

    _bagItems.clear();
    if (_charInfo && _itemDb) {
        for (auto& [bagNum, id] : _charInfo->bag_item_ids) {
            auto item = _itemDb->getItemById(id);
            if (item) {
                applyWornStats(*item, _charInfo->level);
                _bagItems.append({bagNum, *item});
            }
        }
    }

    rebuildInventoryPanel();
    clearComparison(false);
}

void CharacterTab::setBisNames(const QSet<QString>* bisNames) {
    _bisNames = bisNames;
}

// ── buildUi ───────────────────────────────────────────────────────────────

void CharacterTab::buildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(4);
    splitter->setStyleSheet(
        QString("QSplitter::handle { background: %1; }"
                "QSplitter::handle:hover { background: %2; }")
        .arg(kBorderMid, kBorderAccent));
    outerLayout->addWidget(splitter);

    // ── Colonne gauche : inventaire ──────────────────────────────────────────
    _inventoryScroll = new QScrollArea;
    // La grille d'items équipés (rebuildInventoryPanel) utilise des cellules de
    // taille fixe : 5 colonnes × 42px + spacing/marges ≈ 238px de large. Avec le
    // défilement horizontal désactivé, une largeur minimale plus petite clippe
    // les colonnes de droite de la grille. 260px laisse une marge pour la
    // bordure et la barre de défilement verticale.
    _inventoryScroll->setMinimumWidth(260);
    _inventoryScroll->setWidgetResizable(true);
    _inventoryScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _inventoryScroll->setStyleSheet(
        QString("QScrollArea { border: none; border-right: 1px solid %1; background: %2; }")
        .arg(kBorderMid, kBgBase));
    _inventoryContent = new QWidget;
    _inventoryContent->setStyleSheet(QString("background: %1;").arg(kBgBase));
    _inventoryScroll->setWidget(_inventoryContent);
    splitter->addWidget(_inventoryScroll);

    // ── Colonne droite : recherche + comparaison ─────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    splitter->addWidget(scroll);

    splitter->setSizes({260, 1000});
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto* container = new QWidget;
    container->setStyleSheet(QString("background: %1;").arg(kBgMain));
    scroll->setWidget(container);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Barre de recherche
    {
        auto* row = new QHBoxLayout;

        _slotFilter = new QComboBox;
        _slotFilter->addItem("Tous slots", 0);
        static const std::vector<std::pair<const char*, int>> SLOT_FILTER_ITEMS = {
            {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
            {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
            {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
            {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
            {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
            {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
        };
        for (auto& [name, bit] : SLOT_FILTER_ITEMS) {
            if (!isSlotAvailable(name)) continue;
            _slotFilter->addItem(name, bit);
        }
        _slotFilter->setStyleSheet(kComboStyle);
        _slotFilter->setAccessibleName("Filtre par emplacement");
        row->addWidget(_slotFilter);

        _searchCombo = new SearchComboBox;
        _searchCombo->setMinimumWidth(300);
        _searchCombo->lineEdit()->setPlaceholderText(
            QString::fromUtf8("Rechercher un item\xe2\x80\xa6"));
        _searchCombo->setStyleSheet(kComboStyle);
        _searchCombo->setAccessibleName("Rechercher un item");

        _searchModel = new QStringListModel(this);
        auto* completer = new QCompleter(_searchModel, this);
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        _searchCombo->setCompleter(completer);

        connect(_searchCombo, &SearchComboBox::popup_requested,
                this, &CharacterTab::onSearchPopup);
        connect(completer, QOverload<const QString&>::of(&QCompleter::activated),
                this, [this](const QString& text) {
            // Strip the BIS prefix "⭐ " (U+2B50 U+0020) if present before matching
            static const QString kBisPrefix = QString::fromUtf8("\xe2\xad\x90 ");
            QString rawText = text.startsWith(kBisPrefix) ? text.mid(kBisPrefix.length()) : text;
            for (int i = 0; i < _searchResults.size(); ++i) {
                if (QString::fromStdString(_searchResults[i].name) == rawText) {
                    onItemSelected(i);
                    return;
                }
            }
        });
        row->addWidget(_searchCombo);

        _clearBtn = new QPushButton("Clear");
        _clearBtn->setEnabled(false);
        _clearBtn->setAccessibleName("Effacer la recherche");
        _clearBtn->setStyleSheet(
            QString("QPushButton { background: %1; border: 1px solid %2; "
                    "border-radius: 3px; color: %3; padding: 3px 10px; font-size: 13px; }"
                    "QPushButton:hover { border-color: %4; color: %4; }"
                    "QPushButton:disabled { color: %5; border-color: %6; }")
            .arg(kBorderMid, kBorderCard, kTextBase, kAccentBlue).arg(kTextDim).arg(kBorderDisabled));
        connect(_clearBtn, &QPushButton::clicked, this, [this]() {
            _searchCombo->lineEdit()->clear();
            _searchResults.clear();
            _searchModel->setStringList({});
            _clearBtn->setEnabled(false);
            clearComparison();
        });
        row->addWidget(_clearBtn);
        row->addStretch();
        auto* rw = new QWidget;
        rw->setStyleSheet("background: transparent;");
        rw->setLayout(row);
        layout->addWidget(rw);
    }

    // Zone d'aperçu simple — item équipé cliqué, sans comparaison (masquée par défaut)
    {
        _itemPreviewArea = new QWidget;
        _itemPreviewArea->setStyleSheet("background: transparent;");
        _itemPreviewLayout = new QVBoxLayout(_itemPreviewArea);
        _itemPreviewLayout->setContentsMargins(0, 0, 0, 0);
        _itemPreviewLayout->setSpacing(6);
        _itemPreviewArea->setVisible(false);
        layout->addWidget(_itemPreviewArea);
    }

    // Zone de comparaison (masquée par défaut)
    {
        _comparisonArea = new QWidget;
        _comparisonArea->setStyleSheet("background: transparent;");
        _comparisonLayout = new QVBoxLayout(_comparisonArea);
        _comparisonLayout->setContentsMargins(0, 0, 0, 0);
        _comparisonLayout->setSpacing(6);
        _comparisonArea->setVisible(false);
        layout->addWidget(_comparisonArea);
    }

    layout->addStretch();
}

// ── rebuildInventoryPanel ─────────────────────────────────────────────────

// Icônes d'item — déployées à côté de l'exécutable dans imgs/items/item_<icon>.png
// Le nom du fichier utilise la colonne `icon` de la table `items` (graphique
// partagé entre items similaires), pas l'id de l'item.
// (CMake copie resources/imgs/items au build, l'installeur suit via {#BinDir}\*)
static QPixmap loadItemIcon(int iconId, int size)
{
    if (iconId <= 0) return {};

    // Cache des pixmaps redimensionnés : évite de relire et rescaler le PNG
    // depuis le disque à chaque rebuild de l'inventaire (setCharacter() est
    // appelé à chaque changement de personnage et à chaque rafraîchissement
    // du file watcher). Plusieurs items partagent souvent la même icône.
    static std::map<std::pair<int,int>, QPixmap> cache;
    auto key = std::make_pair(iconId, size);
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;

    QString path = QCoreApplication::applicationDirPath()
                 + "/imgs/items/item_" + QString::number(iconId) + ".png";
    QPixmap pm(path);
    if (pm.isNull()) {
        cache.emplace(key, QPixmap{});
        return {};
    }
    QPixmap scaled = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    cache.emplace(key, scaled);
    return scaled;
}

static const std::map<std::string, const char*> SLOT_ABBREV = {
    {"Charm","Charm"},{"Left Ear","L.Ear"},{"Head","Head"},{"Face","Face"},
    {"Right Ear","R.Ear"},{"Neck","Neck"},{"Shoulders","Shoul."},{"Arms","Arms"},
    {"Back","Back"},{"Left Wrist","L.Wrist"},{"Right Wrist","R.Wrist"},
    {"Range","Range"},{"Hands","Hands"},{"Primary","Primry"},
    {"Secondary","Second."},{"Left Finger","L.Ring"},{"Right Finger","R.Ring"},
    {"Chest","Chest"},{"Legs","Legs"},{"Feet","Feet"},{"Waist","Waist"},{"Ammo","Ammo"},
};

void CharacterTab::rebuildInventoryPanel()
{
    if (!_inventoryContent) return;

    if (auto* old = _inventoryContent->layout()) {
        QLayoutItem* child;
        while ((child = old->takeAt(0))) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        delete old;
    }

    auto* vl = new QVBoxLayout(_inventoryContent);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto addTitle = [&](const QString& title) {
        auto* lbl = new QLabel(title);
        lbl->setStyleSheet(
            QString("background: %1; color: %2; font-size: 11px; font-weight: bold; "
                    "padding: 4px 8px; border-bottom: 1px solid %3; border-top: 1px solid %3;")
            .arg(kSurfaceSection, kAccentBlue, kBorderMid));
        vl->addWidget(lbl);
    };

    auto addRow = [&](const std::string& slotName, const QString& itemName,
                      const ItemData* item, std::function<void()> onClick) {
        bool isEmpty = (item == nullptr);
        auto* w = new QWidget;
        w->setStyleSheet(QString("background: transparent; border-bottom: 1px solid %1;").arg(kBorderSub));
        auto* rl = new QHBoxLayout(w);
        rl->setContentsMargins(6, 2, 4, 2);
        rl->setSpacing(4);

        auto abIt = SLOT_ABBREV.find(slotName);
        const char* ab = (abIt != SLOT_ABBREV.end()) ? abIt->second : slotName.c_str();
        auto* slotLbl = new QLabel(ab);
        slotLbl->setFixedWidth(46);
        slotLbl->setStyleSheet(
            QString("color: %1; font-size: 11px; background: transparent;")
                .arg(kTextSlotLabel));
        rl->addWidget(slotLbl);

        if (isEmpty) {
            auto* nameLbl = new QLabel(QString::fromUtf8("\xe2\x80\x94 vide \xe2\x80\x94"));
            nameLbl->setStyleSheet(
                QString("color: %1; font-size: 12px; background: transparent;")
                    .arg(kTextEmptySlot));
            rl->addWidget(nameLbl, 1);
        } else {
            bool isBisItem = _bisNames && _bisNames->contains(itemName);
            QString displayName = isBisItem
                ? QString::fromUtf8("\xe2\xad\x90 ") + itemName
                : itemName;
            const char* nameColor = isBisItem ? kAccentGold : kTextBase;
            auto* nameBtn = new QPushButton(displayName);
            nameBtn->setFlat(true);
            nameBtn->setStyleSheet(
                QString("QPushButton { text-align: left; color: %1; background: transparent; "
                        "border: 1px solid transparent; font-size: 12px; padding: 0; }"
                        "QPushButton:hover { color: %2; }"
                        "QPushButton:focus { border: 1px solid %3; border-radius: 2px; }")
                .arg(nameColor, isBisItem ? kAccentGold : kGreen, kBorderAccent));
            nameBtn->setToolTip(formatItemTooltip(*item, kTextPrimary));
            if (onClick) connect(nameBtn, &QPushButton::clicked, onClick);
            rl->addWidget(nameBtn, 1);
        }
        vl->addWidget(w);
    };

    // ── Équipé — grille graphique façon paperdoll (icônes item_<id>.png) ──────
    static const int kEquipCellSize = 42;
    static const int kEquipIconSize = 32;

    auto makeEquipCell = [&](const std::string& slotName) -> QWidget* {
        auto eqIt = _equippedItems.find(slotName);
        bool isEmpty = (eqIt == _equippedItems.end());

        auto* btn = new QPushButton;
        btn->setFlat(true);
        btn->setFixedSize(kEquipCellSize, kEquipCellSize);
        btn->setCursor(isEmpty ? Qt::ArrowCursor : Qt::PointingHandCursor);
        btn->setFocusPolicy(isEmpty ? Qt::NoFocus : Qt::StrongFocus);
        bool isBisCell = !isEmpty && _bisNames &&
            _bisNames->contains(QString::fromStdString(eqIt->second.name));
        const char* borderColor = isBisCell ? kAccentGold : kBorderCard;
        QString cellCss = QString(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px; }")
            .arg(isEmpty ? kBgBase : kBgTile, borderColor);
        if (!isEmpty) {
            const char* hoverColor = isBisCell ? "#ffd97d" : kBorderAccent;
            cellCss += QString(
                "QPushButton:hover { border-color: %1; }"
                "QPushButton:focus { border-color: %1; }").arg(hoverColor);
        }
        btn->setStyleSheet(cellCss);

        QString fullSlotName = QString::fromStdString(slotName);
        auto abIt = SLOT_ABBREV.find(slotName);
        const char* ab = (abIt != SLOT_ABBREV.end()) ? abIt->second : slotName.c_str();

        if (isEmpty) {
            btn->setToolTip(QString::fromUtf8("%1 \xe2\x80\x94 vide").arg(ab));
            btn->setAccessibleName(fullSlotName + QString::fromUtf8(" \xe2\x80\x94 vide"));
        } else {
            const ItemData& item = eqIt->second;
            QPixmap icon = loadItemIcon(item.icon, kEquipIconSize);
            if (!icon.isNull()) {
                btn->setIcon(QIcon(icon));
                btn->setIconSize(icon.size());
            } else {
                btn->setText(ab);
                // kTextTileKey (pas kTextSlotLabel) : la cellule a un fond kBgTile,
                // et kTextSlotLabel n'atteint que 3.90:1 dessus (échec WCAG AA) —
                // kTextTileKey est garanti ≥4.5:1 sur tous les fonds de tuile.
                btn->setStyleSheet(btn->styleSheet() + QString(
                    "QPushButton { color: %1; font-size: 10px; }").arg(kTextTileKey));
            }
            btn->setToolTip(formatItemTooltip(item, kTextPrimary));
            btn->setAccessibleName(fullSlotName + " : " + QString::fromStdString(item.name));

            ItemData itemCopy = item;
            std::string sn = slotName;
            connect(btn, &QPushButton::clicked, this, [this, itemCopy, sn]() {
                showItemPreview(itemCopy, QString::fromStdString(sn));
            });
        }
        return btn;
    };

    // Bloc armure/bijoux : grille N colonnes, ordre visuel façon paperdoll
    static const int kEquipCols = 5;
    static const std::vector<std::string> kEquipArmorOrder = {
        "Charm","Left Ear","Head","Face","Right Ear","Neck",
        "Shoulders","Arms","Back","Left Wrist","Right Wrist","Hands",
        "Chest","Legs","Feet","Waist","Left Finger","Right Finger",
    };
    static const std::vector<std::string> kEquipWeaponOrder = {
        "Primary","Secondary","Range","Ammo",
    };

    addTitle(QString::fromUtf8("\xe2\x9a\x94  \xc3\x89quip\xc3\xa9"));

    auto* gridW = new QWidget;
    gridW->setStyleSheet("background: transparent;");
    auto* grid = new QGridLayout(gridW);
    grid->setContentsMargins(6, 8, 6, 4);
    grid->setSpacing(4);
    grid->setColumnStretch(kEquipCols, 1);
    int gi = 0;
    for (const auto& slotName : kEquipArmorOrder) {
        if (!isSlotAvailable(slotName)) continue;
        grid->addWidget(makeEquipCell(slotName), gi / kEquipCols, gi % kEquipCols);
        ++gi;
    }
    vl->addWidget(gridW);

    // Bloc armes : rangée centrée séparée (visuellement distincte de l'armure)
    auto* weaponW = new QWidget;
    weaponW->setStyleSheet("background: transparent;");
    auto* weaponL = new QHBoxLayout(weaponW);
    weaponL->setContentsMargins(8, 0, 8, 8);
    weaponL->setSpacing(6);
    weaponL->addStretch();
    for (const auto& slotName : kEquipWeaponOrder) {
        if (!isSlotAvailable(slotName)) continue;
        weaponL->addWidget(makeEquipCell(slotName));
    }
    weaponL->addStretch();
    vl->addWidget(weaponW);

    // ── Sacs — groupés par numéro de bag ─────────────────────────────────────
    // Construire la map {bagNum → [items]}
    std::map<int, std::vector<const ItemData*>> byBag;
    for (auto& [bagNum, item] : _bagItems)
        byBag[bagNum].push_back(&item);

    if (byBag.empty()) {
        addTitle(QString::fromUtf8("Sacs"));
        auto* lbl = new QLabel(QString::fromUtf8("(vide)"));
        lbl->setStyleSheet(QString("color: %1; font-size: 12px; padding: 4px 8px;").arg(kTextEmptySlot));
        vl->addWidget(lbl);
    } else {
        for (auto& [bagNum, items] : byBag) {
            addTitle(QString("Bag %1  (%2)").arg(bagNum).arg(items.size()));
            for (const auto* bagItem : items) {
                auto allSlots = detectSlots(*bagItem);
                if (allSlots.empty()) {
                    addRow(std::string(), QString::fromStdString(bagItem->name), bagItem, nullptr);
                } else {
                    ItemData item = *bagItem;
                    QString firstSlot = allSlots[0];
                    addRow(std::string(), QString::fromStdString(item.name), &item,
                           [this, item, firstSlot, allSlots]() {
                               clearComparison(false);
                               showComparison(item, firstSlot, allSlots);
                           });
                }
            }
        }
    }

    vl->addStretch();
}

// ── makeStatsBar ─────────────────────────────────────────────────────────

QFrame* CharacterTab::makeStatsBar(const QString& label,
                                    const PlayerTotals& totals,
                                    int attrCap, int resistCap,
                                    const PlayerTotals* refTotals)
{
    // Catégories à afficher selon la classe du personnage
    std::vector<std::pair<std::string, std::vector<std::string>>> cats;
    if (_charInfo && !_charInfo->class_name.empty()) {
        auto it = CLASS_CATEGORIES.find(_charInfo->class_name);
        if (it != CLASS_CATEGORIES.end()) {
            for (auto& [name, stats] : STAT_CATEGORIES) {
                if (it->second.count(name))
                    cats.push_back({name, stats});
            }
        }
    }
    if (cats.empty())
        cats = STAT_CATEGORIES;

    // Defense toujours en première position
    auto defIt = std::find_if(cats.begin(), cats.end(),
                              [](const auto& p){ return p.first == "Defense"; });
    if (defIt != cats.end() && defIt != cats.begin())
        std::rotate(cats.begin(), defIt, defIt + 1);

    auto* frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(
        QString("QFrame { background: %1; border: none; }").arg(kBgMain));
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);

    // Grille 2×2 de panneaux de catégories
    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    grid->setContentsMargins(0, 0, 0, 0);

    for (int idx = 0; idx < (int)cats.size(); ++idx) {
        auto& [catName, catStats] = cats[idx];
        int row = idx / 2, col = idx % 2;

        auto colIt = CAT_COLORS.find(catName);
        if (colIt == CAT_COLORS.end()) continue;
        auto& [bg, border, accent] = colIt->second;

        auto* panel = new QFrame;
        panel->setStyleSheet(
            QString("QFrame { background: %1; border-radius: 4px; border: 1px solid %2; }")
            .arg(bg).arg(border));
        auto* panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(6, 4, 6, 6);
        panelLayout->setSpacing(3);

        // Titre catégorie (label UTF-8 depuis CAT_LABELS)
        auto labelIt = CAT_LABELS.find(catName);
        const char* catLabelStr = (labelIt != CAT_LABELS.end()) ? labelIt->second : catName.c_str();
        auto* catLbl = new QLabel(QString::fromUtf8(catLabelStr));
        catLbl->setStyleSheet(
            QString("font-size: 14px; color: %1; font-variant: small-caps; "
                    "font-weight: bold; border: none; background: transparent;")
            .arg(accent));
        panelLayout->addWidget(catLbl);

        // Rangée de tiles de stats
        auto* tilesW = new QWidget;
        tilesW->setStyleSheet("background: transparent;");
        auto* tilesL = new QHBoxLayout(tilesW);
        tilesL->setSpacing(3);
        tilesL->setContentsMargins(0, 0, 0, 0);

        for (auto& stat : catStats) {
            bool hasCap = isAttrStat(stat) || isResistStat(stat);
            int cap = isAttrStat(stat) ? attrCap : (isResistStat(stat) ? resistCap : 0);

            int rawVal     = playerTotalStat(stat, totals);
            int dispVal    = hasCap ? std::min(rawVal, cap) : rawVal;
            bool atCap     = hasCap && rawVal >= cap;

            const char *tileBg, *tileFg;
            if (!hasCap)      { tileBg = kBgTileNoLimit; tileFg = kGreen; }
            else if (atCap)   { tileBg = kBgTileAtCap;   tileFg = kAccentAtCap; }
            else               { tileBg = kBgTile;        tileFg = kTextBase; }

            auto* tile = new QFrame;
            tile->setStyleSheet(
                QString("QFrame { background: %1; border-radius: 3px; border: none; }")
                .arg(tileBg));
            auto* tileL = new QVBoxLayout(tile);
            tileL->setContentsMargins(5, 3, 5, 3);
            tileL->setSpacing(0);

            auto* nameLbl = new QLabel(
                QString::fromStdString(STAT_LABELS.count(stat) ? STAT_LABELS.at(stat) : stat));
            nameLbl->setStyleSheet(
                QString("font-size: 13px; color: %1; border: none; background: transparent;")
                .arg(kTextSecondary));
            nameLbl->setAlignment(Qt::AlignCenter);
            tileL->addWidget(nameLbl);

            QString sfx = QString::fromStdString(
                STAT_SUFFIX.count(stat) ? STAT_SUFFIX.at(stat) : "");
            QString valHtml;
            if (hasCap) {
                valHtml = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>"
                                  "<span style='color:%5;font-size:13px;'>/%4</span>")
                    .arg(tileFg).arg(dispVal).arg(sfx).arg(cap).arg(kTextDim);
            } else {
                valHtml = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>")
                    .arg(tileFg).arg(dispVal).arg(sfx);
            }

            if (refTotals) {
                int refRaw  = playerTotalStat(stat, *refTotals);
                int refDisp = hasCap ? std::min(refRaw, cap) : refRaw;
                int delta   = dispVal - refDisp;
                if (delta > 0)
                    valHtml += QString("<span style='color:%1;font-size:13px;'> +%2</span>")
                                   .arg(kGreen).arg(delta);
                else if (delta < 0)
                    valHtml += QString("<span style='color:%1;font-size:13px;'> %2</span>")
                                   .arg(kTextDeltaNeg).arg(delta);
            }

            auto* valLbl = new QLabel;
            valLbl->setText(valHtml);
            valLbl->setTextFormat(Qt::RichText);
            valLbl->setStyleSheet("border: none; background: transparent;");
            valLbl->setAlignment(Qt::AlignCenter);
            tileL->addWidget(valLbl);

            tile->setToolTip(buildStatTooltip(stat, dispVal, hasCap, cap, totals));

            tilesL->addWidget(tile);
        }
        tilesL->addStretch();
        panelLayout->addWidget(tilesW);

        grid->addWidget(panel, row, col);
    }

    outer->addLayout(grid);
    return frame;
}

// ── makeItemCard (supprimé — utilise ui/item_card.h) ─────────────────────

static QFrame* buildSlotCard(const ItemData* item, const ItemData* refItem,
                               const QString& slotTitle,
                               const std::map<int,SpellData>*  spells          = nullptr,
                               const std::map<int,QString>*    limitSpellNames = nullptr,
                               bool                            isBis           = false)
{
    return makeItemCard(item, refItem, spells, slotTitle, limitSpellNames, {}, isBis);
}

// ── onSearchPopup ─────────────────────────────────────────────────────────

void CharacterTab::onSearchPopup()
{
    QString query = _searchCombo->lineEdit()->text().trimmed();
    if (query.length() < 2) return;

    int slotBit = _slotFilter ? _slotFilter->currentData().toInt() : 0;
    auto results = _itemDb->searchItems(query, 50, slotBit);
    _searchResults.clear();

    int charLevel = _charInfo ? _charInfo->level : 65;
    QStringList names;
    for (auto item : results) {
        if (!canEquip(item)) continue;
        applyWornStats(item, charLevel);
        _searchResults.append(item);
        QString qname = QString::fromStdString(item.name);
        bool isBis = _bisNames && _bisNames->contains(qname);
        names << (isBis ? QString::fromUtf8("\xe2\xad\x90 ") + qname : qname);
    }

    _searchModel->setStringList(names);
    if (!_searchResults.isEmpty()) {
        _clearBtn->setEnabled(true);
        _searchCombo->completer()->complete();
    }
}

// ── onItemSelected ────────────────────────────────────────────────────────

void CharacterTab::onItemSelected(int index)
{
    if (index < 0 || index >= _searchResults.size()) return;
    loadItemIntoComparison(_searchResults[index]);
}

// ── loadItemIntoComparison ────────────────────────────────────────────────

void CharacterTab::loadItemIntoComparison(const ItemData& item)
{
    auto eqSlots = detectSlots(item);
    if (eqSlots.empty()) return;

    clearComparison(false);
    showComparison(item, eqSlots[0], eqSlots);
}

// ── loadItemSpells / buildLimitSpellNames ─────────────────────────────────
// Précharge les sorts worn/focus/proc/click/scroll d'un item et construit les
// noms des sorts SE_LimitSpell (SPA 139) référencés, pour les tooltips enrichis.

std::map<int,SpellData> CharacterTab::loadItemSpells(const ItemData& item) const
{
    std::map<int,SpellData> m;
    auto load = [&](int id) {
        if (id > 0 && !m.count(id)) {
            auto s = _itemDb->getSpellById(id);
            if (s) m[id] = std::move(*s);
        }
    };
    load(item.worneffect); load(item.focuseffect);
    load(item.proceffect); load(item.clickeffect); load(item.scrolleffect);
    return m;
}

std::map<int,QString> CharacterTab::buildLimitSpellNames(const std::map<int,SpellData>& spells) const
{
    std::map<int,QString> names;
    for (auto& [sid_key, sp] : spells) {
        for (int i = 0; i < 12; ++i) {
            if (sp.spa[i] == 254) break;
            if (sp.spa[i] == 139) {
                int sid = std::abs(sp.effect_base_value[i]);
                if (sid > 0 && !names.count(sid)) {
                    auto ref = _itemDb->getSpellById(sid);
                    if (ref) names[sid] = QString::fromStdString(ref->name);
                }
            }
        }
    }
    return names;
}

// ── showComparison ────────────────────────────────────────────────────────

void CharacterTab::showComparison(const ItemData& newItem, const QString& slot,
                                   const std::vector<QString>& allSlots)
{
    // Boutons de slot (si l'item va dans plusieurs slots)
    if (allSlots.size() > 1) {
        auto* slotW = new QWidget;
        slotW->setStyleSheet("background: transparent;");
        auto* slotL = new QHBoxLayout(slotW);
        slotL->setContentsMargins(0, 0, 0, 0);
        slotL->setSpacing(4);

        auto* slotLbl = new QLabel("Slot :");
        slotLbl->setStyleSheet(
            QString("color: %1; font-size: 14px; border: none; background: transparent;")
            .arg(kTextSecondary));
        slotL->addWidget(slotLbl);

        for (auto& eqSlot : allSlots) {
            auto* btn = new QPushButton(eqSlot);
            bool isCurrent = (eqSlot == slot);
            btn->setEnabled(!isCurrent);
            btn->setStyleSheet(
                isCurrent
                ? QString("QPushButton { background: %1; border: 1px solid %2; "
                          "border-radius: 3px; color: %2; padding: 2px 8px; font-size: 14px; }")
                  .arg(kBorderMid, kAccentBlue)
                : QString("QPushButton { background: %1; border: 1px solid %2; "
                          "border-radius: 3px; color: %3; padding: 2px 8px; font-size: 14px; }"
                          "QPushButton:hover { border-color: %4; color: %4; }")
                  .arg(kBgCard, kBorderCard, kTextBase, kAccentBlue));
            connect(btn, &QPushButton::clicked, [this, newItem, eqSlot, allSlots]() {
                clearComparison(false);
                showComparison(newItem, eqSlot, allSlots);
            });
            slotL->addWidget(btn);
        }
        slotL->addStretch();
        _comparisonLayout->addWidget(slotW);
        _comparisonArea->setVisible(true);
    }

    // Item actuellement équipé dans ce slot
    const ItemData* curItem = nullptr;
    {
        auto it = _equippedItems.find(slot.toStdString());
        if (it != _equippedItems.end()) curItem = &it->second;
    }

    // Calcul des totaux "après équipement"
    auto afterEquipped = _equippedItems;
    afterEquipped[slot.toStdString()] = newItem;

    if (_charInfo && _charInfo->level > 0) {
        std::vector<ItemData> afterVec;
        afterVec.reserve(afterEquipped.size());
        for (auto& [s, it] : afterEquipped) afterVec.push_back(it);

        int primType = 0;
        if (auto pit = afterEquipped.find("Primary"); pit != afterEquipped.end())
            primType = pit->second.itemtype;
        PlayerTotals afterTotals = calculateTotals(*_charInfo, afterVec, primType);

        // Notifier MainWindow des stats "après équipement" avec les items correspondants
        emit statsChanged(afterTotals, afterEquipped);
    }

    // Cartes de comparaison côte à côte
    auto* colW = new QWidget;
    colW->setStyleSheet("background: transparent;");
    auto* colL = new QHBoxLayout(colW);
    colL->setSpacing(6);
    colL->setContentsMargins(0, 0, 0, 0);

    // Précharge les spells pour les tooltips
    auto newSpells = loadItemSpells(newItem);
    std::map<int,SpellData> curSpells;
    if (curItem) curSpells = loadItemSpells(*curItem);

    auto newLimitNames = buildLimitSpellNames(newSpells);
    auto curLimitNames = buildLimitSpellNames(curSpells);

    QString curName = curItem
        ? QString::fromStdString(curItem->name)
        : QString::fromUtf8("(vide)");
    bool isBisCur = curItem && _bisNames &&
        _bisNames->contains(QString::fromStdString(curItem->name));
    bool isBisNew = _bisNames &&
        _bisNames->contains(QString::fromStdString(newItem.name));

    colL->addWidget(buildSlotCard(curItem, nullptr, curName,
                                  curItem ? &curSpells : nullptr,
                                  curItem ? &curLimitNames : nullptr,
                                  isBisCur));
    colL->addWidget(makeItemCard(&newItem, curItem,
                                 newSpells.empty() ? nullptr : &newSpells,
                                 {},
                                 newLimitNames.empty() ? nullptr : &newLimitNames,
                                 {},
                                 isBisNew));
    _comparisonLayout->addWidget(colW);

    // Résumé : score + UPGRADE/DOWNGRADE
    auto weights = _config->getClassWeights(
        _charInfo ? _charInfo->class_name : "");
    int scoreDelta = 0;
    for (auto& stat : SCORED_STATS) {
        int newVal = getItemStat(stat, newItem);
        int curVal = curItem ? getItemStat(stat, *curItem) : 0;
        float w = weights.count(stat) ? weights.at(stat) : 0.f;
        scoreDelta += (int)((newVal - curVal) * w);
    }

    bool isUpgrade = scoreDelta > 0;
    QString upgradeText =
        isUpgrade ? "UPGRADE" : (scoreDelta == 0 ? "=" : "DOWNGRADE");
    QString upgradeColor =
        isUpgrade ? kGreen : (scoreDelta == 0 ? kTextSecondary : kRed);

    auto* sumFrame = new QFrame;
    sumFrame->setStyleSheet(
        QString("QFrame { background: %1; border-radius: 4px; border: 1px solid %2; }")
        .arg(kBgCard, kBorderCard));
    auto* sumL = new QHBoxLayout(sumFrame);
    sumL->setContentsMargins(8, 6, 8, 6);

    QString sign = scoreDelta > 0 ? "+" : "";
    auto* scoreLbl = new QLabel(
        QString("Score : %1%2  <b>%3</b>").arg(sign).arg(scoreDelta).arg(upgradeText));
    scoreLbl->setTextFormat(Qt::RichText);
    scoreLbl->setStyleSheet(
        QString("color: %1; font-size: 13px; border: none; background: transparent;")
        .arg(upgradeColor));
    sumL->addWidget(scoreLbl);
    sumL->addStretch();

    _comparisonLayout->addWidget(sumFrame);

    // Bouton Équiper
    {
        auto* equipBtn = new QPushButton(
            QString::fromUtf8("\xe2\x9c\x93  \xc3\x89quiper dans ") + slot);
        equipBtn->setStyleSheet(
            QString("QPushButton { background: %1; border: 1px solid %2; "
                    "border-radius: 3px; color: %2; padding: 4px 12px; font-size: 13px; }"
                    "QPushButton:hover { background: %2; color: %3; }"
                    "QPushButton:focus { border: 1px solid %4; }")
            .arg(kBgActionEquip, kGreen, kBgMain, kBorderAccent));
        connect(equipBtn, &QPushButton::clicked,
                [this, slot=slot.toStdString(), item=newItem]() {
                    emit equipRequested(slot, item);
                });
        _comparisonLayout->addWidget(equipBtn);
    }

    // Bouton Source (B)
    {
        auto* srcBtn = new QPushButton(
            QString::fromUtf8("Qui droppe cet item ?"));
        srcBtn->setStyleSheet(
            QString("QPushButton { background: %1; border: 1px solid %2; "
                    "border-radius: 3px; color: %3; padding: 3px 10px; font-size: 13px; }"
                    "QPushButton:hover { border-color: %4; color: %4; }"
                    "QPushButton:focus { border-color: %4; color: %4; }")
            .arg(kBgActionSource, kBorderCard, kTextSecondary, kAccentBlue));
        connect(srcBtn, &QPushButton::clicked,
                [this, itemId=newItem.id, itemName=QString::fromStdString(newItem.name)]() {
                    onShowSources(itemId, itemName);
                });
        _comparisonLayout->addWidget(srcBtn);
    }

    _comparisonArea->setVisible(true);
    if (_clearBtn) _clearBtn->setEnabled(true);
}

// ── clearComparison ───────────────────────────────────────────────────────

void CharacterTab::clearComparison(bool emitReset)
{
    while (_comparisonLayout->count()) {
        auto* child = _comparisonLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    _comparisonArea->setVisible(false);
    clearItemPreview();
    if (_clearBtn) _clearBtn->setEnabled(false);
    if (emitReset && _totals)
        emit statsChanged(*_totals, _equippedItems);
}

// ── showItemPreview / clearItemPreview ────────────────────────────────────
// Aperçu simple d'un item équipé — une seule carte, sans comparaison ni
// boutons Équiper/Source (mutuellement exclusif avec la zone de comparaison).

void CharacterTab::showItemPreview(const ItemData& item, const QString& slotTitle)
{
    clearComparison(false);

    auto spells = loadItemSpells(item);
    auto limitNames = buildLimitSpellNames(spells);
    bool isBisPreview = _bisNames &&
        _bisNames->contains(QString::fromStdString(item.name));
    _itemPreviewLayout->addWidget(makeItemCard(&item, nullptr,
                                                spells.empty() ? nullptr : &spells,
                                                {},
                                                limitNames.empty() ? nullptr : &limitNames,
                                                slotTitle,
                                                isBisPreview));
    _itemPreviewArea->setVisible(true);
}

void CharacterTab::clearItemPreview()
{
    while (_itemPreviewLayout->count()) {
        auto* child = _itemPreviewLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    _itemPreviewArea->setVisible(false);
}

// ── onShowSources ─────────────────────────────────────────────────────────

void CharacterTab::onShowSources(int itemId, const QString& itemName)
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString::fromUtf8("Sources \xe2\x80\x94 ") + itemName);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(480, 360);
    dlg->setStyleSheet(QString("background: %1; color: %2;").arg(kBgMain, kTextBase));

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto* header = new QLabel(
        QString("<b>%1</b> est dropp\xc3\xa9 par :").arg(itemName));
    header->setTextFormat(Qt::RichText);
    header->setStyleSheet(QString("color: %1; font-size: 14px;").arg(kAccentBlue));
    layout->addWidget(header);

    // État chargement (remplacé dès que la requête revient)
    auto* loadingLbl = new QLabel(QString::fromUtf8("Chargement\xe2\x80\xa6"));
    loadingLbl->setAlignment(Qt::AlignCenter);
    loadingLbl->setStyleSheet(QString("color: %1; font-size: 13px; font-style: italic;").arg(kTextDim));
    layout->addWidget(loadingLbl, 1);

    // Zone de résultats (masquée jusqu'à la fin de la requête)
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { border: 1px solid %1; background: %2; }").arg(kBorderCard, kSurfaceDialog));
    scroll->setVisible(false);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* innerL = new QVBoxLayout(inner);
    innerL->setContentsMargins(6, 6, 6, 6);
    innerL->setSpacing(3);
    scroll->setWidget(inner);
    layout->addWidget(scroll, 1);

    auto* closeBtn = new QPushButton("Fermer");
    closeBtn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; "
                "border-radius: 3px; color: %3; padding: 4px 16px; font-size: 13px; }"
                "QPushButton:hover { border-color: %4; }")
        .arg(kBorderMid, kBorderCard, kTextBase, kAccentBlue));
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    // Requête async — le watcher est parente de this (pas dlg) pour survivre à une fermeture rapide
    auto* watcher = new QFutureWatcher<QList<NpcSourceData>>(this);
    QPointer<QDialog>  dlgPtr      = dlg;
    QPointer<QLabel>   loadingPtr  = loadingLbl;
    QPointer<QScrollArea> scrollPtr = scroll;
    QPointer<QVBoxLayout> innerLPtr = innerL;

    connect(watcher, &QFutureWatcher<QList<NpcSourceData>>::finished,
            this, [watcher, dlgPtr, loadingPtr, scrollPtr, innerLPtr]() {
        watcher->deleteLater();
        if (!dlgPtr) return;  // dialog fermé avant la fin
        auto sources = watcher->result();

        if (loadingPtr) loadingPtr->setVisible(false);
        if (!innerLPtr || !scrollPtr) return;

        if (sources.isEmpty()) {
            auto* lbl = new QLabel(
                QString::fromUtf8("Aucun NPC ne droppe cet item dans la base de donn\xc3\xa9" "es."));
            lbl->setStyleSheet(QString("color: %1; font-size: 13px;").arg(kTextSecondary));
            lbl->setAlignment(Qt::AlignCenter);
            innerLPtr->addWidget(lbl);
        } else {
            for (const auto& src : sources) {
                QString zone = src.zone_long_name.empty()
                    ? "?"
                    : QString::fromStdString(src.zone_long_name);
                QString text = QString("<b>%1</b>  <span style='color:%5;font-size:13px;'>"
                                       "niv.%2 \xe2\x80\x94 %3 \xe2\x80\x94 ~%4%</span>")
                    .arg(QString::fromStdString(src.name).replace('_', ' '))
                    .arg(src.level)
                    .arg(zone)
                    .arg(src.drop_chance, 0, 'f', 1)
                    .arg(kTextSecondary);
                auto* lbl = new QLabel(text);
                lbl->setTextFormat(Qt::RichText);
                lbl->setStyleSheet("border: none; background: transparent; font-size: 13px;");
                innerLPtr->addWidget(lbl);
            }
            innerLPtr->addStretch();
        }
        scrollPtr->setVisible(true);
    });

    ItemDatabase* db = _itemDb;
    watcher->setFuture(QtConcurrent::run([db, itemId]() {
        return db->getNpcSources(itemId);
    }));

    dlg->exec();
}

// ── detectSlots ──────────────────────────────────────────────────────────

std::vector<QString> CharacterTab::detectSlots(const ItemData& item) const
{
    std::vector<QString> result;
    // D'abord les slots où on a déjà quelque chose d'équipé (priorité)
    for (auto& [slotName, bit] : SLOT_BITMASK) {
        if ((item.item_slots & bit) && _equippedItems.count(slotName))
            result.push_back(QString::fromStdString(slotName));
    }
    if (!result.empty()) return result;
    // Sinon, tous les slots compatibles
    for (auto& [slotName, bit] : SLOT_BITMASK) {
        if (item.item_slots & bit)
            result.push_back(QString::fromStdString(slotName));
    }
    return result;
}

// ── canEquip ─────────────────────────────────────────────────────────────

bool CharacterTab::canEquip(const ItemData& item) const
{
    // Doit avoir au moins un slot valide
    bool hasSlot = false;
    for (auto& [slotName, bit] : SLOT_BITMASK) {
        if (item.item_slots & bit) { hasSlot = true; break; }
    }
    if (!hasSlot) return false;

    if (!_charInfo) return true;

    // Vérification de classe
    if (item.classes != 65535 && item.classes != 0) {
        auto it = CLASS_BITS.find(_charInfo->class_name);
        if (it != CLASS_BITS.end() && !(item.classes & it->second))
            return false;
    }

    // Vérification de niveau
    if (item.reqlevel > 0 && _charInfo->level > 0 &&
        _charInfo->level < item.reqlevel)
        return false;

    return true;
}

// ── buildStatTooltip ──────────────────────────────────────────────────────

QString CharacterTab::buildStatTooltip(const std::string& stat,
                                        int dispVal, bool hasCap, int cap,
                                        const PlayerTotals& totals) const
{
    // Couleur d'accent selon la catégorie du stat
    const char* catAccent = kTextBase;
    for (auto& [catName, catStats] : STAT_CATEGORIES) {
        for (auto& s : catStats) {
            if (s == stat) {
                auto it = CAT_COLORS.find(catName);
                if (it != CAT_COLORS.end()) { catAccent = it->second.accent; break; }
            }
        }
    }

    QString name = QString::fromStdString(STAT_LABELS.count(stat) ? STAT_LABELS.at(stat) : stat);
    QString sfx  = QString::fromStdString(STAT_SUFFIX.count(stat) ? STAT_SUFFIX.at(stat) : "");

    // Valeur brute (avant cap) pour les stats capped
    int rawVal = dispVal;
    if      (stat == "astr") rawVal = totals.str_v;
    else if (stat == "asta") rawVal = totals.sta;
    else if (stat == "adex") rawVal = totals.dex;
    else if (stat == "aagi") rawVal = totals.agi;
    else if (stat == "awis") rawVal = totals.wis;
    else if (stat == "aint") rawVal = totals.int_v;
    else if (stat == "acha") rawVal = totals.cha;
    else if (stat == "mr")   rawVal = totals.mr;
    else if (stat == "fr")   rawVal = totals.fr;
    else if (stat == "cr")   rawVal = totals.cr;
    else if (stat == "dr")   rawVal = totals.dr;
    else if (stat == "pr")   rawVal = totals.pr;

    // ── Constructeurs de lignes HTML ──────────────────────────────────────
    auto secRow = [](const char* lbl, const char* color) -> QString {
        return QString(
            "<tr><td colspan='2' style='color:%1;font-size:13px;font-weight:bold;"
            "padding-top:5px;'>\xe2\x94\x80 %2</td></tr>")
            .arg(color).arg(lbl);
    };
    auto valRow = [](const QString& lbl, const QString& val,
                     const char* vc, bool italic = false) -> QString {
        QString ls = italic ? QString("color:%1;font-style:italic;").arg(kTextSecondary)
                            : QString("color:%1;").arg(kTextSecondary);
        QString vs = italic ? QString("color:%1;font-style:italic;").arg(vc)
                            : QString("color:%1;").arg(vc);
        return QString(
            "<tr>"
            "<td style='padding-left:10px;padding-right:12px;'>"
            "<span style='%1'>%2</span></td>"
            "<td align='right'><span style='%3'>%4</span></td>"
            "</tr>")
            .arg(ls).arg(lbl).arg(vs).arg(val);
    };

    // ── Ligne d'en-tête : Nom + Valeur / Cap ─────────────────────────────
    QString headerVal;
    if (hasCap) {
        headerVal = QString(
            "<span style='color:%1;font-weight:bold;'>%2%3</span>"
            "<span style='color:%5;'> / %4%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx).arg(cap).arg(kTextDim);
        if (rawVal > cap)
            headerVal += QString("<span style='color:%1;'> (brut %2)</span>").arg(kTextTileKey).arg(rawVal);
    } else {
        headerVal = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx);
    }

    QString rows = QString(
        "<tr>"
        "<td><span style='color:%1;font-weight:bold;font-size:14px;'>%2</span></td>"
        "<td align='right' style='font-size:14px;'>%3</td>"
        "</tr>")
        .arg(catAccent).arg(name).arg(headerVal);

    // ── Décomposition par stat ────────────────────────────────────────────

    if (isAttrStat(stat)) {
        // BASE : valeur de base du personnage
        int baseVal = 0;
        if (_charInfo) {
            if      (stat == "astr") baseVal = _charInfo->base_str;
            else if (stat == "asta") baseVal = _charInfo->base_sta;
            else if (stat == "adex") baseVal = _charInfo->base_dex;
            else if (stat == "aagi") baseVal = _charInfo->base_agi;
            else if (stat == "awis") baseVal = _charInfo->base_wis;
            else if (stat == "aint") baseVal = _charInfo->base_int;
            else if (stat == "acha") baseVal = _charInfo->base_cha;
        }
        rows += secRow("BASE", kAccentBlue);
        rows += valRow("Personnage", QString::number(baseVal), kAccentBlue);

        // ITEMS
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            QString sign = v >= 0 ? "+" : "";
            rows += valRow(QString::fromStdString(item.name),
                           sign + QString::number(v), kGreen);
        }

    } else if (isResistStat(stat)) {
        // Somme items
        int itemSum = 0;
        for (auto& [s, item] : _equippedItems) itemSum += getItemStat(stat, item);
        int baseVal = rawVal - itemSum;

        rows += secRow("BASE", kAccentBlue);
        QString baseLbl = _charInfo
            ? QString("Race + %1 L%2")
                  .arg(QString::fromStdString(_charInfo->class_name))
                  .arg(_charInfo->level)
            : "Race + Classe";
        rows += valRow(baseLbl, QString::number(baseVal), kAccentBlue);

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (v >= 0 ? "+" : "") + QString::number(v), kGreen);
        }

    } else if (stat == "hp") {
        int itemHpSum = 0;
        for (auto& [s, item] : _equippedItems) itemHpSum += item.hp;
        int baseHp = totals.hp.base - itemHpSum;

        rows += secRow("BASE", kAccentBlue);
        if (_charInfo)
            rows += valRow(
                QString("%1 L%2 (STA %3)")
                    .arg(QString::fromStdString(_charInfo->class_name))
                    .arg(_charInfo->level)
                    .arg(std::min(totals.sta, 255)),
                QString::number(baseHp), kAccentBlue);

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.hp >= 0 ? "+" : "") + QString::number(item.hp), kGreen);
        }
        if (itemHpSum > 2000) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow("Items brut", QString::number(itemHpSum),  kOrange, true);
            rows += valRow("Plafond items (RuleI ItemHPCap)", "+2000", kOrange, true);
        }

    } else if (stat == "mana") {
        int itemManaSum = 0;
        for (auto& [s, item] : _equippedItems) itemManaSum += item.mana;
        int baseMana = totals.mana.base - itemManaSum;

        rows += secRow("BASE", kAccentBlue);
        if (_charInfo) {
            bool isInt = (_charInfo->class_name == "Wizard"     ||
                          _charInfo->class_name == "Magician"   ||
                          _charInfo->class_name == "Enchanter"  ||
                          _charInfo->class_name == "Necromancer");
            int primStat = isInt ? std::min(totals.int_v, 255) : std::min(totals.wis, 255);
            rows += valRow(
                QString("%1 L%2 (%3 %4)")
                    .arg(QString::fromStdString(_charInfo->class_name))
                    .arg(_charInfo->level)
                    .arg(isInt ? "INT" : "SAG")
                    .arg(primStat),
                QString::number(baseMana), kAccentBlue);
        }
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.mana >= 0 ? "+" : "") + QString::number(item.mana), kGreen);
        }
        if (itemManaSum > 2000) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow("Items brut", QString::number(itemManaSum), kOrange, true);
            rows += valRow("Plafond items (RuleI ItemManaCap)", "+2000", kOrange, true);
        }

    } else if (stat == "atk") {
        int itemAtkSum = 0;
        for (auto& [s, item] : _equippedItems) itemAtkSum += item.atk;
        int cappedAtk = std::min(itemAtkSum, 250);
        int totalStr = _charInfo ? std::min(totals.str_v, 255) : 0;
        int strBonus = totalStr >= 75 ? (2 * totalStr - 150) / 3 : 0;

        rows += secRow("ITEMS ATK", kGreen);
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.atk == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.atk), kGreen);
        }
        if (!hasItems)
            rows += valRow("(aucun item ATK)", "0", kTextDim);

        rows += secRow("FORMULE", kOrange);
        rows += valRow("Items ATK (cap 250)",  QString::number(cappedAtk),       kOrange, true);
        rows += valRow(QString("Bonus FOR (%1)").arg(totalStr),
                       QString("+%1").arg(strBonus),                             kOrange, true);
        rows += valRow("Affich\xc3\xa9" " = (toHit + offense) \xc3\x97 1000/744",
                       QString::number(totals.atk),                              kOrange, true);

    } else if (stat == "ac") {
        int itemAcSum = 0;
        for (auto& [s, item] : _equippedItems) itemAcSum += item.ac;

        rows += secRow("ITEMS AC", kGreen);
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.ac == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.ac), kGreen);
        }
        if (!hasItems)
            rows += valRow("(aucun)", "0", kTextDim);

        rows += secRow("FORMULE", kOrange);
        rows += valRow("AC brut items",           QString::number(itemAcSum),        kOrange, true);
        rows += valRow("Mitigation (\xc3\x97" "4/3 hors casters)",
                       QString::number(totals.mitigation),                          kOrange, true);
        rows += valRow("Affich\xc3\xa9" " = (avoid + mit) \xc3\x97 1000/847",
                       QString::number(totals.ac),                                  kOrange, true);

    } else if (stat == "haste") {
        // Max-wins : trouver le gagnant
        int maxVal = 0;
        std::string winnerName;
        for (auto& [s, item] : _equippedItems) {
            if (item.haste > maxVal) { maxVal = item.haste; winnerName = item.name; }
        }
        if (maxVal > 0) {
            rows += secRow("ITEMS (max-wins)", kGreen);
            for (auto& [slotName, item] : _equippedItems) {
                if (item.haste == 0) continue;
                bool winner = (item.haste == maxVal && item.name == winnerName);
                QString lbl = QString::fromStdString(item.name);
                if (winner)
                    lbl += QString(" <span style='color:%1;'>\xe2\x86\x90</span>").arg(kAccentAtCap);
                else
                    lbl += QString(" <span style='color:%1;'>(ignor\xc3\xa9" ")</span>").arg(kTextDim);
                rows += valRow(lbl,
                               QString("+%1%").arg(item.haste),
                               winner ? kAccentAtCap : kTextDim);
            }
        }
        if (_charInfo) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow(QString("Cap niveau %1").arg(_charInfo->level),
                           QString("%1%").arg(totals.haste), kOrange, true);
        }

    } else if (stat == "hp_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.hp_regen), kGreen);
        }

    } else if (stat == "mana_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.mana_regen), kGreen);
        }
    }

    return QString(
        "<table cellspacing='0' cellpadding='1' style='font-size:14px;min-width:260px;'>"
        "%1"
        "</table>")
        .arg(rows);
}

// ── expansionCaps ─────────────────────────────────────────────────────────

std::pair<int,int> CharacterTab::expansionCaps() const
{
    auto exp = _config->get("current_expansion");
    if (exp == "Luclin" || exp == "Planes of Power")
        return {255, 500};
    return {200, 200};
}

// ── isSlotAvailable ───────────────────────────────────────────────────────

bool CharacterTab::isSlotAvailable(const std::string& slotName) const
{
    // Le slot Charm n'existe qu'à partir de Planes of Power
    if (slotName == "Charm")
        return _config->get("current_expansion") == "Planes of Power";
    return true;
}
