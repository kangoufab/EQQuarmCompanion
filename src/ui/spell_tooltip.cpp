#include "ui/spell_tooltip.h"
#include "core/spell_stats.h"
#include <QStringList>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

QString formatSpellTooltip(const SpellData& spell, int level,
                            const std::map<int,QString>& spellNames)
{
    static const std::map<int, const char*> SPA_DESC = {
        {0,"HP"}, {1,"AC"}, {2,"ATK"}, {3,"Movement Speed"},
        {4,"STR"}, {5,"DEX"}, {6,"AGI"}, {7,"STA"}, {8,"INT"}, {9,"WIS"},
        {11,"Haste"},
        {35,"Disease Counter"}, {36,"Poison Counter"},
        {46,"Fire Resistance"}, {47,"Cold Resistance"},
        {48,"Poison Resistance"}, {49,"Disease Resistance"}, {50,"Magic Resistance"},
        {55,"Rune"}, {59,"Damage Shield"}, {69,"Max HP"},
        {92,"Hate"}, {97,"Max Mana"}, {98,"Haste"}, {100,"HP Regen"},
        {111,"All Resist"}, {116,"Curse Counter"},
        {124,"Spell Damage"}, {125,"Healing"}, {126,"Spell Haste"},
        {127,"Spell Haste"}, {128,"Spell Duration"}, {129,"Spell Range"},
        {130,"Healing"}, {131,"Spell Mana Cost"}, {132,"Spell Mana Cost"},
        {133,"Spell Damage"}, {159,"All Stats"},
        {169,"Critical Hit Chance"}, {170,"Spell Critical Chance"},
        {171,"Crippling Blow Chance"}, {172,"Avoidance"},
        {173,"Riposte Chance"}, {174,"Dodge Chance"},
        {175,"Parry Chance"}, {176,"Dual Wield Chance"},
        {177,"Double Attack Chance"}, {182,"Hundred Hands Effect"},
        {184,"Hit Chance"}, {185,"Damage Modifier"}, {188,"Block Chance"},
        {189,"Endurance"}, {190,"Max Endurance"}, {196,"Strikethrough"},
    };
    static const std::map<int, const char*> SPA_BOOL = {
        {12,"Invisibility"}, {13,"See Invisible"}, {14,"Water Breathing"},
        {18,"Pacify"}, {22,"Charm"}, {23,"Fear"}, {27,"Dispel Magic"},
        {28,"Invisible to Undead"}, {29,"Invisible to Animals"},
        {31,"Mesmerize"}, {57,"Levitate"}, {61,"Identify"},
        {64,"Spin"}, {65,"Infravision"}, {66,"Ultravision"},
        {71,"Necro Pet"}, {74,"Feign Death"},
    };
    static const std::set<int> SPA_PCT = {
        3, 98, 169, 170, 171, 172, 173, 174, 175, 176, 177, 182, 184, 185, 188, 196
    };
    // SPA names used in SE_LimitEffect (137) — more descriptive than SPA_DESC
    static const std::map<int, const char*> SPA_EFF_NAMES = {
        {0,"Hitpoints"}, {1,"AC"}, {2,"ATK"}, {3,"Movement Speed"},
        {4,"STR"}, {5,"DEX"}, {6,"AGI"}, {7,"STA"}, {8,"INT"}, {9,"WIS"},
        {10,"CHA"}, {11,"Haste"},
        {15,"Mana"}, {21,"Stun"}, {22,"Charm"}, {23,"Fear"},
        {35,"Disease Counter"}, {36,"Poison Counter"},
        {46,"Fire Resistance"}, {47,"Cold Resistance"},
        {48,"Poison Resistance"}, {49,"Disease Resistance"}, {50,"Magic Resistance"},
        {55,"Rune"}, {57,"Levitate"}, {59,"Damage Shield"}, {69,"Max HP"},
        {79,"HP Increase"}, {85,"Root"}, {92,"Hate"}, {97,"Max Mana"},
        {98,"Haste v2"}, {100,"HP Regen"}, {111,"All Resist"}, {116,"Curse Counter"},
        {119,"Haste v3"},
        {124,"Improved Damage"}, {125,"Improved Healing"},
        {126,"Resist Reduction"}, {127,"Spell Haste"},
        {128,"Spell Duration"}, {129,"Spell Range"},
        {131,"Reagent Conservation"}, {132,"Mana Cost Reduction"},
        {147,"Percental Heal"},
    };
    static const std::map<int, const char*> RESIST_NAMES = {
        {1,"MR"}, {2,"FR"}, {3,"CR"}, {4,"PR"}, {5,"DR"},
        {6,"Chromatic"}, {7,"Prismatic"},
    };
    static const std::map<int, const char*> TARGET_NAMES = {
        {1,"AE pc+NPC"}, {2,"Group"}, {3,"AE Summoned"}, {4,"AE Undead"},
        {5,"Self"}, {6,"Single"}, {8,"Animal"}, {9,"Undead"}, {10,"Summoned"},
        {11,"AE LOS"}, {13,"Large Targets"}, {14,"AE pc"}, {16,"Dragon"},
        {18,"AE Plant"}, {20,"Targeted AE"}, {24,"AE Target"}, {32,"Hatelist"},
        {41,"Group v2"}, {42,"Directional"}, {43,"Single in Group"},
    };

    QStringList parts;
    for (int i = 0; i < 12; ++i) {
        int spa     = spell.spa[i];
        int base    = spell.effect_base_value[i];
        int mx      = spell.effect_limit_value[i];
        int formula = spell.effect_formula[i];

        if (spa == 254) break;
        if (spa == 10)  continue;

        if (spa == 11) {
            int val = level ? calcSpellEffectValue(base, mx, formula, level) : (mx ? std::min(base, mx) : base);
            if (val != 0) parts << QString("Haste +%1%").arg(val - 100);
        } else if (spa == 98) {
            if (base != 0) parts << QString("Haste +%1%").arg(std::abs(base) - 100);
        } else if (spa == 119) {
            int val = level ? calcSpellEffectValue(base, mx, formula, level) : std::abs(base);
            parts << QString("Haste V3 (overcap) +%1%").arg(val);
        } else if (spa == 0) {
            if (base > 0) {
                int val = level ? calcSpellEffectValue(base, mx, formula, level) : (mx ? std::min(base, mx) : base);
                parts << QString("Increase HP by +%1 per tick").arg(val);
            } else if (base < 0) {
                parts << QString("Decrease HP by %1").arg(std::abs(base));
            }
        } else if (spa == 15) {
            if (base != 0) {
                const char* verb = base > 0 ? "Increase" : "Decrease";
                QString sfx = base > 0 ? " per tick" : "";
                parts << QString("%1 Mana by %2%3").arg(verb).arg(std::abs(base)).arg(sfx);
            }
        } else if (spa == 21) {
            if (base > 0) parts << QString("Stun for %1 sec").arg(base / 1000);
        } else if (spa == 79) {
            if (base != 0) {
                const char* verb = base < 0 ? "Decrease" : "Increase";
                int val = (mx != 0 && formula != 100) ? std::abs(mx) : std::abs(base);
                parts << QString("%1 HP by %2").arg(verb).arg(val);
            }
        } else if (spa == 92) {
            if (base != 0) {
                const char* verb = base > 0 ? "Increase" : "Decrease";
                parts << QString("%1 Hate by %2").arg(verb).arg(std::abs(base));
            }
        } else if (spa == 100) {
            if (base != 0) parts << QString("Increase HP by +%1 per tick").arg(base);
        } else if (spa >= 120 && spa <= 133) {
            auto it = SPA_DESC.find(spa);
            if (it != SPA_DESC.end() && base != 0) {
                if (spa == 131 || spa == 132)
                    parts << QString("Decrease %1 by %2%").arg(it->second).arg(std::abs(base));
                else
                    parts << QString("Increase %1 by %2%").arg(it->second).arg(std::abs(base));
            }
        } else if (spa == 134) {
            // SE_LimitMaxLevel
            parts << QString("Limit: Max Level (%1)").arg(base);
        } else if (spa == 135) {
            // SE_LimitResist
            int rid = std::abs(base);
            auto it = RESIST_NAMES.find(rid);
            QString rname = (it != RESIST_NAMES.end()) ? it->second : QString("Resist %1").arg(rid);
            parts << (base < 0
                ? QString("Exclude: Resist(%1)").arg(rname)
                : QString("Limit: Resist(%1)").arg(rname));
        } else if (spa == 136) {
            // SE_LimitTarget
            int tid = std::abs(base);
            auto it = TARGET_NAMES.find(tid);
            QString tname = (it != TARGET_NAMES.end()) ? it->second : QString("Target %1").arg(tid);
            parts << (base < 0
                ? QString("Exclude: Target(%1)").arg(tname)
                : QString("Limit: Target(%1)").arg(tname));
        } else if (spa == 137) {
            // SE_LimitEffect: base>=0 → include, base<0 → exclude
            int eid = std::abs(base);
            auto it = SPA_EFF_NAMES.find(eid);
            QString ename = (it != SPA_EFF_NAMES.end()) ? it->second : QString("SPA %1").arg(eid);
            parts << (base < 0
                ? QString("Exclude: Effect(%1)").arg(ename)
                : QString("Limit: Effect(%1)").arg(ename));
        } else if (spa == 138) {
            // SE_LimitSpellType
            if (base == 0)      parts << "Limit: Spell Type (Detrimental only)";
            else if (base == 1) parts << "Limit: Spell Type (Beneficial only)";
            else                parts << QString("Limit: Spell Type (%1)").arg(base);
        } else if (spa == 139) {
            // SE_LimitSpell — display as "Limit: Spell(name)" per EQ client convention
            int sid = std::abs(base);
            auto it = spellNames.find(sid);
            QString sname = (it != spellNames.end()) ? it->second : QString("#%1").arg(sid);
            parts << QString("Limit: Spell(%1)").arg(sname);
        } else if (spa == 140) {
            // SE_LimitMinDur — base is in ticks (6s each)
            parts << QString("Limit: Min Duration (%1 ticks)").arg(base);
        } else if (spa == 141) {
            // SE_LimitInstant
            if (base == 1)      parts << "Limit: Instant spells only";
            else if (base == 0) parts << "Limit: Buff spells only";
        } else if (spa == 142) {
            // SE_LimitMinLevel
            parts << QString("Limit: Min Level (%1)").arg(base);
        } else if (spa == 143) {
            // SE_LimitCastTimeMin — base in ms
            parts << QString("Limit: Cast Time Min (%1ms)").arg(base);
        } else if (spa == 144) {
            // SE_LimitCastTimeMax — base in ms
            parts << QString("Limit: Cast Time Max (%1ms)").arg(base);
        } else {
            auto boolIt = SPA_BOOL.find(spa);
            if (boolIt != SPA_BOOL.end()) {
                parts << QString(boolIt->second);
            } else {
                auto descIt = SPA_DESC.find(spa);
                if (descIt != SPA_DESC.end() && base != 0) {
                    const char* verb = base > 0 ? "Increase" : "Decrease";
                    int val = (mx != 0 && formula != 100) ? std::abs(mx) : std::abs(base);
                    QString sfx = SPA_PCT.count(spa) ? "%" : "";
                    parts << QString("%1 %2 by %3%4").arg(verb).arg(descIt->second).arg(val).arg(sfx);
                }
            }
        }
    }

    QString header = QString("<b>%1</b>").arg(QString::fromStdString(spell.name));
    if (parts.isEmpty()) return header;
    return header + "<br>" + parts.join("<br>");
}
