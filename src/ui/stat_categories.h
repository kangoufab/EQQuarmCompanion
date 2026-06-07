#pragma once
#include "ui/palette.h"
#include "ui/ui_helpers.h"
#include <map>
#include <set>
#include <string>
#include <vector>

// Shared category definitions used by character_tab and stats_bar.
// Centralised here to avoid silent divergence between the two files.

struct CatColors { const char* bg; const char* border; const char* accent; };

inline const std::vector<std::pair<std::string, std::vector<std::string>>> STAT_CATEGORIES = {
    {"Melee",   {"astr","adex","atk","haste"}},
    {"Range",   {"adex","atk","haste"}},
    {"Defense", {"asta","aagi","hp","ac","hp_regen","mr","fr","cr","dr","pr"}},
    {"Sorts",   {"aint","awis","acha","mana","mana_regen"}},
};

// Labels UTF-8 affichés (séparés des clés pour éviter les conflits d'encoding)
inline const std::map<std::string, const char*> CAT_LABELS = {
    {"Melee",   "M\xc3\xaalée"},
    {"Range",   "Distance"},
    {"Defense", "D\xc3\xa9" "fense"},
    {"Sorts",   "Sorts"},
};

// bg/border viennent de sectionTheme() (thème par accent) — accent affiché peut différer
// du thème (ex: Melee utilise le thème kRed mais affiche en kAccentMelee, plus clair).
inline CatColors catColors(const char* themeAccent, const char* displayAccent) {
    auto [bg, border] = sectionTheme(themeAccent);
    return {bg, border, displayAccent};
}

inline const std::map<std::string, CatColors> CAT_COLORS = {
    {"Melee",   catColors(kRed,          kAccentMelee)},
    {"Range",   catColors(kOrange,       kOrange)},
    {"Defense", catColors(kAccentBlue,   kAccentBlue)},
    {"Sorts",   catColors(kAccentPurple, kAccentPurple)},
};

inline const std::map<std::string, std::set<std::string>> CLASS_CATEGORIES = {
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

inline const std::map<std::string, std::string> STAT_LABELS = {
    {"astr","STR"},{"asta","STA"},{"aagi","AGI"},{"adex","DEX"},
    {"awis","WIS"},{"aint","INT"},{"acha","CHA"},
    {"hp","HP"},{"ac","AC"},{"mana","Mana"},{"atk","ATK"},
    {"haste","Haste"},{"hp_regen","HP/tick"},{"mana_regen","Mana/tick"},
    {"mr","Magic"},{"fr","Fire"},{"cr","Cold"},{"dr","Disease"},{"pr","Poison"},
};

inline const std::map<std::string, std::string> STAT_SUFFIX = {
    {"haste","%"},{"hp_regen","/tick"},{"mana_regen","/tick"},
};
