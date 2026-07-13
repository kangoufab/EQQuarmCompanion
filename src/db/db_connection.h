#pragma once
#include <QString>
#include <QtSql/QSqlDatabase>

class DbConnection {
public:
    static DbConnection& instance();

    // Ouvre le fichier SQLite embarqué (quarm_data.db) en lecture seule.
    bool connect(const QString& sqliteFilePath);
    void disconnect();

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QSqlDatabase& db();

private:
    DbConnection() = default;
    QSqlDatabase _db;
};
