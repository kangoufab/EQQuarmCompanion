#include <gtest/gtest.h>
#include "core/character_parser.h"
#include <filesystem>
#include <fstream>
#include <map>

namespace {
    // Écrit `content` dans un fichier temporaire unique et renvoie son chemin.
    std::filesystem::path writeTemp(const char* tag, const std::string& content) {
        auto p = std::filesystem::temp_directory_path()
                 / (std::string("eq_test_parser_") + tag + ".txt");
        std::ofstream f(p, std::ios::binary);
        f << content;
        return p;
    }

    // Indexe le vecteur equipped par nom de slot pour des assertions lisibles.
    std::map<std::string, int> bySlot(const std::vector<std::pair<std::string, int>>& v) {
        std::map<std::string, int> m;
        for (const auto& [slot, id] : v) m[slot] = id;
        return m;
    }
}

// ── parseCharacterFile : en-tête / ligne personnage ─────────────────────

TEST(CharacterParser, ParsesCharacterInfo) {
    auto tmp = writeTemp("info",
        "Character\tName\tClass\tLevel\n"
        "Character\tLaraliel\t1\t65\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name,       "Laraliel");
    EXPECT_EQ(result->class_name, "Warrior");
    EXPECT_EQ(result->level,      65);
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, ParsesBaseStatsAndRace) {
    auto tmp = writeTemp("stats",
        "Character\tName\tClass\tLevel\tRace\tBaseSTR\tBaseSTA\tBaseDEX\tBaseAGI"
        "\tBaseINT\tBaseWIS\tBaseCHA\n"
        "Character\tLaraliel\t10\t65\t1\t100\t95\t90\t85\t80\t75\t70\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->class_name, "Shaman");  // class id 10
    EXPECT_EQ(result->race,     1);
    EXPECT_EQ(result->base_str, 100);
    EXPECT_EQ(result->base_sta, 95);
    EXPECT_EQ(result->base_dex, 90);
    EXPECT_EQ(result->base_agi, 85);
    EXPECT_EQ(result->base_int, 80);
    EXPECT_EQ(result->base_wis, 75);
    EXPECT_EQ(result->base_cha, 70);
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, UnknownClassIdYieldsEmptyClassName) {
    auto tmp = writeTemp("badclass",
        "Character\tName\tClass\tLevel\n"
        "Character\tNobody\t99\t10\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->class_name.empty());
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, ReturnsNulloptForMissingFile) {
    auto result = parseCharacterFile("D:/nonexistent/path/char.txt");
    EXPECT_FALSE(result.has_value());
}

// ── parseCharacterFile : section équipement + sacs ──────────────────────

TEST(CharacterParser, ParsesEquipmentSectionWithSlotExpansion) {
    auto tmp = writeTemp("equip",
        "Character\tName\tClass\tLevel\n"
        "Character\tThok\t1\t60\n"
        "Location\tName\tID\n"
        "Head\tBloodied Crown\t1234\n"
        "Ear\tEarring One\t11\n"
        "Ear\tEarring Two\t12\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    auto eq = bySlot(result->equipped);
    EXPECT_EQ(eq["Head"],       1234);
    EXPECT_EQ(eq["Left Ear"],   11);   // Ear → Left Ear (1er)
    EXPECT_EQ(eq["Right Ear"],  12);   // Ear → Right Ear (2e)
    std::filesystem::remove(tmp);
}

// Comportement documenté (CLAUDE.md) : bag_item_ids = {bag_number, item_id}
// extrait de "GeneralN-SlotM".
TEST(CharacterParser, ParsesBagItemIdsFromGeneralSlots) {
    auto tmp = writeTemp("bags",
        "Character\tName\tClass\tLevel\n"
        "Character\tThok\t1\t60\n"
        "Location\tName\tID\n"
        "General1-Slot1\tMinor Potion\t555\n"
        "General12-Slot3\tCut Gem\t777\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->bag_item_ids.size(), 2u);
    EXPECT_EQ(result->bag_item_ids[0].first,  1);    // bag number
    EXPECT_EQ(result->bag_item_ids[0].second, 555);  // item id
    EXPECT_EQ(result->bag_item_ids[1].first,  12);
    EXPECT_EQ(result->bag_item_ids[1].second, 777);
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, SkipsEmptyAndInvalidEquipmentRows) {
    auto tmp = writeTemp("skip",
        "Character\tName\tClass\tLevel\n"
        "Character\tThok\t1\t60\n"
        "Location\tName\tID\n"
        "Head\tReal Helm\t100\n"
        "Waist\tEmpty\t0\n"        // nom "Empty" → ignoré
        "Chest\t\t200\n"           // nom vide → ignoré
        "Legs\tGhost Plate\t-5\n"); // id <= 0 → ignoré
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    auto eq = bySlot(result->equipped);
    EXPECT_EQ(result->equipped.size(), 1u);
    EXPECT_EQ(eq["Head"], 100);
    EXPECT_EQ(eq.count("Waist"), 0u);
    EXPECT_EQ(eq.count("Chest"), 0u);
    EXPECT_EQ(eq.count("Legs"),  0u);
    std::filesystem::remove(tmp);
}

// ── parseCharacterFile : section AAIndex ────────────────────────────────

TEST(CharacterParser, ParsesAaPurchases) {
    auto tmp = writeTemp("aa",
        "Character\tName\tClass\tLevel\n"
        "Character\tThok\t1\t60\n"
        "AAIndex\n"
        "1001\t3\n"
        "1002\t5\n");
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->aa_purchases.size(), 2u);
    EXPECT_EQ(result->aa_purchases[0].first,  1001);
    EXPECT_EQ(result->aa_purchases[0].second, 3);
    EXPECT_EQ(result->aa_purchases[1].first,  1002);
    EXPECT_EQ(result->aa_purchases[1].second, 5);
    std::filesystem::remove(tmp);
}

// ── parseEquippedItems (fonction autonome) ──────────────────────────────

TEST(CharacterParser, ParsesEquippedItems) {
    auto tmp = writeTemp("equipped",
        "Head\tBloodied Crown\n"
        "Chest\tPlate of the Warlord\n"
        "Ear\tEarring One\n"
        "Ear\tEarring Two\n");
    auto result = parseEquippedItems(tmp);
    std::map<std::string, std::string> items;
    for (const auto& [slot, name] : result) items[slot] = name;
    EXPECT_EQ(items["Head"],      "Bloodied Crown");
    EXPECT_EQ(items["Chest"],     "Plate of the Warlord");
    EXPECT_EQ(items["Left Ear"],  "Earring One");
    EXPECT_EQ(items["Right Ear"], "Earring Two");
    std::filesystem::remove(tmp);
}

// ── findCharacterFiles ──────────────────────────────────────────────────

TEST(CharacterParser, FindCharacterFilesEmptyDirReturnsEmpty) {
    auto result = findCharacterFiles("D:/nonexistent/dir");
    EXPECT_TRUE(result.empty());
}

TEST(CharacterParser, FindCharacterFilesMatchesQuarmyPattern) {
    auto dir = std::filesystem::temp_directory_path() / "eq_test_findchars";
    std::filesystem::create_directories(dir);
    { std::ofstream(dir / "Alice-Quarmy.txt") << "x"; }
    { std::ofstream(dir / "Bob-Quarmy.txt")   << "x"; }
    { std::ofstream(dir / "notes.txt")        << "x"; }  // ne matche pas
    { std::ofstream(dir / "Quarmy.txt")       << "x"; }  // pas de "-" → ne matche pas

    auto result = findCharacterFiles(dir);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].filename().string(), "Alice-Quarmy.txt");  // trié
    EXPECT_EQ(result[1].filename().string(), "Bob-Quarmy.txt");

    std::filesystem::remove_all(dir);
}
