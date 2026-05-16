#include <gtest/gtest.h>
#include "core/stats_calculator.h"
#include "core/types.h"

TEST(StatsCalculator, EmptyEquipmentGivesZeroMitigation) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    auto totals = calculateTotals(ci, {});
    EXPECT_EQ(totals.mitigation, 0);
}

TEST(StatsCalculator, HpCapApplied) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.hp = 10000; item.slots = 4; // chest
    auto totals = calculateTotals(ci, {item});
    // HP capped should be less than raw 10000 (cap applies)
    EXPECT_LT(totals.hp.capped, 10000);
    EXPECT_GT(totals.hp.capped, 0);
}

TEST(StatsCalculator, HpBaseAccumulated) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item1; item1.hp = 100; item1.slots = 4;
    ItemData item2; item2.hp = 50;  item2.slots = 2;
    auto totals = calculateTotals(ci, {item1, item2});
    EXPECT_EQ(totals.hp.base, 150);
}

TEST(StatsCalculator, HasteDoesNotStack) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item1; item1.haste = 40; item1.slots = 4;
    ItemData item2; item2.haste = 35; item2.slots = 2;
    auto totals = calculateTotals(ci, {item1, item2});
    // Haste prend le max, pas la somme
    EXPECT_EQ(totals.haste, 40);
}

TEST(StatsCalculator, ApplyWornStatsNoEffectWhenNoWorn) {
    ItemData item;
    item.worneffect = 0;
    applyWornStats(item, 65);
    EXPECT_EQ(item.hp, 0);
}
