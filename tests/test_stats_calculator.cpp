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
    ItemData item; item.hp = 10000; item.item_slots = 4; // chest
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.hp.capped, 2000);  // RuleI ItemHPCap = 2000
    EXPECT_EQ(totals.hp.base, 10000);   // base non cappé
}

TEST(StatsCalculator, HpBaseAccumulated) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item1; item1.hp = 100; item1.item_slots = 4;
    ItemData item2; item2.hp = 50;  item2.item_slots = 2;
    auto totals = calculateTotals(ci, {item1, item2});
    EXPECT_EQ(totals.hp.base, 150);
}

TEST(StatsCalculator, HasteDoesNotStack) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item1; item1.haste = 40; item1.item_slots = 4;
    ItemData item2; item2.haste = 35; item2.item_slots = 2;
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

TEST(StatsCalculator, HasteCappedAt100ForLevel65) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.haste = 150;  // dépasse le cap de 100%
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.haste, 100);  // cap = 100% à level >= 60
}

TEST(StatsCalculator, AtkCappedAt250) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.atk = 400;
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.atk, 250);  // ItemATKCap = 250
}

TEST(StatsCalculator, NonCasterNoManaAccumulation) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.mana = 500;
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.mana.base, 0);   // Warriors n'ont pas de mana
    EXPECT_EQ(totals.mana.capped, 0);
}
