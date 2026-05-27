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
    // ID 12 = Immune to Slow (table mise à jour depuis emu_constants.h)
    auto abilities = decodeSpecialAbilities("12,1");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_EQ(abilities[0].tag, "Immune to Slow");
    EXPECT_EQ(abilities[0].severity, "red");
}

TEST(NpcAnalysis, DecodeSpecialAbilitiesSummon) {
    // ID 1 = Summon — le param n'est pas affiché dans le tag (kParamFmt ne couvre pas ID 1)
    auto abilities = decodeSpecialAbilities("1,1");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_EQ(abilities[0].tag, "Summon");
    EXPECT_EQ(abilities[0].severity, "orange");
}

TEST(NpcAnalysis, DefensiveDisciplineDBUnaffected) {
    // DI = (max-min)/19, DB = min-DI. Server applies SE_MeleeMitigation only on DI,
    // not DB. disc_max = DB + 10*DI = min + 9*DI. With AoW params (min=299,max=1154):
    // DI=45, DB=254, disc_max=704 → disc reduction is ~39%, not 50%.
    NpcData npc;
    npc.min_hit = 299; npc.max_hit = 1154;
    npc.attack_delay = 17; npc.attack_count = -1;
    npc.level = 70; npc.npc_class = 1;
    PlayerTotals pt; pt.mitigation = 0; pt.agi = 82;

    auto base = incomingDamage(npc, pt, "Warrior", 60, "none");
    auto def  = incomingDamage(npc, pt, "Warrior", 60, "defensive");

    EXPECT_EQ(def.disc_note, "defensive");
    EXPECT_LT(def.est_dps, base.est_dps);
    EXPECT_GT(def.disc_mult, 0.5f);  // disc_mult > 0.5 because DB is unaffected
    EXPECT_LT(def.disc_mult, 1.0f);
    // disc_max_hit = DB + 10*DI = 254 + 450 = 704; base max_hit = 1154
    // Verify via max_dps ratio ≈ 704/1154
    if (base.max_dps > 0.f)
        EXPECT_NEAR(def.max_dps / base.max_dps, 704.f / 1154.f, 0.02f);
}

TEST(NpcAnalysis, ResistRatingGoodWhenHighResist) {
    NpcData npc; npc.level = 60;
    PlayerTotals pt; pt.mr = 320;
    auto r = resistRatings(npc, pt, 65);
    EXPECT_EQ(r.mr.rating, Rating::GOOD);
    EXPECT_GT(r.mr.pct, 65.f);
}
