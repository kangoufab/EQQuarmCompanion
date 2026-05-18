#include "ui/character_tab.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "core/stats_calculator.h"
#include "db/item_database.h"

#undef slots   // Qt macro conflicts with local variable names

#include <QVBoxLayout>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <algorithm>
#include <set>

// ── Constantes portées depuis Python stat_caps.py et character_tab.py ─────
// Clés ASCII neutres pour les maps ; labels affichés via CAT_LABELS

struct CatColors { const char* bg; const char* border; const char* accent; };

static const std::vector<std::pair<std::string, std::vector<std::string>>> STAT_CATEGORIES = {
    {"Melee",   {"astr", "adex", "atk", "haste"}},
    {"Range",   {"adex", "atk", "haste"}},
    {"Defense", {"asta", "aagi", "hp", "ac", "hp_regen", "mr", "fr", "cr", "dr", "pr"}},
    {"Sorts",   {"aint", "awis", "acha", "mana", "mana_regen"}},
};

// Labels UTF-8 affichés (séparés des clés pour éviter les conflits d'encoding)
static const std::map<std::string, const char*> CAT_LABELS = {
    {"Melee",   "M\xc3\xaalée"},
    {"Range",   "Distance"},
    {"Defense", "D\xc3\xa9" "fense"},
    {"Sorts",   "Sorts"},
};

static const std::map<std::string, CatColors> CAT_COLORS = {
    {"Melee",   {"#2a1a1a", "#5a3a3a", "#e57373"}},
    {"Range",   {"#2a241a", "#5a4a3a", "#ffb74d"}},
    {"Defense", {"#1a2236", "#3a4a6a", "#64b5f6"}},
    {"Sorts",   {"#241a2a", "#4a3a5a", "#ba68c8"}},
};

static const std::map<std::string, std::set<std::string>> CLASS_CATEGORIES = {
    {"Warrior",      {"Melee", "Defense"}},
    {"Cleric",       {"Defense", "Sorts"}},
    {"Paladin",      {"Melee", "Defense", "Sorts"}},
    {"Ranger",       {"Melee", "Range", "Defense", "Sorts"}},
    {"Shadowknight", {"Melee", "Defense", "Sorts"}},
    {"Druid",        {"Defense", "Sorts"}},
    {"Monk",         {"Melee", "Range", "Defense"}},
    {"Bard",         {"Melee", "Range", "Defense", "Sorts"}},
    {"Rogue",        {"Melee", "Range", "Defense"}},
    {"Shaman",       {"Defense", "Sorts"}},
    {"Necromancer",  {"Defense", "Sorts"}},
    {"Wizard",       {"Defense", "Sorts"}},
    {"Magician",     {"Defense", "Sorts"}},
    {"Enchanter",    {"Defense", "Sorts"}},
    {"Beastlord",    {"Melee", "Defense", "Sorts"}},
};

static const std::map<std::string, std::string> STAT_LABELS = {
    {"astr", "STR"}, {"asta", "STA"}, {"aagi", "AGI"}, {"adex", "DEX"},
    {"awis", "WIS"}, {"aint", "INT"}, {"acha", "CHA"},
    {"hp", "HP"}, {"ac", "AC"}, {"mana", "Mana"}, {"atk", "ATK"},
    {"haste", "Haste"}, {"hp_regen", "HP/tick"}, {"mana_regen", "Mana/tick"},
    {"mr", "Magic"}, {"fr", "Fire"}, {"cr", "Cold"}, {"dr", "Disease"}, {"pr", "Poison"},
};

static const std::map<std::string, std::string> STAT_SUFFIX = {
    {"haste", "%"}, {"hp_regen", "/tick"}, {"mana_regen", "/tick"},
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

static int getStatValue(const std::string& stat, const PlayerTotals& t) {
    if (stat == "astr")      return t.str_v;
    if (stat == "asta")      return t.sta;
    if (stat == "adex")      return t.dex;
    if (stat == "aagi")      return t.agi;
    if (stat == "aint")      return t.int_v;
    if (stat == "awis")      return t.wis;
    if (stat == "acha")      return t.cha;
    if (stat == "hp")        return t.hp.capped;
    if (stat == "mana")      return t.mana.capped;
    if (stat == "ac")        return t.mitigation;
    if (stat == "atk")       return t.atk;
    if (stat == "haste")     return t.haste;
    if (stat == "hp_regen")  return t.hp_regen;
    if (stat == "mana_regen") return t.mana_regen;
    if (stat == "mr")        return t.mr;
    if (stat == "fr")        return t.fr;
    if (stat == "cr")        return t.cr;
    if (stat == "dr")        return t.dr;
    if (stat == "pr")        return t.pr;
    return 0;
}

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
    clearComparison();
    refreshStats();
}

// ── buildUi ───────────────────────────────────────────────────────────────

void CharacterTab::buildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    outerLayout->addWidget(scroll);

    auto* container = new QWidget;
    container->setStyleSheet("background: #0f1624;");
    scroll->setWidget(container);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Header (classe + niveau)
    {
        auto* frame = new QFrame;
        frame->setStyleSheet(
            "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        _lblHeader = new QLabel("\xe2\x80\x94");
        _lblHeader->setStyleSheet(
            "font-weight: bold; font-size: 12px; color: #e0e0e0; "
            "border: none; background: transparent;");
        fl->addWidget(_lblHeader);
        layout->addWidget(frame);
    }

    // Conteneur pour la barre "Stats actuelles" (reconstruite à chaque refresh)
    {
        auto* bw = new QWidget;
        bw->setStyleSheet("background: transparent;");
        _beforeLayout = new QVBoxLayout(bw);
        _beforeLayout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(bw);
    }

    // Conteneur pour la barre "Si équipé (slot)" (masqué par défaut)
    {
        _afterContainer = new QWidget;
        _afterContainer->setStyleSheet("background: transparent;");
        _afterLayout = new QVBoxLayout(_afterContainer);
        _afterLayout->setContentsMargins(0, 0, 0, 0);
        _afterContainer->setVisible(false);
        layout->addWidget(_afterContainer);
    }

    // Barre de recherche
    {
        auto* row = new QHBoxLayout;
        _searchCombo = new SearchComboBox;
        _searchCombo->setEditable(true);
        _searchCombo->setInsertPolicy(QComboBox::NoInsert);
        _searchCombo->setCompleter(nullptr);
        _searchCombo->setMinimumWidth(300);
        _searchCombo->lineEdit()->setPlaceholderText(
            QString::fromUtf8("Rechercher un item\xe2\x80\xa6"));
        _searchCombo->setStyleSheet(
            "QComboBox { background: #1a2236; border: 1px solid #3a4a6a; "
            "border-radius: 3px; color: #c0c0c0; padding: 3px 6px; font-size: 11px; }"
            "QComboBox:hover { border-color: #64b5f6; }"
            "QComboBox QAbstractItemView { background: #1a2236; border: 1px solid #3a4a6a; "
            "color: #c0c0c0; selection-background-color: #2a3a5a; }");
        connect(_searchCombo, &SearchComboBox::popup_requested,
                this, &CharacterTab::onSearchPopup);
        connect(_searchCombo, qOverload<int>(&QComboBox::activated),
                this, &CharacterTab::onItemSelected);
        row->addWidget(_searchCombo);
        row->addStretch();
        auto* rw = new QWidget;
        rw->setStyleSheet("background: transparent;");
        rw->setLayout(row);
        layout->addWidget(rw);
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

// ── refreshStats ─────────────────────────────────────────────────────────

void CharacterTab::refreshStats()
{
    // Header
    if (_charInfo && !_charInfo->name.empty()) {
        _lblHeader->setText(
            QString::fromStdString(_charInfo->name) + "  —  " +
            QString::fromStdString(_charInfo->class_name) +
            "  niv. " + QString::number(_charInfo->level));
    } else {
        _lblHeader->setText(QString::fromUtf8("\xe2\x80\x94"));
    }

    // Reconstruire la barre "Stats actuelles"
    while (_beforeLayout->count()) {
        auto* child = _beforeLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (_totals && _charInfo && _charInfo->level > 0) {
        auto [attrCap, resistCap] = expansionCaps();
        _beforeLayout->addWidget(
            makeStatsBar("Stats actuelles", *_totals, attrCap, resistCap));
    }
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

    auto* frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(
        "QFrame { background: #0f1624; border: none; }");
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);

    // Label de section
    auto* headerLbl = new QLabel(label);
    headerLbl->setStyleSheet(
        "font-size: 10px; color: #888; font-variant: small-caps; "
        "border: none; background: transparent;");
    outer->addWidget(headerLbl);

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
            QString("font-size: 10px; color: %1; font-variant: small-caps; "
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

            int rawVal     = getStatValue(stat, totals);
            int dispVal    = hasCap ? std::min(rawVal, cap) : rawVal;
            bool atCap     = hasCap && rawVal >= cap;

            const char *tileBg, *tileFg;
            if (!hasCap)      { tileBg = "#1e2a1e"; tileFg = "#81c784"; }
            else if (atCap)   { tileBg = "#1e3a5f"; tileFg = "#4fc3f7"; }
            else               { tileBg = "#252540"; tileFg = "#cccccc"; }

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
                "font-size: 9px; color: #888; border: none; background: transparent;");
            nameLbl->setAlignment(Qt::AlignCenter);
            tileL->addWidget(nameLbl);

            QString sfx = QString::fromStdString(
                STAT_SUFFIX.count(stat) ? STAT_SUFFIX.at(stat) : "");
            QString valHtml;
            if (hasCap) {
                valHtml = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>"
                                  "<span style='color:#555;font-size:9px;'>/%4</span>")
                    .arg(tileFg).arg(dispVal).arg(sfx).arg(cap);
            } else {
                valHtml = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>")
                    .arg(tileFg).arg(dispVal).arg(sfx);
            }

            if (refTotals) {
                int refRaw  = getStatValue(stat, *refTotals);
                int refDisp = hasCap ? std::min(refRaw, cap) : refRaw;
                int delta   = dispVal - refDisp;
                if (delta > 0)
                    valHtml += QString("<span style='color:#2a8a2a;font-size:9px;'> +%1</span>")
                                   .arg(delta);
                else if (delta < 0)
                    valHtml += QString("<span style='color:#aa2222;font-size:9px;'> %1</span>")
                                   .arg(delta);
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

// ── makeItemCard ─────────────────────────────────────────────────────────

static void addSeparator(QVBoxLayout* layout) {
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { border: none; border-top: 1px solid #1e2a3a; "
                       "margin: 3px 0; background: transparent; }");
    layout->addWidget(sep);
}

static void addSectionHeader(QVBoxLayout* layout, const char* label, const char* color) {
    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(
        QString("font-size: 8px; color: %1; font-weight: bold; letter-spacing: 1px; "
                "border: none; background: transparent;").arg(color));
    layout->addWidget(lbl);
}

QFrame* CharacterTab::makeItemCard(const ItemData* item, const ItemData* refItem,
                                    const QString& title, bool showDeltas)
{
    auto* frame = new QFrame;
    frame->setStyleSheet(
        "QFrame { background: #111c2e; border-radius: 5px; border: 1px solid #2a3a5a; }");

    auto* outerL = new QVBoxLayout(frame);
    outerL->setContentsMargins(0, 0, 0, 0);
    outerL->setSpacing(0);

    // ── En-tête (nom de l'item) ───────────────────────────────────────────
    auto* header = new QWidget;
    header->setStyleSheet(
        "QWidget { background: #1a2a40; border-radius: 5px 5px 0 0; "
        "border-bottom: 1px solid #2a3a5a; }");
    auto* hL = new QHBoxLayout(header);
    hL->setContentsMargins(10, 7, 10, 7);
    auto* titleLbl = new QLabel(title);
    titleLbl->setStyleSheet(
        "font-weight: bold; font-size: 11px; color: #d0e8ff; "
        "border: none; background: transparent;");
    titleLbl->setWordWrap(true);
    hL->addWidget(titleLbl);
    outerL->addWidget(header);

    // ── Corps ─────────────────────────────────────────────────────────────
    auto* body = new QWidget;
    body->setStyleSheet("QWidget { background: transparent; }");
    auto* bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(10, 8, 10, 10);
    bodyL->setSpacing(1);

    if (!item) {
        auto* lbl = new QLabel(QString::fromUtf8("(vide)"));
        lbl->setStyleSheet(
            "color: #555; font-style: italic; font-size: 10px; "
            "border: none; background: transparent;");
        bodyL->addWidget(lbl);
        bodyL->addStretch();
        outerL->addWidget(body);
        return frame;
    }

    // Groupes de stats : {titre, couleur, liste de (stat_key, label)}
    struct StatGroup {
        const char* title;
        const char* color;
        std::vector<std::pair<std::string, std::string>> stats;
    };
    static const std::vector<StatGroup> GROUPS = {
        {"COMBAT", "#64b5f6",
            {{"hp","HP"},{"mana","Mana"},{"ac","AC"},{"atk","ATK"},{"damage","Dég"},{"delay","Vitesse"}}},
        {"ATTRIBUTS", "#ffb74d",
            {{"astr","FOR"},{"asta","CON"},{"adex","DEX"},{"aagi","AGI"},
             {"awis","SAG"},{"aint","INT"},{"acha","CHA"}}},
        {"RÉSISTS", "#ef5350",
            {{"mr","Mag"},{"fr","Feu"},{"cr","Froid"},{"dr","Maladie"},{"pr","Poison"}}},
        {"AUTRES", "#81c784",
            {{"haste","Haste"},{"hp_regen","HP/tick"},{"mana_regen","Mana/tick"}}},
    };

    bool firstGroup = true;
    for (auto& grp : GROUPS) {
        // Collecter les stats non-nulles de ce groupe
        struct Row { QString label; int val; int delta; };
        std::vector<Row> rows;
        for (auto& [stat, lbl] : grp.stats) {
            int v = getItemStat(stat, *item);
            int d = refItem ? v - getItemStat(stat, *refItem) : 0;
            if (v == 0 && d == 0) continue;
            rows.push_back({QString::fromStdString(lbl), v, d});
        }
        if (rows.empty()) continue;

        if (!firstGroup) addSeparator(bodyL);
        firstGroup = false;
        addSectionHeader(bodyL, grp.title, grp.color);

        // Grille 3 colonnes : nom · valeur · delta
        auto* grid = new QWidget;
        grid->setStyleSheet("background: transparent;");
        auto* gridL = new QGridLayout(grid);
        gridL->setContentsMargins(0, 2, 0, 2);
        gridL->setVerticalSpacing(2);
        gridL->setHorizontalSpacing(6);

        for (int r = 0; r < (int)rows.size(); ++r) {
            auto& row = rows[r];

            auto* nameLbl = new QLabel(row.label);
            nameLbl->setStyleSheet(
                "color: #667788; font-size: 9px; border: none; background: transparent;");
            gridL->addWidget(nameLbl, r, 0);

            auto* valLbl = new QLabel(QString::number(row.val));
            valLbl->setStyleSheet(
                "color: #c8d8e8; font-size: 10px; font-weight: bold; "
                "border: none; background: transparent;");
            valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            gridL->addWidget(valLbl, r, 1);

            if (showDeltas && row.delta != 0) {
                QString sign  = row.delta > 0 ? "+" : "";
                QString color = row.delta > 0 ? "#4caf50" : "#ef5350";
                auto* dLbl = new QLabel(sign + QString::number(row.delta));
                dLbl->setStyleSheet(
                    QString("color: %1; font-size: 10px; font-weight: bold; "
                            "border: none; background: transparent;").arg(color));
                dLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                gridL->addWidget(dLbl, r, 2);
            }
        }
        gridL->setColumnStretch(0, 3);
        gridL->setColumnStretch(1, 1);
        gridL->setColumnStretch(2, 1);
        bodyL->addWidget(grid);
    }

    // ── Effets ────────────────────────────────────────────────────────────
    bool hasEffect = false;
    auto addEffect = [&](int id, const std::string& name, const char* pfx, const char* col) {
        if (id <= 0 || name.empty()) return;
        if (!hasEffect) { addSeparator(bodyL); hasEffect = true; }
        auto* lbl = new QLabel(
            QString("<span style='color:%1;font-weight:bold;'>%2</span>"
                    "<span style='color:#aaa;'> %3</span>")
            .arg(col).arg(pfx).arg(QString::fromStdString(name)));
        lbl->setTextFormat(Qt::RichText);
        lbl->setStyleSheet("font-size: 9px; font-style: italic; "
                           "border: none; background: transparent;");
        lbl->setWordWrap(true);
        bodyL->addWidget(lbl);
    };
    addEffect(item->worneffect,  item->worneffect_name,  "Worn",  "#8888ff");
    addEffect(item->focuseffect, item->focuseffect_name, "Focus", "#88cc88");
    if (item->damage > 0)
        addEffect(item->proceffect, item->proceffect_name, "Proc", "#ffaa44");

    bodyL->addStretch();
    outerL->addWidget(body);
    return frame;
}

// ── onSearchPopup ─────────────────────────────────────────────────────────

void CharacterTab::onSearchPopup()
{
    QString query = _searchCombo->lineEdit()->text().trimmed();
    if (query.length() < 2) return;

    auto results = _itemDb->searchItems(query, 50);
    _searchResults.clear();
    _searchCombo->blockSignals(true);
    _searchCombo->clear();

    for (const auto& item : results) {
        if (!canEquip(item)) continue;
        _searchResults.append(item);
        _searchCombo->addItem(QString::fromStdString(item.name));
    }

    _searchCombo->lineEdit()->setText(query);
    _searchCombo->blockSignals(false);
}

// ── onItemSelected ────────────────────────────────────────────────────────

void CharacterTab::onItemSelected(int index)
{
    if (index < 0 || index >= _searchResults.size()) return;
    const ItemData& item = _searchResults[index];

    auto eqSlots = detectSlots(item);
    if (eqSlots.empty()) return;

    clearComparison();
    showComparison(item, eqSlots[0], eqSlots);
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
            "color: #888; font-size: 10px; border: none; background: transparent;");
        slotL->addWidget(slotLbl);

        for (auto& eqSlot : allSlots) {
            auto* btn = new QPushButton(eqSlot);
            bool isCurrent = (eqSlot == slot);
            btn->setEnabled(!isCurrent);
            btn->setStyleSheet(
                isCurrent
                ? "QPushButton { background: #2a3a5a; border: 1px solid #64b5f6; "
                  "border-radius: 3px; color: #64b5f6; padding: 2px 8px; font-size: 10px; }"
                : "QPushButton { background: #1a2236; border: 1px solid #3a4a6a; "
                  "border-radius: 3px; color: #c0c0c0; padding: 2px 8px; font-size: 10px; }"
                  "QPushButton:hover { border-color: #64b5f6; color: #64b5f6; }");
            connect(btn, &QPushButton::clicked, [this, newItem, eqSlot, allSlots]() {
                clearComparison();
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

        // Barre "Si équipé"
        while (_afterLayout->count()) {
            auto* child = _afterLayout->takeAt(0);
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        auto [attrCap, resistCap] = expansionCaps();
        _afterLayout->addWidget(
            makeStatsBar(
                "Si " + QString::fromUtf8("\xc3\xa9") + "quip"
                + QString::fromUtf8("\xc3\xa9") + " (" + slot + ")",
                afterTotals, attrCap, resistCap, _totals));
        _afterContainer->setVisible(true);
    }

    // Cartes de comparaison côte à côte
    auto* colW = new QWidget;
    colW->setStyleSheet("background: transparent;");
    auto* colL = new QHBoxLayout(colW);
    colL->setSpacing(6);
    colL->setContentsMargins(0, 0, 0, 0);

    QString curName = curItem
        ? QString::fromStdString(curItem->name)
        : QString::fromUtf8("(vide)");
    colL->addWidget(makeItemCard(curItem,   nullptr,  curName,   false));
    colL->addWidget(makeItemCard(&newItem,  curItem,
                                 QString::fromStdString(newItem.name), true));
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
        isUpgrade ? "#4caf50" : (scoreDelta == 0 ? "#888" : "#ef5350");

    auto* sumFrame = new QFrame;
    sumFrame->setStyleSheet(
        "QFrame { background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a; }");
    auto* sumL = new QHBoxLayout(sumFrame);
    sumL->setContentsMargins(8, 6, 8, 6);

    QString sign = scoreDelta > 0 ? "+" : "";
    auto* scoreLbl = new QLabel(
        QString("Score : %1%2  <b>%3</b>").arg(sign).arg(scoreDelta).arg(upgradeText));
    scoreLbl->setTextFormat(Qt::RichText);
    scoreLbl->setStyleSheet(
        QString("color: %1; font-size: 11px; border: none; background: transparent;")
        .arg(upgradeColor));
    sumL->addWidget(scoreLbl);
    sumL->addStretch();

    _comparisonLayout->addWidget(sumFrame);
    _comparisonArea->setVisible(true);
}

// ── clearComparison ───────────────────────────────────────────────────────

void CharacterTab::clearComparison()
{
    while (_comparisonLayout->count()) {
        auto* child = _comparisonLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    while (_afterLayout->count()) {
        auto* child = _afterLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    _afterContainer->setVisible(false);
    _comparisonArea->setVisible(false);
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
    const char* catAccent = "#cccccc";
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
            "<tr><td colspan='2' style='color:%1;font-size:9px;font-weight:bold;"
            "padding-top:5px;'>\xe2\x94\x80 %2</td></tr>")
            .arg(color).arg(lbl);
    };
    auto valRow = [](const QString& lbl, const QString& val,
                     const char* vc, bool italic = false) -> QString {
        QString ls = italic ? "color:#999;font-style:italic;" : "color:#999;";
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
            "<span style='color:#555;'> / %4%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx).arg(cap);
        if (rawVal > cap)
            headerVal += QString("<span style='color:#777;'> (brut %1)</span>").arg(rawVal);
    } else {
        headerVal = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx);
    }

    QString rows = QString(
        "<tr>"
        "<td><span style='color:%1;font-weight:bold;font-size:12px;'>%2</span></td>"
        "<td align='right' style='font-size:12px;'>%3</td>"
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
        rows += secRow("BASE", "#64b5f6");
        rows += valRow("Personnage", QString::number(baseVal), "#64b5f6");

        // ITEMS
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            QString sign = v >= 0 ? "+" : "";
            rows += valRow(QString::fromStdString(item.name),
                           sign + QString::number(v), "#81c784");
        }

    } else if (isResistStat(stat)) {
        // Somme items
        int itemSum = 0;
        for (auto& [s, item] : _equippedItems) itemSum += getItemStat(stat, item);
        int baseVal = rawVal - itemSum;

        rows += secRow("BASE", "#64b5f6");
        QString baseLbl = _charInfo
            ? QString("Race + %1 L%2")
                  .arg(QString::fromStdString(_charInfo->class_name))
                  .arg(_charInfo->level)
            : "Race + Classe";
        rows += valRow(baseLbl, QString::number(baseVal), "#64b5f6");

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (v >= 0 ? "+" : "") + QString::number(v), "#81c784");
        }

    } else if (stat == "hp") {
        int itemHpSum = 0;
        for (auto& [s, item] : _equippedItems) itemHpSum += item.hp;
        int baseHp = totals.hp.base - itemHpSum;

        rows += secRow("BASE", "#64b5f6");
        if (_charInfo)
            rows += valRow(
                QString("%1 L%2 (STA %3)")
                    .arg(QString::fromStdString(_charInfo->class_name))
                    .arg(_charInfo->level)
                    .arg(std::min(totals.sta, 255)),
                QString::number(baseHp), "#64b5f6");

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.hp >= 0 ? "+" : "") + QString::number(item.hp), "#81c784");
        }
        if (itemHpSum > 2000) {
            rows += secRow("FORMULE", "#ffb74d");
            rows += valRow("Items brut", QString::number(itemHpSum),  "#ffb74d", true);
            rows += valRow("Plafond items (RuleI ItemHPCap)", "+2000", "#ffb74d", true);
        }

    } else if (stat == "mana") {
        int itemManaSum = 0;
        for (auto& [s, item] : _equippedItems) itemManaSum += item.mana;
        int baseMana = totals.mana.base - itemManaSum;

        rows += secRow("BASE", "#64b5f6");
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
                QString::number(baseMana), "#64b5f6");
        }
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.mana >= 0 ? "+" : "") + QString::number(item.mana), "#81c784");
        }
        if (itemManaSum > 2000) {
            rows += secRow("FORMULE", "#ffb74d");
            rows += valRow("Items brut", QString::number(itemManaSum), "#ffb74d", true);
            rows += valRow("Plafond items (RuleI ItemManaCap)", "+2000", "#ffb74d", true);
        }

    } else if (stat == "atk") {
        int itemAtkSum = 0;
        for (auto& [s, item] : _equippedItems) itemAtkSum += item.atk;
        int cappedAtk = std::min(itemAtkSum, 250);
        int totalStr = _charInfo ? std::min(totals.str_v, 255) : 0;
        int strBonus = totalStr >= 75 ? (2 * totalStr - 150) / 3 : 0;

        rows += secRow("ITEMS ATK", "#81c784");
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.atk == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.atk), "#81c784");
        }
        if (!hasItems)
            rows += valRow("(aucun item ATK)", "0", "#555");

        rows += secRow("FORMULE", "#ffb74d");
        rows += valRow("Items ATK (cap 250)",  QString::number(cappedAtk),       "#ffb74d", true);
        rows += valRow(QString("Bonus FOR (%1)").arg(totalStr),
                       QString("+%1").arg(strBonus),                             "#ffb74d", true);
        rows += valRow("Affich\xc3\xa9" " = (toHit + offense) \xc3\x97 1000/744",
                       QString::number(totals.atk),                              "#ffb74d", true);

    } else if (stat == "ac") {
        int itemAcSum = 0;
        for (auto& [s, item] : _equippedItems) itemAcSum += item.ac;

        rows += secRow("ITEMS AC", "#81c784");
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.ac == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.ac), "#81c784");
        }
        if (!hasItems)
            rows += valRow("(aucun)", "0", "#555");

        rows += secRow("FORMULE", "#ffb74d");
        rows += valRow("AC brut items",           QString::number(itemAcSum),        "#ffb74d", true);
        rows += valRow("Mitigation (\xc3\x97" "4/3 hors casters)",
                       QString::number(totals.mitigation),                          "#ffb74d", true);
        rows += valRow("Affich\xc3\xa9" " = (avoid + mit) \xc3\x97 1000/847",
                       QString::number(totals.mitigation),                          "#ffb74d", true);

    } else if (stat == "haste") {
        // Max-wins : trouver le gagnant
        int maxVal = 0;
        std::string winnerName;
        for (auto& [s, item] : _equippedItems) {
            if (item.haste > maxVal) { maxVal = item.haste; winnerName = item.name; }
        }
        if (maxVal > 0) {
            rows += secRow("ITEMS (max-wins)", "#81c784");
            for (auto& [slotName, item] : _equippedItems) {
                if (item.haste == 0) continue;
                bool winner = (item.haste == maxVal && item.name == winnerName);
                QString lbl = QString::fromStdString(item.name);
                if (winner)
                    lbl += " <span style='color:#4fc3f7;'>\xe2\x86\x90</span>";
                else
                    lbl += " <span style='color:#555;'>(ignor\xc3\xa9" ")</span>";
                rows += valRow(lbl,
                               QString("+%1%").arg(item.haste),
                               winner ? "#4fc3f7" : "#555");
            }
        }
        if (_charInfo) {
            rows += secRow("FORMULE", "#ffb74d");
            rows += valRow(QString("Cap niveau %1").arg(_charInfo->level),
                           QString("%1%").arg(totals.haste), "#ffb74d", true);
        }

    } else if (stat == "hp_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.hp_regen), "#81c784");
        }

    } else if (stat == "mana_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", "#81c784"); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.mana_regen), "#81c784");
        }
    }

    return QString(
        "<table cellspacing='0' cellpadding='1' style='font-size:10px;min-width:260px;'>"
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
