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
};
