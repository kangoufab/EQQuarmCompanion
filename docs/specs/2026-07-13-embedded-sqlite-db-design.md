# BDD embarquée SQLite — remplacement de MySQL externe — Design Spec

**Date :** 2026-07-13

---

## Objectif

Aujourd'hui, chaque utilisateur d'EqQuarmCompanion doit installer et configurer un serveur MySQL/MariaDB local, y importer la BDD `quarm`, puis renseigner host/port/user/password dans les Settings. C'est un prérequis externe lourd pour un companion app.

Le but est d'embarquer les données nécessaires (items, sorts, NPCs, races, zones, loot, AA) directement avec l'application sous forme d'un fichier SQLite livré à côté de l'exécutable, en supprimant entièrement la dépendance MySQL côté app livrée, **sans dégrader les performances**, et en gardant la possibilité de régénérer ces données depuis la BDD Quarm source quand elle évolue.

---

## Contraintes retenues (issues du brainstorming)

- Suppression complète de MySQL côté app livrée (pas de mode "avancé" gardant une connexion MySQL externe) : `DbConfig`, l'onglet "Connexion DB" des Settings, et la clé `"db"` de `config.json`/`config_defaults.json` disparaissent.
- L'outil de régénération est un exécutable C++ dans le repo (pas un script Python externe) — reste dans l'écosystème Qt/CMake du projet.
- L'outil de régénération prend ses paramètres de connexion MySQL en arguments CLI (pas de fichier de config dev-only à maintenir).
- Aucune dégradation de performance par rapport à l'usage actuel (MySQL sur `localhost`).

---

## 1. Portée des données

Tables copiées telles quelles (toutes colonnes, pas de liste blanche à maintenir à la main) car plusieurs tables (`spells_new`, `npc_types`) ont des dizaines de colonnes consommées dynamiquement par le code existant (12 slots d'effet par sort, ~30 colonnes NPC) :

`items`, `spells_new`, `npc_types`, `races`, `spawnentry`, `spawn2`, `zone`, `npc_spells_entries`, `loottable_entries`, `lootdrop_entries`, `altadv_vars`, `aa_effects`.

Tout le reste du schéma Quarm (comptes joueurs, guildes, scripts de quête, état serveur live, etc.) reste exclu — ce n'est ni utilisé par l'app, ni pertinent pour un snapshot de données de référence.

---

## 2. Outil d'export — `tools/db_export`

Nouveau target CMake, exécutable C++ autonome lié à `Qt6::Sql` (driver `QMYSQL` en lecture source, `QSQLITE` en écriture destination) et à `eq_core` si des types partagés sont utiles.

**Interface CLI :**
```
db_export --host <host> --port <port> --user <user> --password <password> --database quarm --out resources/quarm_data.db
```

**Déroulé, pour chaque table de la liste ci-dessus :**
1. Introspection du schéma source via `information_schema.columns` (nom de colonne + type MySQL).
2. Mapping de type simple vers l'affinité SQLite :
   - `INT`, `BIGINT`, `TINYINT`, `SMALLINT`, `MEDIUMINT` → `INTEGER`
   - `FLOAT`, `DOUBLE`, `DECIMAL` → `REAL`
   - tout le reste (`VARCHAR`, `TEXT`, `CHAR`, `ENUM`, ...) → `TEXT`
3. `CREATE TABLE` généré dynamiquement côté SQLite avec ces colonnes/types.
4. `SELECT *` par lots côté MySQL, `INSERT` côté SQLite dans une transaction unique (la vitesse d'exécution de l'outil lui-même n'est pas critique — c'est un outil dev, pas le runtime livré).
5. Recréation des index nécessaires aux jointures/filtres réellement exploités par l'app, pour ne pas dégrader les perfs par rapport au MySQL actuel :
   - clé primaire `id` de chaque table qui en a une (`items`, `spells_new`, `npc_types`, `races`, `spawn2`, `zone`, `npc_spells_entries`, `aa_effects`). Exception : `altadv_vars`, `lootdrop_entries`, `loottable_entries` et `spawnentry` sont des tables de jointure Quarm sans colonne `id` — leur clé primaire est composite et ses colonnes sont déjà couvertes par les index de jointure/filtre listés ci-dessous.
   - `spawnentry.npcID`, `spawn2.spawngroupID`, `zone.short_name`
   - `npc_spells_entries.spellid`
   - `loottable_entries.lootdrop_id`, `lootdrop_entries.item_id`
   - `items.worneffect`, `items.focuseffect`, `items.proceffect`, `items.clickeffect`, `items.scrolleffect` (colonnes de jointure vers `spells_new` dans `item_database.cpp`)
   - `npc_types.race` (jointure vers `races`)

   Les recherches `LIKE '%terme%'` (`searchItems`, `searchNpcs`) restent en full-scan comme aujourd'hui (le FULLTEXT était déjà débranché côté MySQL, cf. note existante dans CLAUDE.md) — pas de régression introduite par le passage à SQLite.

---

## 3. Côté application

- `DbConnection::connect()` : remplace le driver `QMYSQL` par `QSQLITE`, ouvre `QCoreApplication::applicationDirPath() + "/quarm_data.db"` en lecture seule. Signature simplifiée (plus de host/port/user/password).
- Suppression complète :
  - `DbConfig` (`src/core/config.h`) et `Config::getDbConfig()` (`src/core/config.cpp`)
  - Onglet "Connexion DB" de `SettingsDialog` (`buildDbTab()` et champs associés dans `settings_dialog.cpp/.h`)
  - Clé `"db"` dans `resources/config_defaults.json` et tout `config.json` existant (pas de migration nécessaire — la clé est simplement ignorée si présente dans un ancien fichier)
- `main.cpp` : la connexion DB au démarrage n'a plus besoin de lire de config — elle pointe directement sur le fichier bundlé.
- Déploiement : `quarm_data.db` copié à côté de l'exécutable par CMake, même mécanisme `copy_directory_if_different` que `resources/imgs/items/`. L'installeur Inno Setup le récupère automatiquement via `{#BinDir}\*` — aucun changement du script d'installeur nécessaire.

---

## 4. Nettoyage dialecte SQL

Peu de points, déjà identifiés par grep sur `src/db/*.cpp` :

- `CONCAT('Race ', nt.race)` (dans `getNpcById`, `npc_database.cpp`) → `'Race ' || nt.race`
- `CHAR_LENGTH(nt.name)` (dans `searchNpcs`, `npc_database.cpp`) → `LENGTH(nt.name)`
- Suppression pure et simple du code mort `hasNpcsFulltextIndex` / `hasItemsFulltextIndex` et des requêtes `information_schema.STATISTICS` associées (déjà non branché sur les recherches actuelles d'après CLAUDE.md, et de toute façon inapplicable à SQLite).

Le reste des requêtes (JOIN, LEFT JOIN, GROUP BY, COALESCE, CASE WHEN, sous-requêtes) est du SQL standard supporté nativement par SQLite, aucun changement nécessaire.

---

## 5. Régénération des données

Documentée dans CLAUDE.md comme étape dev ponctuelle (au même titre que "toujours régénérer l'installer après un build release") : quand la BDD Quarm source évolue (patch serveur, correction de données), un développeur relance `db_export` contre son MySQL local, remplace `resources/quarm_data.db`, puis rebuild + régénère l'installeur.

---

## 6. Tests

- Les 36 tests `core/` existants (`tests/test_config.cpp`, etc.) ne sont pas affectés — aucune dépendance DB, seule `test_config.cpp` référence `DbConfig`/`getDbConfig` et devra être mise à jour en même temps que leur suppression.
- `tests/test_integration_npc.cpp` (actuellement derrière `BUILD_INTEGRATION_TESTS`, connexion MySQL en dur `localhost:3306/quarm/root/rooteq` dans `SetUpTestSuite`) est adapté pour ouvrir `quarm_data.db` (SQLite) généré par `db_export`, au lieu d'un MySQL live. Il devient un test de régression sur les données exportées plutôt qu'un test d'intégration contre un serveur MySQL externe.

---

## Hors périmètre

- Pas de réintroduction de la recherche FULLTEXT (ni MySQL ni SQLite FTS5) — non demandé, resterait un `LIKE` full-scan comme aujourd'hui.
- Pas de mode de connexion MySQL externe conservé en option avancée.
- Pas de compression/chiffrement du fichier `quarm_data.db` — c'est un snapshot de données de jeu publiques, pas de données sensibles.
