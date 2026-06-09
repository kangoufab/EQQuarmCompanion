#include "ui/item_tooltip.h"
#include "ui/palette.h"
#include <QStringList>
#include <map>
#include <utility>
#include <vector>

QString formatItemTooltip(const ItemData& item, const QString& accentColor)
{
    static const std::vector<std::pair<const char*,int>> SLOT_BITS_IT = {
        {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
        {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
        {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
        {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
        {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
        {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
    };
    static const std::map<int,const char*> WTYPES_IT = {
        {0,"1H Slash"},{1,"2H Slash"},{2,"Piercing"},{3,"1H Blunt"},{4,"2H Blunt"},
        {5,"Archery"},{7,"Throwing"},{8,"Shield"},{10,"2H Pierce"},{11,"H2H"},{35,"Bow"},
    };

    QString name = QString::fromStdString(item.name);
    QString html = QString("<span style='color:%1;font-weight:bold;font-size:13px;"
                           "text-decoration:none;'>%2</span>")
                   .arg(accentColor, name.toHtmlEscaped());

    QStringList sub;
    if (item.item_slots) {
        QStringList slotNames;
        for (auto& [n,b] : SLOT_BITS_IT) if (item.item_slots & b) slotNames << n;
        if (!slotNames.isEmpty()) sub << slotNames.join(", ");
    }
    if (item.reqlevel > 0) sub << QString("Req. %1").arg(item.reqlevel);
    if (!sub.isEmpty())
        html += QString("<div style='color:%1;font-size:11px;margin-top:2px;'>%2</div>")
                .arg(kTextSecondary, sub.join("  \xc2\xb7  "));

    struct Row  { QString label, value; };
    struct Stat { const char* label; int value; bool pct = false; };
    struct Section { QString title; const char* color; std::vector<Row> rows; };

    auto statRows = [&](std::vector<Stat> defs) {
        std::vector<Row> rows;
        for (auto& d : defs)
            if (d.value != 0)
                rows.push_back({d.label, d.pct ? QString("+%1%").arg(d.value)
                                                : QString::number(d.value)});
        return rows;
    };

    std::vector<Section> sections;
    sections.push_back({"COMBAT", kAccentBlue, statRows({
        {"HP", item.hp}, {"Mana", item.mana}, {"AC", item.ac}, {"ATK", item.atk},
        {"Haste", item.haste, true},
        {"HP/tick", item.hp_regen}, {"Mana/tick", item.mana_regen},
    })});

    if (item.damage > 0 && item.delay > 0) {
        auto wt = WTYPES_IT.find(item.itemtype);
        QString wtitle = QString("ARME \xc2\xb7 %1").arg(
            wt != WTYPES_IT.end() ? wt->second : QString("type %1").arg(item.itemtype));
        float ratio = item.damage / (item.delay / 10.f);
        sections.push_back({wtitle, kAccentGold, {
            {"D\xc3\xa9g\xc3\xa2ts", QString::number(item.damage)},
            {"D\xc3\xa9lai", QString("%1s").arg(item.delay / 10.0, 0, 'f', 1)},
            {"Ratio", QString::number(ratio, 'f', 2)},
        }});
    }

    sections.push_back({"ATTRIBUTS", kOrange, statRows({
        {"FOR", item.astr}, {"CON", item.asta}, {"DEX", item.adex}, {"AGI", item.aagi},
        {"SAG", item.awis}, {"INT", item.aint}, {"CHA", item.acha},
    })});

    sections.push_back({"R\xc3\x89SISTS", kRed, statRows({
        {"MR", item.mr}, {"FR", item.fr}, {"CR", item.cr}, {"DR", item.dr}, {"PR", item.pr},
    })});

    std::vector<Row> effRows;
    auto addEff = [&](int id, const std::string& effName, const char* pfx, const char* col) {
        if (id <= 0 || effName.empty()) return;
        effRows.push_back({
            QString("<span style='color:%1;font-weight:bold;'>%2</span>"
                    "<span style='color:%3;'> %4</span>")
            .arg(col, pfx, kHtmlLabel, QString::fromStdString(effName).toHtmlEscaped()),
            QString()
        });
    };
    addEff(item.worneffect,   item.worneffect_name,   "Worn",   kAccentWorn);
    addEff(item.focuseffect,  item.focuseffect_name,  "Focus",  kAccentFocus);
    if (item.damage > 0)
        addEff(item.proceffect, item.proceffect_name, "Proc",   kAccentProc);
    addEff(item.clickeffect,  item.clickeffect_name,  "Click",  kAccentPurple);
    addEff(item.scrolleffect, item.scrolleffect_name, "Scroll", kAccentPurple);
    if (!effRows.empty())
        sections.push_back({"EFFETS", kAccentPurple, std::move(effRows)});

    bool any = false;
    for (auto& s : sections) if (!s.rows.empty()) { any = true; break; }
    if (!any) return html;

    html += "<table cellspacing='0' style='min-width:220px;border-collapse:collapse;"
            "margin-top:5px;'>";
    for (auto& s : sections) {
        if (s.rows.empty()) continue;
        html += QString("<tr><td colspan='2' style='color:%1;font-size:11px;font-weight:bold;"
                        "padding:4px 0 2px;'>\xe2\x94\x80 %2</td></tr>")
                .arg(s.color, s.title);
        for (auto& r : s.rows) {
            if (r.value.isEmpty()) {
                html += QString("<tr><td colspan='2' style='padding:1px 4px 1px 10px;"
                                "font-style:italic;font-size:12px;'>%1</td></tr>")
                        .arg(r.label);
            } else {
                html += QString("<tr><td style='color:%1;padding:1px 20px 1px 10px;'>%2</td>"
                                "<td align='right' style='color:%3;font-weight:bold;'>%4</td></tr>")
                        .arg(kHtmlLabel, r.label, accentColor, r.value);
            }
        }
    }
    html += "</table>";
    return html;
}
