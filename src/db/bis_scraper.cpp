#include "db/bis_scraper.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static constexpr qint64 kCacheTtlSeconds = 7 * 24 * 3600;

static const QMap<QString, QString> kExpSlug = {
    {"Classic",         "classic"},
    {"Kunark",          "ruins-of-kunark"},
    {"Velious",         "scars-of-velious"},
    {"Luclin",          "shadows-of-luclin"},
    {"Planes of Power", "planes-of-power"},
};

static const QMap<QString, QString> kClassSlug = {
    {"Shadowknight", "shadow-knight"},
};

BisScaper::BisScaper(QObject* parent) : QObject(parent) {}

QString BisScaper::classToSlug(const QString& className) {
    return kClassSlug.value(className, className.toLower());
}

QString BisScaper::expansionToSlug(const QString& expansion) {
    return kExpSlug.value(expansion, expansion.toLower().replace(' ', '-'));
}

QString BisScaper::cacheFilePath(const QString& className, const QString& expansion) {
    QString dir = QCoreApplication::applicationDirPath() + "/bis_cache";
    return dir + "/" + className + "_" + expansion + ".json";
}

bool BisScaper::cacheIsValid(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) return false;
    return fi.lastModified().secsTo(QDateTime::currentDateTime()) < kCacheTtlSeconds;
}

QSet<QString> BisScaper::loadFromCache(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return {};
    QJsonObject obj = doc.object();
    QSet<QString> names;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        for (const QJsonValue& v : it.value().toArray())
            names.insert(v.toString());
    return names;
}

void BisScaper::saveToCache(const QString& path, const QList<BisEntry>& entries) const {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QJsonObject obj;
    for (const BisEntry& e : entries) {
        QJsonArray arr = obj.value(e.slotName).toArray();
        if (!arr.contains(QJsonValue(e.itemName)))
            arr.append(e.itemName);
        obj[e.slotName] = arr;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

QSet<QString> BisScaper::entriesToSet(const QList<BisEntry>& entries) {
    QSet<QString> s;
    for (const BisEntry& e : entries)
        s.insert(e.itemName);
    return s;
}

void BisScaper::fetchBis(const QString& className,
                          const QString& expansion,
                          std::function<void(QSet<QString>)> cb)
{
    QString cachePath = cacheFilePath(className, expansion);

    if (cacheIsValid(cachePath)) {
        if (cb) cb(loadFromCache(cachePath));
        return;
    }

    QString url = "https://www.eqprogression.com/"
                  + classToSlug(className)
                  + "-best-in-slot-bis-gearing-guide-"
                  + expansionToSlug(expansion) + "/";
    auto* reply = _nam.get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, cachePath, cb = std::move(cb)]() mutable {
        QByteArray data;
        if (reply->error() == QNetworkReply::NoError)
            data = reply->readAll();
        reply->deleteLater();
        if (data.isEmpty()) {
            if (cb) cb({});
            return;
        }
        QList<BisEntry> entries = parseHtml(data);
        saveToCache(cachePath, entries);
        if (cb) cb(entriesToSet(entries));
    });
}

QList<BisEntry> BisScaper::parseHtml(const QByteArray& html) const {
    QList<BisEntry> result;
    QString page = QString::fromUtf8(html);

    static const QRegularExpression rowRe(
        R"(<tr[^>]*>(.*?)</tr>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression cellRe(
        R"(<td[^>]*>(.*?)</td>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    // Capture le nom jusqu'au premier '<' (pas d'ancre </span> finale) : certains
    // items insèrent un <br /> juste après le nom dans le span interne (slots Face,
    // Fingers…), ce qui faisait échouer un motif exigeant </span> immédiatement et
    // ignorait silencieusement ces items. [^<]+ s'arrête déjà au tag, le nom est trim.
    static const QRegularExpression tooltipRe(
        R"(<span[^>]*data-tooltip[^>]*>\s*<span[^>]*>([^<]+))",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QMap<QString, QStringList> slotMap = {
        {"Ears",    {"Left Ear",    "Right Ear"}},
        {"Fingers", {"Left Finger", "Right Finger"}},
        {"Wrists",  {"Left Wrist",  "Right Wrist"}},
        {"Ranged",  {"Range"}},
    };

    auto rowIt = rowRe.globalMatch(page);
    while (rowIt.hasNext()) {
        auto rowMatch = rowIt.next();
        QString rowHtml = rowMatch.captured(1);

        QStringList cells;
        auto cellIt = cellRe.globalMatch(rowHtml);
        while (cellIt.hasNext())
            cells << cellIt.next().captured(1);
        if (cells.size() < 2) continue;

        QString rawSlot = cells[0];
        rawSlot.remove(QRegularExpression("<[^>]+>"));
        rawSlot = rawSlot.trimmed();
        if (rawSlot.isEmpty()) continue;

        QString itemsHtml = cells[1];
        auto tipIt = tooltipRe.globalMatch(itemsHtml);
        while (tipIt.hasNext()) {
            QString itemName = tipIt.next().captured(1).trimmed();
            if (itemName.isEmpty()) continue;
            for (const QString& eqSlot : slotMap.value(rawSlot, {rawSlot})) {
                BisEntry e;
                e.slotName = eqSlot;
                e.itemName = itemName;
                e.itemId   = 0;
                result.append(e);
            }
        }
    }
    return result;
}
