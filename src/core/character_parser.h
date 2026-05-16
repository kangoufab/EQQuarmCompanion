#pragma once
#include "core/types.h"
#include <filesystem>
#include <optional>
#include <vector>

// Parse character profile data from EQ export file
[[nodiscard]] std::optional<CharacterInfo>
parseCharacterInfo(const std::filesystem::path& path);

// Parse equipped items from EQ export file
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
parseEquippedItems(const std::filesystem::path& path);

// Find all character files matching the pattern *-Quarmy.txt
[[nodiscard]] std::vector<std::filesystem::path>
findCharacterFiles(const std::filesystem::path& directory);
