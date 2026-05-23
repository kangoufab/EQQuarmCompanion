#include "core/character_parser.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

// ── Slot expansion (aggregate → individual) ─────────────────────────────
static const std::map<std::string, std::vector<std::string>> SLOT_EXPANSION = {
    {"Ear",      {"Left Ear", "Right Ear"}},
    {"Wrist",    {"Left Wrist", "Right Wrist"}},
    {"Fingers",  {"Left Finger", "Right Finger"}},
};

static const std::set<std::string> EQUIPPED_LOCATIONS = {
    "Head", "Face", "Left Ear", "Right Ear", "Neck", "Shoulders", "Arms",
    "Back", "Left Wrist", "Right Wrist", "Range", "Hands", "Primary",
    "Secondary", "Left Finger", "Right Finger", "Chest", "Legs", "Feet",
    "Waist", "Ammo", "Charm",
};

static const std::map<int, std::string> EQ_CLASS_NAMES = {
    {1, "Warrior"}, {2, "Cleric"}, {3, "Paladin"}, {4, "Ranger"},
    {5, "Shadowknight"}, {6, "Druid"}, {7, "Monk"}, {8, "Bard"},
    {9, "Rogue"}, {10, "Shaman"}, {11, "Necromancer"}, {12, "Wizard"},
    {13, "Magician"}, {14, "Enchanter"}, {15, "Beastlord"}, {16, "Berserker"},
};

// ── Utility: split TSV line into parts ──────────────────────────────────
static std::vector<std::string> splitTsv(const std::string& line)
{
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '\t')) {
        parts.push_back(part);
    }
    return parts;
}

// ── Trim trailing whitespace ────────────────────────────────────────────
static std::string rtrim(std::string s)
{
    auto pos = s.find_last_not_of(" \t\r\n");
    if (pos != std::string::npos) {
        s.erase(pos + 1);
    } else {
        s.clear();
    }
    return s;
}

// ── Parse Character Info ────────────────────────────────────────────────
std::optional<CharacterInfo> parseCharacterFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return std::nullopt;

    std::optional<CharacterInfo> result;
    bool inEquipSection = false;
    std::map<std::string, int> slotCounters;
    std::vector<std::string> charHeader;
    std::string line;

    while (std::getline(file, line)) {
        line = rtrim(line);
        auto parts = splitTsv(line);
        if (parts.empty()) continue;

        // ── Character header row ──────────────────────────────────────────
        if (parts[0] == "Character" && parts.size() > 1 && parts[1] == "Name") {
            charHeader = parts;
            continue;
        }

        // ── Character data row ────────────────────────────────────────────
        if (!charHeader.empty() && !result.has_value()
            && parts[0] == "Character" && parts.size() > 1) {
            std::map<std::string, size_t> col;
            for (size_t i = 0; i < charHeader.size(); ++i)
                col[charHeader[i]] = i;

            if (col.find("Name") == col.end() || col.find("Class") == col.end()
                || col.find("Level") == col.end())
                return std::nullopt;

            try {
                CharacterInfo info;
                info.name       = parts[col["Name"]];
                info.class_name = [&]{
                    int id = std::stoi(parts[col["Class"]]);
                    auto it = EQ_CLASS_NAMES.find(id);
                    return it != EQ_CLASS_NAMES.end() ? it->second : std::string{};
                }();
                info.level = std::stoi(parts[col["Level"]]);

                auto readStat = [&](const char* key) -> int {
                    auto it = col.find(key);
                    if (it == col.end() || it->second >= parts.size()) return 0;
                    try { return std::stoi(parts[it->second]); } catch (...) { return 0; }
                };
                info.race     = readStat("Race");
                info.base_str = readStat("BaseSTR");
                info.base_sta = readStat("BaseSTA");
                info.base_dex = readStat("BaseDEX");
                info.base_agi = readStat("BaseAGI");
                info.base_int = readStat("BaseINT");
                info.base_wis = readStat("BaseWIS");
                info.base_cha = readStat("BaseCHA");
                result = std::move(info);
            } catch (...) {
                return std::nullopt;
            }
            continue;
        }

        // ── Section AAIndex ──────────────────────────────────────────────
        if (parts[0] == "AAIndex") {
            // Lire les lignes suivantes : eqmacid\trank
            std::string aaLine;
            while (std::getline(file, aaLine)) {
                aaLine = rtrim(aaLine);
                auto ap = splitTsv(aaLine);
                if (ap.size() < 2) break;
                try {
                    int eqmacid = std::stoi(ap[0]);
                    int rank    = std::stoi(ap[1]);
                    if (result.has_value())
                        result->aa_purchases.emplace_back(eqmacid, rank);
                } catch (...) { break; }
            }
            continue;
        }

        // ── Equipment section header ──────────────────────────────────────
        if (parts[0] == "Location" && parts.size() >= 3 && parts[1] == "Name" && parts[2] == "ID") {
            inEquipSection = true;
            continue;
        }

        // ── Equipment data rows ───────────────────────────────────────────
        if (inEquipSection && result.has_value() && parts.size() >= 3) {
            std::string location = parts[0];
            std::string itemName = parts[1];
            int itemId = 0;
            try { itemId = std::stoi(parts[2]); } catch (...) {}

            if (itemName.empty() || itemName == "Empty" || itemId <= 0) continue;

            auto expandIt = SLOT_EXPANSION.find(location);
            if (expandIt != SLOT_EXPANSION.end()) {
                const auto& slots = expandIt->second;
                int idx = slotCounters[location]++;
                if (idx < static_cast<int>(slots.size()))
                    result->equipped.emplace_back(slots[idx], itemId);
            } else if (EQUIPPED_LOCATIONS.count(location)) {
                result->equipped.emplace_back(location, itemId);
            } else if (location.rfind("General", 0) == 0
                       && location.find("-Slot") != std::string::npos) {
                result->bag_item_ids.push_back(itemId);
            }
        }
    }

    return result;
}

// ── Parse Equipped Items ────────────────────────────────────────────────
std::vector<std::pair<std::string, std::string>> parseEquippedItems(const std::filesystem::path& path)
{
    std::vector<std::pair<std::string, std::string>> equipped;
    std::map<std::string, int> slotCounters;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return equipped;

    std::string line;
    while (std::getline(file, line)) {
        line = rtrim(line);
        auto parts = splitTsv(line);

        if (parts.size() < 2) continue;

        std::string location = parts[0];
        std::string name = parts[1];

        // Skip empty or header names
        if (name.empty() || name == "Name" || name == "Empty") continue;

        // Handle aggregated slots (Ear, Wrist, Fingers)
        auto expandIt = SLOT_EXPANSION.find(location);
        if (expandIt != SLOT_EXPANSION.end()) {
            const auto& slots = expandIt->second;
            int idx = slotCounters[location];
            if (idx < static_cast<int>(slots.size())) {
                equipped.emplace_back(slots[idx], name);
                slotCounters[location]++;
            }
        }
        // Handle standard equipped locations
        else if (EQUIPPED_LOCATIONS.find(location) != EQUIPPED_LOCATIONS.end()) {
            equipped.emplace_back(location, name);
        }
    }

    return equipped;
}

// ── Find Character Files ────────────────────────────────────────────────
std::vector<std::filesystem::path> findCharacterFiles(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;

    if (!std::filesystem::exists(directory)) {
        return result;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().filename().string();
                // Match pattern: *-Quarmy.txt
                size_t pos = filename.rfind("-Quarmy.txt");
                if (pos != std::string::npos && pos + 11 == filename.size()) {
                    result.push_back(entry.path());
                }
            }
        }
    } catch (const std::exception&) {
        // Silently ignore errors (e.g., permission denied)
    }

    // Sort results for deterministic ordering
    std::sort(result.begin(), result.end());
    return result;
}
