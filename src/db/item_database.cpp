#include "db/item_database.h"
#include "db/db_connection.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>
#include <QStringList>
#include <map>
#include <string>
#include <vector>
#undef slots  // Qt macro — conflicts with field names in plain structs

// ── AA effect → stat key (portée de item_database.py) ─────────────────────
static const std::map<int, const char*> AA_EFFECT_TO_STAT = {
    {4,"astr"},{5,"adex"},{6,"aagi"},{7,"asta"},{8,"aint"},{9,"awis"},{10,"acha"},
    {46,"fr"},{47,"cr"},{48,"pr"},{49,"dr"},{50,"mr"},
    {214,"hp"},{259,"ac"},{262,"mana"},
};
static constexpr int ND_SKILL_ID = 107;
static constexpr int PE_SKILL_ID = 279;
static const std::map<int,float> ND_BONUS = {{1,2.0f},{2,5.0f},{3,10.0f}};

// ── SQL helpers ────────────────────────────────────────────────────────────

// Build COALESCE(CASE WHEN sw.effectid{i} = spa THEN sw.col{i} ... END, default)
static QString spaCase(int spa, const QString& col, int defaultVal = 0) {
    QString whens;
    for (int i = 1; i <= 12; ++i)
        whens += " WHEN sw.effectid" + QString::number(i)
               + " = " + QString::number(spa)
               + " THEN sw." + col + QString::number(i);
    return "COALESCE(CASE" + whens + " ELSE " + QString::number(defaultVal)
           + " END, " + QString::number(defaultVal) + ")";
}

// Build COALESCE(CASE WHEN sw.effectid{i} IN (spas) THEN sw.col{i} ... END, default)
static QString spaCaseMulti(const QList<int>& spas, const QString& col, int defaultVal = 0) {
    QStringList ids;
    for (int s : spas) ids << QString::number(s);
    QString inList = ids.join(", ");
    QString whens;
    for (int i = 1; i <= 12; ++i)
        whens += " WHEN sw.effectid" + QString::number(i)
               + " IN (" + inList + ")"
               + " THEN sw." + col + QString::number(i);
    return "COALESCE(CASE" + whens + " ELSE " + QString::number(defaultVal)
           + " END, " + QString::number(defaultVal) + ")";
}

// ── Column list ────────────────────────────────────────────────────────────

static QString buildCols() {
    return
        "i.id, i.Name AS name, i.ac, i.hp, i.mana, i.damage, i.delay, i.itemtype, "
        "i.range AS itemrange, "
        "i.astr, i.asta, i.aagi, i.adex, i.awis, i.aint, i.acha, "
        "i.cr, i.fr, i.dr, i.mr, i.pr, i.slots, i.classes, i.races, i.reqlevel, "
        "i.worneffect, COALESCE(sw.name, '') AS worneffect_name, "
        "i.focuseffect, COALESCE(sf.name, '') AS focuseffect_name, "
        "i.proceffect, COALESCE(sp.name, '') AS proceffect_name, "
        "i.wornlevel, " +
        spaCase(2,  "effect_base_value")    + " AS atk_base, " +
        spaCase(2,  "formula", 100)         + " AS atk_formula, " +
        spaCase(2,  "max")                  + " AS atk_max, " +
        spaCase(11, "effect_base_value")    + " AS haste_base, " +
        spaCase(11, "formula", 100)         + " AS haste_formula, " +
        spaCase(11, "max")                  + " AS haste_max, " +
        spaCaseMulti({0, 100}, "effect_base_value") + " AS hp_regen_base, " +
        spaCaseMulti({0, 100}, "formula", 100)      + " AS hp_regen_formula, " +
        spaCaseMulti({0, 100}, "max")               + " AS hp_regen_max, " +
        spaCase(15, "effect_base_value")    + " AS mana_regen_base, " +
        spaCase(15, "formula", 100)         + " AS mana_regen_formula, " +
        spaCase(15, "max")                  + " AS mana_regen_max";
}

static QString buildFrom() {
    return
        "items i"
        " LEFT JOIN spells_new sw ON sw.id = i.worneffect  AND i.worneffect  > 0"
        " LEFT JOIN spells_new sf ON sf.id = i.focuseffect AND i.focuseffect > 0"
        " LEFT JOIN spells_new sp ON sp.id = i.proceffect  AND i.proceffect  > 0";
}

// ── Map query row to ItemData ──────────────────────────────────────────────

static ItemData rowToItemData(QSqlQuery& q) {
    ItemData item;
    item.id             = q.value("id").toInt();
    item.name           = q.value("name").toString().toStdString();
    item.ac             = q.value("ac").toInt();
    item.hp             = q.value("hp").toInt();
    item.mana           = q.value("mana").toInt();
    item.damage         = q.value("damage").toInt();
    item.delay          = q.value("delay").toInt();
    item.itemtype       = q.value("itemtype").toInt();
    item.astr           = q.value("astr").toInt();
    item.asta           = q.value("asta").toInt();
    item.aagi           = q.value("aagi").toInt();
    item.adex           = q.value("adex").toInt();
    item.awis           = q.value("awis").toInt();
    item.aint           = q.value("aint").toInt();
    item.acha           = q.value("acha").toInt();
    item.cr             = q.value("cr").toInt();
    item.fr             = q.value("fr").toInt();
    item.dr             = q.value("dr").toInt();
    item.mr             = q.value("mr").toInt();
    item.pr             = q.value("pr").toInt();
    item.item_slots     = q.value("slots").toInt();
    item.classes        = q.value("classes").isNull() ? 65535 : q.value("classes").toInt();
    item.reqlevel       = q.value("reqlevel").toInt();
    item.worneffect     = q.value("worneffect").toInt();
    item.focuseffect    = q.value("focuseffect").toInt();
    item.proceffect     = q.value("proceffect").toInt();
    item.worneffect_name  = q.value("worneffect_name").toString().toStdString();
    item.focuseffect_name = q.value("focuseffect_name").toString().toStdString();
    item.proceffect_name  = q.value("proceffect_name").toString().toStdString();

    // Worn effects — formula 100 = fixed value, others = level-scaled (use base as approx)
    auto resolveWorn = [&](const char* baseCol) -> int {
        return q.value(baseCol).toInt(); // base value is sufficient for lv60 display
    };
    item.atk       = resolveWorn("atk_base");
    item.haste     = resolveWorn("haste_base");
    item.hp_regen  = resolveWorn("hp_regen_base");
    item.mana_regen = resolveWorn("mana_regen_base");

    return item;
}

// ── ItemDatabase ───────────────────────────────────────────────────────────

// Noms de classes → index 1-based dans spells_new.classesN
static const std::map<std::string, int> SPELL_CLASS_ID = {
    {"Warrior",1},{"Cleric",2},{"Paladin",3},{"Ranger",4},
    {"Shadowknight",5},{"Druid",6},{"Monk",7},{"Bard",8},
    {"Rogue",9},{"Shaman",10},{"Necromancer",11},{"Wizard",12},
    {"Magician",13},{"Enchanter",14},{"Beastlord",15},{"Berserker",16},
};

static SpellData rowToSpellData(QSqlQuery& q, int minLevelCol = -1) {
    SpellData sd;
    sd.id         = q.value("id").toInt();
    sd.name       = q.value("name").toString().toStdString();
    sd.targettype = q.value("targettype").toInt();
    for (int i = 0; i < 12; ++i) {
        int n = i + 1;
        sd.spa[i]               = q.value(QString("effectid%1").arg(n)).toInt();
        sd.effect_base_value[i] = q.value(QString("effect_base_value%1").arg(n)).toInt();
        sd.effect_limit_value[i]= q.value(QString("max%1").arg(n)).toInt();
        sd.effect_formula[i]    = q.value(QString("formula%1").arg(n)).toInt();
    }
    for (int i = 0; i < 15; ++i)
        sd.classes[i] = q.value(QString("classes%1").arg(i+1)).toInt();
    return sd;
}

ItemDatabase::ItemDatabase(QObject* parent) : QObject(parent) {}

std::optional<ItemData> ItemDatabase::getItemById(int id) {
    QString cols = buildCols();
    QString from = buildFrom();

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        QString("SELECT %1 FROM %2 WHERE i.id = :id LIMIT 1").arg(cols).arg(from)
    );
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "getItemById failed:" << q.lastError().text();
        return std::nullopt;
    }
    if (!q.next()) return std::nullopt;
    return rowToItemData(q);
}

QList<ItemData> ItemDatabase::searchItems(const QString& nameFragment, int limit) {
    QList<ItemData> result;
    QString cols = buildCols();
    QString from = buildFrom();

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        QString("SELECT %1 FROM %2 WHERE i.Name LIKE :name"
                " ORDER BY CHAR_LENGTH(i.Name) ASC LIMIT :lim")
            .arg(cols).arg(from)
    );
    q.bindValue(":name", QString("%%1%").arg(nameFragment));
    q.bindValue(":lim", limit);
    if (!q.exec()) {
        qWarning() << "searchItems failed:" << q.lastError().text();
        return result;
    }
    while (q.next())
        result.append(rowToItemData(q));
    return result;
}

std::optional<SpellData> ItemDatabase::getSpellById(int id) {
    if (id <= 0) return std::nullopt;
    QString effectCols;
    for (int i = 1; i <= 12; ++i)
        effectCols += QString("effectid%1, effect_base_value%1, max%1, formula%1, ").arg(i);
    effectCols.chop(2);
    QString sql = QString(
        "SELECT id, name, targettype, %1 FROM spells_new WHERE id = :id").arg(effectCols);
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(sql);
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return std::nullopt;
    SpellData sd;
    sd.id        = q.value("id").toInt();
    sd.name      = q.value("name").toString().toStdString();
    sd.targettype= q.value("targettype").toInt();
    for (int i = 0; i < 12; ++i) {
        int n = i + 1;
        sd.spa[i]               = q.value(QString("effectid%1").arg(n)).toInt();
        sd.effect_base_value[i] = q.value(QString("effect_base_value%1").arg(n)).toInt();
        sd.effect_limit_value[i]= q.value(QString("max%1").arg(n)).toInt();
        sd.effect_formula[i]    = q.value(QString("formula%1").arg(n)).toInt();
    }
    return sd;
}

AaStats ItemDatabase::getAaStats(const std::vector<std::pair<int,int>>& purchases) {
    AaStats result;
    if (purchases.empty()) return result;

    auto& db = DbConnection::instance().db();

    // ── 1. altadv_vars : eqmacid → {skill_id, max_level} ─────────────────
    QStringList idList;
    for (auto& [id, rank] : purchases) idList << QString::number(id);
    QSqlQuery q1(db);
    q1.exec(QString("SELECT eqmacid, skill_id, max_level FROM altadv_vars"
                    " WHERE eqmacid IN (%1)").arg(idList.join(',')));
    std::map<int, std::pair<int,int>> avMap; // eqmacid → {skill_id, max_level}
    while (q1.next())
        avMap[q1.value(0).toInt()] = {q1.value(1).toInt(), q1.value(2).toInt()};

    // ── 2. Calculer les aaid pour chaque achat ────────────────────────────
    QStringList aaidList;
    for (auto& [eqmacid, rank] : purchases) {
        auto it = avMap.find(eqmacid);
        if (it == avMap.end()) continue;
        auto [skillId, maxLevel] = it->second;
        int effectiveRank = std::min(rank, maxLevel);
        aaidList << QString::number(skillId + effectiveRank - 1);
    }

    // ── 3. aa_effects : SUM(base1) par effectid ───────────────────────────
    if (!aaidList.isEmpty()) {
        QSqlQuery q2(db);
        q2.exec(QString("SELECT effectid, SUM(base1) AS total FROM aa_effects"
                        " WHERE aaid IN (%1) GROUP BY effectid").arg(aaidList.join(',')));
        while (q2.next()) {
            int effectid = q2.value(0).toInt();
            int total    = q2.value(1).toInt();
            auto it = AA_EFFECT_TO_STAT.find(effectid);
            if (it != AA_EFFECT_TO_STAT.end())
                result.stats[it->second] += total;
        }
    }

    // ── 4. Natural Durability + Physical Enhancement (pourcentage HP) ─────
    std::map<int,int> purchaseMap;
    for (auto& [id, rank] : purchases) purchaseMap[id] = rank;

    QSqlQuery q3(db);
    q3.exec(QString("SELECT eqmacid, skill_id, max_level FROM altadv_vars"
                    " WHERE skill_id IN (%1, %2)").arg(ND_SKILL_ID).arg(PE_SKILL_ID));
    std::map<int, std::pair<int,int>> ndpe; // skill_id → {eqmacid, max_level}
    while (q3.next())
        ndpe[q3.value(1).toInt()] = {q3.value(0).toInt(), q3.value(2).toInt()};

    int ndRank = 0, peRank = 0;
    if (auto it = ndpe.find(ND_SKILL_ID); it != ndpe.end()) {
        auto [emacid, maxLvl] = it->second;
        if (purchaseMap.count(emacid))
            ndRank = std::min(purchaseMap[emacid], maxLvl);
    }
    if (auto it = ndpe.find(PE_SKILL_ID); it != ndpe.end()) {
        auto [emacid, maxLvl] = it->second;
        if (purchaseMap.count(emacid))
            peRank = std::min(purchaseMap[emacid], maxLvl);
    }

    auto ndIt = ND_BONUS.find(ndRank);
    if (ndIt != ND_BONUS.end()) {
        result.nd_pct = ndIt->second;
        if (ndRank >= 1 && peRank >= 1)
            result.nd_pct += 2.0f;
    }

    return result;
}

QList<SpellData> ItemDatabase::getBeneficialSpellsByClass(const QString& className, int maxLevel) {
    QList<SpellData> result;
    auto it = SPELL_CLASS_ID.find(className.toStdString());
    if (it == SPELL_CLASS_ID.end()) return result;
    int cid = it->second;

    // Colonnes effect (12 slots)
    QString effectCols;
    for (int i = 1; i <= 12; ++i)
        effectCols += QString("effectid%1, effect_base_value%1, max%1, formula%1, ").arg(i);
    // Colonnes classes (15 classes — Berserker/16 absent dans spells_new Quarm)
    QString classCols;
    for (int i = 1; i <= 15; ++i)
        classCols += QString("classes%1, ").arg(i);
    classCols.chop(2); // remove trailing ", "

    QString sql = QString(
        "SELECT id, name, targettype, %1 %2"
        " FROM spells_new"
        " WHERE goodEffect = 1 AND classes%3 > 0 AND classes%3 <= :maxlvl"
        " ORDER BY classes%3 DESC, name ASC")
        .arg(effectCols).arg(classCols).arg(cid);

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(sql);
    q.bindValue(":maxlvl", maxLevel);
    if (!q.exec()) {
        qWarning() << "getBeneficialSpellsByClass failed:" << q.lastError().text();
        return result;
    }
    while (q.next())
        result.append(rowToSpellData(q));
    return result;
}
