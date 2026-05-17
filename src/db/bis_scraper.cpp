#include "db/bis_scraper.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

// URL base: https://www.eqprogression.com
// Path pattern: /{class-slug}-best-in-slot-bis-gearing-guide-{expansion-slug}/
// Class slug: "shadow-knight" for Shadowknight, lowercase otherwise
// The Python BisScraper uses eqprogression.com — fetchBis takes className and
// builds the URL the same way, defaulting to Classic expansion.

static QString classToSlug(const QString& className) {
    if (className.compare("Shadowknight", Qt::CaseInsensitive) == 0)
        return "shadow-knight";
    return className.toLower();
}

BisScaper::BisScaper(QObject* parent) : QObject(parent) {}

void BisScaper::fetchBis(const QString& className,
                          std::function<void(QList<BisEntry>)> cb)
{
    _callback = std::move(cb);
    QString slug = classToSlug(className);
    QString url  = "https://www.eqprogression.com/" + slug +
                   "-best-in-slot-bis-gearing-guide-classic/";
    auto* reply = _nam.get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, &BisScaper::onReplyFinished);
}

void BisScaper::onReplyFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    QByteArray data = reply->readAll();
    reply->deleteLater();
    if (_callback) _callback(parseHtml(data));
}

QList<BisEntry> BisScaper::parseHtml(const QByteArray& html) const {
    QList<BisEntry> result;
    QString page = QString::fromUtf8(html);

    // Parse table rows: each <tr> has <td>slot</td><td>items...</td>
    // Items are in <span data-tooltip="..."><span>ItemName</span></span>
    // Slot column is first <td> text content
    static const QRegularExpression rowRe(
        R"(<tr[^>]*>(.*?)</tr>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression cellRe(
        R"(<td[^>]*>(.*?)</td>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression tooltipRe(
        R"(<span[^>]*data-tooltip[^>]*>\s*<span[^>]*>([^<]+)</span>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    // Slot name → BisEntry list (aggregate slots like Ears → Left Ear, Right Ear)
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

        // Extract cells
        QStringList cells;
        auto cellIt = cellRe.globalMatch(rowHtml);
        while (cellIt.hasNext())
            cells << cellIt.next().captured(1);

        if (cells.size() < 2) continue;

        // Slot name: strip tags from first cell
        QString rawSlot = cells[0];
        rawSlot.remove(QRegularExpression("<[^>]+>"));
        rawSlot = rawSlot.trimmed();
        if (rawSlot.isEmpty()) continue;

        // Item names from second cell via data-tooltip spans
        QString itemsHtml = cells[1];
        auto tipIt = tooltipRe.globalMatch(itemsHtml);
        while (tipIt.hasNext()) {
            QString itemName = tipIt.next().captured(1).trimmed();
            if (itemName.isEmpty()) continue;

            // Map aggregate slot names to EQ inventory slots
            QStringList eqSlots = slotMap.value(rawSlot, {rawSlot});
            for (const QString& eqSlot : eqSlots) {
                BisEntry e;
                e.slotName = eqSlot;
                e.itemName = itemName;
                e.itemId   = 0; // ID not available from HTML, resolved separately
                result.append(e);
            }
        }
    }
    return result;
}
