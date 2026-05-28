# Global Loot, Plage Survie, Tests d'Intégration — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ajouter les world drops (global_loot) en section séparée dans Fight tab, afficher la plage de survie min–max dans la table DPS, et créer une suite de tests d'intégration DB validant les formules NPC.

**Architecture:** T0 étend `NpcData` (prérequis partagé) ; T1 ajoute `getNpcGlobalLoot` + affichage ; T2 modifie `buildDpsSlowTable` ; T3 crée un exécutable de test séparé lié à `eq_db`+Qt. T1/T2/T3 sont indépendantes et exécutables en parallèle après T0.

**Tech Stack:** C++17, Qt6 Widgets/SQL, GTest, MariaDB 12.2, CMake preset `windows-x64-debug`.

**Build command — toujours via script `.ps1` :**
```powershell
# Créer C:\Users\fabri\AppData\Local\Temp\build_debug.ps1 :
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
# Exécuter via : & "$env:TEMP\build_debug.ps1"
```

---

## Fichiers modifiés

| Fichier | Tâches |
|---------|--------|
| `src/core/types.h` | T0 |
| `src/db/npc_database.h` | T1 |
| `src/db/npc_database.cpp` | T0, T1 |
| `src/ui/fight_tab.cpp` | T1 (affichage), T2 |
| `tests/test_integration_npc.cpp` | T3 (nouveau) |
| `tests/CMakeLists.txt` | T3 |

---

## Task 0 — NpcData : ajouter bodytype + zone_id (prérequis)

**Files:**
- Modify: `src/core/types.h:35-44`
- Modify: `src/db/npc_database.cpp:118` (SELECT) et `src/db/npc_database.cpp:179` (mapping) et `src/db/npc_database.cpp:183-194` (zone sub-query)

- [ ] **Step 1 : Ajouter les champs à NpcData dans types.h**

Dans `src/core/types.h`, remplacer le struct `NpcData` (ligne 35) :

```cpp
struct NpcData {
    int  id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int  min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int  mr{}, fr{}, cr{}, dr{}, pr{};
    int  npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    int  avoidance{}, slow_mitigation{};
    bool raid_target{}, is_quest{}, encounter{};
    int  zone_type{-1};
    int  bodytype{};
    int  zone_id{};
    std::string name, special_abilities, zone_long_name, race_name, class_name;
};
```

- [ ] **Step 2 : Assigner bodytype dans getNpcById**

Dans `src/db/npc_database.cpp`, après la ligne `npc.encounter = q.value("encounter").toBool();` (ligne ~179), ajouter :

```cpp
    npc.bodytype        = q.value("bodytype").toInt();
```

(`nt.bodytype` est déjà dans le SELECT à la ligne 118 — aucune modification SQL nécessaire.)

- [ ] **Step 3 : Ajouter zone_id à la sous-query zone**

Dans `getNpcById`, modifier la chaîne SQL de la sous-query zone (ligne ~183) :

```cpp
    zq.prepare(
        "SELECT MIN(z.long_name) AS zone_long_name, MIN(z.type) AS zone_type,"
        " MIN(z.zoneidnumber) AS zone_id"
        " FROM spawnentry se"
        " JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
        " JOIN zone z ON z.short_name = s2.zone"
        " WHERE se.npcID = :id"
    );
```

Puis dans le bloc `if (zq.exec() && zq.next())`, après la ligne qui assigne `npc.zone_type`, ajouter :

```cpp
        npc.zone_id = zq.value("zone_id").toInt();
```

- [ ] **Step 4 : Build de vérification**

Créer `$env:TEMP\build_debug.ps1` avec le contenu décrit dans l'en-tête, l'exécuter.
Résultat attendu : compilation sans erreur. Les 28 tests existants doivent toujours passer (`ctest --test-dir build/debug --output-on-failure`).

- [ ] **Step 5 : Commit T0**

```bash
git add src/core/types.h src/db/npc_database.cpp
git commit -m "feat(npc): NpcData + bodytype + zone_id — prérequis global_loot"
```

---

## Task 1 — #3 : getNpcGlobalLoot + affichage World drops

**Files:**
- Modify: `src/db/npc_database.h`
- Modify: `src/db/npc_database.cpp`
- Modify: `src/ui/fight_tab.cpp`

### 1a — Extraire computeLootChances

- [ ] **Step 1 : Ajouter la fonction statique computeLootChances**

Dans `src/db/npc_database.cpp`, **avant** la définition de `getNpcLoot` (ligne ~263), insérer :

```cpp
// ── computeLootChances ────────────────────────────────────────────────────
// Lit les lignes d'un QSqlQuery déjà exécuté et calcule les drop chances
// selon l'algorithme EQMacEmu loot.cpp. Colonnes requises : item_id, name,
// item_chance, table_probability, table_multiplier, droplimit, mindrop,
// lootdrop_id, slots, nodrop, classes, races, reqlevel.
static QList<LootItem> computeLootChances(QSqlQuery& q) {
    struct RawRow {
        int    item_id{}, slots{}, nodrop{};
        int    classes{65535}, races{65535}, reqlevel{};
        double item_chance{}, table_probability{}, table_multiplier{};
        int    droplimit{}, mindrop{}, lootdrop_id{};
        QString name;
    };
    QList<RawRow> rows;
    QMap<int, double> totals;

    while (q.next()) {
        RawRow r;
        r.item_id           = q.value("item_id").toInt();
        r.name              = q.value("name").toString();
        r.item_chance       = q.value("item_chance").toDouble();
        r.table_probability = q.value("table_probability").toDouble();
        r.table_multiplier  = q.value("table_multiplier").toDouble();
        r.droplimit         = q.value("droplimit").toInt();
        r.mindrop           = q.value("mindrop").toInt();
        r.lootdrop_id       = q.value("lootdrop_id").toInt();
        r.slots             = q.value("slots").toInt();
        r.nodrop            = q.value("nodrop").toInt();
        r.classes           = q.value("classes").isNull() ? 65535 : q.value("classes").toInt();
        r.races             = q.value("races").isNull()   ? 65535 : q.value("races").toInt();
        r.reqlevel          = q.value("reqlevel").toInt();
        totals[r.lootdrop_id] += r.item_chance;
        rows.append(r);
    }

    QList<LootItem> result;
    for (const auto& r : rows) {
        double table_prob   = r.table_probability / 100.0;
        double multiplier   = (r.table_multiplier > 1) ? r.table_multiplier : 1.0;
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
        li.item_id    = r.item_id;
        li.name       = r.name.toStdString();
        li.chance     = static_cast<float>(chance);
        li.item_slots = r.slots;
        li.nodrop     = r.nodrop;
        li.classes    = r.classes;
        li.races      = r.races;
        li.reqlevel   = r.reqlevel;
        result.append(li);
    }

    std::sort(result.begin(), result.end(),
              [](const LootItem& a, const LootItem& b) { return a.chance > b.chance; });
    return result;
}
```

- [ ] **Step 2 : Simplifier getNpcLoot pour utiliser computeLootChances**

Dans `getNpcLoot`, remplacer tout le bloc à partir de `struct RawRow` jusqu'à `return result;` (lignes ~287-353) par :

```cpp
    return computeLootChances(q);
```

La partie qui reste dans `getNpcLoot` est uniquement : la vérification `loottableId == 0`, la préparation SQL, le bind `:id`, l'exécution et la vérification d'erreur. Concrètement `getNpcLoot` devient :

```cpp
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
        " i.slots, i.nodrop, i.classes, i.races, i.reqlevel"
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
    return computeLootChances(q);
}
```

- [ ] **Step 3 : Build de vérification après refactor**

Exécuter `build_debug.ps1`. Les 28 tests doivent passer (comportement de `getNpcLoot` inchangé).

### 1b — Implémenter getNpcGlobalLoot

- [ ] **Step 4 : Déclarer getNpcGlobalLoot dans npc_database.h**

Dans `src/db/npc_database.h`, ajouter après la déclaration de `getNpcLoot` :

```cpp
    // Retourne les world drops (global_loot) applicables à ce NPC.
    // Filtres : level, race, bodytype, zone_id (NULL = wildcard). Max 50 items.
    [[nodiscard]] QList<LootItem> getNpcGlobalLoot(int level, int race,
                                                   int bodytype, int zone_id);
```

- [ ] **Step 5 : Implémenter getNpcGlobalLoot dans npc_database.cpp**

Ajouter après la fin de `getNpcLoot` :

```cpp
// ── getNpcGlobalLoot ──────────────────────────────────────────────────────

QList<LootItem> NpcDatabase::getNpcGlobalLoot(int level, int race,
                                              int bodytype, int zone_id) {
    QList<LootItem> result;
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT i.id AS item_id, i.Name AS name,"
        " lde.chance AS item_chance, lde.equip_item, lde.lootdrop_id,"
        " lte.probability AS table_probability,"
        " lte.multiplier  AS table_multiplier,"
        " lte.droplimit, lte.mindrop,"
        " i.slots, i.nodrop, i.classes, i.races, i.reqlevel"
        " FROM global_loot gl"
        " JOIN loottable_entries lte ON lte.loottable_id = gl.loottable_id"
        " JOIN lootdrop_entries  lde ON lde.lootdrop_id  = lte.lootdrop_id"
        " JOIN items             i   ON i.id             = lde.item_id"
        " WHERE gl.enabled = 1"
        "   AND (gl.min_level = 0 OR gl.min_level <= :level)"
        "   AND (gl.max_level = 0 OR gl.max_level >= :level)"
        "   AND (gl.race     IS NULL OR gl.race     = '' OR gl.race     = :race)"
        "   AND (gl.bodytype IS NULL OR gl.bodytype = '' OR gl.bodytype = :bodytype)"
        "   AND (gl.zone     IS NULL OR gl.zone     = '' OR gl.zone     = :zone_id)"
    );
    q.bindValue(":level",    level);
    q.bindValue(":race",     QString::number(race));
    q.bindValue(":bodytype", QString::number(bodytype));
    q.bindValue(":zone_id",  QString::number(zone_id));
    if (!q.exec()) {
        qWarning() << "getNpcGlobalLoot failed:" << q.lastError().text();
        return result;
    }
    return computeLootChances(q);
}
```

- [ ] **Step 6 : Build de vérification**

Exécuter `build_debug.ps1`. Résultat attendu : compilation sans erreur.

### 1c — Affichage dans Fight tab

- [ ] **Step 7 : Ajouter la section World drops dans buildLeftPanel**

Dans `src/ui/fight_tab.cpp`, **après** la ligne `layout->addWidget(fLoot);` (ligne ~466) et **avant** `layout->addStretch();`, insérer :

```cpp
    // World drops (global_loot)
    {
        auto globalLoot = _npcDb->getNpcGlobalLoot(
            npc.level, npc.race, npc.bodytype, npc.zone_id);
        if (!globalLoot.isEmpty()) {
            auto [fGlobal, flGlobal] = sectionFrame("#78909c");
            flGlobal->addWidget(sectionLabel("World drops", "#78909c"));
            auto* note = new QLabel("(filtres zone/race appliqu\xc3\xa9s)");
            note->setStyleSheet(
                "color:#444444;font-size:11px;font-style:italic;background:transparent;");
            flGlobal->addWidget(note);
            for (const auto& item : globalLoot) {
                bool equippable = item.item_slots > 0;
                bool usable     = charCanEquip(item, _charInfo);
                auto* row = new QWidget;
                row->setStyleSheet("background:transparent;");
                auto* rowL = new QHBoxLayout(row);
                rowL->setContentsMargins(0, 1, 0, 1); rowL->setSpacing(4);

                auto* btn = new QPushButton(
                    QString("%1  %2%")
                        .arg(QString::fromStdString(item.name))
                        .arg(item.chance, 0, 'f', 1));
                btn->setFlat(true);
                btn->setStyleSheet(QString(
                    "QPushButton{color:%1;background:transparent;text-align:left;"
                    "font-size:13px;padding:0 2px;border:none;}"
                    "QPushButton:hover{color:#ffffff;}")
                    .arg(!equippable ? "#555555" : usable ? "#e0e0e0" : "#666666"));
                if (item.item_id > 0)
                    connect(btn, &QPushButton::clicked,
                            [this, id = item.item_id] { onLootClicked(id); });
                rowL->addWidget(btn);

                if (item.nodrop) {
                    auto* nd = new QLabel("NO DROP");
                    nd->setStyleSheet(
                        "color:#ef5350;font-size:10px;background:transparent;");
                    rowL->addWidget(nd);
                }
                rowL->addStretch();
                flGlobal->addWidget(row);
            }
            layout->addWidget(fGlobal);
        }
    }
```

- [ ] **Step 8 : Build final T1 + test visuel**

Exécuter `build_debug.ps1`. Lancer `build/debug/bin/EqQuarmCompanion.exe`. Chercher un NPC niveau 35+ (ex : un boss Luclin). Vérifier que la section "World drops" apparaît sous le loot normal avec items cliquables. Vérifier qu'un NPC niveau 1 sans zone ne montre pas de world drops inutiles.

- [ ] **Step 9 : Commit T1**

```bash
git add src/db/npc_database.h src/db/npc_database.cpp src/ui/fight_tab.cpp
git commit -m "feat(fight): global_loot section World drops + refactor computeLootChances"
```

---

## Task 2 — #4 : Plage de survie min–max dans buildDpsSlowTable

**Files:**
- Modify: `src/ui/fight_tab.cpp:720-754`

- [ ] **Step 1 : Modifier la cellule DPS dans buildDpsSlowTable**

Dans `src/ui/fight_tab.cpp`, à l'intérieur de la boucle `for (int ci = ...)` dans `buildDpsSlowTable`, remplacer le bloc de lignes 720–754 :

```cpp
            float meleeSlowed = dmg.est_dps * (atkSpeed / 100.f);
            float total = meleeSlowed + spDps;
            float surv  = (total > 0.f && hp > 0) ? hp / total : 0.f;

            const char* sc = surv >= 120.f ? kGreen : (surv >= 60.f ? kOrange : kRed);
            QString survStr = surv > 0.f ? QString("~%1s").arg(surv, 0, 'f', 0) : "?";
            const char* dpsC = !landPct ? "#888888" : "#e0e0e0";

            float minTotal = dmg.min_dps * (atkSpeed / 100.f) + spDps;
            float maxTotal = dmg.max_dps * (atkSpeed / 100.f) + spDps;
```

Par :

```cpp
            float minTotal = dmg.min_dps * (atkSpeed / 100.f) + spDps;
            float maxTotal = dmg.max_dps * (atkSpeed / 100.f) + spDps;

            float meleeSlowed = dmg.est_dps * (atkSpeed / 100.f);
            float total = meleeSlowed + spDps;
            float surv  = (total > 0.f && hp > 0) ? static_cast<float>(hp) / total : 0.f;

            const char* sc = surv >= 120.f ? kGreen : (surv >= 60.f ? kOrange : kRed);
            QString survStr = surv > 0.f ? QString("~%1s").arg(surv, 0, 'f', 0) : "?";

            // Plage survie min–max (affichée si écart > 10%)
            {
                float survMin = (maxTotal > 0.f && hp > 0)
                                ? static_cast<float>(hp) / maxTotal : 0.f;
                float survMax = (minTotal > 0.f && hp > 0)
                                ? static_cast<float>(hp) / minTotal : 0.f;
                if (survMin > 0.f && survMax > survMin * 1.10f) {
                    QString capStr = (survMax >= 999.f)
                        ? "&gt;999"
                        : QString::number(static_cast<int>(survMax));
                    survStr += QString(" <span style='color:#555555;font-size:12px'>"
                                       "[%1\xe2\x80\x93%2s]</span>")
                               .arg(static_cast<int>(survMin)).arg(capStr);
                }
            }

            const char* dpsC = !landPct ? "#888888" : "#e0e0e0";
```

- [ ] **Step 2 : Build + test visuel**

Exécuter `build_debug.ps1`. Lancer l'app. Charger un NPC avec min_hit ≠ max_hit (ex : un boss avec grande variance). Vérifier que les cellules affichent `~Xs [Amin–Amaxs]` quand la variance est suffisante. Vérifier qu'un NPC sans variance DPS ne montre pas de plage.

- [ ] **Step 3 : Commit T2**

```bash
git add src/ui/fight_tab.cpp
git commit -m "feat(fight): plage survie min-max dans table DPS×slow"
```

---

## Task 3 — #5 : Tests d'intégration DB

**Files:**
- Create: `tests/test_integration_npc.cpp`
- Modify: `tests/CMakeLists.txt`

### 3a — CMakeLists

- [ ] **Step 1 : Ajouter la target optionnelle dans tests/CMakeLists.txt**

Ajouter à la fin de `tests/CMakeLists.txt` :

```cmake
option(BUILD_INTEGRATION_TESTS "Build DB integration tests (requires live Quarm DB)" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(eq_integration_tests
        test_integration_npc.cpp)
    target_link_libraries(eq_integration_tests PRIVATE
        eq_core eq_db GTest::gtest
        Qt6::Core Qt6::Sql Qt6::Network Qt6::Concurrent)
    target_include_directories(eq_integration_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src")
    # Pas de gtest_discover_tests → n'apparaît pas dans ctest standard
endif()
```

### 3b — Fichier de test

- [ ] **Step 2 : Créer tests/test_integration_npc.cpp**

```cpp
#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QVariant>
#include "core/npc_analysis.h"
#include "core/types.h"
#include "db/db_connection.h"
#include "db/npc_database.h"

// ── Helpers ───────────────────────────────────────────────────────────────

static int findNpcIdByLevel(int level) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare("SELECT id FROM npc_types WHERE level = :lv AND mindmg > 0 LIMIT 1");
    q.bindValue(":lv", level);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

static int findNpcIdWithSa12() {
    QSqlQuery q(DbConnection::instance().db());
    q.exec("SELECT id FROM npc_types WHERE special_abilities LIKE '%12,1%' LIMIT 1");
    return (q.isActive() && q.next()) ? q.value(0).toInt() : 0;
}

static int findNpcIdHighMr(int minMr) {
    QSqlQuery q(DbConnection::instance().db());
    q.prepare("SELECT id FROM npc_types WHERE MR >= :mr AND level >= 50 LIMIT 1");
    q.bindValue(":mr", minMr);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

// ── Fixture ───────────────────────────────────────────────────────────────

static NpcDatabase* s_npcDb   = nullptr;
static bool         s_dbReady = false;

class NpcIntegrationTest : public ::testing::Test {
public:
    static void SetUpTestSuite() {
        DbConfig cfg;
        cfg.host     = "localhost";
        cfg.port     = 3306;
        cfg.user     = "root";
        cfg.password = "rooteq";
        cfg.database = "quarm";
        s_dbReady = DbConnection::instance().connect(cfg);
        if (s_dbReady) s_npcDb = new NpcDatabase();
    }
    static void TearDownTestSuite() {
        delete s_npcDb;
        s_npcDb = nullptr;
    }

protected:
    void SetUp() override {
        if (!s_dbReady)
            GTEST_SKIP() << "Quarm DB non accessible (localhost:3306 quarm)";
    }
};

// ── Groupe A : slowLandPct ────────────────────────────────────────────────

TEST_F(NpcIntegrationTest, A1_SlowLandPct_ResultInRange) {
    int id = findNpcIdByLevel(50);
    if (!id) GTEST_SKIP() << "Pas de NPC lv50 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mr.capped = 100;
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_GE(*pct, 0.f);
    EXPECT_LE(*pct, 100.f);
}

TEST_F(NpcIntegrationTest, A2_SlowLandPct_ImmuneNpcReturnsNullopt) {
    int id = findNpcIdWithSa12();
    if (!id) GTEST_SKIP() << "Pas de NPC avec SA 12 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    auto pct = slowLandPct(*npc, 65, 1, 0);
    EXPECT_FALSE(pct.has_value()) << "NPC immune au slow doit retourner nullopt";
}

TEST_F(NpcIntegrationTest, A3_SlowLandPct_HighMrLowChance) {
    int id = findNpcIdHighMr(200);
    if (!id) GTEST_SKIP() << "Pas de NPC MR>=200 lv50+ dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mr.capped = 0;  // aucun resist debuff ni resist joueur
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_LE(*pct, 30.f) << "Mob MR>=200 difficile a slow sans debuff";
}

TEST_F(NpcIntegrationTest, A4_SlowLandPct_DebuffIncreasesChance) {
    int id = findNpcIdHighMr(150);
    if (!id) GTEST_SKIP() << "Pas de NPC MR>=150 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    float pctNoDebuff  = slowLandPct(*npc, 65, 1, 0).value_or(0.f);
    float pctWithDebuff = slowLandPct(*npc, 65, 1, -60).value_or(0.f);  // Malo = -60 MR
    EXPECT_GT(pctWithDebuff, pctNoDebuff)
        << "Debuff MR doit augmenter la chance de slow";
}

TEST_F(NpcIntegrationTest, A5_SlowLandPct_LowLevelEasyToSlow) {
    int id = findNpcIdByLevel(10);
    if (!id) GTEST_SKIP() << "Pas de NPC lv10 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mr.capped = 200;
    auto pct = slowLandPct(*npc, 65, 1, 0);
    ASSERT_TRUE(pct.has_value());
    EXPECT_GE(*pct, 50.f) << "NPC lv10 avec joueur lv65 MR200 doit être easy to slow";
}

// ── Groupe B : incomingDamage ─────────────────────────────────────────────

TEST_F(NpcIntegrationTest, B1_IncomingDamage_MinLeEstLeMax) {
    int id = findNpcIdByLevel(55);
    if (!id) GTEST_SKIP() << "Pas de NPC lv55 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500; pt.ac = 800;
    auto r = incomingDamage(*npc, pt, "Warrior", 65, "none");
    EXPECT_LE(r.min_dps, r.est_dps + 0.01f)
        << "min_dps doit être <= est_dps";
    EXPECT_GE(r.max_dps, r.est_dps - 0.01f)
        << "max_dps doit être >= est_dps";
    EXPECT_GT(r.est_dps, 0.f);
}

TEST_F(NpcIntegrationTest, B2_IncomingDamage_DefensiveReducesDps) {
    int id = findNpcIdByLevel(60);
    if (!id) GTEST_SKIP() << "Pas de NPC lv60 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500;
    auto base = incomingDamage(*npc, pt, "Warrior", 65, "none");
    auto def  = incomingDamage(*npc, pt, "Warrior", 65, "defensive");
    EXPECT_LT(def.est_dps, base.est_dps)
        << "Defensive disc doit réduire est_dps";
}

TEST_F(NpcIntegrationTest, B3_IncomingDamage_EvasiveReducesDps) {
    int id = findNpcIdByLevel(60);
    if (!id) GTEST_SKIP() << "Pas de NPC lv60 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals pt; pt.mitigation = 500;
    auto base = incomingDamage(*npc, pt, "Warrior", 65, "none");
    auto evas = incomingDamage(*npc, pt, "Warrior", 65, "evasive");
    EXPECT_LT(evas.est_dps, base.est_dps)
        << "Evasive disc doit réduire est_dps";
}

TEST_F(NpcIntegrationTest, B4_IncomingDamage_HighAcReducesMitDown) {
    int id = findNpcIdByLevel(55);
    if (!id) GTEST_SKIP() << "Pas de NPC lv55 dans la DB";
    auto npc = s_npcDb->getNpcById(id);
    ASSERT_TRUE(npc.has_value());
    PlayerTotals lowAc; lowAc.mitigation = 0;
    PlayerTotals highAc; highAc.mitigation = 1200;
    auto rLow  = incomingDamage(*npc, lowAc,  "Warrior", 65, "none");
    auto rHigh = incomingDamage(*npc, highAc, "Warrior", 65, "none");
    EXPECT_LT(rHigh.mitigation_pct, rLow.mitigation_pct + 0.01f)
        << "Mitigation pct doit diminuer quand player_mit augmente";
}

// ── Groupe C : getNpcGlobalLoot ───────────────────────────────────────────

TEST_F(NpcIntegrationTest, C1_GlobalLoot_Lv35NpcGetsResults) {
    // Box of Abukar (row 1) s'applique à tout NPC lv35+ zone=NULL
    auto items = s_npcDb->getNpcGlobalLoot(35, 0, 0, 0);
    EXPECT_GT(items.size(), 0)
        << "Un NPC lv35 sans filtre zone/race doit avoir des world drops";
}

TEST_F(NpcIntegrationTest, C2_GlobalLoot_ChancesInValidRange) {
    auto items = s_npcDb->getNpcGlobalLoot(50, 0, 0, 0);
    for (const auto& item : items) {
        EXPECT_GT(item.chance, 0.f)
            << "Chance de drop doit être > 0 pour " << item.name;
        EXPECT_LE(item.chance, 100.f)
            << "Chance de drop doit être <= 100 pour " << item.name;
    }
}

TEST_F(NpcIntegrationTest, C3_GlobalLoot_LevelFilterWorks) {
    auto lv1  = s_npcDb->getNpcGlobalLoot(1,  0, 0, 0);
    auto lv50 = s_npcDb->getNpcGlobalLoot(50, 0, 0, 0);
    EXPECT_LE(lv1.size(), lv50.size())
        << "Un NPC lv1 doit avoir <= world drops qu'un NPC lv50";
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 3 : Configurer le preset debug pour BUILD_INTEGRATION_TESTS**

Créer `$env:TEMP\build_integration.ps1` :

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
& $cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=ON
& $cmake --build build/debug --target eq_integration_tests
```

Exécuter via `& "$env:TEMP\build_integration.ps1"`. Résultat attendu : build propre, exécutable `build/debug/bin/eq_integration_tests.exe` créé.

- [ ] **Step 4 : Lancer les tests d'intégration**

Créer `$env:TEMP\run_integration.ps1` :

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
& "build/debug/bin/eq_integration_tests.exe" --gtest_output=xml:build/debug/integration_results.xml
```

Résultat attendu : tous les tests passent ou sont marqués SKIPPED (jamais FAILED). Les tests SKIPPED indiquent que la DB ne contient pas le NPC cherché — acceptable. Un test FAILED indique un vrai problème dans les formules.

- [ ] **Step 5 : Vérifier que ctest standard reste à 28/28**

```powershell
# Créer $env:TEMP\run_ctest.ps1 :
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
Set-Location build/debug
& "C:\Qt\Tools\CMake_64\bin\ctest.exe" --output-on-failure
```

Résultat attendu : `28/28 tests passed`.

- [ ] **Step 6 : Commit T3**

```bash
git add tests/test_integration_npc.cpp tests/CMakeLists.txt
git commit -m "test(integration): suite DB npc — slowLandPct, incomingDamage, globalLoot"
```

---

## Build release final

- [ ] **Build release**

Créer `$env:TEMP\build_release.ps1` :

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/release --target EqQuarmCompanion
```

Résultat attendu : build propre sans warnings supplémentaires.

- [ ] **Commit docs + push**

```bash
git add docs/
git commit -m "docs: plan implémentation global_loot + survie + tests intégration"
git push
```
