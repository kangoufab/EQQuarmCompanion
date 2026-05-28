#pragma once
#include "core/types.h"
#include <QList>
#include <QObject>
#include <optional>

class NpcDatabase : public QObject {
    Q_OBJECT
public:
    explicit NpcDatabase(QObject* parent = nullptr);

    [[nodiscard]] QList<NpcData>         searchNpcs(const QString& nameFragment);
    [[nodiscard]] std::optional<NpcData> getNpcById(int id);
    [[nodiscard]] QList<SpellData>       getNpcSpells(int npcSpellsId);
    [[nodiscard]] QList<LootItem>        getNpcLoot(int loottableId);

    // Retourne les world drops (global_loot) applicables à ce NPC.
    // Filtres : level, race, bodytype, zone_id (NULL = wildcard). Max 50 items.
    [[nodiscard]] QList<LootItem> getNpcGlobalLoot(int level, int race,
                                                   int bodytype, int zone_id);
};
