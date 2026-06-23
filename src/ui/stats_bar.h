#pragma once
#include "core/types.h"
#include "ui/spell_tooltip.h"
#include <QFrame>
#include <QString>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Entrée d'effet worn ou focus : (nom_effet, spell_id, nom_item_source)
// spell_id=0 → pas de tooltip SPA. nom_item_source peut être vide.
using EffectEntry = std::tuple<std::string, int, std::string>;

// Bandeau de stats joueur (tuiles par catégorie).
// extra        : détail par source pour les tooltips sur les tuiles (optionnel).
// wornEffects / focusEffects : entrées (nom, spell_id, item_source).
// spellDetails : map spell_id → SpellData pour les tooltips worn/focus.
// nameResolver : optional fallback for spell name resolution (SPA 139).
QFrame* makePlayerStatsBar(
    const PlayerTotals& totals,
    const std::string& className,
    const std::string& expansion,
    const PlayerTotalsExtra& extra                              = {},
    const std::vector<EffectEntry>& wornEffects                = {},
    const std::vector<EffectEntry>& focusEffects               = {},
    const std::map<int, SpellData>& spellDetails               = {},
    const SpellNameResolver& nameResolver                      = {});
