# Onglet Sorts — Design Spec

_Date : 2026-05-02_

## Contexte

Ajout d'un onglet **Sorts** dans l'interface existante du personnage, permettant de sélectionner des sorts bénéfiques, des songs de Bard et des sorts préjudiciables, et de voir l'impact cumulé sur les statistiques du personnage (équipement + sorts).

**Problème résolu :** évaluer les stats effectives d'un personnage en conditions de raid ou de groupe nécessite de tenir compte des buffs actifs. L'outil ne proposait jusqu'ici aucune simulation de sorts.

---

## Décisions de design

| Aspect | Choix |
|--------|-------|
| Intégration UI | `QTabWidget` dans `CharacterTab` : onglet "Équipement" (existant) + onglet "Sorts" |
| Nouveau widget | `SpellsTab(QWidget)` dans `ui/spells_tab.py` |
| Sections | 3 : Sorts bénéfiques (15 slots) / Songs de Bard (6 slots) / Sorts préjudiciables (2 slots) |
| Recherche | Combobox éditable par section, déclenchée au clic flèche (pattern `_SearchComboBox`) |
| Filtre bénéfiques | `goodEffect=1` + classe du personnage + niveau |
| Filtre préjudiciables | `goodEffect=0` + classe du personnage + niveau |
| Filtre songs | `classes8 > 0 AND classes8 <= level` (classe Bard), sans filtre `goodEffect` |
| Calcul stats | `spell_to_stat_dict()` → pseudo-items ajoutés à `compute_totals()` |
| Bandeau stats | Même rendu que l'onglet Équipement — stats totales équipement + sorts |
| Persistance | Aucune — sélection réinitialisée à chaque session |
| Haste / Regen | Affichés dans les tooltips des sorts uniquement, pas dans le bandeau |

---

## Architecture

### Fichiers modifiés

```
eq-item-evaluator/
├── ui/
│   ├── main_window.py          # inchangé
│   ├── widgets.py              # NOUVEAU — SearchComboBox, make_stats_bar (extraits de character_tab)
│   ├── character_tab.py        # wrap dans QTabWidget, instancie SpellsTab, imports depuis widgets.py
│   └── spells_tab.py           # NOUVEAU — SpellsTab widget
└── core/
    ├── item_database.py        # MODIFIÉ — ajout search_spells()
    └── spell_stats.py          # NOUVEAU — spell_to_stat_dict()
```

---

## Composants — détail

### `ui/widgets.py` — NOUVEAU fichier (extractions partagées)

Extrait de `character_tab.py` pour être réutilisé par `SpellsTab` :
- `SearchComboBox` (classe, anciennement `_SearchComboBox`) — combobox éditable avec `showPopup` overridé
- `make_stats_bar(items, expansion, level, class_name, race_id, aa_stats, primary_itemtype)` — construit le `QFrame` de bandeau de stats ; appelé par `CharacterTab` et `SpellsTab`

### `ui/character_tab.py` — modifications

- Le `QVBoxLayout` racine de `CharacterTab.__init__` est remplacé par un `QTabWidget`
- L'onglet **"Équipement"** contient le widget existant inchangé
- L'onglet **"Sorts"** instancie `SpellsTab` avec les mêmes paramètres (`char_info`, `equipped_items`, `config`, `aa_stats`)
- `_SearchComboBox` et `_make_stats_bar` remplacés par les imports depuis `ui/widgets.py`

### `ui/spells_tab.py` — SpellsTab

```python
class SpellsTab(QWidget):
    def __init__(self, char_info, equipped_items, config, aa_stats, parent=None)
```

**Layout vertical :**

```
QVBoxLayout
├── Section "Sorts bénéfiques (0/15)"
│   ├── _SearchComboBox (mode="beneficial")
│   └── QVBoxLayout _beneficial_list  ← entrées sort + bouton ×
├── Section "Songs de Bard (0/6)"
│   ├── _SearchComboBox (mode="songs")
│   └── QVBoxLayout _songs_list
├── Section "Sorts préjudiciables (0/2)"
│   ├── _SearchComboBox (mode="detrimental")
│   └── QVBoxLayout _detrimental_list
└── Bandeau stats (QFrame réutilisant _make_stats_bar)
```

**Format d'affichage d'un sort dans la liste :**
Chaque entrée = `QHBoxLayout` : `QLabel("Nom du sort (Niv. X)")` + stretch + `QPushButton("×")`. Tooltip au survol via `_format_spell_tooltip()` existant.

**Méthodes clés :**
- `_on_spell_selected(spell, mode)` — ajoute le sort à la liste correspondante si pas pleine ; déclenche `_refresh_stats()`
- `_on_spell_removed(spell, mode)` — retire le sort ; déclenche `_refresh_stats()`
- `_refresh_stats()` — recompute `compute_totals(equipped_items + spell_dicts)` et met à jour le bandeau

**Limites de slots :**
- Bénéfiques : 15
- Songs : 6
- Préjudiciables : 2

Quand un slot est plein, la combobox est désactivée jusqu'à libération d'un slot.

### `core/item_database.py` — `search_spells()`

```python
def search_spells(
    self,
    query: str,
    class_name: str,
    level: int,
    mode: str,  # "beneficial" | "detrimental" | "songs"
) -> list[dict]:
```

**Requête SQL selon mode :**

```sql
-- beneficial / detrimental
SELECT id, name, classes{N}, effectid1..12, effect_base_value1..12, formula1..12, max1..12
FROM spells_new
WHERE name LIKE %query%
  AND classes{N} > 0 AND classes{N} <= {level}
  AND goodEffect = {1 ou 0}
ORDER BY CHAR_LENGTH(name) ASC
LIMIT 50

-- songs
SELECT id, name, classes8, effectid1..12, effect_base_value1..12, formula1..12, max1..12
FROM spells_new
WHERE name LIKE %query%
  AND classes8 > 0 AND classes8 <= {level}
ORDER BY CHAR_LENGTH(name) ASC
LIMIT 50
```

`N` = index de classe dans `spells_new.classes{N}` → même mapping que `_CLASS_ID` dans `stats_calculator.py` (Warrior=1 … Beastlord=15).

### `core/spell_stats.py` — `spell_to_stat_dict()`

```python
def spell_to_stat_dict(spell: dict) -> dict[str, int]:
```

Parcourt les 12 slots SPA du sort. Pour chaque slot, si `effectid` est dans le mapping ci-dessous, accumule `effect_base_value` dans le dict résultat.

**Mapping SPA → stat key (à vérifier contre données réelles en DB) :**

| SPA | Stat key |
|-----|----------|
| 1   | `ac`     |
| 2   | `atk`    |
| 4   | `astr`   |
| 5   | `adex`   |
| 6   | `aagi`   |
| 7   | `asta`   |
| 8   | `aint`   |
| 9   | `awis`   |
| 10  | `acha`   |
| 55  | `hp`     |
| 69  | `mr`     |
| 46  | `fr`     |
| 47  | `cr`     |
| 48  | `pr`     |
| 49  | `dr`     |

**SPAs ignorés pour le calcul stats (tooltip uniquement) :**
- SPA 11 — Haste
- SPA 98 — Haste (Bard)
- SPA 100 — HP Regen
- SPA 15 — Mana Regen
- SPA 3 — Movement Speed
- SPA 134–143 — Limits (focus conditions)

---

## Flux principal

1. `CharacterTab` instancie `SpellsTab` avec les mêmes données que l'onglet Équipement
2. L'utilisateur tape un nom dans une `_SearchComboBox` et clique la flèche → `search_spells()` requête la DB
3. Sélection d'un sort → `_on_spell_selected()` l'ajoute à la liste + rafraîchit le bandeau
4. `_refresh_stats()` appelle `spell_to_stat_dict()` sur chaque sort sélectionné, concatène les pseudo-items aux items équipés, appelle `compute_totals()`, met à jour le bandeau
5. Clic `×` → `_on_spell_removed()` → même rafraîchissement

---

## Hors périmètre

- Haste, HP Regen, Mana Regen : tooltip uniquement
- Effets conditionnels (SPA 134–143) : tooltip uniquement
- Persistance des sorts sélectionnés entre sessions
- Interaction entre onglet Équipement et onglet Sorts (les sorts actifs n'influencent pas la comparaison d'items)
- Détection automatique des sorts actifs depuis le fichier personnage

---

## Dépendances

Aucune nouvelle dépendance Python. Réutilise :
- `compute_totals()` de `core/stats_calculator.py`
- `make_stats_bar()` et `SearchComboBox` extraits vers `ui/widgets.py`
- `_format_spell_tooltip()` de `ui/character_tab.py` (importé dans `spells_tab.py`)
