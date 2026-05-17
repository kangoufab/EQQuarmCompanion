#include <gtest/gtest.h>
#include "core/npc_analysis.h"
#include "core/types.h"

TEST(NpcAnalysis, IncomingDamageBaseCase) {
    NpcData npc;
    npc.min_hit = 100; npc.max_hit = 200;
    npc.attack_delay = 20; npc.attack_count = 1;
    PlayerTotals pt; pt.mitigation = 0;
    auto r = incomingDamage(npc, pt, "Warrior", 65, "none");
    EXPECT_FLOAT_EQ(r.avg_hit, 150.f);
    EXPECT_GT(r.est_dps, 0.f);
}

TEST(NpcAnalysis, EvasiveDisciplineReducesHits) {
    NpcData npc; npc.min_hit = 100; npc.max_hit = 200;
    npc.attack_delay = 20; npc.attack_count = 1;
    PlayerTotals pt; pt.mitigation = 0;
    auto base = incomingDamage(npc, pt, "Warrior", 65, "none");
    auto evas = incomingDamage(npc, pt, "Warrior", 65, "evasive");
    EXPECT_LT(evas.est_dps, base.est_dps);
    EXPECT_EQ(evas.disc_note, "evasive");
}

TEST(NpcAnalysis, SlowLandPctImmuneReturnsNullopt) {
    NpcData npc;
    npc.special_abilities = "12,1";  // SA 12 = immune to slow
    auto pct = slowLandPct(npc, 65, 1, 0);
    EXPECT_FALSE(pct.has_value());
}

TEST(NpcAnalysis, SlowLandPctHighMrLowChance) {
    NpcData npc; npc.mr = 300; npc.level = 60;
    auto pct = slowLandPct(npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_LT(*pct, 20.f);
}

TEST(NpcAnalysis, DecodeSpecialAbilitiesImmuneToSlow) {
    auto abilities = decodeSpecialAbilities("11,1");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_EQ(abilities[0].tag, "Immune to Slow");
    EXPECT_EQ(abilities[0].severity, "red");
}

TEST(NpcAnalysis, DecodeSpecialAbilitiesSummon) {
    auto abilities = decodeSpecialAbilities("1,50");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_TRUE(abilities[0].tag.find("50") != std::string::npos);
    EXPECT_EQ(abilities[0].severity, "orange");
}

TEST(NpcAnalysis, ResistRatingGoodWhenHighResist) {
    NpcData npc; npc.level = 60;
    PlayerTotals pt; pt.mr = 320;
    auto r = resistRatings(npc, pt, 65);
    EXPECT_EQ(r.mr.rating, Rating::GOOD);
    EXPECT_GT(r.mr.pct, 65.f);
}
