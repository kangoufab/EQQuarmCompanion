#pragma once
#include "core/types.h"
#include <QString>
#include <map>
class QFrame;

// Unified item card (Option B — compact card with header + grouped stats).
// item             : item to display. null = shows an empty slot card.
// ref              : reference item for Δ comparison. null = no delta column.
// spells           : pre-loaded SpellData keyed by spell_id.
// titleOverride    : if non-empty, overrides the header (use for slot names).
// limitSpellNames  : optional map of spell_id → name for SE_LimitSpell (139).
// highlightSlot    : if non-empty, bolds the matching slot entry in the slot list.
// isBis            : if true, shows a ⭐ BiS badge in the card header.
QFrame* makeItemCard(
    const ItemData*                  item,
    const ItemData*                  ref              = nullptr,
    const std::map<int,SpellData>*   spells           = nullptr,
    const QString&                   titleOverride    = {},
    const std::map<int,QString>*     limitSpellNames  = nullptr,
    const QString&                   highlightSlot    = {},
    bool                             isBis            = false
);
