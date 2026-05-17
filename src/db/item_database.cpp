#include "db/item_database.h"
#include "db/db_connection.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>
#undef slots  // Qt macro — conflicts with field names in plain structs

// ── SQL helpers ────────────────────────────────────────────────────────────

// Build COALESCE(CASE WHEN sw.effectid{i} = spa THEN sw.col{i} ... END, default)
static QString spaCase(int spa, const QString& col, int defaultVal = 0) {
    QString whens;
    for (int i = 1; i <= 12; ++i)
        whens += QString(" WHEN sw.effectid%1 = %2 THEN sw.%3%1").arg(i).arg(spa).arg(col);
    return QString("COALESCE(CASE%1 ELSE %2 END, %2)").arg(whens).arg(defaultVal);
}

// Build COALESCE(CASE WHEN sw.effectid{i} IN (spas) THEN sw.col{i} ... END, default)
static QString spaCaseMulti(const QList<int>& spas, const QString& col, int defaultVal = 0) {
    QStringList ids;
    for (int s : spas) ids << QString::number(s);
    QString inList = ids.join(", ");
    QString whens;
    for (int i = 1; i <= 12; ++i)
        whens += QString(" WHEN sw.effectid%1 IN (%2) THEN sw.%3%1").arg(i).arg(inList).arg(col);
    return QString("COALESCE(CASE%1 ELSE %2 END, %2)").arg(whens).arg(defaultVal);
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
    item.slots          = q.value("slots").toInt();
    item.reqlevel       = q.value("reqlevel").toInt();
    item.worneffect     = q.value("worneffect").toInt();
    item.focuseffect    = q.value("focuseffect").toInt();
    item.proceffect     = q.value("proceffect").toInt();
    item.worneffect_name  = q.value("worneffect_name").toString().toStdString();
    item.focuseffect_name = q.value("focuseffect_name").toString().toStdString();
    item.proceffect_name  = q.value("proceffect_name").toString().toStdString();
    return item;
}

// ── ItemDatabase ───────────────────────────────────────────────────────────

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
