#pragma once
#include "core/types.h"
#include <string>
#include <vector>

[[nodiscard]] std::vector<std::string>
getActiveEffects(const std::vector<ItemData>& equippedItems, int level);
