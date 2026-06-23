// buildStatTooltip : decomposition HTML par source d'un stat (BASE / ITEMS / FORMULE).
// Extrait de character_tab.cpp (god-file). Definit une methode membre de CharacterTab ;
// header et API inchanges, donc AUTOMOC non impacte.
#include "ui/character_tab.h"
#include "ui/character_tab_internal.h"
#include "ui/stat_categories.h"
#include "ui/palette.h"
#undef slots   // macro Qt : conflit avec des noms locaux

#include <QString>
#include <algorithm>
#include <string>
#include <utility>
QString CharacterTab::buildStatTooltip(const std::string& stat,
                                        int dispVal, bool hasCap, int cap,
                                        const PlayerTotals& totals) const
{
    // Couleur d'accent selon la catégorie du stat
    const char* catAccent = kTextBase;
    for (auto& [catName, catStats] : STAT_CATEGORIES) {
        for (auto& s : catStats) {
            if (s == stat) {
                auto it = CAT_COLORS.find(catName);
                if (it != CAT_COLORS.end()) { catAccent = it->second.accent; break; }
            }
        }
    }

    QString name = QString::fromStdString(STAT_LABELS.count(stat) ? STAT_LABELS.at(stat) : stat);
    QString sfx  = QString::fromStdString(STAT_SUFFIX.count(stat) ? STAT_SUFFIX.at(stat) : "");

    // Valeur brute (avant cap) pour les stats capped
    int rawVal = dispVal;
    if      (stat == "astr") rawVal = totals.str_v;
    else if (stat == "asta") rawVal = totals.sta;
    else if (stat == "adex") rawVal = totals.dex;
    else if (stat == "aagi") rawVal = totals.agi;
    else if (stat == "awis") rawVal = totals.wis;
    else if (stat == "aint") rawVal = totals.int_v;
    else if (stat == "acha") rawVal = totals.cha;
    else if (stat == "mr")   rawVal = totals.mr;
    else if (stat == "fr")   rawVal = totals.fr;
    else if (stat == "cr")   rawVal = totals.cr;
    else if (stat == "dr")   rawVal = totals.dr;
    else if (stat == "pr")   rawVal = totals.pr;

    // ── Constructeurs de lignes HTML ──────────────────────────────────────
    auto secRow = [](const char* lbl, const char* color) -> QString {
        return QString(
            "<tr><td colspan='2' style='color:%1;font-size:13px;font-weight:bold;"
            "padding-top:5px;'>\xe2\x94\x80 %2</td></tr>")
            .arg(color).arg(lbl);
    };
    auto valRow = [](const QString& lbl, const QString& val,
                     const char* vc, bool italic = false) -> QString {
        QString ls = italic ? QString("color:%1;font-style:italic;").arg(kTextSecondary)
                            : QString("color:%1;").arg(kTextSecondary);
        QString vs = italic ? QString("color:%1;font-style:italic;").arg(vc)
                            : QString("color:%1;").arg(vc);
        return QString(
            "<tr>"
            "<td style='padding-left:10px;padding-right:12px;'>"
            "<span style='%1'>%2</span></td>"
            "<td align='right'><span style='%3'>%4</span></td>"
            "</tr>")
            .arg(ls).arg(lbl).arg(vs).arg(val);
    };

    // ── Ligne d'en-tête : Nom + Valeur / Cap ─────────────────────────────
    QString headerVal;
    if (hasCap) {
        headerVal = QString(
            "<span style='color:%1;font-weight:bold;'>%2%3</span>"
            "<span style='color:%5;'> / %4%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx).arg(cap).arg(kTextDim);
        if (rawVal > cap)
            headerVal += QString("<span style='color:%1;'> (brut %2)</span>").arg(kTextTileKey).arg(rawVal);
    } else {
        headerVal = QString("<span style='color:%1;font-weight:bold;'>%2%3</span>")
            .arg(catAccent).arg(dispVal).arg(sfx);
    }

    QString rows = QString(
        "<tr>"
        "<td><span style='color:%1;font-weight:bold;font-size:14px;'>%2</span></td>"
        "<td align='right' style='font-size:14px;'>%3</td>"
        "</tr>")
        .arg(catAccent).arg(name).arg(headerVal);

    // ── Décomposition par stat ────────────────────────────────────────────

    if (isAttrStat(stat)) {
        // BASE : valeur de base du personnage
        int baseVal = 0;
        if (_charInfo) {
            if      (stat == "astr") baseVal = _charInfo->base_str;
            else if (stat == "asta") baseVal = _charInfo->base_sta;
            else if (stat == "adex") baseVal = _charInfo->base_dex;
            else if (stat == "aagi") baseVal = _charInfo->base_agi;
            else if (stat == "awis") baseVal = _charInfo->base_wis;
            else if (stat == "aint") baseVal = _charInfo->base_int;
            else if (stat == "acha") baseVal = _charInfo->base_cha;
        }
        rows += secRow("BASE", kAccentBlue);
        rows += valRow("Personnage", QString::number(baseVal), kAccentBlue);

        // ITEMS
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            QString sign = v >= 0 ? "+" : "";
            rows += valRow(QString::fromStdString(item.name),
                           sign + QString::number(v), kGreen);
        }

    } else if (isResistStat(stat)) {
        // Somme items
        int itemSum = 0;
        for (auto& [s, item] : _equippedItems) itemSum += getItemStat(stat, item);
        int baseVal = rawVal - itemSum;

        rows += secRow("BASE", kAccentBlue);
        QString baseLbl = _charInfo
            ? QString("Race + %1 L%2")
                  .arg(QString::fromStdString(_charInfo->class_name))
                  .arg(_charInfo->level)
            : "Race + Classe";
        rows += valRow(baseLbl, QString::number(baseVal), kAccentBlue);

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            int v = getItemStat(stat, item);
            if (v == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (v >= 0 ? "+" : "") + QString::number(v), kGreen);
        }

    } else if (stat == "hp") {
        int itemHpSum = 0;
        for (auto& [s, item] : _equippedItems) itemHpSum += item.hp;
        int baseHp = totals.hp.base - itemHpSum;

        rows += secRow("BASE", kAccentBlue);
        if (_charInfo)
            rows += valRow(
                QString("%1 L%2 (STA %3)")
                    .arg(QString::fromStdString(_charInfo->class_name))
                    .arg(_charInfo->level)
                    .arg(std::min(totals.sta, 255)),
                QString::number(baseHp), kAccentBlue);

        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.hp >= 0 ? "+" : "") + QString::number(item.hp), kGreen);
        }
        if (itemHpSum > 2000) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow("Items brut", QString::number(itemHpSum),  kOrange, true);
            rows += valRow("Plafond items (RuleI ItemHPCap)", "+2000", kOrange, true);
        }

    } else if (stat == "mana") {
        int itemManaSum = 0;
        for (auto& [s, item] : _equippedItems) itemManaSum += item.mana;
        int baseMana = totals.mana.base - itemManaSum;

        rows += secRow("BASE", kAccentBlue);
        if (_charInfo) {
            bool isInt = (_charInfo->class_name == "Wizard"     ||
                          _charInfo->class_name == "Magician"   ||
                          _charInfo->class_name == "Enchanter"  ||
                          _charInfo->class_name == "Necromancer");
            int primStat = isInt ? std::min(totals.int_v, 255) : std::min(totals.wis, 255);
            rows += valRow(
                QString("%1 L%2 (%3 %4)")
                    .arg(QString::fromStdString(_charInfo->class_name))
                    .arg(_charInfo->level)
                    .arg(isInt ? "INT" : "SAG")
                    .arg(primStat),
                QString::number(baseMana), kAccentBlue);
        }
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           (item.mana >= 0 ? "+" : "") + QString::number(item.mana), kGreen);
        }
        if (itemManaSum > 2000) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow("Items brut", QString::number(itemManaSum), kOrange, true);
            rows += valRow("Plafond items (RuleI ItemManaCap)", "+2000", kOrange, true);
        }

    } else if (stat == "atk") {
        int itemAtkSum = 0;
        for (auto& [s, item] : _equippedItems) itemAtkSum += item.atk;
        int cappedAtk = std::min(itemAtkSum, 250);
        int totalStr = _charInfo ? std::min(totals.str_v, 255) : 0;
        int strBonus = totalStr >= 75 ? (2 * totalStr - 150) / 3 : 0;

        rows += secRow("ITEMS ATK", kGreen);
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.atk == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.atk), kGreen);
        }
        if (!hasItems)
            rows += valRow("(aucun item ATK)", "0", kTextDim);

        rows += secRow("FORMULE", kOrange);
        rows += valRow("Items ATK (cap 250)",  QString::number(cappedAtk),       kOrange, true);
        rows += valRow(QString("Bonus FOR (%1)").arg(totalStr),
                       QString("+%1").arg(strBonus),                             kOrange, true);
        rows += valRow("Affich\xc3\xa9" " = (toHit + offense) \xc3\x97 1000/744",
                       QString::number(totals.atk),                              kOrange, true);

    } else if (stat == "ac") {
        int itemAcSum = 0;
        for (auto& [s, item] : _equippedItems) itemAcSum += item.ac;

        rows += secRow("ITEMS AC", kGreen);
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.ac == 0) continue;
            hasItems = true;
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1").arg(item.ac), kGreen);
        }
        if (!hasItems)
            rows += valRow("(aucun)", "0", kTextDim);

        rows += secRow("FORMULE", kOrange);
        rows += valRow("AC brut items",           QString::number(itemAcSum),        kOrange, true);
        rows += valRow("Mitigation (soft-capped)",
                       QString::number(totals.mitigation),                          kOrange, true);
        rows += valRow("Avoidance (def+AGI+AA)",
                       QString::number(totals.avoidance),                           kOrange, true);
        rows += valRow("Affich\xc3\xa9" " = (avoid + mit) \xc3\x97 1000/847",
                       QString::number(totals.ac),                                  kOrange, true);

    } else if (stat == "haste") {
        // Max-wins : trouver le gagnant
        int maxVal = 0;
        std::string winnerName;
        for (auto& [s, item] : _equippedItems) {
            if (item.haste > maxVal) { maxVal = item.haste; winnerName = item.name; }
        }
        if (maxVal > 0) {
            rows += secRow("ITEMS (max-wins)", kGreen);
            for (auto& [slotName, item] : _equippedItems) {
                if (item.haste == 0) continue;
                bool winner = (item.haste == maxVal && item.name == winnerName);
                QString lbl = QString::fromStdString(item.name);
                if (winner)
                    lbl += QString(" <span style='color:%1;'>\xe2\x86\x90</span>").arg(kAccentAtCap);
                else
                    lbl += QString(" <span style='color:%1;'>(ignor\xc3\xa9" ")</span>").arg(kTextDim);
                rows += valRow(lbl,
                               QString("+%1%").arg(item.haste),
                               winner ? kAccentAtCap : kTextDim);
            }
        }
        if (_charInfo) {
            rows += secRow("FORMULE", kOrange);
            rows += valRow(QString("Cap niveau %1").arg(_charInfo->level),
                           QString("%1%").arg(totals.haste), kOrange, true);
        }

    } else if (stat == "hp_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.hp_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.hp_regen), kGreen);
        }

    } else if (stat == "mana_regen") {
        bool hasItems = false;
        for (auto& [slotName, item] : _equippedItems) {
            if (item.mana_regen == 0) continue;
            if (!hasItems) { rows += secRow("ITEMS", kGreen); hasItems = true; }
            rows += valRow(QString::fromStdString(item.name),
                           QString("+%1/tick").arg(item.mana_regen), kGreen);
        }
    }

    return QString(
        "<table cellspacing='0' cellpadding='1' style='font-size:14px;min-width:260px;'>"
        "%1"
        "</table>")
        .arg(rows);
}
