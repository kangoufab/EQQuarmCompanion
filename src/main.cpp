#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <filesystem>
#include "core/config.h"
#include "db/db_connection.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include "ui/main_window.h"

static QFile* g_logFile = nullptr;

static void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    if (!g_logFile) return;
    QTextStream out(g_logFile);
    QString prefix;
    switch (type) {
        case QtDebugMsg:   prefix = "[D]"; break;
        case QtWarningMsg: prefix = "[W]"; break;
        default:           prefix = "[I]"; break;
    }
    out << QDateTime::currentDateTime().toString("hh:mm:ss") << " " << prefix << " " << msg << "\n";
    out.flush();
}

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

    auto exeDirForLog = std::filesystem::path(
        QCoreApplication::applicationDirPath().toStdWString());
    auto logPath = exeDirForLog / "eqquarm_debug.log";
    g_logFile = new QFile(QString::fromStdWString(logPath.wstring()));
    g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(messageHandler);


    auto* splash = makeSplash();
    splash->show();
    app.processEvents();

    auto cfgPath  = exeDirForLog / "config.json";
    auto defsPath = exeDirForLog / "config_defaults.json";

    Config config(cfgPath, defsPath);

    // Connexion DB (non bloquant si échec — certaines fonctions désactivées)
    auto dbCfg = config.getDbConfig();
    bool dbOk  = DbConnection::instance().connect(dbCfg);
    if (!dbOk)
        qWarning() << "DB non connectée — vérifier les paramètres dans config.json";

    NpcDatabase  npcDb;
    ItemDatabase itemDb;
    MainWindow window(&config, &npcDb, &itemDb);
    window.show();
    splash->finish(&window);
    delete splash;

    return app.exec();
}
