#pragma once
#include "core/types.h"
#include <vector>

// Calcule la valeur d'un effet de sort au niveau donné.
// Porte calc_spell_value() de EQMacEmu / spell_stats.py.
[[nodiscard]] int calcSpellEffectValue(int base, int max, int formula, int level);

// Retourne la valeur DPS incomingspells pour le joueur (0 si liste vide).
[[nodiscard]] float spellIncomingDps(
    const std::vector<SpellData>& npcSpells,
    const PlayerTotals& player,
    int playerLevel, int npcLevel);
