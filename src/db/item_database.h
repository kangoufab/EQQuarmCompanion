#pragma once
#include "core/types.h"
#include <QList>
#include <QObject>
#include <optional>

class ItemDatabase : public QObject {
    Q_OBJECT
public:
    explicit ItemDatabase(QObject* parent = nullptr);

    [[nodiscard]] std::optional<ItemData> getItemById(int id);
    [[nodiscard]] QList<ItemData>         searchItems(const QString& nameFragment,
                                                       int limit = 30);
};
