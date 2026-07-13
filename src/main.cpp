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
#include <system_error>
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
    QPixmap logo(":/app_icon.png");
    const int W = 540, H = 200;
    QPixmap pix(W, H);
    pix.fill(QColor("#1a1a2e"));
    QPainter p(&pix);
    if (!logo.isNull()) {
        QPixmap scaled = logo.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        p.drawPixmap(20, (H - scaled.height()) / 2, scaled);
    }
    p.setPen(QColor("#64b5f6"));
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(QRect(200, 50, 320, 50), Qt::AlignLeft | Qt::AlignVCenter, "EQ Quarm Companion");
    p.setPen(QColor("#888888"));
    p.setFont(QFont("Arial", 10));
    p.drawText(QRect(200, 110, 320, 30), Qt::AlignLeft | Qt::AlignVCenter, "Chargement en cours...");
    p.end();
    auto* s = new QSplashScreen(pix);
    s->setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    return s;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EQ Quarm Companion");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    auto exeDir = std::filesystem::path(
        QCoreApplication::applicationDirPath().toStdWString());

    // config.json et log sont stockés dans %APPDATA%\EqQuarmCompanion plutôt
    // qu'à côté de l'exe : évite la virtualisation UAC (VirtualStore) quand
    // l'app est installée dans Program Files. config_defaults.json reste une
    // ressource en lecture seule livrée avec l'exe.
    QString appData = qEnvironmentVariable("APPDATA");
    std::filesystem::path dataDir = appData.isEmpty()
        ? exeDir
        : std::filesystem::path(appData.toStdWString()) / L"EqQuarmCompanion";
    std::error_code ec;
    std::filesystem::create_directories(dataDir, ec);

    auto logPath = dataDir / "eqquarm_debug.log";
    g_logFile = new QFile(QString::fromStdWString(logPath.wstring()));
    g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(messageHandler);


    auto* splash = makeSplash();
    splash->show();
    app.processEvents();

    auto cfgPath  = dataDir / "config.json";
    auto defsPath = exeDir / "config_defaults.json";

    // Migration : ancienne config à côté de l'exe → %APPDATA%\EqQuarmCompanion.
    auto legacyCfg = exeDir / "config.json";
    if (!std::filesystem::exists(cfgPath) && std::filesystem::exists(legacyCfg))
        std::filesystem::copy_file(legacyCfg, cfgPath,
            std::filesystem::copy_options::overwrite_existing, ec);

    Config config(cfgPath, defsPath);

    // Connexion DB embarquée (SQLite bundlé à côté de l'exe — non bloquant si échec)
    auto dbPath = QString::fromStdWString((exeDir / "quarm_data.db").wstring());
    bool dbOk   = DbConnection::instance().connect(dbPath);
    if (!dbOk)
        qWarning() << "DB non connectée — quarm_data.db introuvable à côté de l'exécutable";

    NpcDatabase  npcDb;
    ItemDatabase itemDb;
    MainWindow window(&config, &npcDb, &itemDb);
    window.show();
    splash->finish(&window);
    delete splash;

    return app.exec();
}
