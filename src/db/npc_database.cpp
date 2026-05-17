#include "db/npc_database.h"
#include "db/db_connection.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>
#include <QRegularExpression>
#undef slots  // Qt macro — conflicts with field names in plain structs

NpcDatabase::NpcDatabase(QObject* parent) : QObject(parent) {}

// ── Helper: fallback zone for script-spawned NPCs ─────────────────────────

static QString fallbackZone(const QString& name) {
    QString base = name;
    base.remove(QRegularExpression("^#+"));
    base.remove(QRegularExpression("_+$"));
    base = base.trimmed();
    if (base.isEmpty()) return {};

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT MIN(z.long_name) AS zone_long_name"
        " FROM npc_types nt"
        " JOIN spawnentry se ON se.npcID = nt.id"
        " JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
        " JOIN zone z ON z.short_name = s2.zone"
        " WHERE nt.name LIKE :name"
    );
    q.bindValue(":name", QString("%%1%").arg(base));
    if (!q.exec()) {
        qWarning() << "fallbackZone query failed:" << q.lastError().text();
        return {};
    }
    if (q.next()) return q.value("zone_long_name").toString();
    return {};
}

// ── searchNpcs ─────────────────────────────────────────────────────────────

QList<NpcData> NpcDatabase::searchNpcs(const QString& nameFragment) {
    QList<NpcData> result;
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT nt.id, nt.name, nt.level, MIN(z.long_name) AS zone_long_name"
        " FROM npc_types nt"
        " LEFT JOIN spawnentry se ON se.npcID = nt.id"
        " LEFT JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
        " LEFT JOIN zone z ON z.short_name = s2.zone"
        " WHERE nt.name LIKE :name"
        " GROUP BY nt.id, nt.name, nt.level"
        " ORDER BY CHAR_LENGTH(nt.name) ASC LIMIT 50"
    );
    q.bindValue(":name", QString("%%1%").arg(nameFragment));
    if (!q.exec()) {
        qWarning() << "searchNpcs failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        NpcData npc;
        npc.id    = q.value("id").toInt();
        npc.name  = q.value("name").toString().toStdString();
        npc.level = q.value("level").toInt();
        QString zone = q.value("zone_long_name").toString();
        if (zone.isEmpty())
            zone = fallbackZone(QString::fromStdString(npc.name));
        npc.zone_long_name = zone.toStdString();
        result.append(npc);
    }
    return result;
}

// ── getNpcById ─────────────────────────────────────────────────────────────

std::optional<NpcData> NpcDatabase::getNpcById(int id) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT id, name, level, maxlevel, hp, mana, race, `class` AS npc_class, bodytype,"
        " AC AS ac, ATK AS atk, mindmg AS min_hit, maxdmg AS max_hit,"
        " attack_delay, attack_count, Accuracy AS accuracy,"
        " STR AS str_v, STA AS sta, DEX AS dex, AGI AS agi,"
        " _INT AS int_v, WIS AS wis, CHA AS cha,"
        " MR AS mr, CR AS cr, DR AS dr, FR AS fr, PR AS pr,"
        " hp_regen_rate, mana_regen_rate, runspeed,"
        " special_abilities, npc_spells_id, loottable_id"
        " FROM npc_types WHERE id = :id LIMIT 1"
    );
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "getNpcById query failed:" << q.lastError().text();
        return std::nullopt;
    }
    if (!q.next()) return std::nullopt;

    NpcData npc;
    npc.id              = q.value("id").toInt();
    npc.name            = q.value("name").toString().toStdString();
    npc.level           = q.value("level").toInt();
    npc.hp              = q.value("hp").toInt();
    npc.ac              = q.value("ac").toInt();
    npc.atk             = q.value("atk").toInt();
    npc.min_hit         = q.value("min_hit").toInt();
    npc.max_hit         = q.value("max_hit").toInt();
    npc.attack_delay    = q.value("attack_delay").toInt();
    npc.attack_count    = q.value("attack_count").toInt();
    npc.accuracy        = q.value("accuracy").toInt();
    npc.mr              = q.value("mr").toInt();
    npc.fr              = q.value("fr").toInt();
    npc.cr              = q.value("cr").toInt();
    npc.dr              = q.value("dr").toInt();
    npc.pr              = q.value("pr").toInt();
    npc.race            = q.value("race").toInt();
    npc.npc_class       = q.value("npc_class").toInt();
    npc.npc_spells_id   = q.value("npc_spells_id").toInt();
    npc.loottable_id    = q.value("loottable_id").toInt();
    npc.special_abilities = q.value("special_abilities").toString().toStdString();

    // Zone lookup
    QSqlQuery zq(DbConnection::instance().db());
    zq.prepare(
        "SELECT MIN(z.long_name) AS zone_long_name"
        " FROM spawnentry se"
        " JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
        " JOIN zone z ON z.short_name = s2.zone"
        " WHERE se.npcID = :id"
    );
    zq.bindValue(":id", id);
    QString zone;
    if (zq.exec() && zq.next())
        zone = zq.value("zone_long_name").toString();
    if (zone.isEmpty())
        zone = fallbackZone(QString::fromStdString(npc.name));
    npc.zone_long_name = zone.toStdString();

    return npc;
}

// ── getNpcSpells ───────────────────────────────────────────────────────────

QList<SpellData> NpcDatabase::getNpcSpells(int npcSpellsId) {
    QList<SpellData> result;
    if (npcSpellsId == 0) return result;

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT sn.id, sn.name, sn.resisttype AS resist_type,"
        " sn.ResistDiff AS resist_diff, sn.no_partial_resist,"
        " nse.type AS spell_type, nse.recast_delay,"
        " sn.targettype, sn.aoerange, sn.cast_time, sn.recast_time,"
        " sn.buffduration, sn.buffdurationformula,"
        " sn.effectid1, sn.effect_base_value1,"
        " sn.effectid2, sn.effect_base_value2,"
        " sn.effectid3, sn.effect_base_value3,"
        " sn.effectid4, sn.effect_base_value4"
        " FROM npc_spells_entries nse"
        " JOIN spells_new sn ON sn.id = nse.spellid"
        " WHERE nse.npc_spells_id = :id"
        " ORDER BY nse.priority ASC"
    );
    q.bindValue(":id", npcSpellsId);
    if (!q.exec()) {
        qWarning() << "getNpcSpells failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        SpellData sd;
        sd.id              = q.value("id").toInt();
        sd.name            = q.value("name").toString().toStdString();
        sd.resist_type     = q.value("resist_type").toInt();
        sd.spell_type      = q.value("spell_type").toInt();
        sd.recast_delay    = q.value("recast_delay").toInt();
        sd.targettype      = q.value("targettype").toInt();
        sd.cast_time       = q.value("cast_time").toInt();
        sd.buffduration        = q.value("buffduration").toInt();
        sd.buffdurationformula = q.value("buffdurationformula").toInt();

        // Map effectid1-4 to spa[] and effect_base_value1-4 to effect_base_value[]
        for (int i = 0; i < 4; ++i) {
            int slot = i + 1;
            sd.spa[i]              = q.value(QString("effectid%1").arg(slot)).toInt();
            sd.effect_base_value[i] = q.value(QString("effect_base_value%1").arg(slot)).toInt();
        }
        result.append(sd);
    }
    return result;
}

// ── getNpcLoot ─────────────────────────────────────────────────────────────

QList<LootItem> NpcDatabase::getNpcLoot(int loottableId) {
    QList<LootItem> result;
    if (loottableId == 0) return result;

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT i.id AS item_id, i.Name AS name,"
        " lde.chance AS item_chance, lde.equip_item, lde.lootdrop_id,"
        " lte.probability AS table_probability,"
        " lte.multiplier AS table_multiplier,"
        " lte.droplimit, lte.mindrop,"
        " i.slots"
        " FROM loottable_entries lte"
        " JOIN lootdrop_entries lde ON lde.lootdrop_id = lte.lootdrop_id"
        " JOIN items i ON i.id = lde.item_id"
        " WHERE lte.loottable_id = :id"
    );
    q.bindValue(":id", loottableId);
    if (!q.exec()) {
        qWarning() << "getNpcLoot failed:" << q.lastError().text();
        return result;
    }

    // Collect raw rows to compute loot chances (EQMacEmu algorithm)
    struct RawRow {
        int    item_id{}, slots{};
        double item_chance{}, table_probability{}, table_multiplier{};
        int    droplimit{}, mindrop{}, lootdrop_id{};
        QString name;
    };
    QList<RawRow> rows;
    QMap<int, double> totals; // lootdrop_id -> sum of item_chance

    while (q.next()) {
        RawRow r;
        r.item_id          = q.value("item_id").toInt();
        r.name             = q.value("name").toString();
        r.item_chance      = q.value("item_chance").toDouble();
        r.table_probability = q.value("table_probability").toDouble();
        r.table_multiplier  = q.value("table_multiplier").toDouble();
        r.droplimit        = q.value("droplimit").toInt();
        r.mindrop          = q.value("mindrop").toInt();
        r.lootdrop_id      = q.value("lootdrop_id").toInt();
        r.slots            = q.value("slots").toInt();
        totals[r.lootdrop_id] += r.item_chance;
        rows.append(r);
    }

    // Compute effective drop chance per EQMacEmu loot.cpp algorithm
    for (const auto& r : rows) {
        double table_prob  = r.table_probability / 100.0;
        double multiplier  = (r.table_multiplier > 1) ? r.table_multiplier : 1.0;
        double total_chance = totals.value(r.lootdrop_id, 0.0);

        double p_in_call;
        if (r.droplimit == 0 && r.mindrop == 0) {
            p_in_call = r.item_chance / 100.0;
        } else {
            int n_draws = (r.droplimit > 0) ? r.droplimit : 1;
            p_in_call = (total_chance > 0.0)
                ? 1.0 - std::pow(1.0 - r.item_chance / total_chance, n_draws)
                : 0.0;
        }

        double p_not_drop = std::pow(1.0 - table_prob * p_in_call, multiplier);
        double chance = std::max(0.0, (1.0 - p_not_drop) * 100.0);

        LootItem li;
        li.item_id = r.item_id;
        li.name    = r.name.toStdString();
        li.chance  = static_cast<float>(chance);
        li.slots   = r.slots;
        result.append(li);
    }

    // Sort descending by chance
    std::sort(result.begin(), result.end(),
              [](const LootItem& a, const LootItem& b) { return a.chance > b.chance; });

    return result;
}
