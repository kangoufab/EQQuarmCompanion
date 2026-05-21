#pragma once
#include "core/types.h"
#include <QString>
class QFrame;

// Unified item card (Option B — compact card with header + grouped stats).
// item          : item to display. null = shows an empty slot card.
// ref           : reference item for Δ comparison. null = no delta column.
// clickSpell    : pre-loaded SpellData for scroll/click effect. null = none.
// titleOverride : if non-empty, overrides the header (use for slot names).
QFrame* makeItemCard(
    const ItemData*  item,
    const ItemData*  ref           = nullptr,
    const SpellData* clickSpell    = nullptr,
    const QString&   titleOverride = {}
);
