# EQ Item Evaluator — Design Spec

_Dernière mise à jour : 2026-05-02 — reflète l'état réel du code._

## Context

App desktop Windows pour évaluer si un nouvel item est un upgrade pour un personnage EverQuest sur le serveur Project Quarm (EQEmu). Connexion à une base MariaDB locale avec le schéma EQEmu standard.

**Problème résolu :** comparer manuellement les stats d'un item avec l'équipement actuel est fastidieux, et vérifier sa position BiS pour sa classe/extension demande de consulter eqprogression.com séparément.

---

## Décisions de design

| Aspect | Choix |
|--------|-------|
| Plateforme | App desktop Windows |
| Tech stack | Python + PyQt6 |
| Source stats items | MariaDB locale (EQEmu standard, table `items` + JOINs `spells_new`) |
| Source BiS lists | Scraping eqprogression.com, mis en cache JSON local |
| Fichiers personnages | `[nom]-Quarmy.txt` dans un répertoire configurable |
| Nb personnages max | 10 (auto-détectés) |
| Définition upgrade | Score pondéré (poids configurables par classe) + position BiS |
| Layout UI | Combobox de sélection personnage + QStackedWidget, scrollable |
| Vue comparaison | Compacte côte à côte (équipé vs nouvel item, delta coloré) |
| Recherche items | Combobox éditable — recherche lancée au clic sur la flèche, filtre items portables |
| Config poids | Sliders par stat, un tab par classe dans les settings |
| Config stockée | `config.json` local |

---

## Architecture

```
eq-item-evaluator/
├── main.py
├── config.json                  # DB host/port/db, chemin EQ files, extension courante, poids par classe
├── bis_cache/                   # JSON par classe+extension scrappés depuis eqprogression.com
├── core/
│   ├── config.py                # Load/save config.json
│   ├── character_parser.py      # Parse [name]-Quarmy.txt → équipement + info perso + AAs
│   ├── item_database.py         # MariaDB connector : items + spells_new + altadv_vars + aa_effects
│   ├── stat_caps.py             # Caps par extension, listes et labels de stats
│   ├── stats_calculator.py      # Formules EQ : HP/Mana/AC/ATK/résists par classe/race/level
│   ├── upgrade_evaluator.py     # Score pondéré + lookup BiS position
│   └── bis_scraper.py           # Scrape eqprogression.com, cache JSON
└── ui/
    ├── main_window.py           # QMainWindow, combobox personnage + QStackedWidget + menu Settings
    ├── character_tab.py         # QWidget : bandeau stats + recherche + comparaison
    └── settings_dialog.py       # Config MariaDB, répertoire EQ, sliders poids par classe
```

---

## Composants — détail

### `character_parser.py`

- Lit `[name]-Quarmy.txt` (format EQ : colonnes séparées par tabulations)
- `parse(filepath)` → `{slot_name: item_name}` pour les items équipés
  - Gestion des slots agrégés : `Ear` → `Left Ear`/`Right Ear`, `Wrist` → `Left Wrist`/`Right Wrist`, `Fingers` → `Left Finger`/`Right Finger`
- `parse_character_info(filepath)` → `CharacterInfo(name, class_name, level, race_id, base_stats)`
  - `base_stats` = stats de base du perso (STR/STA/AGI/DEX/WIS/INT/CHA) depuis les colonnes `Base*`
- `parse_aa(filepath)` → `[(eqmacid, rank), ...]` depuis la section `AAIndex`
- `find_character_files(directory)` → liste des fichiers `*-Quarmy.txt`
- `get_character_name(filepath)` → nom du perso depuis le nom de fichier

### `item_database.py`

- Connexion MariaDB via `mysql-connector-python`
- `search_items(query)` → liste d'items avec JOIN spells_new (worn/focus/proc effects), LIMIT 50, triés par longueur de nom
- `get_item_by_name(name)` → dict complet d'un item
- `get_spells_by_ids(ids)` → dict `{id: spell_dict}` avec résolution des références SPA 139
- `get_aa_stats(purchases)` → dict de stats sommées depuis `altadv_vars` + `aa_effects`
- `test_connection()` → bool

**Colonnes items (extraites) :** `id, name, ac, hp, mana, damage, delay, itemtype, astr, asta, aagi, adex, awis, aint, acha, cr, fr, dr, mr, pr, slots, classes, races, reqlevel, worneffect, worneffect_name, focuseffect, focuseffect_name, proceffect, proceffect_name, wornlevel, atk` (ATK extrait des slots SPA 2 du sort worn — le champ `atk` n'existe pas dans le schema Quarm)

### `stat_caps.py`

Tables de caps et listes de stats :
- `ATTRIBUTE_CAPS` : 200 (Classic–Velious) / 255 (Luclin+)
- `RESIST_CAPS` : 200 (Classic–Velious) / 300 (Luclin+)
- `ATTRIBUTE_STATS` : astr, asta, aagi, adex, awis, aint, acha
- `RESIST_STATS` : mr, fr, cr, dr, pr
- `UNCAPPED_STATS` : hp, ac, mana, atk
- `BANNER_STATS_ORDER` : ordre d'affichage des 16 stats dans le bandeau
- `STAT_LABELS` : labels lisibles par stat

### `stats_calculator.py`

`compute_totals(items, expansion, level, class_name, race_id, primary_itemtype)` → `{stat: {raw, capped, at_cap, cap}}`

Formules EQ complètes (matchent EQMacEmu source) :
- **Attributs/Résists** : somme des items + caps par expansion
- **Résistances** : + base raciale + bonus de classe par level (Warrior/Ranger/Monk/Paladin/SK/Rogue/Beastlord…)
- **HP** : `level * (class_level_factor/10) * min(STA, 255) / 300 + level_hp + base_hp + 5`
- **Mana** : formule WIS/INT selon type caster (INT pour mages/nécros/wiz/enc, WIS pour clerics/druides/shamans/pals/rangers/bsts)
- **AC affiché** : `(avoidance + mitigation) * 1000 / 847` — inclut defense skill, AGI avoidance, bonus non-casters
- **ATK affiché** : `(ToHit + Offense) * 1000 / 744` — inclut offense skill, weapon skill, STR bonus, item ATK (cap 250)

### `upgrade_evaluator.py`

- `evaluate(current_item, new_item, class_weights, bis_data, slot)` → `EvaluationResult`
- `EvaluationResult` : `score_delta`, `stat_deltas`, `is_upgrade`, `bis_position`, `bis_total`
- Score = somme `(new_stat - current_stat) × weight` pour chaque stat de `SCORED_STATS`

### `bis_scraper.py`

- Scrape eqprogression.com par classe/extension → `bis_cache/[Classe]_[Expansion].json`
- `load_cached(class_name, expansion)` → dict ou None si pas en cache
- `fetch(class_name, expansion)` → fetch HTTP + parse + cache
- `fetch_all(expansion)` → fetch toutes les classes

### `character_tab.py`

- Bandeau "Stats actuelles" toujours visible : 16 tuiles colorées (bleu si proche du cap, vert pour stats sans cap)
- Bandeau "Après équipement" affiché lors d'une sélection : même bandeau avec deltas (+/-) vs avant
- Worn effects et Focus effects affichés dans les bandeaux avec tooltips de sorts
- Recherche : `_SearchComboBox` éditable — recherche déclenchée au clic sur la flèche (override `showPopup`), affiche seulement les items portables (filtrés par classe + level + slot valide)
- Sélection d'item : détection automatique du slot via bitmask, boutons de choix si multi-slot
- Cartes de comparaison côte à côte : item actuel (gris) vs nouvel item (vert/rouge)
- Worn / Focus / Proc effects dans les cartes avec tooltips de sorts
- Résumé : score + indication UPGRADE/DOWNGRADE + position BiS

---

## Filtres de recherche

- **Classe** : items non compatibles avec la classe du perso exclus de la liste
- **Level** : items avec `reqlevel` > level du perso exclus de la liste
- **Slot** : items sans slot d'équipement valide (consommables, spells, etc.) exclus de la liste

---

## Flux principal

1. Démarrage → `config.json` chargé → scan répertoire EQ files → combobox personnage + QStackedWidget
2. Personnage sélectionné : `character_parser` charge équipement + info perso (classe/level/race/base_stats) + AAs
3. `compute_totals` avec items équipés + base_stats + AA stats → bandeau "Stats actuelles"
4. L'user tape un nom dans la `_SearchComboBox` + clique la flèche → résultats filtrés (classe/level/slot)
5. Sélection d'un item → détection du slot → `upgrade_evaluator` calcule delta + BiS position
6. Bandeau "Après équipement" affiché avec deltas
7. Comparaison côte à côte affichée

---

## Dépendances Python

```
PyQt6>=6.6.0
mysql-connector-python>=8.3.0
requests>=2.31.0
beautifulsoup4>=4.12.0
pytest>=8.0.0
pytest-mock>=3.12.0
```

Packaging : PyInstaller → `.exe` standalone Windows

### Compatibilité PyInstaller

- **`config.json`** : résolu via `sys.executable` (dossier de l'exe) quand `sys.frozen` est vrai, sinon via `__file__`
- **`config_defaults.json`** : résolu via `sys._MEIPASS` (bundle PyInstaller) quand frozen
- **`mysql.connector`** : toutes les connexions utilisent `use_pure=True` — l'extension C `_mysql_connector.pyd` ne fonctionne pas dans un bundle PyInstaller (dépendances natives manquantes)
