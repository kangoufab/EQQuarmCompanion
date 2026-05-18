#include "core/spell_stats.h"
#include <algorithm>
#include <cmath>

// Porte calc_spell_value() de spell_stats.py / EQMacEmu CalcSpellEffectValue_formula.
// Formules utilisées par les stat buffs.
int calcSpellEffectValue(int base, int max, int formula, int level) {
    int result;
    if (formula == 0 || formula == 100) {
        result = base;
    } else if (formula >= 1 && formula <= 99) {
        result = base + level * formula;
    } else if (formula == 101) {
        result = base + level / 2;
    } else if (formula == 102) {
        result = base + level;
    } else if (formula == 103) {
        result = base + level * 2;
    } else if (formula == 104) {
        result = base + level * 3;
    } else if (formula == 105) {
        result = base + level * 4;
    } else if (formula == 109) {
        result = base + level / 4;
    } else if (formula == 110) {
        result = base + level / 6;
    } else if (formula == 119) {
        result = base + level / 8;
    } else if (formula == 121) {
        result = base + level / 3;
    } else {
        result = base;
    }

    if (max != 0) {
        if (base >= 0) {
            result = std::min(result, max);
        } else {
            result = std::max(result, max);
        }
    }
    return result;
}

// Valeur d'un sort (dégâts, heal, etc.) pour un niveau donné.
// Retourne la valeur du premier slot d'effet non nul.
float spellValue(const SpellData& sp, int level) {
    for (int i = 0; i < 12; ++i) {
        if (sp.spa[i] != 0 || sp.effect_base_value[i] != 0) {
            return static_cast<float>(calcSpellEffectValue(
                sp.effect_base_value[i],
                sp.effect_limit_value[i],
                sp.effect_formula[i],
                level));
        }
    }
    return 0.f;
}

// Retourne le DPS moyen des sorts NPC entrants contre le joueur.
// Version stub : calcule la somme des dégâts par sort divisée par le cast time.
// Retourne 0 si la liste est vide.
float spellIncomingDps(
    const std::vector<SpellData>& npcSpells,
    const PlayerTotals& /*player*/,
    int /*playerLevel*/, int npcLevel)
{
    if (npcSpells.empty()) return 0.f;

    float totalDps = 0.f;
    for (const auto& spell : npcSpells) {
        float spellDamage = 0.f;
        for (int i = 0; i < 12; ++i) {
            // SPA 0 = SE_CurrentHP (dégâts directs si valeur négative)
            if (spell.spa[i] == 0 && spell.effect_base_value[i] < 0) {
                int val = calcSpellEffectValue(
                    spell.effect_base_value[i],
                    spell.effect_limit_value[i],
                    spell.effect_formula[i],
                    npcLevel);
                spellDamage += static_cast<float>(-val);
            }
        }
        // cast_time en millisecondes ; éviter division par zéro
        float castTimeSec = (spell.cast_time > 0)
            ? static_cast<float>(spell.cast_time) / 1000.f
            : 6.f; // défaut 6s si non renseigné
        // recast_delay en millisecondes
        float recastSec = (spell.recast_delay > 0)
            ? static_cast<float>(spell.recast_delay) / 1000.f
            : 0.f;
        float cycleSec = castTimeSec + recastSec;
        if (cycleSec > 0.f && spellDamage > 0.f) {
            totalDps += spellDamage / cycleSec;
        }
    }
    return totalDps;
}

// ── spellToStatDict ────────────────────────────────────────────────────────
// Porte spell_to_stat_dict() de spell_stats.py.
// SPA 254 = SE_Blank (fin des effets). SPA 0 = SE_CurrentHP (hp_regen bard).

std::map<std::string, int> spellToStatDict(const SpellData& spell, int casterLevel) {
    std::map<std::string, int> result;

    static const std::pair<int, const char*> SPA_MAP[] = {
        {0,  "hp_regen"}, {1,  "ac"},   {2,  "atk"},
        {4,  "astr"},     {5,  "adex"}, {6,  "aagi"}, {7,  "asta"},
        {8,  "aint"},     {9,  "awis"}, {10, "acha"},
        {15, "mana_regen"},
        {46, "fr"},  {47, "cr"},  {48, "pr"},  {49, "dr"},  {50, "mr"},
        {69, "hp"},  {97, "mana"}, {100, "hp_regen"},
    };

    for (int i = 0; i < 12; ++i) {
        int spa = spell.spa[i];
        if (spa == 254) break;    // SE_Blank — pas d'effets suivants
        int base = spell.effect_base_value[i];
        if (base == 0) continue;
        int formula = spell.effect_formula[i] ? spell.effect_formula[i] : 100;
        int mx      = spell.effect_limit_value[i];
        int value   = calcSpellEffectValue(base, mx, formula, casterLevel);

        if (spa == 11) {
            result["haste"]    += (value - 100);
        } else if (spa == 98) {
            result["haste_v2"] += (value - 100);
        } else if (spa == 119) {
            result["haste_v3"] += value;
        } else if (spa == 111) {
            for (const char* r : {"mr", "fr", "cr", "dr", "pr"})
                result[r] += value;
        } else if (spa == 159) {
            for (const char* a : {"astr", "asta", "aagi", "adex", "awis", "aint", "acha"})
                result[a] += value;
        } else {
            for (auto& [s, k] : SPA_MAP)
                if (s == spa) { result[k] += value; break; }
        }
    }
    return result;
}
