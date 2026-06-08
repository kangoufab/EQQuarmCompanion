#pragma once
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <functional>

struct BisEntry {
    QString slotName;
    QString itemName;
    int     itemId{};
};

class BisScaper : public QObject {
    Q_OBJECT
public:
    explicit BisScaper(QObject* parent = nullptr);

    // Fetches BIS data for className + expansion (e.g. "Warrior", "Luclin").
    // Reads local JSON cache if valid (TTL 7 days). Fetches from network otherwise.
    // Calls callback with a flat QSet of all BIS item names across all slots.
    void fetchBis(const QString& className,
                  const QString& expansion,
                  std::function<void(QSet<QString>)> callback);

private slots:
    void onReplyFinished();

private:
    static QString cacheFilePath(const QString& className, const QString& expansion);
    static bool    cacheIsValid(const QString& path);
    static QSet<QString> loadFromCache(const QString& path);
    void           saveToCache(const QString& path, const QList<BisEntry>& entries) const;
    static QString expansionToSlug(const QString& expansion);
    static QString classToSlug(const QString& className);
    [[nodiscard]] QList<BisEntry> parseHtml(const QByteArray& html) const;
    static QSet<QString> entriesToSet(const QList<BisEntry>& entries);

    QNetworkAccessManager              _nam;
    std::function<void(QSet<QString>)> _callback;
    QString                            _cachePath;
};
