# Design — Global Loot, Plage Survie, Tests d'Intégration

**Date :** 2026-05-29
**Projet :** EqQuarmCompanion
**Priorités :** B (Ergonomie) + C (Fiabilité/Précision)

---

## Contexte

Trois améliorations indépendantes identifiées lors d'une revue du code et de la DB :

1. **#3 global_loot** — La table `global_loot` (world drops EQ) existe en DB et est référencée dans CLAUDE.md mais n'est jamais interrogée dans `NpcDatabase::getNpcLoot`. Les items monde (Fungi Tunic, Fine Steel, etc.) sont absents de la liste de loot Fight tab.
2. **#4 Plage survie min/max** — Le temps de survie affiché (`~Xs`) utilise uniquement `est_dps` (moyenne). Les colonnes `min_dps`/`max_dps` existent mais ne sont pas exploitées pour afficher une plage `[Xmin–Xmax]`.
3. **#5 Tests d'intégration DB** — Les 28 tests existants sont purement unitaires (données hardcodées). Aucun ne valide les formules `slowLandPct` / `incomingDamage` contre de vraies données Quarm.

---

## Architecture

```
T0: NpcData ← bodytype + zone_id  (prérequis partagé)
 ├── T1: getNpcGlobalLoot()        (#3)
 ├── T2: Plage survie min/max      (#4)
 └── T3: Tests d'intégration DB   (#5)
```

T0 est le seul prérequis : `bodytype` est déjà fetchée en SQL (`nt.bodytype`) mais jamais assignée dans `NpcData`. `zone_id` s'ajoute à la sous-query zone existante.

T1, T2, T3 sont **entièrement indépendantes** entre elles et peuvent être implémentées en parallèle une fois T0 terminé.

---

## Fichiers modifiés

| Fichier | Tâches |
|---------|--------|
| `src/core/types.h` | T0 |
| `src/db/npc_database.h` | T1 |
| `src/db/npc_database.cpp` | T0, T1 |
| `src/ui/fight_tab.cpp` | T1 (affichage), T2 |
| `tests/test_integration_npc.cpp` | T3 (nouveau fichier) |
| `CMakeLists.txt` | T3 (nouvelle target) |

---

## T0 — NpcData : bodytype + zone_id

**Fichier :** `src/core/types.h`

Ajouter deux champs à `NpcData` :

```cpp
struct NpcData {
    // ... champs existants ...
    int bodytype{};   // type de corps (humanoid=1, undead=2, animal=3, etc.)
    int zone_id{};    // zoneidnumber de la première zone de spawn (0 si inconnu)
};
```

**Fichier :** `src/db/npc_database.cpp` — `getNpcById()`

1. `bodytype` : déjà dans le SELECT (`nt.bodytype`) — ajouter `npc.bodytype = q.value("bodytype").toInt();` dans le mapping.
2. `zone_id` : modifier la sous-query zone pour inclure `MIN(z.zoneidnumber) AS zone_id`, puis assigner `npc.zone_id = zq.value("zone_id").toInt();`.

---

## T1 — #3 : global_loot dans Fight tab

### Schéma global_loot (Quarm DB)

```
id | description | loottable_id | enabled | min_level | max_level
   | rare | raid | race | class | bodytype | zone
   | min_expansion | max_expansion | content_flags | content_flags_disabled
```

Filtres actifs (EQMacEmu loot.cpp) :
- `enabled = 1`
- `min_level = 0 OR min_level <= npc.level`
- `max_level = 0 OR max_level >= npc.level`
- `race IS NULL OR race = '' OR race = npc.race` (wildcard si NULL)
- `bodytype IS NULL OR bodytype = '' OR bodytype = npc.bodytype`
- `zone IS NULL OR zone = '' OR zone = npc.zone_id`

`content_flags` (expansion gates) ignoré : l'app affichera tous les drops enabled et laissera l'utilisateur juger selon son expansion.

### Nouveau header : `src/db/npc_database.h`

```cpp
// Retourne les world drops (global_loot) applicables à ce NPC, max 50.
[[nodiscard]] QList<LootItem> getNpcGlobalLoot(int level, int race,
                                               int bodytype, int zone_id);
```

### Implémentation : `src/db/npc_database.cpp`

Requête SQL :

```sql
SELECT i.id AS item_id, i.Name AS name,
       i.slots, i.nodrop, i.classes, i.races, i.reqlevel,
       lde.chance AS item_chance, lde.equip_item, lde.lootdrop_id,
       lte.probability AS table_probability,
       lte.multiplier  AS table_multiplier,
       lte.droplimit,  lte.mindrop
FROM global_loot gl
JOIN loottable_entries lte ON lte.loottable_id = gl.loottable_id
JOIN lootdrop_entries  lde ON lde.lootdrop_id  = lte.lootdrop_id
JOIN items             i   ON i.id             = lde.item_id
WHERE gl.enabled = 1
  AND (gl.min_level = 0 OR gl.min_level <= :level)
  AND (gl.max_level = 0 OR gl.max_level >= :level)
  AND (gl.race      IS NULL OR gl.race      = '' OR gl.race      = :race)
  AND (gl.bodytype  IS NULL OR gl.bodytype  = '' OR gl.bodytype  = :bodytype)
  AND (gl.zone      IS NULL OR gl.zone      = '' OR gl.zone      = :zone_id)
```

Calcul des chances : identique à `getNpcLoot` (algorithme EQMacEmu `RawRow` + `totals`). Extraire en fonction utilitaire partagée `computeLootChances(QSqlQuery&)` → `QList<LootItem>` pour éviter la duplication.

### Affichage : `src/ui/fight_tab.cpp`

Dans `refreshNpc()`, après le bloc loot existant, appeler `getNpcGlobalLoot` et si la liste n'est pas vide, ajouter :
- Un séparateur `QFrame::HLine`
- Un label `"World drops"` en gris (`#666666`)
- La liste des items avec le même format que le loot normal (nom + %, clic → `onLootClicked`)
- Note sous le label : `"(filtres zone/race appliqués)"` en italique gris clair

---

## T2 — #4 : Plage survie min/max

**Fichier :** `src/ui/fight_tab.cpp` — `buildDpsSlowTable()`

Modifier le calcul de survie dans la boucle des cellules :

```cpp
float survEst = (total    > 0.f && hp > 0) ? static_cast<float>(hp) / total    : 0.f;
float survMin = (maxTotal > 0.f && hp > 0) ? static_cast<float>(hp) / maxTotal : 0.f;
float survMax = (minTotal > 0.f && hp > 0) ? static_cast<float>(hp) / minTotal : 0.f;

QString survStr = survEst > 0.f ? QString("~%1s").arg(survEst, 0, 'f', 0) : "?";

// N'afficher la plage que si l'écart est significatif (> 10%)
if (survMin > 0.f && survMax > survMin * 1.10f) {
    float capMax = std::min(survMax, 999.f);
    QString capStr = (survMax >= 999.f) ? ">999" : QString::number((int)capMax);
    survStr += QString(" <span style='color:#555555;font-size:12px'>"
                       "[%1–%2s]</span>")
               .arg((int)survMin).arg(capStr);
}
```

La ligne range DPS `[min–max/s]` existante reste inchangée en dessous de la cellule.

**Cas limites :**
- `minTotal = 0` (aucune DPS) → pas de plage.
- `minTotal = maxTotal` → pas de plage (moins de 10% d'écart).
- `survMax ≥ 999s` → afficher `>999s`.

---

## T3 — #5 : Tests d'intégration DB

### Structure

**Nouveau fichier :** `tests/test_integration_npc.cpp`

```cpp
class NpcIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Charger config.json depuis le répertoire courant
        // Si DB inaccessible : GTEST_SKIP() avec message
    }
};
```

### Groupes de tests (≈15 cas)

**Groupe A — slowLandPct :** 6 cas

| Test | NPC cible | Assertion |
|------|-----------|-----------|
| A1 | NPC quelconque lv50 MR=100 | 0 ≤ result ≤ 100 |
| A2 | NPC avec SA 12 (immune slow) | result == 0.0f |
| A3 | NPC lv1 MR=0, player lv65 MR=300 | result ≥ 95.f |
| A4 | NPC lv70 MR=250, no debuff | result ≤ 20.f |
| A5 | Avec Turgur's (MR debuff -40) vs A4 | result > résultat A4 |
| A6 | NPC level = player level, MR=200 | result dans ]0, 60] |

**Groupe B — incomingDamage :** 6 cas

| Test | Assertion |
|------|-----------|
| B1 | min_dps ≤ est_dps ≤ max_dps sur tout NPC lv50+ |
| B2 | est_dps > 0 pour NPC avec min_hit > 0 |
| B3 | Defensive disc réduit est_dps vs no-disc |
| B4 | Evasive disc réduit est_dps vs no-disc |
| B5 | est_dps Defensive < est_dps Evasive |
| B6 | player_mit > 0 pour joueur avec AC > 0 |

**Groupe C — getNpcGlobalLoot :** 3 cas

| Test | Assertion |
|------|-----------|
| C1 | NPC lv35+ race=NULL zone=NULL : retourne au moins 1 item |
| C2 | Chaque item a drop_chance ∈ ]0, 1] |
| C3 | NPC lv1 : retourne moins d'items qu'un NPC lv60 (filtre min_level actif) |

Les NPCs de test sont récupérés par requête `SELECT id FROM npc_types WHERE level = X LIMIT 1` — pas d'IDs hardcodés. Le test est skippé si le résultat est vide.

### CMakeLists.txt

Nouvelle target optionnelle :

```cmake
option(BUILD_INTEGRATION_TESTS "Build DB integration tests (requires live DB)" OFF)

if(BUILD_INTEGRATION_TESTS)
    add_executable(eq_integration_tests
        tests/test_integration_npc.cpp)
    target_link_libraries(eq_integration_tests
        PRIVATE eq_core eq_db GTest::gtest_main)
endif()
```

Ne casse pas le `ctest` existant (`eq_tests` reste identique). Activation : `cmake --preset windows-x64-debug -DBUILD_INTEGRATION_TESTS=ON`.

---

## Acceptance criteria

| # | Critère |
|---|---------|
| 3a | World drops s'affichent dans section séparée si la liste est non vide |
| 3b | Filtre level/race/bodytype/zone fonctionne (NPC lv1 retourne moins que lv60) |
| 3c | Section absente si aucun global_loot ne correspond |
| 4a | Plage `[min–max]` absente si écart < 10% ou si minTotal = 0 |
| 4b | `>999s` affiché si survMax ≥ 999s |
| 4c | Layout existant non dégradé (range DPS reste en dessous) |
| 5a | `ctest` sans `-DBUILD_INTEGRATION_TESTS=ON` reste à 28/28 |
| 5b | Tous les tests d'intégration passent avec DB Quarm accessible |
| 5c | Tests skippés proprement si DB hors ligne |
