#include "db/db_connection.h"
#include <QtSql/QSqlError>
#include <QDebug>

DbConnection& DbConnection::instance() {
    static DbConnection inst;
    return inst;
}

bool DbConnection::connect(const QString& sqliteFilePath) {
    if (_db.isOpen()) _db.close();
    if (QSqlDatabase::contains("main"))
        QSqlDatabase::removeDatabase("main");

    _db = QSqlDatabase::addDatabase("QSQLITE", "main");
    _db.setDatabaseName(sqliteFilePath);
    _db.setConnectOptions("QSQLITE_OPEN_READONLY");

    if (!_db.open()) {
        qWarning() << "DB connection failed:" << _db.lastError().text();
        return false;
    }
    return true;
}

void DbConnection::disconnect() {
    if (_db.isOpen()) _db.close();
    QSqlDatabase::removeDatabase("main");
}

bool DbConnection::isConnected() const { return _db.isOpen(); }
QSqlDatabase& DbConnection::db() { return _db; }
