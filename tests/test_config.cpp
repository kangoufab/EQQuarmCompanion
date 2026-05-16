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
}

TEST(Config, GetDbConfig) {
    auto defaults = writeTempDefaults();
    auto cfg = std::filesystem::temp_directory_path() / "eq_test_cfg_db.json";
    Config c(cfg, defaults);
    auto db = c.getDbConfig();
    EXPECT_EQ(db.host, "localhost");
    EXPECT_EQ(db.port, 3306);
    EXPECT_EQ(db.database, "quarm");
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
