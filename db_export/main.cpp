#include "schema_introspect.h"
#include "table_copier.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

namespace {

struct ExportArgs {
    QString host = "localhost";
    int     port = 3306;
    QString user = "root";
    QString password;
    QString database = "quarm";
    QString outPath;
};

ExportArgs parseArgs(const QStringList& args) {
    ExportArgs a;
    for (int i = 0; i < args.size(); ++i) {
        auto next = [&]() -> QString { return (i + 1 < args.size()) ? args[++i] : QString(); };
        if      (args[i] == "--host")     a.host = next();
        else if (args[i] == "--port")     a.port = next().toInt();
        else if (args[i] == "--user")     a.user = next();
        else if (args[i] == "--password") a.password = next();
        else if (args[i] == "--database") a.database = next();
        else if (args[i] == "--out")      a.outPath = next();
    }
    return a;
}

// Tables copiées telles quelles + colonnes indexées pour les jointures/filtres
// utilisés par src/db/item_database.cpp et src/db/npc_database.cpp
// (cf. docs/specs/2026-07-13-embedded-sqlite-db-design.md).
const QMap<QString, QStringList> TABLES = {
    {"items",              {"worneffect", "focuseffect", "proceffect", "clickeffect", "scrolleffect"}},
    {"spells_new",         {}},
    {"npc_types",          {"race", "loottable_id", "npc_spells_id"}},
    {"races",              {}},
    {"spawnentry",         {"npcID", "spawngroupID"}},
    {"spawn2",             {"spawngroupID", "zone"}},
    {"zone",               {"short_name"}},
    {"npc_spells_entries", {"npc_spells_id", "spellid"}},
    {"loottable_entries",  {"loottable_id", "lootdrop_id"}},
    {"lootdrop_entries",   {"lootdrop_id", "item_id"}},
    {"altadv_vars",        {"eqmacid", "skill_id"}},
    {"aa_effects",         {"aaid"}},
};

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    auto args = parseArgs(app.arguments().mid(1));
    if (args.outPath.isEmpty()) {
        qWarning() << "Usage: db_export --host H --port P --user U --password W"
                      " --database quarm --out path/to/quarm_data.db";
        return 1;
    }

    auto mysqlDb = QSqlDatabase::addDatabase("QMYSQL", "export_src");
    mysqlDb.setHostName(args.host);
    mysqlDb.setPort(args.port);
    mysqlDb.setUserName(args.user);
    mysqlDb.setPassword(args.password);
    mysqlDb.setDatabaseName(args.database);
    if (!mysqlDb.open()) {
        qWarning() << "MySQL connection failed:" << mysqlDb.lastError().text();
        return 1;
    }

    QFile::remove(args.outPath);
    auto sqliteDb = QSqlDatabase::addDatabase("QSQLITE", "export_dst");
    sqliteDb.setDatabaseName(args.outPath);
    if (!sqliteDb.open()) {
        qWarning() << "SQLite create failed:" << sqliteDb.lastError().text();
        return 1;
    }

    bool allOk = true;
    for (auto it = TABLES.constBegin(); it != TABLES.constEnd(); ++it) {
        qInfo() << "Exporting" << it.key() << "...";
        if (!copyTable(mysqlDb, sqliteDb, it.key(), it.value()))
            allOk = false;
    }

    sqliteDb.close();
    mysqlDb.close();
    QSqlDatabase::removeDatabase("export_src");
    QSqlDatabase::removeDatabase("export_dst");

    qInfo() << (allOk ? "Export completed successfully." : "Export completed with errors.");
    return allOk ? 0 : 1;
}
