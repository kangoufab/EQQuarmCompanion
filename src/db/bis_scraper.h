#pragma once
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
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

    void fetchBis(const QString& className,
                  std::function<void(QList<BisEntry>)> callback);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager _nam;
    std::function<void(QList<BisEntry>)> _callback;

    [[nodiscard]] QList<BisEntry> parseHtml(const QByteArray& html) const;
};
