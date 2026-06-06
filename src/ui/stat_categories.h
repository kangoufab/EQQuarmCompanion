#pragma once
#include "ui/palette.h"
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

inline const std::map<std::string, CatColors> CAT_COLORS = {
    {"Melee",   {"#2a1a1a", "#5a3a3a", kAccentMelee}},
    {"Range",   {"#2a241a", "#5a4a3a", kOrange}},
    {"Defense", {kBgCard,   kBorderCard, kAccentBlue}},
    {"Sorts",   {"#241a2a", "#4a3a5a", kAccentPurple}},
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
