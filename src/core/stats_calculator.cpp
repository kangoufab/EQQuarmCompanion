#include "core/stats_calculator.h"
#include "core/spell_stats.h"
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <string_view>

// ═══════════════════════════════════════════════════════════════════════════
// Constantes portées depuis stat_caps.py et stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════

// Attribute caps par expansion (nous ciblons PoP/Luclin = 255)
static constexpr int ATTRIBUTE_CAP = 255;
// Resist caps PoP/Luclin
static constexpr int RESIST_CAP    = 500;
// RuleI(Character, ItemATKCap)
static constexpr int ATK_ITEM_CAP        = 250;
// RuleI(Character, ItemManaRegenCap) — bonuses.cpp:184
static constexpr int MANA_REGEN_ITEM_CAP = 15;

// ── Skill caps par classe (depuis skill_caps table + stats_calculator.py) ─
// class_id → {skill_id → max_cap}
// Skills: Offense=33, 1HBlunt=0, 1HSlash=1, 2HBlunt=2, 2HSlash=3, 1HPiercing=36, HTH=28
static const std::unordered_map<int, std::unordered_map<int,int>> CLASS_SKILL_CAPS = {
    {1,  {{0,250},{1,250},{2,250},{3,250},{28,100},{33,252},{36,240}}}, // Warrior
    {2,  {{0,175},        {2,175},        {28, 75},{33,200}         }}, // Cleric
    {3,  {{0,225},{1,225},{2,225},{3,225},{28,100},{33,225},{36,225}}}, // Paladin
    {4,  {{0,250},{1,250},{2,250},{3,250},{28,100},{33,252},{36,240}}}, // Ranger
    {5,  {{0,225},{1,225},{2,225},{3,225},{28,100},{33,225},{36,225}}}, // Shadowknight
    {6,  {{0,175},{1,175},{2,175},        {28, 75},{33,200}         }}, // Druid
    {7,  {{0,252},        {2,252},        {28,252},{33,252}         }}, // Monk
    {8,  {{0,250},{1,250},               {28,100},{33,252},{36,250}}}, // Bard
    {9,  {{0,250},{1,250},               {28,100},{33,252},{36,250}}}, // Rogue
    {10, {{0,200},        {2,200},        {28, 75},{33,200},{36,200}}}, // Shaman
    {11, {{0,110},        {2,110},        {28, 75},{33,140},{36,110}}}, // Necromancer
    {12, {{0,110},        {2,110},        {28, 75},{33,140},{36,110}}}, // Wizard
    {13, {{0,110},        {2,110},        {28, 75},{33,140},{36,110}}}, // Magician
    {14, {{0,110},        {2,110},        {28, 75},{33,140},{36,110}}}, // Enchanter
    {15, {{0,225},        {2,225},        {28,250},{33,252},{36,225}}}, // Beastlord
};
static const std::unordered_map<std::string, int> CLASS_ID = {
    {"Warrior",1},{"Cleric",2},{"Paladin",3},{"Ranger",4},{"Shadowknight",5},
    {"Druid",6},{"Monk",7},{"Bard",8},{"Rogue",9},{"Shaman",10},
    {"Necromancer",11},{"Wizard",12},{"Magician",13},{"Enchanter",14},{"Beastlord",15},
};
// itemtype → weapon skill_id (GetSkillByItemType in mob.cpp)
static const std::unordered_map<int,int> ITEMTYPE_TO_SKILL = {
    {0,1},{1,3},{2,36},{3,0},{4,2},{97,36},
};
static int estimateSkill(int classId, int skillId, int level) {
    auto cit = CLASS_SKILL_CAPS.find(classId);
    if (cit == CLASS_SKILL_CAPS.end()) return 0;
    auto sit = cit->second.find(skillId);
    if (sit == cit->second.end()) return 0;
    int cap = sit->second;
    return std::min(cap, cap * std::min(level, 60) / 60);
}
static int calcDisplayedAtk(std::string_view className, int level, int totalStr,
                             int itemAtk, int primaryItemtype)
{
    itemAtk = std::min(itemAtk, ATK_ITEM_CAP);
    int strBonus = (totalStr >= 75) ? (2 * totalStr - 150) / 3 : 0;

    auto cit = CLASS_ID.find(std::string(className));
    int classId = (cit != CLASS_ID.end()) ? cit->second : 0;

    auto wit = ITEMTYPE_TO_SKILL.find(primaryItemtype);
    int weaponSkillId = (wit != ITEMTYPE_TO_SKILL.end()) ? wit->second : 28; // HTH

    int offenseSkill = estimateSkill(classId, 33, level);
    int weaponSkill  = estimateSkill(classId, weaponSkillId, level);

    if (className == "Ranger" && level > 54)
        offenseSkill += level * 4 - 216;

    int toHit  = 7 + offenseSkill + weaponSkill;
    int offVal = weaponSkill + itemAtk + strBonus;
    return (toHit + offVal) * 1000 / 744;
}

// ── Race IDs (EQMacEmu common/races.h) ───────────────────────────────────
static constexpr int RACE_HUMAN     = 1,  RACE_BARBARIAN = 2,  RACE_ERUDITE  = 3;
static constexpr int RACE_WOOD_ELF  = 4,  RACE_HIGH_ELF  = 5,  RACE_DARK_ELF = 6;
static constexpr int RACE_HALF_ELF  = 7,  RACE_DWARF     = 8,  RACE_TROLL    = 9;
static constexpr int RACE_OGRE      = 10, RACE_HALFLING   = 11, RACE_GNOME    = 12;
static constexpr int RACE_IKSAR     = 128, RACE_VAHSHIR   = 130;

// ── Résistances de base (portées de EQMacEmu zone/client_mods.cpp) ────────
struct BaseResists { int mr, fr, cr, dr, pr; };
static BaseResists calcBaseResists(int race, std::string_view cls, int level)
{
    // MR raciale
    int mr = (race == RACE_ERUDITE || race == RACE_DWARF) ? 30 : 25;

    // FR raciale
    int fr;
    if (race == RACE_IKSAR)        fr = 30;
    else if (race == RACE_TROLL)   fr = 5;
    else                           fr = 25;

    // CR raciale
    int cr;
    if (race == RACE_BARBARIAN)    cr = 35;
    else if (race == RACE_IKSAR)   cr = 15;
    else                           cr = 25;

    // DR raciale
    int dr;
    if (race == RACE_HALFLING)     dr = 20;
    else if (race == RACE_ERUDITE) dr = 10;
    else                           dr = 15;

    // PR raciale
    int pr = (race == RACE_DWARF || race == RACE_HALFLING) ? 20 : 15;

    // Bonus de classe (mêmes conditions que le serveur)
    auto lvlBonus49 = [&](int base) { return level > 49 ? base + (level - 49) : base; };
    auto lvlBonus50 = [&]() { return level > 50 ? level - 50 : 0; };

    if      (cls == "Warrior")     mr += level / 2;
    if      (cls == "Ranger")    { fr += lvlBonus49(4); cr += lvlBonus49(4); }
    else if (cls == "Monk")      { fr += lvlBonus49(8); dr += lvlBonus50(); pr += lvlBonus50(); }
    if      (cls == "Paladin")     dr += lvlBonus49(8);
    else if (cls == "Shadowknight"){ dr += lvlBonus49(4); pr += lvlBonus49(4); }
    else if (cls == "Beastlord") { dr += lvlBonus49(4); cr += lvlBonus49(4); }
    if      (cls == "Rogue")       pr += lvlBonus49(8);

    return {mr, fr, cr, dr, pr};
}

// ── Classes qui ont du mana ───────────────────────────────────────────────
static bool hasMana(std::string_view cls) {
    static const char* MANA_CLASSES[] = {
        "Cleric","Druid","Shaman","Paladin","Ranger","Shadowknight",
        "Wizard","Magician","Enchanter","Necromancer","Bard","Beastlord"
    };
    for (auto c : MANA_CLASSES)
        if (cls == c) return true;
    return false;
}

// ── Pure casters (AC non scalée ×4/3) ────────────────────────────────────
static bool isPureCaster(std::string_view cls) {
    return cls == "Necromancer" || cls == "Wizard" ||
           cls == "Magician"   || cls == "Enchanter";
}

// ── Caster type pour mana (I = INT, W = WIS) ─────────────────────────────
static char casterType(std::string_view cls) {
    if (cls == "Wizard"  || cls == "Magician" ||
        cls == "Enchanter" || cls == "Necromancer") return 'I';
    if (cls == "Cleric"  || cls == "Druid"   || cls == "Shaman" ||
        cls == "Paladin" || cls == "Ranger"  || cls == "Beastlord") return 'W';
    return 'N'; // no mana
}

// ═══════════════════════════════════════════════════════════════════════════
// _class_level_factor — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int classLevelFactor(std::string_view cls, int level) {
    if (cls == "Warrior") {
        if (level < 20) return 220;
        if (level < 30) return 230;
        if (level < 40) return 250;
        if (level < 53) return 270;
        if (level < 57) return 280;
        if (level < 60) return 290;
        if (level < 70) return 300;
        return 311;
    }
    if (cls == "Paladin" || cls == "Shadowknight") {
        if (level < 35) return 210;
        if (level < 45) return 220;
        if (level < 51) return 230;
        if (level < 56) return 240;
        if (level < 60) return 250;
        if (level < 68) return 260;
        return 270;
    }
    if (cls == "Monk" || cls == "Bard" || cls == "Rogue" || cls == "Beastlord") {
        if (level < 51) return 180;
        if (level < 58) return 190;
        if (level < 70) return 200;
        return 210;
    }
    if (cls == "Ranger") {
        if (level < 58) return 200;
        if (level < 70) return 210;
        return 220;
    }
    if (cls == "Druid" || cls == "Cleric" || cls == "Shaman") {
        return level < 70 ? 150 : 157;
    }
    if (cls == "Magician" || cls == "Wizard" || cls == "Necromancer" || cls == "Enchanter") {
        return level < 70 ? 120 : 127;
    }
    // Fallback (devrait couvrir tous les cas)
    if (level < 35) return 210;
    if (level < 45) return 220;
    if (level < 51) return 230;
    if (level < 56) return 240;
    if (level < 60) return 250;
    return 260;
}

// Quarm server (EQMacEmu) n'a PAS de cap sur les HP ou Mana items.
// itembonuses.HP et itembonuses.Mana sont sommés sans cap dans bonuses.cpp.
// Ces fonctions retournent INT_MAX pour ne pas cappér en pratique.
int hpCap(std::string_view /*cls*/, int /*level*/)   { return INT_MAX; }
int manaCap(std::string_view /*cls*/, int /*level*/) { return INT_MAX; }

int attackCap(std::string_view /*cls*/, int /*level*/) {
    // RuleI(Character, ItemATKCap) = 250
    return ATK_ITEM_CAP;
}

// ═══════════════════════════════════════════════════════════════════════════
// _calc_base_hp — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int calcBaseHp(std::string_view cls, int level, int totalSta) {
    int clf    = classLevelFactor(cls, level);
    int lm     = clf / 10;
    int levelHp = level * lm;
    int sta    = (totalSta <= 255) ? totalSta : 255 + (totalSta - 255) / 2;
    return levelHp * sta / 300 + levelHp;
}

// ═══════════════════════════════════════════════════════════════════════════
// _calc_base_mana — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int calcBaseMana(std::string_view /*primaryStat*/, int level, int wi) {
    int lesser    = std::max(0, (wi - 199) / 2);
    int mindFactor = wi - lesser;
    if (wi > 100)
        return ((5 * (mindFactor + 20)) / 2) * 3 * level / 40;
    return ((5 * (mindFactor + 200)) / 2) * 3 * level / 100;
}

// ═══════════════════════════════════════════════════════════════════════════
// _agi_avoidance — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int agiAvoidance(int agi, int level) {
    if (agi < 40)
        return (25 * (agi - 40)) / 40;
    if (agi >= 60 && agi <= 74)
        return (2 * (28 - (200 - agi) / 5)) / 3;
    if (agi >= 75) {
        int adj;
        if (level < 7)       adj = 35;
        else if (level < 20) adj = 55;
        else if (level < 40) adj = 70;
        else                 adj = 80;
        int capped = std::min(agi, 200);
        return (2 * (adj - (200 - capped) / 5)) / 3;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Defense skill max par classe — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int defenseSkillMax(std::string_view cls) {
    if (cls == "Warrior" || cls == "Paladin" || cls == "Shadowknight" ||
        cls == "Monk"    || cls == "Bard"    || cls == "Rogue")
        return 252;
    if (cls == "Ranger" || cls == "Beastlord")
        return 240;
    if (cls == "Cleric" || cls == "Druid" || cls == "Shaman")
        return 200;
    // Necromancer, Wizard, Magician, Enchanter
    return 145;
}

// ═══════════════════════════════════════════════════════════════════════════
// _calc_displayed_ac — portée depuis stats_calculator.py
// Retourne (displayed_ac, mitigation, avoidance)
// ═══════════════════════════════════════════════════════════════════════════
struct AcResult { int displayed, mitigation, avoidance, defSkill; };
// spellAc doit être passé séparément : le serveur divise l'AC sort par 4 (casters: /3)
// APRÈS le scaling 4/3 et l'anti-twink, contrairement à l'item AC.
static AcResult calcDisplayedAc(std::string_view cls, int level, int totalAgi,
                                 int itemAc, int spellAc = 0, int race = 0) {
    bool pureCaster = isPureCaster(cls);
    int defSkill  = std::min(level, 60) * defenseSkillMax(cls) / 60;
    int avoidance = std::max(1, defSkill * 400 / 225 + agiAvoidance(totalAgi, level));

    // Item AC ×4/3 pour non-casters (attack.cpp GetMitigation ligne ~4922)
    int mitigation = itemAc;
    if (!pureCaster)
        mitigation = 4 * mitigation / 3;

    // Anti-twink cap (ignoré si ignoreCap=true pour CalcAC affichée, mais le serveur
    // passe ignoreCap=true dans CalcAC → on l'applique quand même pour la cohérence)
    if (level < 50 && mitigation > level * 6 + 25)
        mitigation = level * 6 + 25;

    // Iksar racial AC (attack.cpp lignes 5094-5108)
    if (race == RACE_IKSAR) {
        if (level < 10)       mitigation += 10;
        else if (level > 35)  mitigation += 35;
        else                  mitigation += level;
    }

    // Contribution defense skill → mitigation (attack.cpp lignes 5113-5124)
    if (defSkill > 0)
        mitigation += pureCaster ? defSkill / 2 : defSkill / 3;

    // Spell AC divisé (attack.cpp lignes 5126-5131)
    if (spellAc > 0)
        mitigation += pureCaster ? spellAc / 3 : spellAc / 4;

    // AGI/20 → mitigation (attack.cpp ligne 5133-5134)
    if (totalAgi > 70)
        mitigation += totalAgi / 20;

    if (mitigation < 0) mitigation = 0;

    int displayed = (avoidance + mitigation) * 1000 / 847;
    return {displayed, mitigation, avoidance, defSkill};
}

// ═══════════════════════════════════════════════════════════════════════════
// _calc_base_hp_regen — portée depuis stats_calculator.py
// LevelRegen (sitting) — EQMacEmu client_mods.cpp
// ═══════════════════════════════════════════════════════════════════════════
static int calcBaseHpRegen(int level, int race) {
    int regen = 2; // 1 base + 1 sitting
    if (level >= 20) regen++;
    if (level >= 50) regen++;
    if (level >= 51) regen++;
    if (level >= 56) regen++;
    if (level >= 60) regen++;
    if (level >= 61) regen++;
    if (level >= 63) regen++;
    if (level >= 65) regen++;
    // Troll=9, Iksar=128 : bonus raciaux puis ×2
    if (race == 9 || race == 128) {
        if (level >= 51) regen++;
        if (level >= 56) regen++;
        regen *= 2;
    }
    return regen;
}

// ═══════════════════════════════════════════════════════════════════════════
// _calc_base_mana_regen — portée depuis stats_calculator.py
// EQMacEmu client_mods.cpp CalcManaRegen (sitting + meditate)
// ═══════════════════════════════════════════════════════════════════════════
static int calcBaseManaRegen(std::string_view cls, int level) {
    if (!hasMana(cls)) return 0;
    int levelBonus = (level > 61 ? 1 : 0) + (level > 63 ? 1 : 0);
    if (cls == "Bard" || cls == "Shadowknight")
        return 2 + levelBonus;
    // Toutes les classes avec méditate (INT/WIS casters + hybrids)
    int skill = std::min(level * 4, 235);
    return 4 + skill / 15 + levelBonus;
}

// ═══════════════════════════════════════════════════════════════════════════
// _haste_cap_pct — portée depuis stats_calculator.py
// ═══════════════════════════════════════════════════════════════════════════
static int hasteCapPct(int level) {
    if (level >= 60) return 100;
    if (level >= 51) return 85;
    return level + 25;
}

// ═══════════════════════════════════════════════════════════════════════════
// calculateTotals — portée depuis stats_calculator.py::compute_totals()
// ═══════════════════════════════════════════════════════════════════════════
PlayerTotals calculateTotals(const CharacterInfo& ci,
                              const std::vector<ItemData>& items,
                              int primaryItemtype,
                              PlayerTotalsExtra* extra,
                              const AaStats* aa)
{
    PlayerTotals t;
    if (ci.level <= 0) return t;

    t.str_v = ci.base_str;
    t.sta   = ci.base_sta;
    t.dex   = ci.base_dex;
    t.agi   = ci.base_agi;
    t.int_v = ci.base_int;
    t.wis   = ci.base_wis;
    t.cha   = ci.base_cha;

    auto br = calcBaseResists(ci.race, ci.class_name, ci.level);
    t.mr = br.mr;  t.fr = br.fr;  t.cr = br.cr;
    t.dr = br.dr;  t.pr = br.pr;

    // Items accumulation (raw)
    int itemAtkRaw = 0, itemHpRegenRaw = 0, itemManaRegenRaw = 0, rawAc = 0;
    int itemStr = 0, itemSta = 0, itemDex = 0, itemAgi = 0;
    int itemInt = 0, itemWis = 0, itemCha = 0;
    int itemMr = 0, itemFr = 0, itemCr = 0, itemDr = 0, itemPr = 0;
    int itemHp = 0, itemMana = 0, itemHaste = 0;
    const bool classHasMana = hasMana(ci.class_name);

    for (const auto& item : items) {
        itemHp      += item.hp;
        if (classHasMana) itemMana += item.mana;
        itemAtkRaw  += item.atk;
        rawAc       += item.ac;
        itemStr     += item.astr;  itemSta += item.asta;
        itemDex     += item.adex;  itemAgi += item.aagi;
        itemInt     += item.aint;  itemWis += item.awis;  itemCha += item.acha;
        itemMr      += item.mr;    itemFr  += item.fr;    itemCr  += item.cr;
        itemDr      += item.dr;    itemPr  += item.pr;
        itemHaste    = std::max(itemHaste, item.haste);
        itemHpRegenRaw   += item.hp_regen;
        itemManaRegenRaw += item.mana_regen;
    }

    t.str_v += itemStr;  t.sta   += itemSta;  t.dex += itemDex;
    t.agi   += itemAgi;  t.int_v += itemInt;  t.wis += itemWis;  t.cha += itemCha;
    t.mr    += itemMr;   t.fr    += itemFr;   t.cr  += itemCr;
    t.dr    += itemDr;   t.pr    += itemPr;
    t.haste  = std::min(itemHaste, hasteCapPct(ci.level));

    // ── AAs ───────────────────────────────────────────────────────────────
    int aaStr=0,aaSta=0,aaDex=0,aaAgi=0,aaInt=0,aaWis=0,aaCha=0;
    int aaMr=0,aaFr=0,aaCr=0,aaDr=0,aaPr=0,aaHp=0,aaHpRegen=0,aaManaRegen=0;
    if (aa) {
        auto getAa = [&](const char* k) {
            auto it = aa->stats.find(k); return it != aa->stats.end() ? it->second : 0;
        };
        aaStr  = getAa("astr"); aaSta = getAa("asta"); aaDex = getAa("adex");
        aaAgi  = getAa("aagi"); aaInt = getAa("aint"); aaWis = getAa("awis");
        aaCha  = getAa("acha");
        aaMr   = getAa("mr");   aaFr  = getAa("fr");  aaCr  = getAa("cr");
        aaDr   = getAa("dr");   aaPr  = getAa("pr");
        aaHp       = getAa("hp");
        aaHpRegen  = getAa("hp_regen");
        aaManaRegen= getAa("mana_regen");
        t.str_v += aaStr;  t.sta   += aaSta;  t.dex += aaDex;
        t.agi   += aaAgi;  t.int_v += aaInt;  t.wis += aaWis;  t.cha += aaCha;
        t.mr    += aaMr;   t.fr    += aaFr;   t.cr  += aaCr;
        t.dr    += aaDr;   t.pr    += aaPr;
        // aaHp NON ajouté à itemHp : contourne le cap items et n'est pas soumis
        // au multiplicateur Natural Durability (client_mods.cpp:252 → aabonuses.HP+5)
        // Pas de mana pool AA sur Quarm : CalcMaxMana() n'utilise pas aabonuses.Mana.
        // Pas d'AC AA dans GetMitigation() : aabonuses.AC absent de l'appel (attack.cpp:5291).
    }

    // HP Regen : base + items cappés à 30 (L≤60) + AAs non cappés
    // Référence : client_mods.cpp CalcHPRegen:171 (aabonuses.HPRegen ajouté avant le cap items)
    {
        int baseRegen    = calcBaseHpRegen(ci.level, ci.race);
        int itemRegenCap = (ci.level <= 60) ? 30 : ci.level - 30;
        t.hp_regen = baseRegen + std::min(itemHpRegenRaw, itemRegenCap) + aaHpRegen;
    }

    // Mana Regen : base + items cappés + AAs non cappés
    // Référence : client_mods.cpp CalcManaRegen:964 (aabonuses.ManaRegen uncapped)
    {
        int baseRegen = calcBaseManaRegen(ci.class_name, ci.level);
        int cappedItem = classHasMana ? std::min(itemManaRegenRaw, MANA_REGEN_ITEM_CAP) : 0;
        t.mana_regen = baseRegen + cappedItem + (classHasMana ? aaManaRegen : 0);
    }

    int totalSta = std::min(t.sta, ATTRIBUTE_CAP);
    int totalAgi = std::min(t.agi, ATTRIBUTE_CAP);

    // HP : baseHp + itemHp (pas de cap) + ND sur (base+items) + aaHp + 5 fixe
    // Référence : client_mods.cpp CalcMaxHP lignes 205-252
    int baseHp   = calcBaseHp(ci.class_name, ci.level, totalSta);
    int ndBonus  = 0;
    if (aa && aa->nd_pct > 0.0f)
        ndBonus = static_cast<int>((baseHp + itemHp) * aa->nd_pct / 100.0f);
    t.hp.base = t.hp.capped = baseHp + itemHp + ndBonus + aaHp + 5;

    // Mana : baseMana + itemMana (pas de cap) + aaMana
    int baseMana = 0;
    if (classHasMana) {
        char ct = casterType(ci.class_name);
        int wi = (ct == 'I') ? std::min(t.int_v, ATTRIBUTE_CAP)
                             : std::min(t.wis,   ATTRIBUTE_CAP);
        baseMana = calcBaseMana("", ci.level, wi);
        t.mana.base = t.mana.capped = baseMana + itemMana;
    }

    // ATK
    {
        int totalStr = std::min(t.str_v, ATTRIBUTE_CAP);
        t.atk = calcDisplayedAtk(ci.class_name, ci.level, totalStr,
                                  itemAtkRaw, primaryItemtype);
    }

    // AC : itemAc + aaAc dans rawAc, spellAc=0 ici (pas de sorts dans calculateTotals)
    const auto acResult = calcDisplayedAc(ci.class_name, ci.level, totalAgi, rawAc, 0, ci.race);
    t.mitigation = acResult.mitigation;
    t.ac         = acResult.displayed;

    // ── Remplissage de PlayerTotalsExtra ────────────────────────────────
    if (extra) {
        auto& s = extra->stats;

        // Helper : copie les contributions par AA depuis AaStats::sources
        auto addAaSources = [&](StatInfo& si, const char* key) {
            if (!aa) return;
            auto it = aa->sources.find(key);
            if (it != aa->sources.end()) si.aa_sources = it->second;
        };

        // Construction des item_sources pour chaque stat
        struct StatKey { const char* key; int ItemData::* field; };
        static const StatKey ATTR_KEYS[] = {
            {"astr",&ItemData::astr},{"asta",&ItemData::asta},{"adex",&ItemData::adex},
            {"aagi",&ItemData::aagi},{"aint",&ItemData::aint},{"awis",&ItemData::awis},
            {"acha",&ItemData::acha},
        };
        static const StatKey RESIST_KEYS[] = {
            {"mr",&ItemData::mr},{"fr",&ItemData::fr},{"cr",&ItemData::cr},
            {"dr",&ItemData::dr},{"pr",&ItemData::pr},
        };

        for (auto& [key,fld] : ATTR_KEYS) {
            StatInfo si;
            for (const auto& item : items)
                if (item.*fld != 0) si.item_sources.push_back({item.name, item.*fld});
            s[key] = si; // base_val et raw fixés explicitement ci-dessous
        }
        // Correct base_val per attribute
        struct AttrFix { const char* k; int base; int item; int aa; };
        const AttrFix ATTR_FIX[] = {
            {"astr", ci.base_str, itemStr, aaStr},
            {"asta", ci.base_sta, itemSta, aaSta},
            {"adex", ci.base_dex, itemDex, aaDex},
            {"aagi", ci.base_agi, itemAgi, aaAgi},
            {"aint", ci.base_int, itemInt, aaInt},
            {"awis", ci.base_wis, itemWis, aaWis},
            {"acha", ci.base_cha, itemCha, aaCha},
        };
        for (auto& [k, base, item, aa_v] : ATTR_FIX) {
            s[k].base_val = base;
            s[k].aa_val   = aa_v;
            s[k].raw      = base + item + aa_v;
            addAaSources(s[k], k);
        }

        const int baseResistArr[5] = {br.mr, br.fr, br.cr, br.dr, br.pr};
        const int itemResistArr[5] = {itemMr, itemFr, itemCr, itemDr, itemPr};
        const int aaResistArr[5]   = {aaMr, aaFr, aaCr, aaDr, aaPr};
        int ri = 0;
        for (auto& [key,fld] : RESIST_KEYS) {
            StatInfo si;
            si.base_val = baseResistArr[ri];
            si.aa_val   = aaResistArr[ri];
            si.raw = baseResistArr[ri] + itemResistArr[ri] + aaResistArr[ri];
            for (const auto& item : items)
                if (item.*fld != 0) si.item_sources.push_back({item.name, item.*fld});
            addAaSources(si, key);
            s[key] = si;
            ++ri;
        }

        // Haste (max-wins — on marque le gagnant)
        {
            StatInfo si;
            si.raw = itemHaste;
            si.base_val = 0;
            int cap = hasteCapPct(ci.level);
            for (const auto& item : items)
                if (item.haste > 0) {
                    std::string label = item.name + (item.haste == itemHaste ? " ✓" : "");
                    si.item_sources.push_back({label, item.haste});
                }
            si.formula.push_back({"Cap niveau " + std::to_string(ci.level), std::to_string(cap) + "%"});
            s["haste"] = si;
        }

        // HP
        {
            StatInfo si;
            si.raw = t.hp.capped;
            si.base_val = baseHp;
            si.aa_val   = aaHp + ndBonus;
            addAaSources(si, "hp");
            for (const auto& item : items)
                if (item.hp != 0) si.item_sources.push_back({item.name, item.hp});
            si.formula.push_back({"Base " + ci.class_name + " L" + std::to_string(ci.level)
                + " (STA " + std::to_string(std::min(ci.base_sta + itemSta, 255)) + ")",
                std::to_string(baseHp)});
            if (ndBonus > 0) {
                float ndp = aa ? aa->nd_pct : 0.0f;
                si.formula.push_back({"Natural Durability (" + std::to_string((int)ndp) + "%)", "+" + std::to_string(ndBonus)});
            }
            if (aaHp > 0)
                si.formula.push_back({"AAs HP", "+" + std::to_string(aaHp)});
            si.formula.push_back({"+5 fixe (CalcMaxHP)", "+5"});
            si.formula.push_back({"Total", std::to_string(t.hp.capped)});
            s["hp"] = si;
        }

        // Mana
        if (classHasMana) {
            StatInfo si;
            si.raw = baseMana + itemMana;
            si.base_val = baseMana;
            for (const auto& item : items)
                if (item.mana != 0) si.item_sources.push_back({item.name, item.mana});
            si.formula.push_back({"Base mana L" + std::to_string(ci.level), std::to_string(baseMana)});
            si.formula.push_back({"Total", std::to_string(t.mana.capped)});
            s["mana"] = si;
        }

        // HP Regen
        {
            int baseRegen    = calcBaseHpRegen(ci.level, ci.race);
            int itemRegenCap = (ci.level <= 60) ? 30 : ci.level - 30;
            int cappedItem   = std::min(itemHpRegenRaw, itemRegenCap);
            StatInfo si;
            si.base_val = baseRegen;
            si.aa_val   = aaHpRegen;
            si.raw      = t.hp_regen;
            addAaSources(si, "hp_regen");
            for (const auto& item : items)
                if (item.hp_regen != 0) si.item_sources.push_back({item.name, item.hp_regen});
            si.formula.push_back({"Base regen L" + std::to_string(ci.level), std::to_string(baseRegen) + "/tick"});
            if (itemHpRegenRaw > itemRegenCap)
                si.formula.push_back({"Items HP regen (cap " + std::to_string(itemRegenCap) + ")",
                    std::to_string(itemHpRegenRaw) + " → " + std::to_string(cappedItem)});
            if (aaHpRegen > 0)
                si.formula.push_back({"AAs HP regen", "+" + std::to_string(aaHpRegen) + "/tick"});
            si.formula.push_back({"Total", std::to_string(t.hp_regen) + "/tick"});
            s["hp_regen"] = si;
        }

        // Mana Regen
        if (classHasMana) {
            int baseRegen  = calcBaseManaRegen(ci.class_name, ci.level);
            int cappedItem = std::min(itemManaRegenRaw, MANA_REGEN_ITEM_CAP);
            StatInfo si;
            si.base_val = baseRegen;
            si.aa_val   = aaManaRegen;
            si.raw      = t.mana_regen;
            addAaSources(si, "mana_regen");
            for (const auto& item : items)
                if (item.mana_regen != 0) si.item_sources.push_back({item.name, item.mana_regen});
            si.formula.push_back({"Base méditate L" + std::to_string(ci.level), std::to_string(baseRegen) + "/tick"});
            if (itemManaRegenRaw > MANA_REGEN_ITEM_CAP)
                si.formula.push_back({"Flowing Thought (cap " + std::to_string(MANA_REGEN_ITEM_CAP) + ")",
                    std::to_string(itemManaRegenRaw) + " → " + std::to_string(cappedItem)});
            if (aaManaRegen > 0)
                si.formula.push_back({"AAs Mana regen", "+" + std::to_string(aaManaRegen) + "/tick"});
            si.formula.push_back({"Total", std::to_string(t.mana_regen) + "/tick"});
            s["mana_regen"] = si;
        }

        // ATK
        {
            int totalStr = std::min(t.str_v, ATTRIBUTE_CAP);
            int strBonus = (totalStr >= 75) ? (2 * totalStr - 150) / 3 : 0;
            StatInfo si;
            si.raw = t.atk;
            for (const auto& item : items)
                if (item.atk != 0) si.item_sources.push_back({item.name, item.atk});
            si.formula.push_back({"Items ATK brut (cap " + std::to_string(ATK_ITEM_CAP) + ")",
                std::to_string(itemAtkRaw) + (itemAtkRaw > ATK_ITEM_CAP ? " → " + std::to_string(ATK_ITEM_CAP) : "")});
            si.formula.push_back({"Bonus STR (" + std::to_string(totalStr) + ")", "+" + std::to_string(strBonus)});
            si.formula.push_back({"Affiché (×1000/744)", std::to_string(t.atk)});
            s["atk"] = si;
        }

        // AC
        {
            bool pureCast = isPureCaster(ci.class_name);
            StatInfo si;
            si.raw = acResult.displayed;
            for (const auto& item : items)
                if (item.ac != 0) si.item_sources.push_back({item.name, item.ac});
            si.formula.push_back({"Items+AAs AC", std::to_string(rawAc)});
            si.formula.push_back({"Mitigation" + std::string(pureCast ? "" : " (×4/3)"),
                std::to_string(acResult.mitigation)});
            si.formula.push_back({"Défense (skill " + std::to_string(acResult.defSkill) + "/"
                + std::string(pureCast ? "2" : "3") + ")",
                "+" + std::to_string(pureCast ? acResult.defSkill/2 : acResult.defSkill/3)});
            si.formula.push_back({"Avoidance (défense + AGI " + std::to_string(totalAgi) + ")",
                std::to_string(acResult.avoidance)});
            si.formula.push_back({"Affiché (×1000/847)", std::to_string(acResult.displayed)});
            s["ac"] = si;
        }
    }

    return t;
}

// ═══════════════════════════════════════════════════════════════════════════
// calculateTotalsWithSpells — idem calculateTotals + accumulation des sorts
// ═══════════════════════════════════════════════════════════════════════════
PlayerTotals calculateTotalsWithSpells(
    const CharacterInfo& ci,
    const std::vector<ItemData>& items,
    const std::vector<std::map<std::string, int>>& spellDicts,
    int primaryItemtype,
    PlayerTotalsExtra* extra)
{
    PlayerTotals t;
    if (ci.level <= 0) return t;

    t.str_v = ci.base_str;
    t.sta   = ci.base_sta;
    t.dex   = ci.base_dex;
    t.agi   = ci.base_agi;
    t.int_v = ci.base_int;
    t.wis   = ci.base_wis;
    t.cha   = ci.base_cha;

    {
        auto br = calcBaseResists(ci.race, ci.class_name, ci.level);
        t.mr = br.mr;  t.fr = br.fr;  t.cr = br.cr;
        t.dr = br.dr;  t.pr = br.pr;
    }

    int itemAtkRaw = 0, rawAc = 0;
    int itemHpRegenRaw = 0, itemManaRegenRaw = 0;
    int itemHpRaw = 0, itemManaRaw = 0;
    const bool classHasMana = hasMana(ci.class_name);

    for (const auto& item : items) {
        itemHpRaw        += item.hp;
        if (classHasMana) itemManaRaw += item.mana;
        itemAtkRaw       += item.atk;
        rawAc            += item.ac;
        t.str_v          += item.astr;
        t.sta            += item.asta;
        t.dex            += item.adex;
        t.agi            += item.aagi;
        t.int_v          += item.aint;
        t.wis            += item.awis;
        t.cha            += item.acha;
        t.mr             += item.mr;
        t.fr             += item.fr;
        t.cr             += item.cr;
        t.dr             += item.dr;
        t.pr             += item.pr;
        t.haste           = std::max(t.haste, item.haste);
        itemHpRegenRaw   += item.hp_regen;
        itemManaRegenRaw += item.mana_regen;
    }

    // Accumulation des stats de sorts (avant caps)
    auto getSpell = [&](const std::map<std::string,int>& d, const char* k) {
        auto it = d.find(k); return it != d.end() ? it->second : 0;
    };
    int spellHpRegen = 0, spellManaRegen = 0;
    int spellHp = 0, spellMana = 0, spellAcRaw = 0, spellHasteV3 = 0;
    for (const auto& d : spellDicts) {
        spellHp      += getSpell(d, "hp");
        if (classHasMana) spellMana += getSpell(d, "mana");
        itemAtkRaw   += getSpell(d, "atk");
        spellAcRaw   += getSpell(d, "ac"); // séparé de rawAc : divisé par 4 côté serveur
        t.str_v      += getSpell(d, "astr");
        t.sta        += getSpell(d, "asta");
        t.dex        += getSpell(d, "adex");
        t.agi        += getSpell(d, "aagi");
        t.int_v      += getSpell(d, "aint");
        t.wis        += getSpell(d, "awis");
        t.cha        += getSpell(d, "acha");
        t.mr         += getSpell(d, "mr");
        t.fr         += getSpell(d, "fr");
        t.cr         += getSpell(d, "cr");
        t.dr         += getSpell(d, "dr");
        t.pr         += getSpell(d, "pr");
        // V1+V2 subject to normal cap; V3 (overhaste) applied after cap
        t.haste       = std::max(t.haste, getSpell(d, "haste") + getSpell(d, "haste_v2"));
        spellHasteV3  += getSpell(d, "haste_v3");
        spellHpRegen   += getSpell(d, "hp_regen");
        spellManaRegen += getSpell(d, "mana_regen");
    }

    // HP Regen : base + items cappés + sorts (non soumis au cap items)
    {
        int baseRegen    = calcBaseHpRegen(ci.level, ci.race);
        int itemRegenCap = (ci.level <= 60) ? 30 : ci.level - 30;
        t.hp_regen = baseRegen + std::min(itemHpRegenRaw, itemRegenCap) + spellHpRegen;
    }

    // Mana Regen : base méditate + items cappés + sorts
    {
        int baseRegen  = calcBaseManaRegen(ci.class_name, ci.level);
        int cappedItem = classHasMana ? std::min(itemManaRegenRaw, MANA_REGEN_ITEM_CAP) : 0;
        t.mana_regen = baseRegen + cappedItem + spellManaRegen;
    }

    t.haste = std::min(t.haste, hasteCapPct(ci.level));
    // Overhaste (V3/SE_AttackSpeed3) added after cap — server cap: 25 for level 51+, 10 for 1-50
    if (spellHasteV3 > 0) {
        int overCap = (ci.level >= 51) ? 25 : 10;
        t.haste += std::min(spellHasteV3, overCap);
    }

    int totalSta = std::min(t.sta,   ATTRIBUTE_CAP);
    int totalAgi = std::min(t.agi,   ATTRIBUTE_CAP);

    // HP : pas de cap items, +5 fixe, sorts ajoutés après base+items
    {
        int baseHp = calcBaseHp(ci.class_name, ci.level, totalSta);
        t.hp.base = t.hp.capped = baseHp + itemHpRaw + spellHp + 5;
    }

    // Mana : pas de cap items
    if (classHasMana) {
        char ct = casterType(ci.class_name);
        int wi = (ct == 'I') ? std::min(t.int_v, ATTRIBUTE_CAP)
                             : std::min(t.wis,   ATTRIBUTE_CAP);
        int baseMana = calcBaseMana("", ci.level, wi);
        t.mana.base = t.mana.capped = baseMana + itemManaRaw + spellMana;
    }

    {
        int totalStr = std::min(t.str_v, ATTRIBUTE_CAP);
        t.atk = calcDisplayedAtk(ci.class_name, ci.level, totalStr,
                                  itemAtkRaw, primaryItemtype);
    }

    {
        auto acResult = calcDisplayedAc(ci.class_name, ci.level, totalAgi,
                                        rawAc, spellAcRaw, ci.race);
        t.mitigation = acResult.mitigation;
        t.ac         = acResult.displayed;
    }

    return t;
}

// ═══════════════════════════════════════════════════════════════════════════
// playerTotalStat — lookup par nom de stat depuis PlayerTotals.
// ═══════════════════════════════════════════════════════════════════════════
int playerTotalStat(const std::string& stat, const PlayerTotals& t) {
    if (stat == "astr")       return t.str_v;
    if (stat == "asta")       return t.sta;
    if (stat == "adex")       return t.dex;
    if (stat == "aagi")       return t.agi;
    if (stat == "aint")       return t.int_v;
    if (stat == "awis")       return t.wis;
    if (stat == "acha")       return t.cha;
    if (stat == "hp")         return t.hp.capped;
    if (stat == "mana")       return t.mana.capped;
    if (stat == "ac")         return t.ac;
    if (stat == "atk")        return t.atk;
    if (stat == "haste")      return t.haste;
    if (stat == "hp_regen")   return t.hp_regen;
    if (stat == "mana_regen") return t.mana_regen;
    if (stat == "mr")         return t.mr;
    if (stat == "fr")         return t.fr;
    if (stat == "cr")         return t.cr;
    if (stat == "dr")         return t.dr;
    if (stat == "pr")         return t.pr;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// applyWornStats — calcule atk/haste/hp_regen/mana_regen depuis les formules
// worn de l'item. Port de apply_worn_stats() (item_database.py).
// wornlevel : niveau de scale du sort worn (si 0, utilise level du perso).
// haste     : EQ stocke 115 pour +15% → on soustrait 100 (offset = -100).
// ═══════════════════════════════════════════════════════════════════════════
void applyWornStats(ItemData& item, int level) {
    if (item.worneffect == 0) return;
    int lvl = (item.wornlevel > 0) ? item.wornlevel : level;

    auto resolve = [&](int base, int max, int formula, int offset) -> int {
        if (base == 0) return 0;
        return std::max(0, calcSpellEffectValue(base, max, formula, lvl) + offset);
    };

    item.atk        = resolve(item.atk_base,        item.atk_max,        item.atk_formula,        0);
    item.haste      = resolve(item.haste_base,       item.haste_max,      item.haste_formula,    -100);
    item.hp_regen   = resolve(item.hp_regen_base,    item.hp_regen_max,   item.hp_regen_formula,    0);
    item.mana_regen = resolve(item.mana_regen_base,  item.mana_regen_max, item.mana_regen_formula,  0);
}
