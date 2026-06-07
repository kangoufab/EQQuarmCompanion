#pragma once
#include "core/types.h"
#include "ui/palette.h"
#include <QString>

// HTML tooltip summarizing an item's slot/class/level and stats.
// accentColor = hex color for the item header and stat values (e.g. kTextPrimary).
QString formatItemTooltip(const ItemData& item, const QString& accentColor = kTextPrimary);
