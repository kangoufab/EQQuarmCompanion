#include "core/npc_analysis.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

// ── Internal helpers ──────────────────────────────────────────────────

static float npcDoubleAttackChance(int level) {
    if (level <= 7) return 0.f;
    int daSkill   = level > 50 ? 250 : std::min(level * 5, 210);
    int effective = daSkill + (level > 35 ? level : 0);
    return std::min(1.f, effective / 500.f);
}

static std::unordered_map<int, std::vector<int>>
parseSa(std::string_view raw) {
    std::unordered_map<int, std::vector<int>> result;
    if (raw.empty()) return result;
    std::string s(raw);
    std::replace(s.begin(), s.end(), '^', '\n');
    std::istringstream ss(s);
    std::string entry;
    while (std::getline(ss, entry)) {
        if (entry.empty()) continue;
        std::replace(entry.begin(), entry.end(), ',', ' ');
        std::istringstream es(entry);
        int id; es >> id;
        std::vector<int> params;
        int p; while (es >> p) params.push_back(p);
        result[id] = params;
    }
    return result;
}

static float npcAttacksPerRound(const NpcData& npc) {
    auto sa     = parseSa(npc.special_abilities);
    float base  = static_cast<float>(std::max(1, npc.attack_count));
    float da    = npcDoubleAttackChance(npc.level);
    float triple = (npc.level >= 60 && (npc.npc_class == 1 || npc.npc_class == 32))
                   ? 0.135f : 0.f;
    bool  dw        = sa.count(7) > 0;
    bool  hasFlurry = sa.count(5) > 0;
    float flurry    = hasFlurry
                      ? static_cast<float>(sa.at(5).empty() ? 20 : sa.at(5)[0]) / 100.f
                      : 0.f;
    float attacks = base * (1.f + da + triple) * (dw ? 2.f : 1.f);
    if (hasFlurry) attacks += flurry * base;
    return attacks;
}

// attack.cpp:207-218 — AvoidanceCheck hit probability.
// avoidancePct: 0 baseline, +50 for Evasive discipline (SE_AvoidMeleeChance)
static float avoidanceHitChance(int npcToHit, int playerAvoidance, int avoidancePct) {
    float av = static_cast<float>(playerAvoidance) * (100 + avoidancePct) / 100.f;
    float t  = static_cast<float>(npcToHit) + 10.f;
    float a  = av + 10.f;
    if (t * 1.21f > a)
        return std::max(0.f, 1.f - a / (t * 1.21f * 2.f));
    return std::max(0.f, t * 1.21f / (a * 2.f));
}

// Client::GetAvoidance approximation — defense_skill * 400/225
static int playerAvoidanceScore(std::string_view className, int level) {
    static const std::unordered_map<std::string, int> kDefCap = {
        {"Warrior",252},{"Paladin",252},{"Shadowknight",252},
        {"Monk",252},{"Bard",252},{"Rogue",252},
        {"Ranger",240},{"Beastlord",240},
        {"Cleric",200},{"Druid",200},{"Shaman",200},
        {"Necromancer",145},{"Wizard",145},{"Magician",145},{"Enchanter",145},
    };
    auto it = kDefCap.find(std::string(className));
    int cap = (it != kDefCap.end()) ? it->second : 200;
    int defSkill = std::min(level, 60) * cap / 60;
    int defAv    = defSkill * 400 / 225;
    return std::max(1, defAv);
}

// Mob::GetOffense (attack.cpp:4691) — level formula + DB ATK
static int npcOffense(const NpcData& npc) {
    int level    = std::max(1, npc.level);
    int mobLevel = (level >= 46 && level <= 50) ? 45 : level;
    int base, strOff;
    if (mobLevel < 6) {
        base   = mobLevel * 4;
        strOff = mobLevel;
    } else {
        base   = std::min(mobLevel * 55 / 10 - 4, 320);
        strOff = (mobLevel < 30) ? mobLevel / 2 + 1 : mobLevel * 2 - 40;
    }
    return std::max(1, base + strOff + npc.atk);
}

// Mob::GetToHit (attack.cpp:4801): 7 + WeaponSkill*2 + Accuracy
static int npcToHit(const NpcData& npc) {
    int level = std::max(1, npc.level);
    int ws    = (level > 50) ? 250 : std::min(level * 5, 210);
    return 7 + ws + ws + npc.accuracy;
}

// ── incomingDamage ────────────────────────────────────────────────────

IncomingDamageResult incomingDamage(const NpcData& npc,
                                     const PlayerTotals& player,
                                     std::string_view className,
                                     int level,
                                     std::string_view discipline)
{
    IncomingDamageResult r;
    r.avg_hit = (npc.min_hit + npc.max_hit) / 2.f;

    float delaySec = std::max(0.1f, npc.attack_delay / 10.f);
    float attacks  = npcAttacksPerRound(npc);

    // NPC offense & to-hit
    int npcOff = npcOffense(npc);
    int npcTh  = npcToHit(npc);
    r.npc_offense = npcOff;

    // Player mitigation
    int mit = player.mitigation;
    r.player_mit = mit;

    // Expected d20 roll → effective hit within min-max
    int   den     = npcOff + std::max(1, mit);
    int   expRoll = (den > 0) ? std::min(20, std::max(1, npcOff * 20 / den)) : 10;
    float di      = (npc.max_hit > npc.min_hit)
                    ? static_cast<float>(npc.max_hit - npc.min_hit) / 19.f : 0.f;
    float effHit  = (r.avg_hit > 0.f)
                    ? std::max(1.f, npc.min_hit + (expRoll - 1) * di) : 0.f;
    float mitRatio = (r.avg_hit > 0.f) ? std::max(0.f, 1.f - effHit / r.avg_hit) : 0.f;
    r.mitigation_pct = mitRatio * 100.f;
    r.exp_roll = expRoll;

    // Base avoidance score from class/level (no mitigation stat used here)
    int baseAv  = (level > 0) ? playerAvoidanceScore(className, level) : 300;
    float baseHC = avoidanceHitChance(npcTh, baseAv, 0);

    // Discipline modifiers (mirrors incoming_damage() in npc_analysis.py)
    float discMult = 1.f;
    if (discipline == "defensive") {
        discMult = 0.5f;
        r.disc_note = "defensive";
    } else if (discipline == "evasive") {
        float evasHC = avoidanceHitChance(npcTh, baseAv, 50);
        discMult = (baseHC > 0.f) ? evasHC / baseHC : 1.f;
        r.disc_note = "evasive";
    }
    r.disc_mult = discMult;

    // Base DPS = eff_hit × attacks × hit_chance / delay
    float baseDps = effHit * attacks * baseHC / delaySec;
    r.est_dps     = baseDps * discMult;

    return r;
}

// ── resistRatings ─────────────────────────────────────────────────────

ResistRatings resistRatings(const NpcData& npc, const PlayerTotals& pt, int /*level*/)
{
    auto rate = [&](int playerRes) -> ResistRating {
        ResistRating rr;
        rr.value  = playerRes;
        float den = playerRes + npc.level * 2.f;
        rr.pct    = den > 0.f ? playerRes / den * 100.f : 0.f;
        rr.rating = rr.pct >= 65.f ? Rating::GOOD
                  : rr.pct >= 35.f ? Rating::MEDIUM
                  : Rating::LOW;
        return rr;
    };
    return { rate(pt.mr), rate(pt.fr), rate(pt.cr), rate(pt.dr), rate(pt.pr) };
}

// ── offenseRatings ────────────────────────────────────────────────────

OffenseRatings offenseRatings(const NpcData& npc, const PlayerTotals& pt,
                               std::string_view className)
{
    OffenseRatings r;
    float ratio       = (npc.ac > 0 && pt.atk > 0)
                        ? static_cast<float>(npc.ac) / pt.atk : 0.f;
    r.melee.player_atk = pt.atk;
    r.melee.npc_ac     = npc.ac;
    r.melee.rating = ratio < 0.6f ? OffenseRating::EASY
                   : ratio < 0.9f ? OffenseRating::MEDIUM
                   : OffenseRating::HARD;

    static const std::unordered_set<std::string> kMeleeOnly =
        {"Warrior","Monk","Rogue","Berserker"};
    if (kMeleeOnly.count(std::string(className))) return r;

    auto spellRate = [](int res) -> OffenseRating {
        return res <= 100 ? OffenseRating::EASY
             : res <= 200 ? OffenseRating::MEDIUM
             : OffenseRating::HARD;
    };
    r.mr = SpellOffense{npc.mr, spellRate(npc.mr)};
    r.fr = SpellOffense{npc.fr, spellRate(npc.fr)};
    r.cr = SpellOffense{npc.cr, spellRate(npc.cr)};
    r.dr = SpellOffense{npc.dr, spellRate(npc.dr)};
    r.pr = SpellOffense{npc.pr, spellRate(npc.pr)};
    return r;
}

// ── slowLandPct ───────────────────────────────────────────────────────

std::optional<float> slowLandPct(const NpcData& npc, int playerLevel,
                                  int resistType, int resistDiff)
{
    auto sa = parseSa(npc.special_abilities);
    if (sa.count(12)) return std::nullopt;  // immune to slow

    int npcRes = (resistType == 1) ? npc.mr : npc.dr;
    npcRes = std::max(0, npcRes + resistDiff);

    int levelMod     = (playerLevel - npc.level) * 2;
    int check        = std::max(0, npcRes - levelMod);
    int resistChance = std::max(4, std::min(198, check * 3 / 2));
    return 100.f - resistChance / 200.f * 100.f;
}

// ── spellResistPct ────────────────────────────────────────────────────

std::optional<float> spellResistPct(const SpellData& sp,
                                     const PlayerTotals& pt,
                                     int playerLevel, int npcLevel)
{
    if (sp.resist_type == 0) return std::nullopt;
    int playerRes = 0;
    switch (sp.resist_type) {
        case 1: playerRes = pt.mr; break;
        case 2: playerRes = pt.fr; break;
        case 3: playerRes = pt.cr; break;
        case 4: playerRes = pt.pr; break;
        case 5: playerRes = pt.dr; break;
        default: return std::nullopt;
    }
    int levelMod = (playerLevel - npcLevel) * 2;
    int check    = playerRes + levelMod;
    int rChance  = std::max(4, std::min(198, check * 3 / 2));
    return 100.f - rChance / 200.f * 100.f;
}

// ── Spell helpers ─────────────────────────────────────────────────────

std::string resistLabel(const SpellData& sp) {
    static const std::unordered_map<int,std::string> kMap =
        {{0,"—"},{1,"MR"},{2,"FR"},{3,"CR"},{4,"PR"},{5,"DR"}};
    auto it = kMap.find(sp.resist_type);
    return it != kMap.end() ? it->second : "?";
}

std::string effectiveSpellType(const SpellData& sp) {
    // Porter depuis npc_analysis.py::effective_spell_type()
    // Lire le Python pour la logique complète basée sur spell_type + SPAs
    (void)sp;
    return "spell";
}

std::string targetTypeLabel(int tt) {
    static const std::unordered_map<int,std::string> kMap = {
        {1,"self"},{2,"pet"},{5,"single"},{6,"single"},{8,"pb-ae"},
        {11,"target-ae"},{16,"target-ae"},{40,"pet"},{41,"corpse"}
    };
    auto it = kMap.find(tt);
    return it != kMap.end() ? it->second : "target";
}

std::string formatSpellSummary(const SpellData& sp, int npcLevel) {
    // Porter depuis npc_analysis.py::format_spell_summary()
    (void)sp; (void)npcLevel;
    return "";
}

// ── decodeSpecialAbilities ────────────────────────────────────────────

std::vector<SpecialAbility> decodeSpecialAbilities(std::string_view raw) {
    // IDs from EQMacEmu common/emu_constants.h (SPECATK_* enum)
    static const std::unordered_map<int, std::pair<std::string,std::string>> kTable = {
        {1,  {"Summon",                     "orange"}},
        {2,  {"Enrage",                     "orange"}},
        {3,  {"Rampage",                    "orange"}},
        {4,  {"AE Rampage",                 "red"   }},
        {5,  {"Flurry",                     "orange"}},
        {6,  {"Triple Attack",              "grey"  }},
        {7,  {"Dual Wield",                 "grey"  }},
        {9,  {"Bane Attack",                "orange"}},
        {10, {"Magical Attack",             "orange"}},
        {11, {"Ranged Attack",              "grey"  }},
        {12, {"Immune to Slow",             "red"   }},
        {13, {"Immune to Mez",              "red"   }},
        {14, {"Immune to Charm",            "red"   }},
        {15, {"Immune to Stun",             "red"   }},
        {16, {"Immune to Snare",            "red"   }},
        {17, {"Immune to Fear",             "red"   }},
        {18, {"Immune to Dispel",           "red"   }},
        {19, {"Immune to Melee",            "red"   }},
        {20, {"Immune to Magic",            "red"   }},
        {21, {"Immune to Fleeing",          "red"   }},
        {22, {"Immune to Melee (non-bane)", "red"   }},
        {23, {"Immune to Non-Magic Melee",  "red"   }},
        {24, {"Immune to Aggro",            "orange"}},
        {25, {"Immune to Being Aggro",      "orange"}},
        {26, {"Belly-Caster Only",          "orange"}},
        {27, {"Immune to Feign Death",      "orange"}},
        {28, {"Immune to Taunt",            "orange"}},
        {31, {"Immune to Pacify",           "orange"}},
        {35, {"Immune to Player Damage",    "red"   }},
        {43, {"Always Calls for Help",      "orange"}},
        {50, {"Reverse Slow",               "orange"}},
        {51, {"Immune to Haste",            "orange"}},
        {53, {"Immune to Riposte",          "orange"}},
    };
    // Abilities where params[0] (parts[2]) is shown: id → format suffix
    static const std::unordered_map<int, std::string> kParamFmt = {
        {2, "@ {}%hp"},   // Enrage HP threshold
        {5, "{}%"},       // Flurry custom chance
    };

    std::vector<SpecialAbility> result;
    auto sa = parseSa(raw);
    for (auto& [id, params] : sa) {
        SpecialAbility ab;
        auto it = kTable.find(id);
        if (it == kTable.end()) continue;  // silently skip unknown IDs
        ab.tag      = it->second.first;
        ab.severity = it->second.second;
        auto pit = kParamFmt.find(id);
        if (pit != kParamFmt.end() && params.size() >= 1 && params[0] != 0) {
            std::string fmt = pit->second;
            auto pos = fmt.find("{}");
            if (pos != std::string::npos)
                fmt.replace(pos, 2, std::to_string(params[0]));
            ab.tag += " (" + fmt + ")";
        }
        result.push_back(ab);
    }
    return result;
}
