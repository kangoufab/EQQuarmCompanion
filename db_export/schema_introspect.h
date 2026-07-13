#pragma once
#include <QString>
#include <vector>

struct ColumnDef {
    QString name;
    QString sqliteType; // "INTEGER", "REAL", ou "TEXT"
};

class QSqlDatabase;

// Lit information_schema.columns pour `table` sur la connexion MySQL `mysqlDb`
// et retourne les colonnes dans l'ordre du schéma, avec leur type SQLite mappé.
std::vector<ColumnDef> introspectColumns(QSqlDatabase& mysqlDb, const QString& table);
