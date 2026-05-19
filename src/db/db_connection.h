#pragma once
#include "core/config.h"
#include <QtSql/QSqlDatabase>
#include <QString>

class DbConnection {
public:
    static DbConnection& instance();

    bool connect(const DbConfig& cfg);
    void disconnect();

    // Ouvre une connexion temporaire pour tester les paramètres sans perturber la connexion active
    static bool testConnection(const DbConfig& cfg);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QSqlDatabase& db();

private:
    DbConnection() = default;
    QSqlDatabase _db;
};
