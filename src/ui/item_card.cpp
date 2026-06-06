#include "ui/item_card.h"
#include "ui/spell_tooltip.h"
#include "core/npc_analysis.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// ── Static helpers ────────────────────────────────────────────────────────

static int itemStat(const ItemData& it, const std::string& k) {
    if (k=="hp")         return it.hp;
    if (k=="mana")       return it.mana;
    if (k=="ac")         return it.ac;
    if (k=="atk")        return it.atk;
    if (k=="astr")       return it.astr;
    if (k=="asta")       return it.asta;
    if (k=="adex")       return it.adex;
    if (k=="aagi")       return it.aagi;
    if (k=="awis")       return it.awis;
    if (k=="aint")       return it.aint;
    if (k=="acha")       return it.acha;
    if (k=="mr")         return it.mr;
    if (k=="fr")         return it.fr;
    if (k=="cr")         return it.cr;
    if (k=="dr")         return it.dr;
    if (k=="pr")         return it.pr;
    if (k=="haste")      return it.haste;
    if (k=="hp_regen")   return it.hp_regen;
    if (k=="mana_regen") return it.mana_regen;
    return 0;
}

// Skill IDs from EQ::skills::SkillType enum (EQMacEmu common/skills.h).
// Verified against GetSkillTypeMap() in common/skills.cpp.
static const std::map<int,QString> SKILL_NAMES_IC = {
    {0,"1H Blunt"},{1,"1H Slashing"},{2,"2H Blunt"},{3,"2H Slashing"},
    {4,"Abjuration"},{5,"Alteration"},{6,"Apply Poison"},{7,"Archery"},
    {8,"Backstab"},{9,"Bind Wound"},{10,"Bash"},{11,"Block"},
    {12,"Brass Instr."},{13,"Channeling"},{14,"Conjuration"},{15,"Defense"},
    {16,"Disarm"},{17,"Disarm Traps"},{18,"Divination"},{19,"Dodge"},
    {20,"Double Attack"},{21,"Dragon Punch"},{22,"Dual Wield"},{23,"Eagle Strike"},
    {24,"Evocation"},{25,"Feign Death"},{26,"Flying Kick"},{27,"Forage"},
    {28,"Hand to Hand"},{29,"Hide"},{30,"Kick"},{31,"Meditate"},
    {32,"Mend"},{33,"Offense"},{34,"Parry"},{35,"Pick Lock"},
    {36,"1H Piercing"},{37,"Riposte"},{38,"Round Kick"},{39,"Safe Fall"},
    {40,"Sense Heading"},{41,"Singing"},{42,"Sneak"},
    {43,"Spec. Abjur."},{44,"Spec. Alter."},{45,"Spec. Conjur."},
    {46,"Spec. Divin."},{47,"Spec. Evoc."},
    {48,"Pick Pockets"},{49,"Stringed Instr."},{50,"Swimming"},
    {51,"Throwing"},{52,"Tiger Claw"},{53,"Tracking"},{54,"Wind Instr."},
    {55,"Fishing"},{56,"Make Poison"},{57,"Tinkering"},{58,"Research"},
    {59,"Alchemy"},{60,"Baking"},{61,"Tailoring"},{62,"Sense Traps"},
    {63,"Blacksmithing"},{64,"Fletching"},{65,"Brewing"},
    {66,"Alcohol Tolerance"},{67,"Begging"},{68,"Jewelry Making"},
    {69,"Pottery"},{70,"Percussion Instr."},{71,"Intimidation"},
    {72,"Berserking"},{73,"Taunt"},
};

static const std::vector<std::pair<const char*,int>> SLOT_BITS_IC = {
    {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
    {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
    {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
    {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
    {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
    {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
};
static const std::vector<std::pair<const char*,int>> CLASS_BITS_IC = {
    {"WAR",1},{"CLR",2},{"PAL",4},{"RNG",8},{"SHD",16},{"DRU",32},
    {"MNK",64},{"BRD",128},{"ROG",256},{"SHM",512},{"NEC",1024},
    {"WIZ",2048},{"MAG",4096},{"ENC",8192},{"BST",16384},{"BER",32768},
};
static const std::vector<std::pair<const char*,int>> RACE_BITS_IC = {
    {"HUM",1},{"BAR",2},{"ERU",4},{"ELF",8},{"HIE",16},{"DEF",32},
    {"HEF",64},{"DWF",128},{"TRL",256},{"OGR",512},{"HLF",1024},{"GNM",2048},
    {"IKS",4096},{"VAH",8192},{"FRG",16384},
};

// ── makeItemCard ──────────────────────────────────────────────────────────

QFrame* makeItemCard(const ItemData* item, const ItemData* ref,
                     const std::map<int,SpellData>* spells, const QString& titleOverride,
                     const std::map<int,QString>* limitSpellNames)
{
    auto* frame = new QFrame;
    frame->setStyleSheet(
        "QFrame { background: #111c2e; border-radius: 5px; border: 1px solid #2a3a5a; }");

    auto* outerL = new QVBoxLayout(frame);
    outerL->setContentsMargins(0, 0, 0, 0);
    outerL->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setStyleSheet(
        "QWidget { background: #1a2a40; border-radius: 5px 5px 0 0; "
        "border-bottom: 1px solid #2a3a5a; }");
    auto* hL = new QVBoxLayout(header);
    hL->setContentsMargins(10, 7, 10, 7);
    hL->setSpacing(2);

    QString displayName = !titleOverride.isEmpty()
        ? titleOverride
        : (item ? QString::fromStdString(item->name) : "(vide)");
    auto* nameLbl = new QLabel(displayName);
    nameLbl->setStyleSheet(
        "font-weight: bold; font-size: 13px; color: #d0e8ff; "
        "border: none; background: transparent;");
    nameLbl->setWordWrap(true);
    hL->addWidget(nameLbl);

    if (item) {
        QStringList sub;
        if (item->item_slots) {
            QStringList slotNames;
            for (auto& [n,b] : SLOT_BITS_IC) if (item->item_slots & b) slotNames << n;
            if (!slotNames.isEmpty()) sub << slotNames.join(", ");
        }
        if (item->reqlevel > 0) sub << QString("Req. %1").arg(item->reqlevel);
        if (ref && !ref->name.empty())
            sub << QString("vs %1").arg(QString::fromStdString(ref->name));
        if (!sub.isEmpty()) {
            auto* subLbl = new QLabel(sub.join("  \xc2\xb7  "));
            subLbl->setStyleSheet(
                "color: #667788; font-size: 11px; border: none; background: transparent;");
            subLbl->setWordWrap(true);
            hL->addWidget(subLbl);
        }
    }
    outerL->addWidget(header);

    // ── Body ──────────────────────────────────────────────────────────────
    auto* body = new QWidget;
    body->setStyleSheet("QWidget { background: transparent; }");
    auto* bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(10, 8, 10, 10);
    bodyL->setSpacing(1);

    if (!item) {
        auto* lbl = new QLabel(QString::fromUtf8("(vide)"));
        lbl->setStyleSheet(
            "color: #555; font-style: italic; font-size: 14px; "
            "border: none; background: transparent;");
        bodyL->addWidget(lbl);
        bodyL->addStretch();
        outerL->addWidget(body);
        return frame;
    }

    bool firstSection = true;

    auto addSep = [&] {
        if (!firstSection) {
            auto* s = new QFrame;
            s->setFrameShape(QFrame::HLine);
            s->setStyleSheet(
                "QFrame { border: none; border-top: 1px solid #1e2a3a; "
                "margin: 3px 0; background: transparent; }");
            bodyL->addWidget(s);
        }
        firstSection = false;
    };

    auto addTitle = [&](const QString& text, const char* color) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(
            QString("font-size: 11px; color: %1; font-weight: bold; "
                    "letter-spacing: 1px; border: none; background: transparent;")
            .arg(color));
        bodyL->addWidget(lbl);
    };

    // 3-column stat grid: label | value | Δ
    // rows = {label, value_str, delta_str, delta_positive}
    using GridRow = std::tuple<QString,QString,QString,bool>;
    auto addGrid = [&](const std::vector<GridRow>& rows) {
        auto* grid = new QWidget;
        grid->setStyleSheet("background: transparent;");
        auto* gl = new QGridLayout(grid);
        gl->setContentsMargins(0, 2, 0, 2);
        gl->setVerticalSpacing(2);
        gl->setHorizontalSpacing(6);

        for (int r = 0; r < (int)rows.size(); ++r) {
            auto& [lbl, val, delta, dPos] = rows[r];

            auto* lw = new QLabel(lbl);
            lw->setStyleSheet("color: #667788; font-size: 12px; border: none; background: transparent;");
            gl->addWidget(lw, r, 0);

            auto* vw = new QLabel(val);
            vw->setStyleSheet("color: #c8d8e8; font-size: 13px; font-weight: bold; border: none; background: transparent;");
            vw->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            gl->addWidget(vw, r, 1);

            if (!delta.isEmpty()) {
                QString col = dPos ? "#4caf50" : "#ef5350";
                auto* dw = new QLabel(delta);
                dw->setStyleSheet(
                    QString("color: %1; font-size: 13px; font-weight: bold; "
                            "border: none; background: transparent;").arg(col));
                dw->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                gl->addWidget(dw, r, 2);
            }
        }
        gl->setColumnStretch(0, 3);
        gl->setColumnStretch(1, 1);
        gl->setColumnStretch(2, 1);
        bodyL->addWidget(grid);
    };

    // Generic integer stat section
    struct SD { const char* key, *label; bool inv = false, pct = false; };
    auto addStatSection = [&](const QString& title, const char* color,
                               std::vector<SD> defs)
    {
        std::vector<GridRow> rows;
        for (auto& d : defs) {
            int v = itemStat(*item, d.key);
            int r = ref ? itemStat(*ref, d.key) : 0;
            int delta = v - r;
            if (v == 0 && delta == 0) continue;
            QString vs = d.pct ? QString("+%1%").arg(v) : QString::number(v);
            QString ds;
            bool dpos = true;
            if (ref && delta != 0) {
                int sign = d.inv ? -delta : delta;
                dpos = sign > 0;
                ds = (delta > 0 ? "+" : "") + (d.pct ? QString("%1%").arg(delta)
                                                      : QString::number(delta));
            }
            rows.emplace_back(d.label, vs, ds, dpos);
        }
        if (rows.empty()) return;
        addSep();
        addTitle(title, color);
        addGrid(rows);
    };

    // ── COMBAT ────────────────────────────────────────────────────────────
    addStatSection("COMBAT", "#64b5f6", {
        {"hp","HP"}, {"mana","Mana"}, {"ac","AC"}, {"atk","ATK"},
        {"haste","Haste",false,true},
        {"hp_regen","HP/tick"}, {"mana_regen","Mana/tick"},
    });

    // ── ARME (weapon) ─────────────────────────────────────────────────────
    if (item->damage > 0 && item->delay > 0) {
        static const std::map<int,const char*> WTYPES = {
            {0,"1H Slash"},{1,"2H Slash"},{2,"Piercing"},{3,"1H Blunt"},{4,"2H Blunt"},
            {5,"Archery"},{7,"Throwing"},{8,"Shield"},{10,"2H Pierce"},{11,"H2H"},{35,"Bow"},
        };
        auto wt = WTYPES.find(item->itemtype);
        QString wtitle = QString("ARME  %1").arg(
            wt != WTYPES.end() ? wt->second : QString("type %1").arg(item->itemtype));

        int   rDmg   = ref ? ref->damage : 0;
        int   rDly   = ref ? ref->delay  : 0;
        float ratio  = item->damage / (item->delay / 10.f);
        float rRatio = (rDmg && rDly) ? rDmg / (rDly / 10.f) : 0.f;

        std::vector<GridRow> rows;
        // Dégâts
        {
            QString ds;
            bool dpos = true;
            if (ref && rDmg) {
                int d = item->damage - rDmg;
                ds   = (d > 0 ? "+" : "") + QString::number(d);
                dpos = d > 0;
            }
            rows.emplace_back("D\xc3\xa9g\xc3\xa2ts", QString::number(item->damage), ds, dpos);
        }
        // Délai
        {
            QString ds;
            bool dpos = true;
            if (ref && rDly) {
                int d = item->delay - rDly;
                ds   = (d > 0 ? "+" : "") + QString::number(d) + "s";
                dpos = d < 0; // lower = better
            }
            rows.emplace_back("D\xc3\xa9lai",
                QString("%1s").arg(item->delay / 10.0, 0, 'f', 1), ds, dpos);
        }
        // Ratio
        {
            QString ds;
            bool dpos = true;
            if (ref && rRatio > 0.f) {
                float d = ratio - rRatio;
                ds   = (d > 0 ? "+" : "") + QString::number(d, 'f', 2);
                dpos = d > 0;
            }
            rows.emplace_back("Ratio", QString::number(ratio, 'f', 2), ds, dpos);
        }
        addSep();
        addTitle(wtitle, "#ffd54f");
        addGrid(rows);
    }

    // ── ATTRIBUTS ─────────────────────────────────────────────────────────
    addStatSection("ATTRIBUTS", "#ffb74d", {
        {"astr","FOR"}, {"asta","CON"}, {"adex","DEX"}, {"aagi","AGI"},
        {"awis","SAG"}, {"aint","INT"}, {"acha","CHA"},
    });

    // ── RÉSISTS ───────────────────────────────────────────────────────────
    addStatSection("R\xc3\x89SISTS", "#ef5350", {
        {"mr","MR"}, {"fr","FR"}, {"cr","CR"}, {"dr","DR"}, {"pr","PR"},
    });

    // ── EFFETS ────────────────────────────────────────────────────────────
    {
        bool hadEffect = false;
        auto addEff = [&](int id, const std::string& name, const char* pfx, const char* col) {
            if (id <= 0 || name.empty()) return;
            if (!hadEffect) {
                addSep();
                addTitle("EFFETS", "#ba68c8");
                hadEffect = true;
            }
            auto* lbl = new QLabel(
                QString("<span style='color:%1;font-weight:bold;'>%2</span>"
                        "<span style='color:#aaa;'> %3</span>")
                .arg(col).arg(pfx).arg(QString::fromStdString(name)));
            lbl->setTextFormat(Qt::RichText);
            lbl->setStyleSheet(
                "font-size: 13px; font-style: italic; "
                "border: none; background: transparent;");
            lbl->setWordWrap(true);
            if (spells) {
                auto it = spells->find(id);
                if (it != spells->end()) {
                    static const std::map<int,QString> kEmpty;
                    const auto& names = limitSpellNames ? *limitSpellNames : kEmpty;
                    QString tip = formatSpellTooltip(it->second, 0, names);
                    if (!tip.isEmpty())
                        lbl->setToolTip(tip);
                }
            }
            bodyL->addWidget(lbl);
        };

        addEff(item->worneffect,   item->worneffect_name,   "Worn",   "#8888ff");
        addEff(item->focuseffect,  item->focuseffect_name,  "Focus",  "#88cc88");
        if (item->damage > 0)
            addEff(item->proceffect, item->proceffect_name, "Proc",   "#ffaa44");
        addEff(item->clickeffect,  item->clickeffect_name,  "Click",  "#ba68c8");
        addEff(item->scrolleffect, item->scrolleffect_name, "Scroll", "#ba68c8");

        // Skill mod
        if (item->skillmodtype >= 0 && item->skillmodvalue != 0) {
            if (!hadEffect) {
                addSep();
                addTitle("EFFETS", "#ba68c8");
                hadEffect = true;
            }
            auto it = SKILL_NAMES_IC.find(item->skillmodtype);
            QString sname = it != SKILL_NAMES_IC.end()
                ? it->second
                : QString("Skill %1").arg(item->skillmodtype);
            auto* lbl = new QLabel(
                QString("<span style='color:#ffb74d;font-weight:bold;'>Skill</span>"
                        "<span style='color:#aaa;'> %1 %2%3</span>")
                .arg(sname)
                .arg(item->skillmodvalue > 0 ? "+" : "")
                .arg(item->skillmodvalue));
            lbl->setTextFormat(Qt::RichText);
            lbl->setStyleSheet("font-size: 13px; border: none; background: transparent;");
            bodyL->addWidget(lbl);
        }
    }

    // ── SORT (click/scroll spell details inline) ──────────────────────────
    {
        const SpellData* clickSpell = nullptr;
        if (spells) {
            int id = item->clickeffect > 0 ? item->clickeffect : item->scrolleffect;
            if (id > 0) {
                auto it = spells->find(id);
                if (it != spells->end()) clickSpell = &it->second;
            }
        }
        if (clickSpell) {
            static const std::map<int,const char*> RESIST_LBL = {
                {1,"MR"},{2,"FR"},{3,"CR"},{4,"PR"},{5,"DR"},
                {6,"Chromatic"},{7,"Prismatic"},
            };
            addSep();
            addTitle(QString("SORT  %1").arg(QString::fromStdString(clickSpell->name)),
                     "#7e57c2");

            QStringList meta;
            auto rit = RESIST_LBL.find(clickSpell->resist_type);
            if (rit != RESIST_LBL.end())
                meta << QString("Resist: %1").arg(rit->second);
            meta << QString("Cible: %1").arg(
                QString::fromStdString(targetTypeLabel(clickSpell->targettype)));
            auto* metaLbl = new QLabel(meta.join("  \xc2\xb7  "));
            metaLbl->setStyleSheet("color: #555577; font-size: 11px; background: transparent;");
            bodyL->addWidget(metaLbl);

            QString summary = QString::fromStdString(formatSpellSummary(*clickSpell, 0));
            if (summary.isEmpty()) summary = "\xe2\x80\x94";
            auto* effLbl = new QLabel(summary);
            effLbl->setStyleSheet("color: #aaaaaa; font-size: 13px; background: transparent;");
            effLbl->setWordWrap(true);
            bodyL->addWidget(effLbl);
        }
    }

    // ── Classes / Races ───────────────────────────────────────────────────
    {
        // Classes : 15 pre-GoD (no Berserker) → 32767; covers 32767 and 65535
        constexpr int ALL_CLS = (1 << 15) - 1;  // 32767
        // Races : 14 Luclin-era races (Human→Vah Shir, no Froglok) → 16383
        constexpr int ALL_RAC = (1 << 14) - 1;  // 16383

        QString clsTxt, racTxt;
        if (!item->classes || (item->classes & ALL_CLS) == ALL_CLS)
            clsTxt = "All classes";
        else {
            QStringList l;
            for (auto& [n,b] : CLASS_BITS_IC) if (item->classes & b) l << n;
            clsTxt = l.join(" ");
        }
        if (!item->races || (item->races & ALL_RAC) == ALL_RAC)
            racTxt = "All races";
        else {
            QStringList l;
            for (auto& [n,b] : RACE_BITS_IC) if (item->races & b) l << n;
            racTxt = l.join(" ");
        }

        auto* s = new QFrame;
        s->setFrameShape(QFrame::HLine);
        s->setStyleSheet("QFrame { border: none; border-top: 1px solid #1e2a3a; margin: 3px 0; background: transparent; }");
        bodyL->addWidget(s);
        auto* lbl = new QLabel(clsTxt + "  \xc2\xb7  " + racTxt);
        lbl->setStyleSheet(
            "color: #445566; font-size: 11px; border: none; background: transparent;");
        lbl->setWordWrap(true);
        bodyL->addWidget(lbl);
    }

    bodyL->addStretch();
    outerL->addWidget(body);
    return frame;
}
