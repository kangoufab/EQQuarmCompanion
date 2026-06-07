#pragma once
#include "core/types.h"
#include <QString>

// HTML tooltip summarizing an item's slot/class/level and stats.
// accentColor = hex color for the item header and stat values (e.g. kTextPrimary).
QString formatItemTooltip(const ItemData& item, const QString& accentColor = "#e0e0e0");
