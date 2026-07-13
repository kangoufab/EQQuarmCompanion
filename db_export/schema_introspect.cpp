#include "schema_introspect.h"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

namespace {

QString mapSqliteType(const QString& mysqlType) {
    QString t = mysqlType.toUpper();
    if (t.contains("INT")) return "INTEGER";
    if (t.contains("FLOAT") || t.contains("DOUBLE") || t.contains("DECIMAL")) return "REAL";
    return "TEXT";
}

} // namespace

std::vector<ColumnDef> introspectColumns(QSqlDatabase& mysqlDb, const QString& table) {
    std::vector<ColumnDef> cols;
    QSqlQuery q(mysqlDb);
    q.prepare(
        "SELECT COLUMN_NAME, DATA_TYPE FROM information_schema.columns"
        " WHERE table_schema = DATABASE() AND table_name = :t"
        " ORDER BY ORDINAL_POSITION"
    );
    q.bindValue(":t", table);
    if (!q.exec()) {
        qWarning() << "introspectColumns failed for" << table << ":" << q.lastError().text();
        return cols;
    }
    while (q.next()) {
        ColumnDef c;
        c.name       = q.value(0).toString();
        c.sqliteType = mapSqliteType(q.value(1).toString());
        cols.push_back(c);
    }
    return cols;
}
