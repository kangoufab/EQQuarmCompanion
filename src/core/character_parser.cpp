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
std::optional<CharacterInfo> parseCharacterInfo(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return std::nullopt;

    std::vector<std::string> header;
    std::string line;

    while (std::getline(file, line)) {
        line = rtrim(line);
        auto parts = splitTsv(line);

        if (parts.empty()) continue;

        // Looking for header: "Character Name Class Level Race ..."
        if (parts[0] == "Character" && parts.size() > 1 && parts[1] == "Name") {
            header = parts;
            continue;
        }

        // When we have a header, look for data line starting with "Character"
        if (!header.empty() && parts[0] == "Character" && parts.size() > 1) {
            // Build column index map
            std::map<std::string, size_t> col;
            for (size_t i = 0; i < header.size(); ++i) {
                col[header[i]] = i;
            }

            // Extract required fields
            if (col.find("Name") == col.end() || col.find("Class") == col.end()
                || col.find("Level") == col.end()) {
                return std::nullopt;
            }

            try {
                size_t nameIdx = col["Name"];
                size_t classIdx = col["Class"];
                size_t levelIdx = col["Level"];

                if (nameIdx >= parts.size() || classIdx >= parts.size()
                    || levelIdx >= parts.size()) {
                    return std::nullopt;
                }

                std::string name = parts[nameIdx];
                int classId = std::stoi(parts[classIdx]);
                int level = std::stoi(parts[levelIdx]);

                auto classIt = EQ_CLASS_NAMES.find(classId);
                std::string className = classIt != EQ_CLASS_NAMES.end() ? classIt->second : "";

                CharacterInfo info;
                info.name = name;
                info.class_name = className;
                info.level = level;
                return info;
            } catch (const std::exception&) {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
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
