#include "core/spell_stacking.h"
#include "core/spell_stats.h"
#include <algorithm>
#include <set>

// ── spellsStack (simplifié, pour analyse NPC) ──────────────────────────────

static bool isBardSong(const SpellData& sp) {
    return sp.classes[7] != 255;
}

bool spellsStack(const SpellData& a, const SpellData& b) {
    bool aBard = isBardSong(a);
    bool bBard = isBardSong(b);
    if (aBard != bBard) return true;
    for (int i = 0; i < 12; ++i) {
        if (a.spa[i] == 0 && b.spa[i] == 0) continue;
        if (a.spa[i] == b.spa[i]) return false;
    }
    return true;
}

// ── Logique complète EQMacEmu SPA 148/149 ─────────────────────────────────
// Porte buffstacking.cpp de EQMacEmu et spell_stacking.py.

static int calcSlotValue(const SpellData& spell, int slot, int level) {
    return calcSpellEffectValue(
        spell.effect_base_value[slot],
        spell.effect_limit_value[slot],
        spell.effect_formula[slot] ? spell.effect_formula[slot] : 100,
        level);
}

// Retourne true si blocker.SPA_148 empêche candidate.
static bool check148Blocks(const SpellData& blocker, const SpellData& candidate) {
    for (int i = 0; i < 12; ++i) {
        int spa = blocker.spa[i];
        if (spa == 254) break;
        if (spa != 148) continue;

        int blocked_spa = blocker.effect_base_value[i];
        int formula_raw = blocker.effect_formula[i];
        int target_slot = (formula_raw > 200) ? (formula_raw - 201) : formula_raw;
        int threshold   = std::abs(blocker.effect_limit_value[i]);

        if (target_slot < 0 || target_slot >= 12) continue;
        if (candidate.spa[target_slot] != blocked_spa) continue;

        int cand_max  = candidate.effect_limit_value[target_slot];
        int cand_base = candidate.effect_base_value[target_slot];
        int cand_val  = cand_max ? cand_max : cand_base;

        // Same-line upgrade: candidate has its own SPA 148 with same base+formula?
        for (int j = 0; j < 12; ++j) {
            if (candidate.spa[j] == 148 &&
                candidate.effect_base_value[j] == blocked_spa &&
                candidate.effect_formula[j] == formula_raw)
            {
                cand_val = std::abs(candidate.effect_limit_value[j]);
                break;
            }
        }
        if (cand_val <= threshold) return true;
    }
    return false;
}

// Retourne true si overwriter.SPA_149 retire existing.
static bool check149Overwrites(const SpellData& overwriter, const SpellData& existing, int level) {
    for (int i = 0; i < 12; ++i) {
        int spa = overwriter.spa[i];
        if (spa == 254) break;
        if (spa != 149) continue;

        int target_spa  = overwriter.effect_base_value[i];
        int formula_raw = overwriter.effect_formula[i];
        int target_slot = formula_raw - 201;
        int threshold   = overwriter.effect_limit_value[i];

        if (target_slot < 0 || target_slot >= 12) continue;
        if (existing.spa[target_slot] != target_spa) continue;

        int old_val = calcSlotValue(existing, target_slot, level);
        if (old_val <= threshold) return true;
    }
    return false;
}

// Slots explicitement gérés par SPA 148 ou 149 dans ce sort.
static std::set<int> controlledSlots(const SpellData& spell) {
    std::set<int> controlled;
    for (int i = 0; i < 12; ++i) {
        int spa = spell.spa[i];
        if (spa == 254) break;
        if (spa != 148 && spa != 149) continue;
        int formula_raw = spell.effect_formula[i];
        int target_slot = (formula_raw > 200) ? (formula_raw - 201) : formula_raw;
        if (target_slot >= 0 && target_slot < 12)
            controlled.insert(target_slot);
    }
    return controlled;
}

// Compare deux sorts. Retourne {winner_id, loser_id} si conflit, sinon nullopt.
static std::optional<std::pair<int,int>> comparePair(
    const SpellData& a, const SpellData& b, int level)
{
    bool bBlocksA = check148Blocks(b, a);
    bool aBlocksB = check148Blocks(a, b);
    if (bBlocksA && !aBlocksB) return std::make_pair(b.id, a.id);
    if (aBlocksB && !bBlocksA) return std::make_pair(a.id, b.id);

    if (check149Overwrites(a, b, level)) return std::make_pair(a.id, b.id);
    if (check149Overwrites(b, a, level)) return std::make_pair(b.id, a.id);

    auto controlled = controlledSlots(a);
    auto cb = controlledSlots(b);
    controlled.insert(cb.begin(), cb.end());

    for (int i = 0; i < 12; ++i) {
        int spaA = a.spa[i];
        int spaB = b.spa[i];
        if (spaA == 254 || spaB == 254) continue;
        if (spaA == 0  || spaB == 0)  continue; // slot non initialisé
        if (spaA == 148 || spaA == 149 || spaB == 148 || spaB == 149) continue;
        if (controlled.count(i)) continue;
        if (spaA != spaB) continue;

        int valA = calcSlotValue(a, i, level);
        int valB = calcSlotValue(b, i, level);
        if (valA == 0 && valB == 0) continue;
        if (valA >= valB) return std::make_pair(a.id, b.id);
        return std::make_pair(b.id, a.id);
    }
    return std::nullopt;
}

// ── Fonctions publiques ────────────────────────────────────────────────────

std::optional<int> findConflictInPool(
    const SpellData& candidate,
    const std::vector<SpellData>& pool,
    int level)
{
    for (const auto& existing : pool) {
        auto result = comparePair(existing, candidate, level);
        if (result) {
            auto [winner_id, loser_id] = *result;
            if (loser_id == candidate.id) return winner_id;
        }
    }
    return std::nullopt;
}

StackResult checkStacking(const std::vector<SpellData>& spells, int level) {
    std::map<int, int> losers;
    for (int i = 0; i < (int)spells.size(); ++i) {
        for (int j = i + 1; j < (int)spells.size(); ++j) {
            auto result = comparePair(spells[i], spells[j], level);
            if (result) {
                auto [winner_id, loser_id] = *result;
                if (!losers.count(loser_id))
                    losers[loser_id] = winner_id;
            }
        }
    }
    StackResult out;
    out.conflicts = losers;
    for (const auto& sp : spells)
        if (!losers.count(sp.id))
            out.effective.push_back(sp);
    return out;
}
