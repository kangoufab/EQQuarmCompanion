#include "ui/spell_tooltip.h"
#include "ui/palette.h"
#include "core/spell_stats.h"
#include <QString>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

QString formatSpellTooltip(const SpellData& spell, int level,
                            const std::map<int,QString>& spellNames,
                            const QString& accentColor,
                            const SpellNameResolver& nameResolver)
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
    static const std::map<int, const char*> SPA_EFF_NAMES = {
        {0,"Hitpoints"}, {1,"AC"}, {2,"ATK"}, {3,"Movement Speed"},
        {4,"STR"}, {5,"DEX"}, {6,"AGI"}, {7,"STA"}, {8,"INT"}, {9,"WIS"},
        {10,"CHA"}, {11,"Haste"},
        {15,"Mana"}, {21,"Stun"}, {22,"Charm"}, {23,"Fear"},
        {32,"Summon Item"}, {33,"Summon Pet"},
        {35,"Disease Counter"}, {36,"Poison Counter"},
        {40,"Divine Aura"},
        {46,"Fire Resistance"}, {47,"Cold Resistance"},
        {48,"Poison Resistance"}, {49,"Disease Resistance"}, {50,"Magic Resistance"},
        {55,"Rune"}, {57,"Levitate"}, {59,"Damage Shield"}, {69,"Max HP"},
        {71,"Necro Pet"}, {79,"HP Increase"}, {85,"Weapon Proc"}, {92,"Hate"},
        {97,"Max Mana"}, {98,"Haste v2"}, {99,"Root"}, {100,"Heal Over Time"},
        {101,"Complete Heal"}, {111,"All Resist"}, {116,"Curse Counter"},
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

    struct Row { QString label, value; };
    std::vector<Row> eff, cond;

    auto addEff  = [&](const QString& lbl, const QString& val) { eff .push_back({lbl, val}); };
    auto addCond = [&](const QString& lbl, const QString& val) { cond.push_back({lbl, val}); };

    for (int i = 0; i < 12; ++i) {
        int spa     = spell.spa[i];
        int base    = spell.effect_base_value[i];
        int mx      = spell.effect_limit_value[i];
        int formula = spell.effect_formula[i];

        if (spa == 254) break;
        if (spa == 10)  continue;

        if (spa == 11) {
            int val = level ? calcSpellEffectValue(base, mx, formula, level) : (mx ? std::min(base, mx) : base);
            if (val != 0) addEff("Haste", QString("+%1%").arg(val - 100));
        } else if (spa == 98) {
            if (base != 0) addEff("Haste", QString("+%1%").arg(std::abs(base) - 100));
        } else if (spa == 119) {
            int val = level ? calcSpellEffectValue(base, mx, formula, level) : std::abs(base);
            addEff("Haste V3", QString("+%1%").arg(val));
        } else if (spa == 0) {
            if (base > 0) {
                int val = level ? calcSpellEffectValue(base, mx, formula, level) : (mx ? std::min(base, mx) : base);
                addEff("HP Regen", QString("+%1/tick").arg(val));
            } else if (base < 0) {
                addEff("HP", QString::number(base));
            }
        } else if (spa == 15) {
            if (base != 0) {
                if (base > 0) addEff("Mana", QString("+%1/tick").arg(base));
                else          addEff("Mana", QString::number(base));
            }
        } else if (spa == 21) {
            if (base > 0) addEff("Stun", QString("%1 sec").arg(base / 1000));
        } else if (spa == 79) {
            if (base != 0) {
                int val = (mx != 0 && formula != 100) ? std::abs(mx) : std::abs(base);
                addEff("HP", base < 0 ? QString("-%1").arg(val) : QString("+%1").arg(val));
            }
        } else if (spa == 92) {
            if (base != 0)
                addEff("Hate", base > 0 ? QString("+%1").arg(base) : QString::number(base));
        } else if (spa == 100) {
            if (base != 0) addEff("HP Regen", QString("+%1/tick").arg(base));
        } else if (spa >= 120 && spa <= 133) {
            auto it = SPA_DESC.find(spa);
            if (it != SPA_DESC.end() && base != 0) {
                QString val = (spa == 131 || spa == 132)
                    ? QString("-%1%").arg(std::abs(base))
                    : QString("+%1%").arg(std::abs(base));
                addEff(it->second, val);
            }
        } else if (spa == 134) {
            addCond("Max Level", QString::number(base));
        } else if (spa == 135) {
            int rid = std::abs(base);
            auto it = RESIST_NAMES.find(rid);
            QString rname = (it != RESIST_NAMES.end()) ? it->second : QString("Resist %1").arg(rid);
            addCond(base < 0 ? "Excl. Resist" : "Resist", rname);
        } else if (spa == 136) {
            int tid = std::abs(base);
            auto it = TARGET_NAMES.find(tid);
            QString tname = (it != TARGET_NAMES.end()) ? it->second : QString("Target %1").arg(tid);
            addCond(base < 0 ? "Excl. Target" : "Target", tname);
        } else if (spa == 137) {
            int eid = std::abs(base);
            auto it = SPA_EFF_NAMES.find(eid);
            QString ename = (it != SPA_EFF_NAMES.end()) ? it->second : QString("SPA %1").arg(eid);
            addCond(base < 0 ? "Excl. Effect" : "Effect", ename);
        } else if (spa == 138) {
            if (base == 0)      addCond("Spell Type", "Detrimental");
            else if (base == 1) addCond("Spell Type", "Beneficial");
            else                addCond("Spell Type", QString::number(base));
        } else if (spa == 139) {
            int sid = std::abs(base);
            QString sname;
            auto it = spellNames.find(sid);
            if (it != spellNames.end())
                sname = it->second;
            else if (nameResolver)
                sname = nameResolver(sid);
            addCond(base < 0 ? "Excl. Spell" : "Limit Spell",
                    sname.isEmpty() ? QString("#%1").arg(sid) : sname);
        } else if (spa == 140) {
            addCond("Min Duration", QString("%1 ticks (%2 sec)").arg(base).arg(base * 6));
        } else if (spa == 141) {
            addCond(base == 1 ? "Instant only" : "Buff spells only", "oui");
        } else if (spa == 142) {
            addCond("Min Level", QString::number(base));
        } else if (spa == 143) {
            addCond("Cast Time Min", QString("%1 ms").arg(base));
        } else if (spa == 144) {
            addCond("Cast Time Max", QString("%1 ms").arg(base));
        } else {
            auto boolIt = SPA_BOOL.find(spa);
            if (boolIt != SPA_BOOL.end()) {
                eff.push_back({boolIt->second, ""});
            } else {
                auto descIt = SPA_DESC.find(spa);
                if (descIt != SPA_DESC.end() && base != 0) {
                    int val = (mx != 0 && formula != 100) ? std::abs(mx) : std::abs(base);
                    QString sfx = SPA_PCT.count(spa) ? "%" : "";
                    QString sign = base > 0 ? "+" : "-";
                    addEff(descIt->second, QString("%1%2%3").arg(sign).arg(val).arg(sfx));
                }
            }
        }
    }

    QString spellName = QString::fromStdString(spell.name);
    QString html = QString("<span style='color:%1;font-weight:bold;font-size:13px;"
                           "text-decoration:none;'>%2</span>")
                   .arg(accentColor, spellName.toHtmlEscaped());

    if (eff.empty() && cond.empty())
        return html;

    html += "<table cellspacing='0' style='min-width:200px;border-collapse:collapse;"
            "margin-top:5px;'>";

    if (!eff.empty()) {
        html += QString("<tr><td colspan='2' style='color:%1;font-size:11px;font-weight:bold;"
                        "padding:4px 0 2px;'>─ EFFETS</td></tr>")
                .arg(accentColor);
        for (auto& r : eff) {
            html += "<tr>";
            if (r.value.isEmpty()) {
                html += QString("<td colspan='2' style='color:%1;padding:1px 4px 1px 10px;'>"
                                "%2</td>")
                        .arg(accentColor, r.label.toHtmlEscaped());
            } else {
                html += QString("<td style='color:%1;padding:1px 20px 1px 10px;'>%2</td>"
                                "<td align='right' style='color:%3;font-weight:bold;'>%4</td>")
                        .arg(kHtmlLabel, r.label.toHtmlEscaped(), accentColor, r.value.toHtmlEscaped());
            }
            html += "</tr>";
        }
    }

    if (!cond.empty()) {
        html += QString("<tr><td colspan='2' style='color:%1;font-size:11px;font-weight:bold;"
                        "padding:6px 0 2px;'>─ CONDITIONS</td></tr>").arg(kHtmlCondHeader);
        for (auto& r : cond) {
            html += QString("<tr>"
                            "<td style='color:%1;padding:1px 20px 1px 10px;'>%2</td>"
                            "<td align='right' style='color:%3;'>%4</td>"
                            "</tr>")
                    .arg(kHtmlCondLabel, r.label.toHtmlEscaped(), kHtmlCondValue, r.value.toHtmlEscaped());
        }
    }

    html += "</table>";
    return html;
}
