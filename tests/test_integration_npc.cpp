#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QVariant>
#include "core/npc_analysis.h"
#include "core/types.h"
#include "db/db_connection.h"
#include "db/npc_database.h"

// ── Helpers ───────────────────────────────────────────────────────────────

static int findNpcIdByLevel(int level) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare("SELECT id FROM npc_types WHERE level = :lv AND mindmg > 0 LIMIT 1");
    q.bindValue(":lv", level);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

static int findNpcIdWithSa12() {
    QSqlQuery q(DbConnection::instance().db());
    q.exec("SELECT id FROM npc_types WHERE special_abilities LIKE '%12,1%' LIMIT 1");
    return (q.isActive() && q.next()) ? q.value(0).toInt() : 0;
}

static int findNpcIdHighMr(int minMr) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare("SELECT id FROM npc_types WHERE MR >= :mr AND level >= 50 LIMIT 1");
    q.bindValue(":mr", minMr);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// Find NPC with MR in [minMr, maxMr] at level >= minLevel, NOT immune to slow (no SA 12)
static int findNpcIdMrRange(int minMr, int maxMr, int minLevel = 40) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT id FROM npc_types"
        " WHERE MR >= :mn AND MR <= :mx AND level >= :lv"
        "   AND (special_abilities NOT LIKE '%12,1%' OR special_abilities IS NULL)"
        " LIMIT 1"
    );
    q.bindValue(":mn", minMr);
    q.bindValue(":mx", maxMr);
    q.bindValue(":lv", minLevel);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// ── Fixture ───────────────────────────────────────────────────────────────

static NpcDatabase* s_npcDb   = nullptr;
static bool         s_dbReady = false;

class NpcIntegrationTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        DbConfig cfg;
        cfg.host     = "localhost";
        cfg.port     = 3306;
        cfg.user     = "root";
        cfg.password = "rooteq";
        cfg.database = "quarm";
        s_dbReady = DbConnection::instance().connect(cfg);
        if (s_dbReady) s_npcDb = new NpcDatabase();
    }
    static void TearDownTestSuite() {
        delete s_npcDb;
        s_npcDb = nullptr;
    }

protected:
    void SetUp() override {
        if (!s_dbReady)
            GTEST_SKIP() << "Quarm DB non accessible (localhost:3306 quarm)";
    }
};

// ── Groupe A : slowLandPct ────────────────────────────────────────────────

TEST_F(NpcIntegrationTest, A1_SlowLandPct_ResultInRange) {
    int id = findNpcIdByLevel(50);
    if (!id) GTEST_SKIP() << "Pas de NPC lv50 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mr = 100;
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_GE(*pct, 0.f);
    EXPECT_LE(*pct, 100.f);
}

TEST_F(NpcIntegrationTest, A2_SlowLandPct_ImmuneNpcReturnsNullopt) {
    int id = findNpcIdWithSa12();
    if (!id) GTEST_SKIP() << "Pas de NPC avec SA 12 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    auto pct = slowLandPct(*npc, 65, 1, 0);
    EXPECT_FALSE(pct.has_value()) << "NPC immune au slow doit retourner nullopt";
}

TEST_F(NpcIntegrationTest, A3_SlowLandPct_HighMrLowChance) {
    int id = findNpcIdHighMr(200);
    if (!id) GTEST_SKIP() << "Pas de NPC MR>=200 lv50+ dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mr = 0;
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_LE(*pct, 30.f) << "Mob MR>=200 difficile a slow sans debuff";
}

TEST_F(NpcIntegrationTest, A4_SlowLandPct_DebuffIncreasesChance) {
    // Need MR in range where resistChance = MR*3/2 < 198 (i.e. MR < 132) before debuff,
    // so that applying a -60 debuff actually lowers resistChance.
    // Use same-level call to eliminate the level modifier.
    int id = findNpcIdMrRange(80, 130, 40);
    if (!id) GTEST_SKIP() << "Pas de NPC MR 80-130 lv40+ dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    float pctNoDebuff   = slowLandPct(*npc, npc->level, 1, 0).value_or(0.f);
    float pctWithDebuff = slowLandPct(*npc, npc->level, 1, -60).value_or(0.f);
    EXPECT_GT(pctWithDebuff, pctNoDebuff)
        << "Debuff MR -60 doit augmenter la chance de slow (NPC MR 80-130)";
}

TEST_F(NpcIntegrationTest, A5_SlowLandPct_LowLevelEasyToSlow) {
    int id = findNpcIdByLevel(10);
    if (!id) GTEST_SKIP() << "Pas de NPC lv10 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_GE(*pct, 50.f) << "NPC lv10 avec joueur lv65 doit etre easy to slow";
}

// ── Groupe B : incomingDamage ─────────────────────────────────────────────

TEST_F(NpcIntegrationTest, B1_IncomingDamage_MinLeEstLeMax) {
    int id = findNpcIdByLevel(55);
    if (!id) GTEST_SKIP() << "Pas de NPC lv55 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500; pt.ac = 800;
    auto r = incomingDamage(*npc, pt, "Warrior", 65, "none");
    EXPECT_LE(r.min_dps, r.est_dps + 0.01f) << "min_dps doit etre <= est_dps";
    EXPECT_GE(r.max_dps, r.est_dps - 0.01f) << "max_dps doit etre >= est_dps";
    EXPECT_GT(r.est_dps, 0.f);
}

TEST_F(NpcIntegrationTest, B2_IncomingDamage_DefensiveReducesDps) {
    int id = findNpcIdByLevel(60);
    if (!id) GTEST_SKIP() << "Pas de NPC lv60 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500;
    auto base = incomingDamage(*npc, pt, "Warrior", 65, "none");
    auto def  = incomingDamage(*npc, pt, "Warrior", 65, "defensive");
    EXPECT_LT(def.est_dps, base.est_dps) << "Defensive disc doit reduire est_dps";
}

TEST_F(NpcIntegrationTest, B3_IncomingDamage_EvasiveReducesDps) {
    int id = findNpcIdByLevel(60);
    if (!id) GTEST_SKIP() << "Pas de NPC lv60 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500;
    auto base = incomingDamage(*npc, pt, "Warrior", 65, "none");
    auto evas = incomingDamage(*npc, pt, "Warrior", 65, "evasive");
    EXPECT_LT(evas.est_dps, base.est_dps) << "Evasive disc doit reduire est_dps";
}

TEST_F(NpcIntegrationTest, B4_IncomingDamage_HighMitIncreasesMitPct) {
    int id = findNpcIdByLevel(55);
    if (!id) GTEST_SKIP() << "Pas de NPC lv55 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals lowMit;  lowMit.mitigation  = 0;
    PlayerTotals highMit; highMit.mitigation = 1200;
    auto rLow  = incomingDamage(*npc, lowMit,  "Warrior", 65, "none");
    auto rHigh = incomingDamage(*npc, highMit, "Warrior", 65, "none");
    // mitigation_pct = (1 - effHit/avgHit)*100 — higher player mitigation → higher mitigation_pct
    EXPECT_GE(rHigh.mitigation_pct, rLow.mitigation_pct - 0.01f)
        << "mitigation_pct doit augmenter (ou rester egal) quand player_mit augmente";
    EXPECT_LT(rHigh.est_dps, rLow.est_dps + 0.01f)
        << "est_dps doit diminuer quand player_mit augmente";
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
