#pragma once
#include "core/types.h"
#include <map>
#include <optional>
#include <utility>
#include <vector>

// Vérifie basiquement si deux sorts peuvent coexister (utilisé pour l'analyse NPC).
// Bard songs stackent toujours avec les non-bard.
[[nodiscard]] bool spellsStack(const SpellData& a, const SpellData& b);

// Logique complète EQMacEmu (SPA 148/149 + comparaison valeur slot par slot).
// Retourne le winner_id si candidate perdrait contre un sort du pool, sinon nullopt.
[[nodiscard]] std::optional<int> findConflictInPool(
    const SpellData& candidate,
    const std::vector<SpellData>& pool,
    int level);

struct StackResult {
    std::vector<SpellData> effective;  // sorts non bloqués
    std::map<int, int>     conflicts;  // loser_id → winner_id
};

// Calcule les conflits de stacking entre tous les sorts de la liste.
[[nodiscard]] StackResult checkStacking(const std::vector<SpellData>& spells, int level);
