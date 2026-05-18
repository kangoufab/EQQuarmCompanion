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

    // Retourne les sorts bénéfiques (goodEffect=1) castables par la classe donnée
    // jusqu'au niveau maxLevel, triés par niveau décroissant puis nom.
    [[nodiscard]] QList<SpellData> getBeneficialSpellsByClass(const QString& className,
                                                               int maxLevel);

    // Retourne un sort par ID (pour tooltips worn/focus effects).
    [[nodiscard]] std::optional<SpellData> getSpellById(int id);

    // Retourne les bonus de stats issus des AAs (altadv_vars + aa_effects).
    [[nodiscard]] AaStats getAaStats(const std::vector<std::pair<int,int>>& purchases);
};
