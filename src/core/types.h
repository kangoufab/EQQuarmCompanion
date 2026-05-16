#pragma once
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

// ── Personnage ────────────────────────────────────────────────────────

struct CharacterInfo {
    std::string name;
    std::string class_name;
    int         level{};
};

// ── NPC ───────────────────────────────────────────────────────────────

struct NpcData {
    int  id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int  min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int  mr{}, fr{}, cr{}, dr{}, pr{};
    int  npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    std::string name, special_abilities, zone_long_name;
};

// ── Stats joueur ──────────────────────────────────────────────────────

struct StatRange { int base{}, capped{}; };

struct PlayerTotals {
    StatRange hp, mana;
    int atk{}, mitigation{};
    int str_v{}, sta{}, dex{}, agi{}, int_v{}, wis{}, cha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int haste{}, hp_regen{}, mana_regen{};
};

// ── Résultats analyse combat ──────────────────────────────────────────

struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, mitigation_pct{}, disc_mult{1.f};
    int   npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};

enum class Rating        { GOOD, MEDIUM, LOW  };
enum class OffenseRating { EASY, MEDIUM, HARD };

struct ResistRating  { int value{}; float pct{}; Rating rating{}; };
struct ResistRatings { ResistRating mr, fr, cr, dr, pr; };

struct MeleeOffense  { int player_atk{}, npc_ac{}; OffenseRating rating{}; };
struct SpellOffense  { int npc_resist{};            OffenseRating rating{}; };
struct OffenseRatings {
    MeleeOffense melee;
    std::optional<SpellOffense> mr, fr, cr, dr, pr;
};

struct SpecialAbility { std::string tag, severity; }; // severity: "red"|"orange"|"grey"

// ── Sorts ─────────────────────────────────────────────────────────────

struct SpellData {
    int id{}, resist_type{}, spell_type{}, recast_delay{}, targettype{};
    std::string name;
    std::array<int, 12> effect_base_value{};
    std::array<int, 12> effect_limit_value{};
    std::array<int, 12> effect_formula{};
    std::array<int, 12> spa{};
    std::array<int, 16> classes{};
    int cast_time{};
    int buffduration{}, buffdurationformula{};
};

// ── Items ─────────────────────────────────────────────────────────────

struct ItemData {
    int id{}, hp{}, mana{}, ac{}, atk{};
    int astr{}, asta{}, adex{}, aagi{}, aint{}, awis{}, acha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int damage{}, delay{}, itemtype{};
    int haste{}, hp_regen{}, mana_regen{};
    int slots{}, reqlevel{};
    int worneffect{}, focuseffect{}, proceffect{};
    std::string name, lore;
    std::string worneffect_name, focuseffect_name, proceffect_name;
};

struct LootItem { int item_id{}, slots{}; float chance{}; std::string name; };
