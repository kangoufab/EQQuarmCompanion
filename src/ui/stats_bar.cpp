#include "ui/stats_bar.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// ── Constantes ────────────────────────────────────────────────────────────

struct CatColorsS { const char* bg; const char* border; const char* accent; };

static const std::vector<std::pair<std::string, std::vector<std::string>>> STAT_CATS = {
    {"Melee",   {"astr","adex","atk","haste"}},
    {"Range",   {"adex","atk","haste"}},
    {"Defense", {"asta","aagi","hp","ac","hp_regen","mr","fr","cr","dr","pr"}},
    {"Sorts",   {"aint","awis","acha","mana","mana_regen"}},
};
static const std::map<std::string, const char*> CAT_LABELS = {
    {"Melee",   "M\xc3\xaalée"},
    {"Range",   "Distance"},
    {"Defense", "D\xc3\xa9" "fense"},
    {"Sorts",   "Sorts"},
};
static const std::map<std::string, CatColorsS> CAT_COLORS = {
    {"Melee",   {"#2a1a1a","#5a3a3a","#e57373"}},
    {"Range",   {"#2a241a","#5a4a3a","#ffb74d"}},
    {"Defense", {"#1a2236","#3a4a6a","#64b5f6"}},
    {"Sorts",   {"#241a2a","#4a3a5a","#ba68c8"}},
};
static const std::map<std::string, std::set<std::string>> CLASS_CATS = {
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
static const std::map<std::string, std::string> STAT_LABELS = {
    {"astr","STR"},{"asta","STA"},{"aagi","AGI"},{"adex","DEX"},
    {"awis","WIS"},{"aint","INT"},{"acha","CHA"},
    {"hp","HP"},{"ac","AC"},{"mana","Mana"},{"atk","ATK"},
    {"haste","Haste"},{"hp_regen","HP/tick"},{"mana_regen","Mana/tick"},
    {"mr","Magic"},{"fr","Fire"},{"cr","Cold"},{"dr","Disease"},{"pr","Poison"},
};
static const std::map<std::string, std::string> STAT_SUFFIX = {
    {"haste","%"},{"hp_regen","/tick"},{"mana_regen","/tick"},
};

static bool isAttrS(const std::string& s) {
    return s=="astr"||s=="asta"||s=="adex"||s=="aagi"||s=="awis"||s=="aint"||s=="acha";
}
static bool isResistS(const std::string& s) {
    return s=="mr"||s=="fr"||s=="cr"||s=="dr"||s=="pr";
}
static int getStatValS(const std::string& stat, const PlayerTotals& t) {
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

// ── makePlayerStatsBar ────────────────────────────────────────────────────

QFrame* makePlayerStatsBar(const PlayerTotals& totals,
                            const std::string& className,
                            const std::string& expansion)
{
    // Catégories selon la classe
    std::vector<std::pair<std::string, std::vector<std::string>>> cats;
    auto classIt = CLASS_CATS.find(className);
    if (classIt != CLASS_CATS.end()) {
        for (auto& [name, stats] : STAT_CATS)
            if (classIt->second.count(name))
                cats.push_back({name, stats});
    }
    if (cats.empty()) cats = STAT_CATS;

    // Defense en premier
    auto defIt = std::find_if(cats.begin(), cats.end(),
                              [](const auto& p){ return p.first == "Defense"; });
    if (defIt != cats.end() && defIt != cats.begin())
        std::rotate(cats.begin(), defIt, defIt + 1);

    // Caps d'expansion
    bool oldExp = (expansion == "Classic" || expansion == "Kunark" || expansion == "Velious");
    int attrCap   = oldExp ? 200 : 255;
    int resistCap = oldExp ? 200 : 500;

    auto* frame = new QFrame;
    frame->setStyleSheet("QFrame { background: #0f1624; border: none; }");
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);

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
        auto* panelL = new QVBoxLayout(panel);
        panelL->setContentsMargins(6, 4, 6, 6);
        panelL->setSpacing(3);

        auto lblIt = CAT_LABELS.find(catName);
        const char* catStr = (lblIt != CAT_LABELS.end()) ? lblIt->second : catName.c_str();
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
            bool hasCap = isAttrS(stat) || isResistS(stat);
            int cap     = isAttrS(stat) ? attrCap : (isResistS(stat) ? resistCap : 0);
            int rawVal  = getStatValS(stat, totals);
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

            auto slbIt = STAT_LABELS.find(stat);
            auto sufIt = STAT_SUFFIX.find(stat);
            QString slabel = slbIt != STAT_LABELS.end()
                ? QString::fromStdString(slbIt->second) : QString::fromStdString(stat);
            QString suffix = sufIt != STAT_SUFFIX.end()
                ? QString::fromStdString(sufIt->second) : QString();

            auto* nameLbl = new QLabel(slabel);
            nameLbl->setStyleSheet(
                "font-size: 8px; color: #888888; border: none; background: transparent;");
            nameLbl->setAlignment(Qt::AlignCenter);

            auto* valLbl = new QLabel(QString::number(dispVal) + suffix);
            valLbl->setStyleSheet(
                QString("font-size: 11px; font-weight: bold; color: %1; "
                        "border: none; background: transparent;").arg(tileFg));
            valLbl->setAlignment(Qt::AlignCenter);

            tileL->addWidget(nameLbl);
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
