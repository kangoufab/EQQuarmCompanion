#include <gtest/gtest.h>
#include "core/spell_stats.h"
#include "core/types.h"

TEST(SpellStats, SpellIncomingDpsZeroForEmptyList) {
    PlayerTotals pt;
    auto dps = spellIncomingDps({}, pt, 65, 60);
    EXPECT_FLOAT_EQ(dps, 0.f);
}
