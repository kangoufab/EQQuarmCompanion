#pragma once
#include "core/types.h"
#include <QList>
#include <QObject>
#include <optional>
#include <unordered_map>

class ItemDatabase : public QObject {
    Q_OBJECT
public:
    explicit ItemDatabase(QObject* parent = nullptr);

    [[nodiscard]] std::optional<ItemData> getItemById(int id);
    [[nodiscard]] QList<ItemData>         searchItems(const QString& nameFragment,
                                                       int limit = 30,
                                                       int slotFilter = 0);

    // Retourne les NPCs qui droppent cet item (loottable → npc_types), max 30.
    [[nodiscard]] QList<NpcSourceData> getNpcSources(int itemId);

    // Retourne les sorts bénéfiques (goodEffect=1) castables par la classe donnée
    // jusqu'au niveau maxLevel, triés par niveau décroissant puis nom.
    [[nodiscard]] QList<SpellData> getBeneficialSpellsByClass(const QString& className,
                                                               int maxLevel);

    // Retourne un sort par ID (pour tooltips worn/focus effects).
    [[nodiscard]] std::optional<SpellData> getSpellById(int id);

    // Retourne les bonus de stats issus des AAs (altadv_vars + aa_effects).
    [[nodiscard]] AaStats getAaStats(const std::vector<std::pair<int,int>>& purchases);

    // Retourne les paires (item_name, clickeffect_spell_id) pour les items avec clickeffect > 0.
    [[nodiscard]] QList<QPair<QString,int>> getItemClickeffects(const QList<int>& itemIds);

    // Retourne le meilleur bardvalue par bardtype (23/24/25/26/50/51) pour les items
    // dont reqlevel <= maxReqlevel. Utilisé pour calculer l'amplification des songs bard.
    [[nodiscard]] std::map<int, int> getBardInstrumentMods(int maxReqlevel);

private:
    std::unordered_map<int, ItemData> _itemCache;
};
