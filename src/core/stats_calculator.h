#pragma once
#include "core/types.h"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Retourne la valeur d'une stat nommée ("hp", "ac", "astr", …) depuis les totaux.
[[nodiscard]] int playerTotalStat(const std::string& stat, const PlayerTotals& t);

// Calcule les totaux capped du joueur à partir de son équipement + AAs.
// Si extra != nullptr, remplit aussi le détail par source (pour les tooltips).
[[nodiscard]] PlayerTotals calculateTotals(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems,
    int primaryItemtype = 0,
    PlayerTotalsExtra* extra = nullptr,
    const AaStats* aaStats = nullptr);

// Idem mais avec des sorts actifs (buffs) ajoutés avant le cap.
[[nodiscard]] PlayerTotals calculateTotalsWithSpells(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems,
    const std::vector<std::map<std::string, int>>& spellDicts,
    int primaryItemtype = 0,
    PlayerTotalsExtra* extra = nullptr);

// Applique les effets worn (procs, focus, etc.) à un item.
void applyWornStats(ItemData& item, int level);

// hpCap / manaCap : Quarm (EQMacEmu) n'a pas de cap items — retournent INT_MAX.
// attackCap : RuleI::Character__ItemATKCap = 250.
[[nodiscard]] int hpCap(std::string_view className, int level);
[[nodiscard]] int manaCap(std::string_view className, int level);
[[nodiscard]] int attackCap(std::string_view className, int level);
