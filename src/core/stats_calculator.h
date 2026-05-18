#pragma once
#include "core/types.h"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Calcule les totaux capped du joueur à partir de son équipement.
// primaryItemtype : itemtype EQ de l'arme en main (0=1HSlash par défaut → HTH si 0)
[[nodiscard]] PlayerTotals calculateTotals(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems,
    int primaryItemtype = 0);

// Idem mais avec des sorts actifs (buffs) ajoutés avant le cap.
// spellDicts : résultats de spellToStatDict() pour chaque sort effectif.
[[nodiscard]] PlayerTotals calculateTotalsWithSpells(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems,
    const std::vector<std::map<std::string, int>>& spellDicts,
    int primaryItemtype = 0);

// Applique les effets worn (procs, focus, etc.) à un item.
void applyWornStats(ItemData& item, int level);

// Caps EQ (valeurs globales EQMacEmu, paramètres réservés pour extension future).
// hpCap   : RuleI::Character__ItemHPCap  = 2000, non dépendant de la classe.
// manaCap : RuleI::Character__ItemManaCap = 2000, non dépendant de la classe.
// attackCap : RuleI::Character__ItemATKCap = 250, non dépendant de la classe.
[[nodiscard]] int hpCap(std::string_view className, int level);
[[nodiscard]] int manaCap(std::string_view className, int level);
[[nodiscard]] int attackCap(std::string_view className, int level);
