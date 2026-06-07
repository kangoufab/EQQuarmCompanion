#pragma once
#include "core/types.h"
#include <QString>
#include <map>
class QFrame;

// Unified item card (Option B — compact card with header + grouped stats).
// item             : item to display. null = shows an empty slot card.
// ref              : reference item for Δ comparison. null = no delta column.
// spells           : pre-loaded SpellData keyed by spell_id — used for effect
//                    tooltips (all effects) and the inline SORT section
//                    (click/scroll only). null = no spell details.
// titleOverride    : if non-empty, overrides the header (use for slot names).
// limitSpellNames  : optional map of spell_id → name for SE_LimitSpell (139)
//                    entries in focus/worn effect tooltips.
// highlightSlot    : if non-empty, bolds and colors (kTextPrimary) the matching
//                    entry in the slot list (e.g. the slot the item is
//                    currently equipped in).
QFrame* makeItemCard(
    const ItemData*                  item,
    const ItemData*                  ref              = nullptr,
    const std::map<int,SpellData>*   spells           = nullptr,
    const QString&                   titleOverride    = {},
    const std::map<int,QString>*     limitSpellNames  = nullptr,
    const QString&                   highlightSlot    = {}
);
