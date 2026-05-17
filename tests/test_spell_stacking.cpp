#include <gtest/gtest.h>
#include "core/spell_stacking.h"
#include "core/types.h"

TEST(SpellStacking, BardSongsAlwaysStackWithNonBard) {
    SpellData bard, nonBard;
    bard.classes[7] = 1;       // bard song (class 8, index 7) — != 255
    nonBard.classes[7] = 255;  // non-bard
    EXPECT_TRUE(spellsStack(bard, nonBard));
}

TEST(SpellStacking, TwoNonBardSameSpaSameSlotConflict) {
    SpellData a, b;
    // Both non-bard (all classes[7] == 255 by default)
    a.spa[0] = 1;  // SPA 1 in slot 0
    b.spa[0] = 1;  // Same SPA in same slot -> conflict
    EXPECT_FALSE(spellsStack(a, b));
}

TEST(SpellStacking, TwoNonBardDifferentSpaNoConflict) {
    SpellData a, b;
    a.spa[0] = 1;  // SPA 1 in slot 0
    b.spa[1] = 2;  // SPA 2 in slot 1 — different slot
    EXPECT_TRUE(spellsStack(a, b));
}
