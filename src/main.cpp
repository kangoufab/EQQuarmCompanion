#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <filesystem>
#include "core/config.h"
#include "db/db_connection.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include "ui/main_window.h"

static QSplashScreen* makeSplash() {
    QPixmap pix(420, 180);
    pix.fill(QColor("#1a1a2e"));
    QPainter p(&pix);
    p.setPen(QColor("#64b5f6"));
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(pix.rect().adjusted(0, -30, 0, 0),
               Qt::AlignCenter, "EQ Quarm Companion");
    p.setPen(QColor("#888888"));
    p.setFont(QFont("Arial", 10));
    p.drawText(pix.rect().adjusted(0, 50, 0, 0),
               Qt::AlignCenter, "Chargement en cours...");
    p.end();
    auto* s = new QSplashScreen(pix);
    s->setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    return s;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EQ Quarm Companion");

    auto* splash = makeSplash();
    splash->show();
    app.processEvents();

    auto exeDir   = std::filesystem::path(
                        QCoreApplication::applicationDirPath().toStdWString());
    auto cfgPath  = exeDir / "config.json";
    auto defsPath = exeDir / "config_defaults.json";

    Config config(cfgPath, defsPath);

    // Connexion DB (non bloquant si échec — certaines fonctions désactivées)
    auto dbCfg = config.getDbConfig();
    bool dbOk  = DbConnection::instance().connect(dbCfg);
    if (!dbOk) {
        qWarning() << "DB non connectée — vérifier les paramètres dans config.json";
    }

    NpcDatabase  npcDb;
    ItemDatabase itemDb;
    MainWindow window(&config, &npcDb, &itemDb);
    window.show();
    splash->finish(&window);
    delete splash;

    return app.exec();
}
