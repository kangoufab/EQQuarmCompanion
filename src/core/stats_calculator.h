#pragma once
#include "core/types.h"
#include <string_view>
#include <vector>

// Calcule les totaux capped du joueur à partir de son équipement.
[[nodiscard]] PlayerTotals calculateTotals(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems);

// Applique les effets worn (procs, focus, etc.) à un item.
void applyWornStats(ItemData& item, int level);

// Caps EQ (valeurs globales EQMacEmu, paramètres réservés pour extension future).
// hpCap   : RuleI::Character__ItemHPCap  = 2000, non dépendant de la classe.
// manaCap : RuleI::Character__ItemManaCap = 2000, non dépendant de la classe.
// attackCap : RuleI::Character__ItemATKCap = 250, non dépendant de la classe.
[[nodiscard]] int hpCap(std::string_view className, int level);
[[nodiscard]] int manaCap(std::string_view className, int level);
[[nodiscard]] int attackCap(std::string_view className, int level);
