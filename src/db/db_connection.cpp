#include "db/db_connection.h"
#include <QtSql/QSqlError>
#include <QDebug>

DbConnection& DbConnection::instance() {
    static DbConnection inst;
    return inst;
}

bool DbConnection::connect(const DbConfig& cfg) {
    if (_db.isOpen()) _db.close();
    if (QSqlDatabase::contains("main"))
        QSqlDatabase::removeDatabase("main");

    _db = QSqlDatabase::addDatabase("QMYSQL", "main");
    _db.setHostName(QString::fromStdString(cfg.host));
    _db.setPort(cfg.port);
    _db.setDatabaseName(QString::fromStdString(cfg.database));
    _db.setUserName(QString::fromStdString(cfg.user));
    _db.setPassword(QString::fromStdString(cfg.password));

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
