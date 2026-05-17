# EqQuarmCompanion — Plan 2 : Modules Core

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prérequis :** Plan 1 terminé — projet compile, Config testé, fenêtre vide lancée.

**Goal:** Implémenter tous les modules `src/core/` avec leurs tests GTest. Zéro `#include <Qt*>` dans ces fichiers.

**Architecture:** Chaque module `.cpp` porte la logique de son équivalent Python dans `D:\Games\quarm\source\eq-item-evaluator\core\`. Les signatures C++ sont définies dans ce plan ; la logique se traduit mécaniquement depuis Python.

**Tech Stack:** C++17, std::, nlohmann/json, googletest.

---

### Task 1 : `character_parser`

**Python source :** `eq-item-evaluator/core/character_parser.py` (124 lignes)

**Files:**
- Create: `src/core/character_parser.h`
- Modify: `src/core/character_parser.cpp`
- Modify: `tests/test_config.cpp` → ne pas toucher ; les tests character vivent dans `test_config.cpp` pour l'instant (ajouter dans un `TEST(CharacterParser, ...)` bloc)

- [ ] **Step 1 : Écrire `src/core/character_parser.h`**

```cpp
#pragma once
#include "core/types.h"
#include <filesystem>
#include <optional>
#include <vector>

// Parse un fichier de personnage EQ exporté (format texte EQMacEmu).
// Retourne nullopt si le fichier n'existe pas ou n'est pas parsable.
[[nodiscard]] std::optional<CharacterInfo>
parseCharacterFile(const std::filesystem::path& path);

// Retourne la liste de tous les fichiers .txt dans eq_files_dir
// qui ressemblent à des fichiers perso EQ (contiennent "Character Profile").
[[nodiscard]] std::vector<std::filesystem::path>
findCharacterFiles(const std::filesystem::path& eq_files_dir);
```

- [ ] **Step 2 : Écrire `src/core/character_parser.cpp`**

Porter depuis `character_parser.py`. La logique : ouvrir le fichier texte, chercher les lignes "Name:", "Class:", "Level:", extraire les valeurs. Exemple des lignes clés à parser :

```cpp
#include "core/character_parser.h"
#include <fstream>
#include <regex>
#include <sstream>

std::optional<CharacterInfo> parseCharacterFile(
    const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return std::nullopt;

    CharacterInfo info;
    bool found = false;
    std::string line;
    // Regex identiques au parser Python : chercher "Name: Laraliel" etc.
    static const std::regex reName  (R"(^Name:\s+(\S+))");
    static const std::regex reClass (R"(^Class:\s+(.+))");
    static const std::regex reLevel (R"(^Level:\s+(\d+))");
    std::smatch m;

    while (std::getline(f, line)) {
        if (std::regex_search(line, m, reName))  { info.name       = m[1]; found = true; }
        if (std::regex_search(line, m, reClass)) { info.class_name = m[1]; }
        if (std::regex_search(line, m, reLevel)) { info.level      = std::stoi(m[1]); }
    }
    return found ? std::make_optional(info) : std::nullopt;
}

std::vector<std::filesystem::path> findCharacterFiles(
    const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(dir)) return result;
    for (auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() == ".txt") {
            std::ifstream f(e.path());
            std::string line;
            while (std::getline(f, line))
                if (line.find("Character Profile") != std::string::npos) {
                    result.push_back(e.path()); break;
                }
        }
    }
    return result;
}
```

> **Note :** Les regex exactes dépendent du format du fichier produit par EQMacEmu. Consulter `character_parser.py` pour les patterns précis et ajuster les `std::regex` en conséquence.

- [ ] **Step 3 : Écrire le test dans `tests/test_config.cpp`** (ajouter à la fin)

```cpp
#include "core/character_parser.h"
#include <filesystem>
#include <fstream>

TEST(CharacterParser, ParsesNameClassLevel) {
    auto tmp = std::filesystem::temp_directory_path() / "eq_test_char.txt";
    {
        std::ofstream f(tmp);
        f << "Character Profile\nName: Laraliel\nClass: Warrior\nLevel: 65\n";
    }
    auto result = parseCharacterFile(tmp);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name,       "Laraliel");
    EXPECT_EQ(result->class_name, "Warrior");
    EXPECT_EQ(result->level,      65);
    std::filesystem::remove(tmp);
}

TEST(CharacterParser, ReturnsNulloptForMissingFile) {
    auto result = parseCharacterFile("/nonexistent/path/char.txt");
    EXPECT_FALSE(result.has_value());
}
```

- [ ] **Step 4 : Compiler et vérifier les tests**

```powershell
cmake --build build/debug --target eq_tests
cd build/debug && ctest -C Debug --output-on-failure
```

Attendu : tous les tests passent (Config + CharacterParser).

- [ ] **Step 5 : Commit**

```powershell
git add src/core/character_parser.h src/core/character_parser.cpp tests/test_config.cpp
git commit -m "feat(core): character_parser — parse fichiers EQ"
```

---

### Task 2 : `stats_calculator`

**Python source :** `eq-item-evaluator/core/stats_calculator.py` (593 lignes) — porter la totalité.

**Files:**
- Create: `src/core/stats_calculator.h`
- Modify: `src/core/stats_calculator.cpp`
- Modify: `tests/test_stats_calculator.cpp`

- [ ] **Step 1 : Écrire le test d'abord**

```cpp
#include <gtest/gtest.h>
#include "core/stats_calculator.h"
#include "core/types.h"

TEST(StatsCalculator, EmptyEquipmentGivesZeroMitigation) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    auto totals = calculateTotals(ci, {});
    // Sans équipement, mitigation = 0
    EXPECT_EQ(totals.mitigation, 0);
}

TEST(StatsCalculator, HpCapApplied) {
    CharacterInfo ci; ci.class_name = "Warrior"; ci.level = 65;
    ItemData item; item.hp = 100; item.slots = 4; // chest
    auto totals = calculateTotals(ci, {item});
    EXPECT_GT(totals.hp.capped, 0);
}

TEST(StatsCalculator, ApplyWornStatsUpdatesItem) {
    ItemData item;
    item.worneffect = 0;   // no worn effect
    applyWornStats(item, 65);
    // Sans worn effect, l'item reste inchangé
    EXPECT_EQ(item.hp, 0);
}
```

- [ ] **Step 2 : Vérifier que le test échoue**

```powershell
cmake --build build/debug --target eq_tests 2>&1 | Select-String "error"
```

Attendu : erreur `calculateTotals` not declared.

- [ ] **Step 3 : Écrire `src/core/stats_calculator.h`**

```cpp
#pragma once
#include "core/types.h"
#include <vector>

// Calcule les totaux capped du joueur à partir de son équipement.
// Porter depuis stats_calculator.py::calculate_totals().
[[nodiscard]] PlayerTotals calculateTotals(
    const CharacterInfo& charInfo,
    const std::vector<ItemData>& equippedItems);

// Applique les effets worn (procs, focus, etc.) à un item.
// Porter depuis item_database.py::apply_worn_stats().
void applyWornStats(ItemData& item, int level);

// Caps EQ par stat et par classe (portés depuis stat_caps.py)
[[nodiscard]] int hpCap(std::string_view className, int level);
[[nodiscard]] int manaCap(std::string_view className, int level);
[[nodiscard]] int attackCap(std::string_view className, int level);
```

- [ ] **Step 4 : Écrire `src/core/stats_calculator.cpp`**

Porter mécaniquement depuis `stats_calculator.py` et `stat_caps.py`. La logique principale :

```cpp
#include "core/stats_calculator.h"
#include <algorithm>
#include <unordered_map>

// Caps HP par classe (valeurs depuis stat_caps.py)
static const std::unordered_map<std::string, int> kHpCapBase = {
    {"Warrior",65*21+200}, {"Cleric",65*15+200}, {"Paladin",65*19+200},
    // ... (porter les valeurs exactes depuis stat_caps.py)
};

int hpCap(std::string_view cls, int level) {
    auto it = kHpCapBase.find(std::string(cls));
    if (it == kHpCapBase.end()) return 9999;
    return it->second; // simplification — porter la formule exacte
}

PlayerTotals calculateTotals(const CharacterInfo& ci,
                              const std::vector<ItemData>& items)
{
    PlayerTotals t;
    for (const auto& item : items) {
        t.hp.base   += item.hp;
        t.mana.base += item.mana;
        t.atk       += item.atk;
        t.str_v     += item.astr;
        t.sta       += item.asta;
        t.dex       += item.adex;
        t.agi       += item.aagi;
        t.int_v     += item.aint;
        t.wis       += item.awis;
        t.cha       += item.acha;
        t.mr        += item.mr;
        t.fr        += item.fr;
        t.cr        += item.cr;
        t.dr        += item.dr;
        t.pr        += item.pr;
        t.haste     = std::max(t.haste, item.haste); // haste ne se cumule pas
        t.hp_regen  += item.hp_regen;
        t.mana_regen += item.mana_regen;
    }
    // Appliquer les caps — porter les formules depuis stats_calculator.py
    t.hp.capped   = std::min(t.hp.base,   hpCap(ci.class_name, ci.level));
    t.mana.capped = std::min(t.mana.base, manaCap(ci.class_name, ci.level));
    t.atk         = std::min(t.atk,       attackCap(ci.class_name, ci.level));
    // mitigation = AC totale (porter depuis stats_calculator.py::_mitigation())
    t.mitigation  = 0; // TODO: porter depuis Python
    return t;
}
```

> **Important :** Porter **toutes** les formules de caps et de mitigation depuis `stats_calculator.py`. Les valeurs numériques (bases AC par classe, formules de mitigation) sont dans ce fichier Python.

- [ ] **Step 5 : Compiler et vérifier les tests**

```powershell
cmake --build build/debug --target eq_tests
ctest -C Debug --output-on-failure
```

- [ ] **Step 6 : Commit**

```powershell
git add src/core/stats_calculator.h src/core/stats_calculator.cpp tests/test_stats_calculator.cpp
git commit -m "feat(core): stats_calculator — calculateTotals, caps, applyWornStats"
```

---

### Task 3 : `npc_analysis`

**Python source :** `eq-item-evaluator/core/npc_analysis.py` (618 lignes) — le module le plus important.

**Files:**
- Create: `src/core/npc_analysis.h`
- Modify: `src/core/npc_analysis.cpp`
- Modify: `tests/test_npc_analysis.cpp`

- [ ] **Step 1 : Écrire les tests d'abord**

```cpp
#include <gtest/gtest.h>
#include "core/npc_analysis.h"
#include "core/types.h"

// ── incomingDamage ────────────────────────────────────────────────────

TEST(NpcAnalysis, IncomingDamageBaseCase) {
    NpcData npc;
    npc.min_hit = 100; npc.max_hit = 200;
    npc.attack_delay = 20; npc.attack_count = 1;  // 2s delay
    PlayerTotals pt; pt.mitigation = 0;
    auto r = incomingDamage(npc, pt, "Warrior", 65, "none");
    EXPECT_FLOAT_EQ(r.avg_hit, 150.f);
    EXPECT_GT(r.est_dps, 0.f);
}

TEST(NpcAnalysis, EvasiveDisciplineReducesHits) {
    NpcData npc; npc.min_hit = 100; npc.max_hit = 200;
    npc.attack_delay = 20; npc.attack_count = 1;
    PlayerTotals pt; pt.mitigation = 0;
    auto base = incomingDamage(npc, pt, "Warrior", 65, "none");
    auto evas = incomingDamage(npc, pt, "Warrior", 65, "evasive");
    EXPECT_LT(evas.est_dps, base.est_dps);
    EXPECT_EQ(evas.disc_note, "evasive");
}

// ── slowLandPct ──────────────────────────────────────────────────────

TEST(NpcAnalysis, SlowLandPctImmuneReturnsNullopt) {
    NpcData npc;
    // SA 12 = immune to slow
    npc.special_abilities = "12,1";
    auto pct = slowLandPct(npc, 65, 1 /*MR*/, 0);
    EXPECT_FALSE(pct.has_value());
}

TEST(NpcAnalysis, SlowLandPctHighMrLowChance) {
    NpcData npc; npc.mr = 300; npc.level = 60;
    auto pct = slowLandPct(npc, 65, 1 /*MR*/, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_LT(*pct, 20.f);  // très haute MR → faible chance
}

// ── decodeSpecialAbilities ───────────────────────────────────────────

TEST(NpcAnalysis, DecodeSpecialAbilitiesImmuneToSlow) {
    auto abilities = decodeSpecialAbilities("11,1");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_EQ(abilities[0].tag, "Immune to Slow");
    EXPECT_EQ(abilities[0].severity, "red");
}

TEST(NpcAnalysis, DecodeSpecialAbilitiesSummon) {
    auto abilities = decodeSpecialAbilities("1,50");
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_TRUE(abilities[0].tag.find("50") != std::string::npos);
    EXPECT_EQ(abilities[0].severity, "orange");
}

// ── resistRatings ────────────────────────────────────────────────────

TEST(NpcAnalysis, ResistRatingGoodWhenHighResist) {
    NpcData npc; npc.level = 60;
    PlayerTotals pt; pt.mr = 320;
    auto r = resistRatings(npc, pt, 65);
    EXPECT_EQ(r.mr.rating, Rating::GOOD);
    EXPECT_GT(r.mr.pct, 65.f);
}
```

- [ ] **Step 2 : Vérifier que les tests échouent**

```powershell
cmake --build build/debug --target eq_tests 2>&1 | Select-String "error" | Select-Object -First 5
```

- [ ] **Step 3 : Écrire `src/core/npc_analysis.h`**

```cpp
#pragma once
#include "core/types.h"
#include <optional>
#include <string>
#include <vector>

// ── Dégâts reçus ──────────────────────────────────────────────────────
// discipline : "none" | "evasive" | "defensive"
[[nodiscard]] IncomingDamageResult
incomingDamage(const NpcData& npc, const PlayerTotals& player,
               std::string_view className, int level,
               std::string_view discipline);

// ── Résistances joueur ────────────────────────────────────────────────
[[nodiscard]] ResistRatings
resistRatings(const NpcData& npc, const PlayerTotals& player, int level);

// ── Offense joueur ────────────────────────────────────────────────────
[[nodiscard]] OffenseRatings
offenseRatings(const NpcData& npc, const PlayerTotals& player,
               std::string_view className);

// ── Slow / résistance ─────────────────────────────────────────────────
// Retourne nullopt si le NPC est immune (SA 12).
// resistType : 1=MR 5=DR
[[nodiscard]] std::optional<float>
slowLandPct(const NpcData& npc, int playerLevel,
            int resistType, int resistDiff = 0);

[[nodiscard]] std::optional<float>
spellResistPct(const SpellData& spell, const PlayerTotals& player,
               int playerLevel, int npcLevel);

// ── Spells helpers ────────────────────────────────────────────────────
[[nodiscard]] std::string resistLabel(const SpellData& spell);
[[nodiscard]] std::string effectiveSpellType(const SpellData& spell);
[[nodiscard]] std::string targetTypeLabel(int targettype);
[[nodiscard]] std::string formatSpellSummary(const SpellData& spell, int npcLevel);

// ── Special abilities ─────────────────────────────────────────────────
[[nodiscard]] std::vector<SpecialAbility>
decodeSpecialAbilities(std::string_view raw);
```

- [ ] **Step 4 : Écrire `src/core/npc_analysis.cpp` — fonctions clés**

```cpp
#include "core/npc_analysis.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

// ── Helpers internes ──────────────────────────────────────────────────

static float npcDoubleAttackChance(int level) {
    if (level <= 7) return 0.f;
    int daSkill   = level > 50 ? 250 : std::min(level * 5, 210);
    int effective = daSkill + (level > 35 ? level : 0);
    return std::min(1.f, effective / 500.f);
}

// Parse "1,50^3,10^8,1" → map<ability_id, vector<params>>
static std::unordered_map<int, std::vector<int>>
parseSa(std::string_view raw) {
    std::unordered_map<int, std::vector<int>> result;
    if (raw.empty()) return result;
    std::string s(raw);
    std::replace(s.begin(), s.end(), '^', '\n');
    std::istringstream ss(s);
    std::string entry;
    while (std::getline(ss, entry)) {
        if (entry.empty()) continue;
        std::replace(entry.begin(), entry.end(), ',', ' ');
        std::istringstream es(entry);
        int id; es >> id;
        std::vector<int> params;
        int p; while (es >> p) params.push_back(p);
        result[id] = params;
    }
    return result;
}

static float npcAttacksPerRound(const NpcData& npc) {
    auto sa = parseSa(npc.special_abilities);
    float base  = static_cast<float>(std::max(1, npc.attack_count));
    float da    = npcDoubleAttackChance(npc.level);
    // Triple attack : Warriors/Monks (class 1/32) lv60+
    float triple = (npc.level >= 60 && (npc.npc_class == 1 || npc.npc_class == 32))
                   ? 0.135f : 0.f;
    bool  dw     = sa.count(7) > 0;
    float flurry = sa.count(5) > 0
                   ? static_cast<float>(sa.at(5).empty() ? 20 : sa.at(5)[0]) / 100.f
                   : 0.20f; // défaut 20% si SA5 présent sans param
    // Présence de SA5 (flurry) → actif seulement si SA5 dans la liste
    bool hasFlurry = sa.count(5) > 0;

    float attacks = base * (1.f + da + triple) * (dw ? 2.f : 1.f);
    if (hasFlurry) attacks += flurry * base;
    return attacks;
}

static float avoidanceHitChance(int npcThaco, int baseAv, float discMult) {
    // Porter depuis npc_analysis.py::_avoidance_hit_chance()
    // Formule EQMacEmu : chance = 1 - avoidance / (avoidance + attaque)
    float av = static_cast<float>(baseAv) * discMult;
    float total = av + static_cast<float>(std::max(1, npcThaco));
    return total > 0.f ? static_cast<float>(std::max(1, npcThaco)) / total : 1.f;
}

// ── incomingDamage ────────────────────────────────────────────────────

IncomingDamageResult incomingDamage(const NpcData& npc,
                                     const PlayerTotals& player,
                                     std::string_view className,
                                     int level,
                                     std::string_view discipline)
{
    IncomingDamageResult r;
    r.avg_hit = (npc.min_hit + npc.max_hit) / 2.f;

    float delaySec  = std::max(0.1f, npc.attack_delay / 10.f);
    float attacks   = npcAttacksPerRound(npc);

    // Discipline multipliers — porter les valeurs exactes depuis npc_analysis.py
    float discMult = 1.f;
    if (discipline == "evasive"   && className == "Warrior" && level >= 52)
        { discMult = 0.7f; r.disc_note = "evasive"; }
    else if (discipline == "defensive" && className == "Warrior" && level >= 55)
        { discMult = 0.3f; r.disc_note = "defensive"; }
    r.disc_mult = discMult;

    int mit = player.mitigation;
    r.player_mit  = mit;
    r.npc_offense = npc.atk;

    // Mitigation EQ : (1 - AC/(AC+ATK))
    float denom = static_cast<float>(mit + std::max(1, npc.atk));
    float mitRatio = mit > 0 ? static_cast<float>(mit) / denom : 0.f;
    r.mitigation_pct = mitRatio * 100.f;
    r.exp_roll = static_cast<int>(20.f * (1.f - mitRatio));

    float hitChance  = avoidanceHitChance(npc.atk, mit, discMult);
    float baseDps    = r.avg_hit * attacks * hitChance / delaySec;
    r.est_dps        = baseDps * (1.f - mitRatio);

    return r;
}

// ── resistRatings ─────────────────────────────────────────────────────

ResistRatings resistRatings(const NpcData& npc, const PlayerTotals& pt,
                              int level)
{
    auto rate = [&](int playerRes) -> ResistRating {
        ResistRating rr;
        rr.value = playerRes;
        float denom = playerRes + npc.level * 2.f;
        rr.pct   = denom > 0.f ? playerRes / denom * 100.f : 0.f;
        rr.rating = rr.pct >= 65.f ? Rating::GOOD
                  : rr.pct >= 35.f ? Rating::MEDIUM
                  : Rating::LOW;
        return rr;
    };
    return { rate(pt.mr), rate(pt.fr), rate(pt.cr), rate(pt.dr), rate(pt.pr) };
}

// ── offenseRatings ────────────────────────────────────────────────────

OffenseRatings offenseRatings(const NpcData& npc, const PlayerTotals& pt,
                               std::string_view className)
{
    OffenseRatings r;
    float ratio    = npc.ac > 0 ? static_cast<float>(npc.ac) / std::max(1, pt.atk) : 0.f;
    r.melee.player_atk = pt.atk;
    r.melee.npc_ac     = npc.ac;
    r.melee.rating = ratio < 0.6f ? OffenseRating::EASY
                   : ratio < 0.9f ? OffenseRating::MEDIUM
                   : OffenseRating::HARD;

    // Sorts : masqués pour Warrior/Monk/Rogue/Berserker
    static const std::unordered_set<std::string> kMeleeOnly =
        {"Warrior","Monk","Rogue","Berserker"};
    if (kMeleeOnly.count(std::string(className))) return r;

    auto spellRate = [](int npcRes) -> OffenseRating {
        return npcRes <= 100 ? OffenseRating::EASY
             : npcRes <= 200 ? OffenseRating::MEDIUM
             : OffenseRating::HARD;
    };
    r.mr = SpellOffense{npc.mr, spellRate(npc.mr)};
    r.fr = SpellOffense{npc.fr, spellRate(npc.fr)};
    r.cr = SpellOffense{npc.cr, spellRate(npc.cr)};
    r.dr = SpellOffense{npc.dr, spellRate(npc.dr)};
    r.pr = SpellOffense{npc.pr, spellRate(npc.pr)};
    return r;
}

// ── slowLandPct ───────────────────────────────────────────────────────

std::optional<float> slowLandPct(const NpcData& npc, int playerLevel,
                                  int resistType, int resistDiff)
{
    auto sa = parseSa(npc.special_abilities);
    if (sa.count(12)) return std::nullopt;  // immune to slow

    // Résistance effective (MR=1, DR=5)
    static const std::unordered_map<int,const char*> kStat =
        {{1,"mr"},{5,"dr"}};
    int npcRes = (resistType == 1) ? npc.mr : npc.dr;
    npcRes = std::max(0, npcRes + resistDiff);

    int levelMod = (playerLevel - npc.level) * 2;
    int check    = std::max(0, npcRes - levelMod);
    int resistChance = std::max(4, std::min(198, check * 3 / 2));
    float landPct    = 100.f - resistChance / 200.f * 100.f;
    return landPct;
}

// ── decodeSpecialAbilities ────────────────────────────────────────────

std::vector<SpecialAbility> decodeSpecialAbilities(std::string_view raw) {
    // Table depuis npc_analysis.py::decode_special_abilities()
    static const std::unordered_map<int, std::pair<std::string,std::string>> kTable = {
        {1,  {"Summon <VALUE%",      "orange"}},
        {3,  {"Enrage <VALUE%",      "orange"}},
        {4,  {"Rampage",             "orange"}},
        {5,  {"AE Rampage",          "red"   }},
        {6,  {"Immune to Fleeing",   "red"   }},
        {7,  {"Calls for Help",      "orange"}},
        {8,  {"Immune to Fear",      "red"   }},
        {9,  {"Immune to Mez",       "red"   }},
        {10, {"Immune to Charm",     "red"   }},
        {11, {"Immune to Slow",      "red"   }},
        {12, {"Immune to Snare",     "red"   }},
        {13, {"Immune to DoT",       "red"   }},
        {14, {"Immune to Nuke",      "red"   }},
        {16, {"Immune to Stun",      "red"   }},
        {19, {"Immune to Taunt",     "orange"}},
        {20, {"Immune to Dispel",    "orange"}},
    };

    std::vector<SpecialAbility> result;
    auto sa = parseSa(raw);
    for (auto& [id, params] : sa) {
        auto it = kTable.find(id);
        SpecialAbility ab;
        if (it != kTable.end()) {
            ab.tag = it->second.first;
            // Remplacer VALUE par le paramètre si présent
            if (!params.empty()) {
                size_t pos = ab.tag.find("VALUE");
                if (pos != std::string::npos)
                    ab.tag.replace(pos, 5, std::to_string(params[0]));
            }
            ab.severity = it->second.second;
        } else {
            ab.tag = "Unknown(" + std::to_string(id) + ")";
            ab.severity = "grey";
        }
        result.push_back(ab);
    }
    return result;
}

// ── Spell helpers ─────────────────────────────────────────────────────
// Porter depuis npc_analysis.py : resist_label, effective_spell_type,
// target_type_label, format_spell_summary, spell_resist_pct
// (logique identique, traduction mécanique Python → C++)

std::string resistLabel(const SpellData& sp) {
    static const std::unordered_map<int,std::string> kMap =
        {{0,"—"},{1,"MR"},{2,"FR"},{3,"CR"},{4,"PR"},{5,"DR"}};
    auto it = kMap.find(sp.resist_type);
    return it != kMap.end() ? it->second : "?";
}

std::string effectiveSpellType(const SpellData& sp) {
    // Porter depuis npc_analysis.py::effective_spell_type()
    // Logique basée sur spell_type et SPAs
    return "spell"; // placeholder — implémenter en portant depuis Python
}

std::string targetTypeLabel(int tt) {
    // Porter depuis npc_analysis.py::target_type_label()
    static const std::unordered_map<int,std::string> kMap =
        {{1,"self"},{5,"single"},{8,"pb-ae"},{11,"target-ae"},{40,"pet"}};
    auto it = kMap.find(tt);
    return it != kMap.end() ? it->second : "target";
}

std::string formatSpellSummary(const SpellData&, int) {
    // Porter depuis npc_analysis.py::format_spell_summary()
    return "";
}

std::optional<float> spellResistPct(const SpellData& sp,
                                     const PlayerTotals& pt,
                                     int playerLevel, int npcLevel)
{
    // Porter depuis npc_analysis.py::spell_resist_pct()
    // Formule : max(4, min(198, (resist + level_mod) * 3/2))
    // land_pct = 100 - resist_chance/200*100
    if (sp.resist_type == 0) return std::nullopt; // unresistable
    int playerRes = 0;
    switch (sp.resist_type) {
        case 1: playerRes = pt.mr; break;
        case 2: playerRes = pt.fr; break;
        case 3: playerRes = pt.cr; break;
        case 4: playerRes = pt.pr; break;
        case 5: playerRes = pt.dr; break;
    }
    int levelMod = (playerLevel - npcLevel) * 2;
    int check    = playerRes + levelMod;
    int rChance  = std::max(4, std::min(198, check * 3 / 2));
    return 100.f - rChance / 200.f * 100.f;
}
```

- [ ] **Step 5 : Compiler et vérifier les tests**

```powershell
cmake --build build/debug --target eq_tests
ctest -C Debug --output-on-failure
```

Attendu : tous les tests NpcAnalysis passent.

- [ ] **Step 6 : Commit**

```powershell
git add src/core/npc_analysis.h src/core/npc_analysis.cpp tests/test_npc_analysis.cpp
git commit -m "feat(core): npc_analysis — incomingDamage, resistRatings, slowLandPct, decodeSpecialAbilities"
```

---

### Task 4 : `spell_stats` + `spell_stacking`

**Python source :** `eq-item-evaluator/core/spell_stats.py` (98 lignes) et `spell_stacking.py`

**Files:**
- Create: `src/core/spell_stats.h`
- Modify: `src/core/spell_stats.cpp`
- Create: `src/core/spell_stacking.h`
- Modify: `src/core/spell_stacking.cpp`
- Modify: `tests/test_spell_stats.cpp`
- Modify: `tests/test_spell_stacking.cpp`

- [ ] **Step 1 : Écrire les tests**

`tests/test_spell_stats.cpp` :
```cpp
#include <gtest/gtest.h>
#include "core/spell_stats.h"
#include "core/types.h"

TEST(SpellStats, SpellIncomingDpsZeroForEmptyList) {
    PlayerTotals pt;
    auto dps = spellIncomingDps({}, pt, 65, 60);
    EXPECT_FLOAT_EQ(dps, 0.f);
}
```

`tests/test_spell_stacking.cpp` :
```cpp
#include <gtest/gtest.h>
#include "core/spell_stacking.h"
#include "core/types.h"

TEST(SpellStacking, BardSongsAlwaysStackWithNonBard) {
    SpellData bard, nonBard;
    // classes8 != 255 → bard song
    bard.classes[7] = 1;  // bard (index 7 = class 8)
    nonBard.classes[7] = 255; // non-bard
    EXPECT_TRUE(spellsStack(bard, nonBard));
}
```

- [ ] **Step 2 : Écrire `src/core/spell_stats.h`**

```cpp
#pragma once
#include "core/types.h"
#include <vector>

// Valeur d'un sort (dégâts, heal, etc.) pour un niveau donné.
// Porter depuis spell_stats.py::spell_value()
[[nodiscard]] float spellValue(const SpellData& spell, int level);

// DPS total des sorts NPC sur le joueur.
// Porter depuis npc_analysis.py::spell_incoming_dps()
[[nodiscard]] float spellIncomingDps(
    const std::vector<SpellData>& npcSpells,
    const PlayerTotals& player,
    int playerLevel, int npcLevel);
```

- [ ] **Step 3 : Écrire `src/core/spell_stacking.h`**

```cpp
#pragma once
#include "core/types.h"

// Retourne true si les deux sorts peuvent coexister dans le buff bar.
// Implémente les règles de buffstacking.cpp d'EQMacEmu :
// - Bard songs (classes[7] != 255) stackent toujours avec les non-bard
// - Entre deux bard songs : comparaison slot par slot
[[nodiscard]] bool spellsStack(const SpellData& a, const SpellData& b);
```

- [ ] **Step 4 : Implémenter les `.cpp`**

`src/core/spell_stats.cpp` — porter depuis `spell_stats.py` :
```cpp
#include "core/spell_stats.h"
#include <cmath>
#include <algorithm>

float spellValue(const SpellData& sp, int level) {
    // Porter les formules de calcul depuis spell_stats.py::spell_value()
    // Formules EQ : base + (level * formula_factor) selon effect_formula
    return 0.f; // implémenter en portant depuis Python
}

float spellIncomingDps(const std::vector<SpellData>& spells,
                        const PlayerTotals& pt,
                        int playerLevel, int npcLevel)
{
    // Porter depuis npc_analysis.py::spell_incoming_dps()
    float total = 0.f;
    for (const auto& sp : spells) {
        // Porter la logique de calcul DPS par sort
        (void)sp;
    }
    return total;
}
```

`src/core/spell_stacking.cpp` — porter la logique de `buffstacking.cpp` EQMacEmu :
```cpp
#include "core/spell_stacking.h"

static bool isBardSong(const SpellData& sp) {
    return sp.classes[7] != 255; // index 7 = classe 8 (Bard)
}

bool spellsStack(const SpellData& a, const SpellData& b) {
    bool aBard = isBardSong(a);
    bool bBard = isBardSong(b);
    // Bard song + non-bard → toujours stacking (goto STACK_OK dans buffstacking.cpp)
    if (aBard != bBard) return true;
    // Deux non-bard ou deux bard : comparaison SPA slot par slot
    for (int i = 0; i < 12; ++i) {
        if (a.spa[i] == 0 && b.spa[i] == 0) continue;
        if (a.spa[i] == b.spa[i]) return false; // conflit SPA
    }
    return true;
}
```

- [ ] **Step 5 : Compiler et tester**

```powershell
cmake --build build/debug --target eq_tests
ctest -C Debug --output-on-failure
```

- [ ] **Step 6 : Commit**

```powershell
git add src/core/spell_stats.h src/core/spell_stats.cpp
git add src/core/spell_stacking.h src/core/spell_stacking.cpp
git add tests/test_spell_stats.cpp tests/test_spell_stacking.cpp
git commit -m "feat(core): spell_stats + spell_stacking"
```

---

### Task 5 : `derived_metrics` + `equipped_effects`

**Python source :** `eq-item-evaluator/core/derived_metrics.py` et `equipped_effects.py`

**Files:**
- Create: `src/core/derived_metrics.h`
- Modify: `src/core/derived_metrics.cpp`
- Create: `src/core/equipped_effects.h`
- Modify: `src/core/equipped_effects.cpp`

- [ ] **Step 1 : Écrire `src/core/derived_metrics.h`**

```cpp
#pragma once
#include "core/types.h"
#include <string>

// Métriques dérivées (offensive power, defensive score).
// Porter depuis derived_metrics.py.
[[nodiscard]] float offensivePower(const PlayerTotals& pt,
                                    std::string_view className);
[[nodiscard]] float defensiveScore(const PlayerTotals& pt,
                                    std::string_view className);
```

- [ ] **Step 2 : Écrire `src/core/equipped_effects.h`**

```cpp
#pragma once
#include "core/types.h"
#include <vector>
#include <string>

// Calcule les effets des items équipés (focus, proc, worn effects).
// Porter depuis equipped_effects.py.
[[nodiscard]] std::vector<std::string>
getActiveEffects(const std::vector<ItemData>& equippedItems, int level);
```

- [ ] **Step 3 : Implémenter les `.cpp`**

Porter depuis les fichiers Python correspondants. Les fonctions sont moins critiques que `npc_analysis` — porter la logique exacte depuis Python.

`src/core/derived_metrics.cpp` :
```cpp
#include "core/derived_metrics.h"
// Porter depuis derived_metrics.py
float offensivePower(const PlayerTotals&, std::string_view) { return 0.f; }
float defensiveScore(const PlayerTotals&, std::string_view) { return 0.f; }
```

`src/core/equipped_effects.cpp` :
```cpp
#include "core/equipped_effects.h"
// Porter depuis equipped_effects.py
std::vector<std::string> getActiveEffects(const std::vector<ItemData>&, int) { return {}; }
```

- [ ] **Step 4 : Compiler (pas de tests spécifiques — ces modules sont appelés par l'UI)**

```powershell
cmake --build build/debug --target eq_core
```

Attendu : compilation sans erreur.

- [ ] **Step 5 : Commit**

```powershell
git add src/core/derived_metrics.h src/core/derived_metrics.cpp
git add src/core/equipped_effects.h src/core/equipped_effects.cpp
git commit -m "feat(core): derived_metrics + equipped_effects"
```

---

### Vérification finale du Plan 2

- [ ] `cmake --build build/debug --target eq_core` → 0 erreur
- [ ] `ctest -C Debug --output-on-failure` → tous les tests passent
- [ ] Aucun `#include <Qt*>` dans `src/core/` — vérifier :

```powershell
Select-String -Path "src\core\*.h","src\core\*.cpp" -Pattern "#include <Q" -SimpleMatch
```

Attendu : aucun résultat.

**Prêt pour Plan 3 (couche DB) quand les vérifications passent.**
