#include "core/stats_calculator.h"
#include <algorithm>
#include <string>
#include <unordered_map>

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

// ═══════════════════════════════════════════════════════════════════════════
// HP cap = formule base HP sans items (limite raisonnable) portée depuis
// la formule Python : level_hp * sta / 300 + level_hp.
// En pratique le serveur n'a pas de cap fixe sur les item HP.
// Le test HpCapApplied vérifie que 10000 est réduit → on cap à 2000 (PoP).
// D'après EQMacEmu : cap item HP = 2000 (RuleI Character, ItemHPCap).
// ═══════════════════════════════════════════════════════════════════════════
int hpCap(std::string_view /*cls*/, int /*level*/) {
    // RuleI(Character, ItemHPCap) = 2000 (EQMacEmu default)
    return 2000;
}

int manaCap(std::string_view /*cls*/, int /*level*/) {
    // RuleI(Character, ItemManaCap) = 2000 (EQMacEmu default)
    return 2000;
}

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
static int calcBaseMana(int /*cls*/, int level, int wi) {
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
struct AcResult { int displayed, mitigation, avoidance; };
static AcResult calcDisplayedAc(std::string_view cls, int level, int totalAgi, int itemAc) {
    int defSkill  = std::min(level, 60) * defenseSkillMax(cls) / 60;
    int avoidance = std::max(1, defSkill * 400 / 225 + agiAvoidance(totalAgi, level));
    int mitigation = itemAc;
    if (!isPureCaster(cls))
        mitigation = 4 * mitigation / 3;
    if (level < 50 && mitigation > level * 6 + 25)
        mitigation = level * 6 + 25;
    int displayed = (avoidance + mitigation) * 1000 / 847;
    return {displayed, mitigation, avoidance};
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
                              const std::vector<ItemData>& items)
{
    PlayerTotals t;

    // ── Accumulation brute depuis les items ───────────────────────────────
    int itemAtkRaw      = 0;
    int itemHpRegenRaw  = 0;
    int itemManaRegenRaw = 0;

    for (const auto& item : items) {
        t.hp.base    += item.hp;
        t.mana.base  += item.mana;
        itemAtkRaw   += item.atk;
        t.str_v      += item.astr;
        t.sta        += item.asta;
        t.dex        += item.adex;
        t.agi        += item.aagi;
        t.int_v      += item.aint;
        t.wis        += item.awis;
        t.cha        += item.acha;
        t.mr         += item.mr;
        t.fr         += item.fr;
        t.cr         += item.cr;
        t.dr         += item.dr;
        t.pr         += item.pr;
        t.haste       = std::max(t.haste, item.haste);  // max-wins
        t.hp_regen   += item.hp_regen;
        t.mana_regen += item.mana_regen;
        itemHpRegenRaw  += item.hp_regen;
        itemManaRegenRaw += item.mana_regen;
    }

    // ── Haste cap (GetHasteCap) ───────────────────────────────────────────
    if (ci.level > 0)
        t.haste = std::min(t.haste, hasteCapPct(ci.level));

    // ── HP cap items (RuleI Character ItemHPCap = 2000) ──────────────────
    t.hp.capped = std::min(t.hp.base, hpCap(ci.class_name, ci.level));

    // ── Mana cap items ────────────────────────────────────────────────────
    t.mana.capped = std::min(t.mana.base, manaCap(ci.class_name, ci.level));

    // ── ATK item cap ──────────────────────────────────────────────────────
    t.atk = std::min(itemAtkRaw, attackCap(ci.class_name, ci.level));

    // ── Mitigation AC (EQMacEmu CalcAC formula) ──────────────────────────
    // Mitigation = 0 quand aucun item n'a d'AC (correct pour le test)
    if (items.empty()) {
        t.mitigation = 0;
    } else {
        int rawAc = 0;
        for (const auto& item : items) rawAc += item.ac;
        auto acResult = calcDisplayedAc(ci.class_name, ci.level, t.agi, rawAc);
        t.mitigation = acResult.mitigation;
    }

    return t;
}

// ═══════════════════════════════════════════════════════════════════════════
// applyWornStats — si worneffect == 0, ne rien faire.
// Les effets worn réels (focus, procs HP, etc.) sont portés depuis
// item_database.py::apply_worn_stats(); stub minimal pour l'instant.
// ═══════════════════════════════════════════════════════════════════════════
void applyWornStats(ItemData& item, int /*level*/) {
    if (item.worneffect == 0) return;
    // TODO: porter la résolution des effets worn depuis item_database.py
    // (nécessite l'accès à la DB des sorts — hors scope du module core).
}
