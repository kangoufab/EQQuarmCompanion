#include <gtest/gtest.h>
#include "core/stats_calculator.h"
#include "core/types.h"

// Warrior L65 sans items : mitigation inclut defense skill (252/3=84) + AGI/20
TEST(StatsCalculator, EmptyEquipmentMitigationHasDefenseSkill) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65; ci.base_agi = 0;
    auto totals = calculateTotals(ci, {});
    // defense skill Warrior L65 = 252, contribution = 252/3 = 84
    EXPECT_GT(totals.mitigation, 0);
    EXPECT_EQ(totals.mitigation, 84); // 0 item AC × 4/3 = 0, +84 defense, +0 agi/20
}

// Quarm n'a pas de cap item HP (pas de RuleI ItemHPCap dans le serveur)
TEST(StatsCalculator, NoItemHpCapInQuarm) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ci.base_sta = 1; // STA minimale pour avoir une base HP calculable
    ItemData item; item.hp = 10000; item.item_slots = 4;
    auto totals = calculateTotals(ci, {item});
    // HP doit inclure les 10000 HP items sans cap
    EXPECT_GT(totals.hp.capped, 10000);
}

// HP de base (base formule + items + 5 fixe)
TEST(StatsCalculator, HpBaseAccumulated) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65; ci.base_sta = 255;
    ItemData item1; item1.hp = 100; item1.item_slots = 4;
    ItemData item2; item2.hp = 50;  item2.item_slots = 2;
    auto totals = calculateTotals(ci, {item1, item2});
    // baseHp (STA 255, Warrior L65) + 150 items + 5 fixe
    int baseHp = 65 * 30 * 255 / 300 + 65 * 30; // classLevelFactor=300, lm=30
    EXPECT_EQ(totals.hp.capped, baseHp + 150 + 5);
}

TEST(StatsCalculator, HasteDoesNotStack) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item1; item1.haste = 40; item1.item_slots = 4;
    ItemData item2; item2.haste = 35; item2.item_slots = 2;
    auto totals = calculateTotals(ci, {item1, item2});
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
    ItemData item; item.haste = 150;
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.haste, 100);
}

// L'ATK affiché est (toHit + offense) × 1000 / 744 — le cap 250 s'applique à itemATK
// mais l'affiché est bien supérieur à 250
TEST(StatsCalculator, AtkItemCapApplied) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65; ci.base_str = 255;
    ItemData item; item.atk = 400;
    auto totals = calculateTotals(ci, {item});
    // L'ATK items brut (400) est cappé à 250 avant le calcul d'affichage
    // On vérifie que le résultat est inférieur à (toHit + 400 + strBonus) × 1000/744
    // En pratique le cap fonctionne car augmenter atk à 1000 ne change pas le résultat
    ItemData item2; item2.atk = 1000;
    auto totals2 = calculateTotals(ci, {item2});
    EXPECT_EQ(totals.atk, totals2.atk); // même résultat malgré différence item ATK
}

TEST(StatsCalculator, NonCasterNoManaAccumulation) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.mana = 500;
    auto totals = calculateTotals(ci, {item});
    EXPECT_EQ(totals.mana.base, 0);
    EXPECT_EQ(totals.mana.capped, 0);
}
