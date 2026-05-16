#include <gtest/gtest.h>
#include "core/config.h"
#include <filesystem>
#include <fstream>

namespace {
    const char* kDefaults = R"({
        "eq_files_dir": "",
        "current_expansion": "Classic",
        "db": {"host":"localhost","port":3306,"user":"root","password":"","database":"quarm"},
        "characters": {},
        "class_weights": {}
    })";

    std::filesystem::path writeTempDefaults() {
        auto p = std::filesystem::temp_directory_path() / "eq_test_defaults.json";
        std::ofstream f(p); f << kDefaults;
        return p;
    }
}

TEST(Config, GetReturnsDefaultValue) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_nonexistent.json";
    Config c(cfg, defaults);
    EXPECT_EQ(c.get("current_expansion"), "Classic");
    std::filesystem::remove(defaults);
}

TEST(Config, SetAndSaveRoundtrip) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_save.json";
    {
        Config c(cfg, defaults);
        c.set("current_expansion", nlohmann::json("Luclin"));
        c.save();
    }
    {
        Config c(cfg, defaults);
        EXPECT_EQ(c.get("current_expansion"), "Luclin");
    }
    std::filesystem::remove(cfg);
    std::filesystem::remove(defaults);
}

TEST(Config, GetDbConfig) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_db.json";
    Config c(cfg, defaults);
    auto db = c.getDbConfig();
    EXPECT_EQ(db.host, "localhost");
    EXPECT_EQ(db.port, 3306);
    EXPECT_EQ(db.database, "quarm");
    std::filesystem::remove(defaults);
}

TEST(Config, GetClassWeights) {
    const char* withWeights = R"({
        "eq_files_dir":"","current_expansion":"Classic",
        "db":{"host":"localhost","port":3306,"user":"root","password":"","database":"quarm"},
        "characters":{},
        "class_weights":{"Warrior":{"ac":4,"hp":3}}
    })";
    auto p = std::filesystem::temp_directory_path() / "eq_test_defs_w.json";
    { std::ofstream f(p); f << withWeights; }
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_w.json";
    Config c(cfg, p);
    auto w = c.getClassWeights("Warrior");
    EXPECT_FLOAT_EQ(w.at("ac"), 4.f);
    EXPECT_FLOAT_EQ(w.at("hp"), 3.f);
    std::filesystem::remove(p);
}

TEST(Config, CorruptConfigFallsBackToDefaults) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_corrupt.json";
    // Écrire un JSON invalide dans config.json
    { std::ofstream f(cfg); f << "{ invalid json !!!"; }
    // Le constructeur doit utiliser les defaults silencieusement (pas de throw)
    Config c(cfg, defaults);
    EXPECT_EQ(c.get("current_expansion"), "Classic");
    std::filesystem::remove(cfg);
    std::filesystem::remove(defaults);
}

// ── CharacterParser tests ─────────────────────────────────────────────
#include "core/character_parser.h"

TEST(CharacterParser, ParsesCharacterInfo) {
    auto tmp = std::filesystem::temp_directory_path() / "eq_test_char.txt";
    {
        std::ofstream f(tmp);
        f << "Character\tName\tClass\tLevel\n";
        f << "Character\tLaraliel\t1\t65\n";
    }
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name,       "Laraliel");
    EXPECT_EQ(result->class_name, "Warrior");
    EXPECT_EQ(result->level,      65);
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, ReturnsNulloptForMissingFile) {
    auto result = parseCharacterFile("D:/nonexistent/path/char.txt");
    EXPECT_FALSE(result.has_value());
}

TEST(CharacterParser, ParsesEquippedItems) {
    auto tmp = std::filesystem::temp_directory_path() / "eq_test_items.txt";
    {
        std::ofstream f(tmp);
        f << "Head\tBloodied Crown\n";
        f << "Chest\tPlate of the Warlord\n";
        f << "Ear\tEarring One\n";
        f << "Ear\tEarring Two\n";
    }
    auto result = parseEquippedItems(tmp);
    std::map<std::string, std::string> items;
    for (const auto& [slot, name] : result) {
        items[slot] = name;
    }
    EXPECT_EQ(items["Head"], "Bloodied Crown");
    EXPECT_EQ(items["Chest"], "Plate of the Warlord");
    EXPECT_EQ(items["Left Ear"], "Earring One");
    EXPECT_EQ(items["Right Ear"], "Earring Two");
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, FindCharacterFilesEmptyDirReturnsEmpty) {
    auto result = findCharacterFiles("D:/nonexistent/dir");
    EXPECT_TRUE(result.empty());
}
