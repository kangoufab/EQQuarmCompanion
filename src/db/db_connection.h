#pragma once
#include "core/config.h"
#include <QtSql/QSqlDatabase>
#include <QString>

class DbConnection {
public:
    static DbConnection& instance();

    bool connect(const DbConfig& cfg);
    void disconnect();

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QSqlDatabase& db();

private:
    DbConnection() = default;
    QSqlDatabase _db;
};
