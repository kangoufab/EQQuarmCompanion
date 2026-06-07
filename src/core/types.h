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
    int         race{};   // EQMacEmu race ID (1=Human, 2=Barbarian, ... 128=Iksar, 130=Vahshir)
    int         base_str{}, base_sta{}, base_dex{};
    int         base_agi{}, base_int{}, base_wis{}, base_cha{};
    // Equipped items from the .quarmy file: (slot_name, item_id)
    std::vector<std::pair<std::string, int>> equipped;
    // {bag_number, item_id} from General1-8 bag slots (not bank/shared bank)
    std::vector<std::pair<int,int>> bag_item_ids;
    // AAs achetés depuis la section AAIndex du fichier .quarmy: (eqmacid, rank)
    std::vector<std::pair<int,int>> aa_purchases;
};

// Statistiques issues des Alternate Advancements (AAs).
struct AaStats {
    std::map<std::string, int> stats; // stat_key → valeur totale
    // Détail par AA et par stat : stat_key → [(nom_aa, valeur), ...]
    std::map<std::string, std::vector<std::pair<std::string,int>>> sources;
    float nd_pct{};                   // % HP bonus de Natural Durability + Physical Enhancement
};

// ── NPC ───────────────────────────────────────────────────────────────

struct NpcData {
    int  id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int  min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int  mr{}, fr{}, cr{}, dr{}, pr{};
    int  npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    int  avoidance{}, slow_mitigation{};
    bool raid_target{}, is_quest{}, encounter{};
    int  zone_type{-1};
    int  bodytype{};
    int  zone_id{};
    std::string name, special_abilities, zone_long_name, race_name, class_name;
};

// ── Stats joueur ──────────────────────────────────────────────────────

struct StatRange { int base{}, capped{}; };

struct PlayerTotals {
    StatRange hp, mana;
    int atk{}, mitigation{}, ac{};
    int str_v{}, sta{}, dex{}, agi{}, int_v{}, wis{}, cha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int haste{}, hp_regen{}, mana_regen{};
};

// Détail d'une stat pour les tooltips : raw avant cap, contributions par source.
struct StatInfo {
    int raw{};          // valeur brute avant cap
    int base_val{};     // contribution base (race/classe/niveau)
    // Contributions par item : (nom_item, valeur)
    std::vector<std::pair<std::string,int>> item_sources;
    int aa_val{};       // contribution AAs (total, conservé pour compatibilité)
    // Contributions par AA individuel : (nom_aa, valeur)
    std::vector<std::pair<std::string,int>> aa_sources;
    // Contributions par sort : (nom_sort, valeur)
    std::vector<std::pair<std::string,int>> spell_sources;
    // Lignes de formule optionnelles affichées en bas du tooltip
    std::vector<std::pair<std::string,std::string>> formula;

    int items_val() const {
        int s = 0; for (auto& [n,v] : item_sources) s += v; return s;
    }
    int spells_val() const {
        int s = 0; for (auto& [n,v] : spell_sources) s += v; return s;
    }
};

struct PlayerTotalsExtra {
    std::map<std::string, StatInfo> stats;
    // Accès rapide — renvoie une StatInfo vide si la stat est inconnue
    const StatInfo& get(const std::string& key) const {
        static const StatInfo empty{};
        auto it = stats.find(key);
        return it != stats.end() ? it->second : empty;
    }
};

// ── Résultats analyse combat ──────────────────────────────────────────

struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, min_dps{}, max_dps{}, mitigation_pct{}, disc_mult{1.f};
    int   npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};

enum class Rating        { GOOD, MEDIUM, LOW  };
enum class OffenseRating { EASY, MEDIUM, HARD };

struct ResistRating  { int value{}; float pct{}; Rating rating{}; };
struct ResistRatings { ResistRating mr, fr, cr, dr, pr; };

struct MeleeOffense  { int player_atk{}, npc_ac{}, npc_avoidance{}; OffenseRating rating{}; };
struct SpellOffense  { int npc_resist{};            OffenseRating rating{}; };
struct OffenseRatings {
    MeleeOffense melee;
    std::optional<SpellOffense> mr, fr, cr, dr, pr;
};

struct SpecialAbility { std::string tag, severity; }; // severity: "red"|"orange"|"grey"

// ── Sorts ─────────────────────────────────────────────────────────────

struct SpellData {
    int id{}, resist_type{}, spell_type{}, recast_delay{}, targettype{};
    int skill{};  // instrument skill (41=Singing, 49=Stringed, 54=Wind, 12=Brass, 70=Percussion)
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
    int id{}, icon{}, hp{}, mana{}, ac{}, atk{};
    int astr{}, asta{}, adex{}, aagi{}, aint{}, awis{}, acha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int damage{}, delay{}, itemtype{};
    int haste{}, hp_regen{}, mana_regen{};
    int item_slots{}, classes{65535}, races{65535}, reqlevel{};
    int worneffect{}, focuseffect{}, proceffect{}, clickeffect{}, scrolleffect{};
    int skillmodtype{-1}, skillmodvalue{};  // C: skill modifier (skillmodtype=-1 = none)
    int nodrop{};                           // G: 1=no-drop, 0=droppable
    std::string name, lore;
    std::string worneffect_name, focuseffect_name, proceffect_name, clickeffect_name, scrolleffect_name;
    // Raw worn-effect formula data — filled by DB, consumed by applyWornStats()
    int wornlevel{};
    int atk_base{}, atk_formula{100}, atk_max{};
    int haste_base{}, haste_formula{100}, haste_max{};
    int hp_regen_base{}, hp_regen_formula{100}, hp_regen_max{};
    int mana_regen_base{}, mana_regen_formula{100}, mana_regen_max{};
};

struct LootItem {
    int item_id{}, item_slots{};
    float chance{};
    int nodrop{};
    int classes{65535}, races{65535}, reqlevel{};
    int scrolleffect{};  // > 0 for spell scrolls (itemtype 20) — id of the granted spell
    std::string name;
};

struct NpcSourceData {
    int id{}, level{};
    float drop_chance{};
    std::string name, zone_long_name;
};
