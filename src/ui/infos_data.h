#pragma once
#include "core/types.h"
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

inline const std::map<std::string, int> kExpCaps = {
    {"Classic",         60},
    {"Kunark",          60},
    {"Velious",         60},
    {"Luclin",          60},
    {"Planes of Power", 65},
};

inline const std::map<std::string, int> kExpIndex = {
    {"Classic", 0}, {"Kunark", 1}, {"Velious", 2},
    {"Luclin",  3}, {"Planes of Power", 4},
};

struct ResistGroup {
    std::string      label;
    std::vector<int> spell_ids;
};

inline const std::map<std::string, std::vector<ResistGroup>> kResistGroups = {
    {"MR", {
        {"occl_multi", {1451, 3375}},
        {"malo_line",  {110, 111, 112, 1577, 1578}},
        {"bard_mr4",   {1764}},
    }},
    {"FR", {
        {"tashan_line", {433, 676, 677, 678, 1702}},
        {"tuyen_fr",    {743, 3367}},
    }},
    {"CR", {
        {"ro_line",  {1437, 1600, 2518}},
        {"tuyen_cr", {744, 3373}},
    }},
    {"PR", {
        {"scent_line", {1511, 1512, 1513}},
        {"tuyen_pr",   {3566, 3370}},
    }},
    {"DR", {
        {"insid_line", {526, 527, 1573, 3695}},
        {"tuyen_dr",   {3567, 3363}},
    }},
};

// Debuff values by expansion index — ported from infos_tab.py::get_resist_debuffs()
// Structure: {expIdx -> {resist_type -> debuff_value}}
inline const std::map<int, std::map<std::string, int>> kDebuffsByExp = {
    {0, {}},   // Classic — no resist debuffs
    {1, {     // Kunark
        {"MR", -62}, {"FR", -35}, {"CR", 0}, {"PR", 0}, {"DR", 0},
    }},
    {2, {     // Velious
        {"MR", -62}, {"FR", -35}, {"CR", -117}, {"PR", -35}, {"DR", -35},
    }},
    {3, {     // Luclin
        {"MR", -154}, {"FR", -219}, {"CR", -117}, {"PR", -113}, {"DR", -113},
    }},
    {4, {     // Planes of Power (same as Luclin for resist debuffs)
        {"MR", -154}, {"FR", -219}, {"CR", -117}, {"PR", -113}, {"DR", -113},
    }},
};

[[nodiscard]] inline std::map<std::string, int>
getResistDebuffs(const std::string& expansionName) {
    auto expIt = kExpIndex.find(expansionName);
    int expIdx = expIt != kExpIndex.end() ? expIt->second : 0;
    auto it = kDebuffsByExp.find(expIdx);
    if (it != kDebuffsByExp.end()) return it->second;
    return {};
}
