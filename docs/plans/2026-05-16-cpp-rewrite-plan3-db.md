# EqQuarmCompanion — Plan 3 : Couche DB

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Prérequis :** Plan 2 terminé — tous les modules `core/` compilent et leurs tests passent.

**Goal:** Implémenter la couche `src/db/` — connexion MySQL, requêtes NPC/items/loot/spells, et scraper BIS. Ces classes utilisent Qt SQL et Qt Network ; elles retournent des types de `core/types.h`.

**Architecture:** `DbConnection` singleton. `NpcDatabase` et `ItemDatabase` contiennent les requêtes SQL. `BisScaper` fait des requêtes HTTP async. Aucune logique métier ici — seulement IO.

**Tech Stack:** Qt6 Sql (QSqlDatabase, QSqlQuery), Qt6 Network (QNetworkAccessManager), QMYSQL driver (runtime plugin).

**Référence Python :** `eq-item-evaluator/core/npc_database.py`, `item_database.py`, `bis_scraper.py`.

---

### Task 1 : `db/db_connection`

**Files:**
- Create: `src/db/db_connection.h`
- Modify: `src/db/db_connection.cpp`

- [ ] **Step 1 : Écrire `src/db/db_connection.h`**

```cpp
#pragma once
#include "core/config.h"
#include <QtSql/QSqlDatabase>
#include <QString>

class DbConnection {
public:
    static DbConnection& instance();

    // Ouvre la connexion MySQL avec les paramètres de Config.
    // Retourne true si succès.
    bool connect(const DbConfig& cfg);
    void disconnect();

    [[nodiscard]] bool       isConnected() const;
    [[nodiscard]] QSqlDatabase& db();

private:
    DbConnection() = default;
    QSqlDatabase _db;
};
```

- [ ] **Step 2 : Écrire `src/db/db_connection.cpp`**

```cpp
#include "db/db_connection.h"
#include <QtSql/QSqlError>
#include <QDebug>

DbConnection& DbConnection::instance() {
    static DbConnection inst;
    return inst;
}

bool DbConnection::connect(const DbConfig& cfg) {
    if (_db.isOpen()) _db.close();

    _db = QSqlDatabase::addDatabase("QMYSQL", "main");
    _db.setHostName(QString::fromStdString(cfg.host));
    _db.setPort(cfg.port);
    _db.setDatabaseName(QString::fromStdString(cfg.database));
    _db.setUserName(QString::fromStdString(cfg.user));
    _db.setPassword(QString::fromStdString(cfg.password));

    if (!_db.open()) {
        qWarning() << "DB connection failed:" << _db.lastError().text();
        return false;
    }
    return true;
}

void DbConnection::disconnect() {
    if (_db.isOpen()) _db.close();
    QSqlDatabase::removeDatabase("main");
}

bool DbConnection::isConnected() const { return _db.isOpen(); }
QSqlDatabase& DbConnection::db() { return _db; }
```

> **Note runtime :** Le driver QMYSQL requiert `qsqlmysql.dll` (fourni par Qt) et `libmysql.dll` (fourni par MariaDB, dans `C:\Program Files\MariaDB\MariaDB Server\lib\`). `windeployqt` copie `qsqlmysql.dll` automatiquement ; `libmysql.dll` doit être dans le PATH ou dans le dossier de l'exe.

- [ ] **Step 3 : Commit**

```powershell
git add src/db/db_connection.h src/db/db_connection.cpp
git commit -m "feat(db): DbConnection — singleton QSqlDatabase QMYSQL"
```

---

### Task 2 : `db/npc_database`

**Python source :** `eq-item-evaluator/core/npc_database.py` (209 lignes)

**Files:**
- Create: `src/db/npc_database.h`
- Modify: `src/db/npc_database.cpp`

- [ ] **Step 1 : Écrire `src/db/npc_database.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QList>
#include <QObject>
#include <optional>

class NpcDatabase : public QObject {
    Q_OBJECT
public:
    explicit NpcDatabase(QObject* parent = nullptr);

    // Recherche jusqu'à 50 NPCs dont le nom contient nameFragment.
    // JOIN npc_types → spawnentry → spawn2 → zone (GROUP BY npc_types.id).
    [[nodiscard]] QList<NpcData> searchNpcs(const QString& nameFragment);

    // Retourne tous les champs de npc_types pour ce NPC.
    [[nodiscard]] std::optional<NpcData> getNpcById(int id);

    // Retourne les sorts castés par le NPC (JOIN npc_spells_entries → spells_new).
    [[nodiscard]] QList<SpellData> getNpcSpells(int npcSpellsId);

    // Retourne les items de la loot table (JOIN loottable_entries → lootdrop_entries → items).
    [[nodiscard]] QList<LootItem> getNpcLoot(int loottableId);
};
```

- [ ] **Step 2 : Écrire `src/db/npc_database.cpp`**

Porter les requêtes SQL depuis `npc_database.py`. Exemple pour `searchNpcs` :

```cpp
#include "db/npc_database.h"
#include "db/db_connection.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDebug>

NpcDatabase::NpcDatabase(QObject* parent) : QObject(parent) {}

QList<NpcData> NpcDatabase::searchNpcs(const QString& fragment) {
    QList<NpcData> result;
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(R"(
        SELECT n.id, n.name, n.level, n.hp, n.ac, n.atk, n.accuracy,
               n.min_hit, n.max_hit, n.attack_delay, n.attack_count,
               n.MR, n.CR, n.DR, n.FR, n.PR,
               n.special_abilities, n.npc_spells_id, n.loottable_id,
               n.race, n.class AS npc_class,
               z.long_name AS zone_long_name
        FROM npc_types n
        LEFT JOIN spawnentry se ON se.npcID = n.id
        LEFT JOIN spawn2 s2    ON s2.id     = se.spawngroupID
        LEFT JOIN zone z       ON z.short_name = s2.zone
        WHERE n.name LIKE :frag
        GROUP BY n.id
        LIMIT 50
    )");
    q.bindValue(":frag", "%" + fragment + "%");

    if (!q.exec()) {
        qWarning() << "searchNpcs error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        NpcData n;
        n.id               = q.value("id").toInt();
        n.name             = q.value("name").toString().toStdString();
        n.level            = q.value("level").toInt();
        n.hp               = q.value("hp").toInt();
        n.ac               = q.value("ac").toInt();
        n.atk              = q.value("atk").toInt();
        n.accuracy         = q.value("accuracy").toInt();
        n.min_hit          = q.value("min_hit").toInt();
        n.max_hit          = q.value("max_hit").toInt();
        n.attack_delay     = q.value("attack_delay").toInt();
        n.attack_count     = q.value("attack_count").toInt();
        n.mr               = q.value("MR").toInt();
        n.cr               = q.value("CR").toInt();
        n.dr               = q.value("DR").toInt();
        n.fr               = q.value("FR").toInt();
        n.pr               = q.value("PR").toInt();
        n.special_abilities = q.value("special_abilities").toString().toStdString();
        n.npc_spells_id    = q.value("npc_spells_id").toInt();
        n.loottable_id     = q.value("loottable_id").toInt();
        n.race             = q.value("race").toInt();
        n.npc_class        = q.value("npc_class").toInt();
        n.zone_long_name   = q.value("zone_long_name").toString().toStdString();
        result.append(n);
    }
    return result;
}

std::optional<NpcData> NpcDatabase::getNpcById(int id) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(R"(
        SELECT n.*, z.long_name AS zone_long_name
        FROM npc_types n
        LEFT JOIN spawnentry se ON se.npcID = n.id
        LEFT JOIN spawn2 s2    ON s2.id = se.spawngroupID
        LEFT JOIN zone z       ON z.short_name = s2.zone
        WHERE n.id = :id
        LIMIT 1
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return std::nullopt;
    // Mapper les colonnes — même logique que searchNpcs
    NpcData n;
    n.id   = q.value("id").toInt();
    n.name = q.value("name").toString().toStdString();
    // ... (mapper toutes les colonnes comme dans searchNpcs)
    n.zone_long_name = q.value("zone_long_name").toString().toStdString();
    return n;
}

QList<SpellData> NpcDatabase::getNpcSpells(int npcSpellsId) {
    // Porter depuis npc_database.py::get_npc_spells()
    // SELECT spells_new.* FROM npc_spells_entries
    // JOIN spells_new ON spells_new.id = npc_spells_entries.spellid
    // WHERE npc_spells_entries.npc_spells_id = :id
    // ORDER BY npc_spells_entries.priority ASC
    QList<SpellData> result;
    // ... (même pattern QSqlQuery que searchNpcs)
    return result;
}

QList<LootItem> NpcDatabase::getNpcLoot(int loottableId) {
    // Porter depuis npc_database.py::get_npc_loot()
    // JOIN loottable_entries → lootdrop → lootdrop_entries → items
    QList<LootItem> result;
    // ...
    return result;
}
```

- [ ] **Step 3 : Commit**

```powershell
git add src/db/npc_database.h src/db/npc_database.cpp
git commit -m "feat(db): NpcDatabase — searchNpcs, getNpcById, getNpcSpells, getNpcLoot"
```

---

### Task 3 : `db/item_database`

**Python source :** `eq-item-evaluator/core/item_database.py` (389 lignes)

**Files:**
- Create: `src/db/item_database.h`
- Modify: `src/db/item_database.cpp`

- [ ] **Step 1 : Écrire `src/db/item_database.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QObject>
#include <optional>

class ItemDatabase : public QObject {
    Q_OBJECT
public:
    explicit ItemDatabase(QObject* parent = nullptr);

    // Retourne l'item avec tous ses champs + noms d'effets résolus.
    [[nodiscard]] std::optional<ItemData> getItemById(int id);

    // Recherche par nom (pour l'autocomplete dans CharacterTab).
    [[nodiscard]] QList<ItemData> searchItems(const QString& nameFragment,
                                               int limit = 30);
};
```

- [ ] **Step 2 : Écrire `src/db/item_database.cpp`**

```cpp
#include "db/item_database.h"
#include "db/db_connection.h"
#include <QtSql/QSqlQuery>

ItemDatabase::ItemDatabase(QObject* parent) : QObject(parent) {}

std::optional<ItemData> ItemDatabase::getItemById(int id) {
    QSqlQuery q(DbConnection::instance().db());
    // Porter la requête depuis item_database.py::get_item_by_id()
    // SELECT i.*, w.name AS worneffect_name, f.name AS focuseffect_name,
    //        p.name AS proceffect_name
    // FROM items i
    // LEFT JOIN spells_new w ON w.id = i.worneffect
    // LEFT JOIN spells_new f ON f.id = i.focuseffect
    // LEFT JOIN spells_new p ON p.id = i.proceffect
    // WHERE i.id = :id
    q.prepare(R"(
        SELECT i.*,
               w.name AS worneffect_name,
               f.name AS focuseffect_name,
               p.name AS proceffect_name
        FROM items i
        LEFT JOIN spells_new w ON w.id = i.worneffect
        LEFT JOIN spells_new f ON f.id = i.focuseffect
        LEFT JOIN spells_new p ON p.id = i.proceffect
        WHERE i.id = :id LIMIT 1
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return std::nullopt;

    ItemData item;
    item.id           = q.value("id").toInt();
    item.name         = q.value("Name").toString().toStdString();
    item.hp           = q.value("hp").toInt();
    item.mana         = q.value("mana").toInt();
    item.ac           = q.value("ac").toInt();
    item.atk          = q.value("atk").toInt();
    item.astr         = q.value("astr").toInt();
    item.asta         = q.value("asta").toInt();
    item.adex         = q.value("adex").toInt();
    item.aagi         = q.value("aagi").toInt();
    item.aint         = q.value("aint").toInt();
    item.awis         = q.value("awis").toInt();
    item.acha         = q.value("acha").toInt();
    item.mr           = q.value("mr").toInt();
    item.fr           = q.value("fr").toInt();
    item.cr           = q.value("cr").toInt();
    item.dr           = q.value("dr").toInt();
    item.pr           = q.value("pr").toInt();
    item.damage       = q.value("damage").toInt();
    item.delay        = q.value("delay").toInt();
    item.itemtype     = q.value("itemtype").toInt();
    item.haste        = q.value("haste").toInt();
    item.hp_regen     = q.value("regen").toInt();
    item.mana_regen   = q.value("manaregen").toInt();
    item.slots        = q.value("slots").toInt();
    item.reqlevel     = q.value("reqlevel").toInt();
    item.lore         = q.value("lore").toString().toStdString();
    item.worneffect   = q.value("worneffect").toInt();
    item.focuseffect  = q.value("focuseffect").toInt();
    item.proceffect   = q.value("proceffect").toInt();
    item.worneffect_name  = q.value("worneffect_name").toString().toStdString();
    item.focuseffect_name = q.value("focuseffect_name").toString().toStdString();
    item.proceffect_name  = q.value("proceffect_name").toString().toStdString();
    return item;
}

QList<ItemData> ItemDatabase::searchItems(const QString& frag, int limit) {
    QList<ItemData> result;
    QSqlQuery q(DbConnection::instance().db());
    q.prepare("SELECT id, Name AS name, slots, reqlevel FROM items "
              "WHERE Name LIKE :f LIMIT :l");
    q.bindValue(":f", "%" + frag + "%");
    q.bindValue(":l", limit);
    if (!q.exec()) return result;
    while (q.next()) {
        ItemData item;
        item.id       = q.value("id").toInt();
        item.name     = q.value("name").toString().toStdString();
        item.slots    = q.value("slots").toInt();
        item.reqlevel = q.value("reqlevel").toInt();
        result.append(item);
    }
    return result;
}
```

- [ ] **Step 3 : Commit**

```powershell
git add src/db/item_database.h src/db/item_database.cpp
git commit -m "feat(db): ItemDatabase — getItemById, searchItems"
```

---

### Task 4 : `db/bis_scraper`

**Python source :** `eq-item-evaluator/core/bis_scraper.py`

**Files:**
- Create: `src/db/bis_scraper.h`
- Modify: `src/db/bis_scraper.cpp`

- [ ] **Step 1 : Écrire `src/db/bis_scraper.h`**

```cpp
#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QList>
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

    // Scrape la liste BIS pour une classe donnée.
    // Appelle callback(entries) quand terminé.
    void fetchBis(const QString& className,
                  std::function<void(QList<BisEntry>)> callback);

private slots:
    void onReplyFinished();

private:
    QNetworkAccessManager _nam;
    std::function<void(QList<BisEntry>)> _callback;

    [[nodiscard]] QList<BisEntry> parseHtml(const QByteArray& html) const;
};
```

- [ ] **Step 2 : Écrire `src/db/bis_scraper.cpp`**

```cpp
#include "db/bis_scraper.h"
#include <QNetworkReply>
#include <QRegularExpression>

BisScaper::BisScaper(QObject* parent) : QObject(parent) {}

void BisScaper::fetchBis(const QString& className,
                          std::function<void(QList<BisEntry>)> cb)
{
    _callback = std::move(cb);
    // Porter l'URL depuis bis_scraper.py
    QString url = "https://www.pqdi.cc/bis/" + className.toLower();
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
    // Porter le parsing HTML depuis bis_scraper.py (beautifulsoup → QRegularExpression)
    // Chercher les patterns <td>SlotName</td> <td><a href="/item/ID">ItemName</a></td>
    QList<BisEntry> result;
    static const QRegularExpression re(
        R"(<td[^>]*>([^<]+)</td>\s*<td[^>]*><a href="/item/(\d+)">([^<]+)</a>)",
        QRegularExpression::DotMatchesEverythingOption);
    auto it = re.globalMatch(QString::fromUtf8(html));
    while (it.hasNext()) {
        auto m = it.next();
        BisEntry e;
        e.slotName = m.captured(1).trimmed();
        e.itemId   = m.captured(2).toInt();
        e.itemName = m.captured(3).trimmed();
        result.append(e);
    }
    return result;
}
```

> **Note :** Ajuster le pattern regex selon la structure HTML réelle du site. Consulter `bis_scraper.py` pour les sélecteurs CSS/patterns exacts utilisés par BeautifulSoup.

- [ ] **Step 3 : Compiler `eq_db`**

```powershell
cmake --build build/debug --target eq_db
```

Attendu : compilation sans erreur.

- [ ] **Step 4 : Commit**

```powershell
git add src/db/bis_scraper.h src/db/bis_scraper.cpp
git commit -m "feat(db): BisScaper — QNetworkAccessManager + HTML parsing"
```

---

### Task 5 : Intégration DB dans `main.cpp`

**Files:**
- Modify: `src/main.cpp` — connecter la DB au démarrage

- [ ] **Step 1 : Mettre à jour `src/main.cpp`**

```cpp
// Ajouter après Config config(cfgPath, defsPath); :
auto dbCfg = config.getDbConfig();
bool dbOk = DbConnection::instance().connect(dbCfg);
if (!dbOk) {
    qWarning() << "Impossible de connecter à la DB — certaines fonctions seront désactivées";
}
```

Ajouter l'include en tête de fichier :
```cpp
#include "db/db_connection.h"
```

- [ ] **Step 2 : Compiler et lancer**

```powershell
cmake --build build/debug --target EqQuarmCompanion
.\build\debug\bin\EqQuarmCompanion.exe
```

Attendu : fenêtre s'ouvre. Si MariaDB est lancé et les credentials dans `config.json` sont corrects, la DB se connecte (log silencieux). Pas de crash.

- [ ] **Step 3 : Commit**

```powershell
git add src/main.cpp
git commit -m "feat: connexion DB au démarrage depuis main.cpp"
```

---

### Vérification finale du Plan 3

- [ ] `cmake --build build/debug` → 0 erreur
- [ ] `.\build\debug\bin\EqQuarmCompanion.exe` → se lance sans crash
- [ ] DB connectée si MariaDB actif (vérifier avec `DbConnection::instance().isConnected()` en debug)

**Prêt pour Plan 4 (UI Character tab) quand les vérifications passent.**
