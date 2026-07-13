#pragma once
#include <QString>
#include <QStringList>

class QSqlDatabase;

// Copie la table `table` de la connexion MySQL `mysqlDb` vers la connexion
// SQLite `sqliteDb` : introspection du schéma, CREATE TABLE, copie des lignes
// par lots dans une transaction, puis création des index sur `indexColumns`
// (colonnes utilisées en jointure/filtre par item_database.cpp/npc_database.cpp).
bool copyTable(QSqlDatabase& mysqlDb, QSqlDatabase& sqliteDb,
               const QString& table, const QStringList& indexColumns = {});
