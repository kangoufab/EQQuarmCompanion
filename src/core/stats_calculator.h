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

// Caps EQ par stat et par classe
[[nodiscard]] int hpCap(std::string_view className, int level);
[[nodiscard]] int manaCap(std::string_view className, int level);
[[nodiscard]] int attackCap(std::string_view className, int level);
