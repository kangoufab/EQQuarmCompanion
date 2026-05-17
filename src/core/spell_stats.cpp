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
