#pragma once
#include "core/types.h"
#include <string_view>

[[nodiscard]] float offensivePower(const PlayerTotals& pt,
                                    std::string_view className);
[[nodiscard]] float defensiveScore(const PlayerTotals& pt,
                                    std::string_view className);
