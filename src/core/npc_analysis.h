#pragma once
#include "core/types.h"
#include <optional>
#include <string>
#include <vector>

// discipline : "none" | "evasive" | "defensive"
[[nodiscard]] IncomingDamageResult
incomingDamage(const NpcData& npc, const PlayerTotals& player,
               std::string_view className, int level,
               std::string_view discipline);

[[nodiscard]] ResistRatings
resistRatings(const NpcData& npc, const PlayerTotals& player, int level);

[[nodiscard]] OffenseRatings
offenseRatings(const NpcData& npc, const PlayerTotals& player,
               std::string_view className);

// Retourne nullopt si le NPC est immune (SA 12). resistType : 1=MR 5=DR
[[nodiscard]] std::optional<float>
slowLandPct(const NpcData& npc, int playerLevel,
            int resistType, int resistDiff = 0);

[[nodiscard]] std::optional<float>
spellResistPct(const SpellData& spell, const PlayerTotals& player,
               int playerLevel, int npcLevel);

[[nodiscard]] std::string resistLabel(const SpellData& spell);
[[nodiscard]] std::string effectiveSpellType(const SpellData& spell);
[[nodiscard]] std::string targetTypeLabel(int targettype);
[[nodiscard]] std::string formatSpellSummary(const SpellData& spell, int npcLevel);

[[nodiscard]] std::vector<SpecialAbility>
decodeSpecialAbilities(std::string_view raw);
