#include "table_copier.h"
#include "schema_introspect.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

bool copyTable(QSqlDatabase& mysqlDb, QSqlDatabase& sqliteDb,
               const QString& table, const QStringList& indexColumns) {
    auto cols = introspectColumns(mysqlDb, table);
    if (cols.empty()) {
        qWarning() << "No columns found for table" << table << "- skipping";
        return false;
    }

    QStringList colDefs, colNames, placeholders;
    for (const auto& c : cols) {
        colDefs << QString("`%1` %2").arg(c.name, c.sqliteType);
        colNames << QString("`%1`").arg(c.name);
        placeholders << "?";
    }

    QSqlQuery createQ(sqliteDb);
    createQ.exec(QString("DROP TABLE IF EXISTS `%1`").arg(table));
    if (!createQ.exec(QString("CREATE TABLE `%1` (%2)").arg(table, colDefs.join(", ")))) {
        qWarning() << "CREATE TABLE failed for" << table << ":" << createQ.lastError().text();
        return false;
    }

    QSqlQuery selectQ(mysqlDb);
    if (!selectQ.exec(QString("SELECT %1 FROM `%2`").arg(colNames.join(", "), table))) {
        qWarning() << "SELECT failed for" << table << ":" << selectQ.lastError().text();
        return false;
    }

    sqliteDb.transaction();
    QSqlQuery insertQ(sqliteDb);
    insertQ.prepare(QString("INSERT INTO `%1` (%2) VALUES (%3)")
                     .arg(table, colNames.join(", "), placeholders.join(", ")));

    int rowCount = 0;
    while (selectQ.next()) {
        for (int i = 0; i < cols.size(); ++i)
            insertQ.bindValue(i, selectQ.value(i));
        if (!insertQ.exec()) {
            qWarning() << "INSERT failed for" << table << ":" << insertQ.lastError().text();
            sqliteDb.rollback();
            return false;
        }
        ++rowCount;
    }
    sqliteDb.commit();
    qInfo() << "Copied" << rowCount << "rows into" << table;

    for (const auto& col : indexColumns) {
        QSqlQuery idxQ(sqliteDb);
        QString idxName = QString("idx_%1_%2").arg(table, col);
        if (!idxQ.exec(QString("CREATE INDEX `%1` ON `%2` (`%3`)")
                       .arg(idxName, table, col)))
            qWarning() << "CREATE INDEX failed for" << table << "." << col << ":"
                       << idxQ.lastError().text();
    }
    return true;
}
