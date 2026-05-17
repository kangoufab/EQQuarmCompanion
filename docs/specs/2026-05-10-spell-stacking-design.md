# Stacking des Sorts Bénéfiques — Design Spec

**Date :** 2026-05-10  
**Mis à jour :** 2026-05-11

---

## Objectif

Les sorts bénéfiques sélectionnés dans le panneau Bard ne stackent pas tous entre eux. Implémenter la logique de stacking d'EQMacEmu (`buffstacking.cpp`) pour que les calculs de stats soient précis, et afficher un indicateur visuel sur les sorts bloqués ou incompatibles.

---

## Comportement attendu

### Sorts sélectionnés en conflit

Quand deux sorts sélectionnés conflictuent, le perdant est marqué comme **bloqué** :

- Son checkbox reste coché (la sélection utilisateur est préservée)
- Son label devient **barré rouge** avec une icône ⚠
- Son tooltip indique "Bloqué par [nom du sort gagnant]"
- Il ne contribue **aucune stat** aux calculs

### UI prédictive (preview)

Quand un sort est sélectionné, les sorts non encore cochés qui seraient **perdants** contre les sorts déjà sélectionnés sont immédiatement grisés/barrés rouge dans la liste, avant même d'être cochés. Les sorts qui **gagneraient** (overwriteraient un sort sélectionné) restent sélectionnables normalement.

### Sorts self-only

Les sorts avec `targettype = 6` (self-only : Divine Aura, Cannibalize, Blessed Armor, etc.) ne peuvent être castés que sur soi-même. Ils ne s'affichent dans la liste que si la classe parcourue est la même que la classe du personnage. Exemple : un Druid 24 ne voit les sorts self-only Druid que lorsqu'il browse la colonne Druid.

### Niveau max de l'extension

Les sorts affichés sont filtrés par le niveau maximum de l'extension courante :

| Extension      | Niveau max |
|----------------|-----------|
| Classic        | 50        |
| Kunark         | 60        |
| Velious        | 60        |
| Luclin         | 60 (Quarm)|
| Planes of Power| 65        |

### Niveau de cast vs niveau du personnage

Les formules de sort level-scaling (ex. formula=104 → `base + level × 3`) utilisent le **niveau max du caster** (cap de l'extension), pas le niveau du personnage recevant le buff. Exception : les sorts self-only utilisent le niveau du personnage lui-même.

---

## Algorithme (fidèle à `buffstacking.cpp`)

Priorité des règles dans l'ordre :

### 1. SPA 148 — SE_StackingCommand_Block

```
Pour chaque slot du blocker avec SPA = 148 :
  blocked_spa  = base[slot]          # SPA ciblé dans le candidat
  target_slot  = formula[slot] - 201 # slot 0-based dans le candidat
  threshold    = abs(max[slot])

  Si candidate.effectid[target_slot] == blocked_spa :
    cand_value = max ou base du candidat à ce slot

    Vérification "même ligne" : si candidate a aussi SPA 148 avec
    même base+formula → substituer abs(candidate.max) comme valeur

    Si cand_value <= threshold → candidat BLOQUÉ
```

### 2. SPA 149 — SE_StackingCommand_Overwrite

```
Pour chaque slot du overwriter avec SPA = 149 :
  target_spa  = base[slot]
  target_slot = formula[slot] - 201
  threshold   = max[slot]

  Si existing.effectid[target_slot] == target_spa :
    old_value = calc_spell_value(existing, target_slot, level)
    Si old_value <= threshold → existing SUPPRIMÉ (overwriter gagne)
```

### 3. Comparaison slot-par-slot (fallback)

```
Controlled slots = tous les slots ciblés par SPA 148/149 dans l'un ou l'autre sort
→ ces slots sont exclus de la comparaison générique pour éviter le double-traitement

Pour chaque slot N (0–11), sauf SPA 254, 148, 149, et controlled slots :
  si effectid_A[N] == effectid_B[N] :
    si val_A == 0 et val_B == 0 → ignorer (placeholder)
    → Conflit : le sort avec la valeur la plus basse est le PERDANT
    → Égalité : le premier dans la liste gagne
    break
```

Résultat : `conflicts = {loser_id: winner_id}`, `effective_spells` = sorts non-bloqués.

---

## Architecture

### `core/spell_stacking.py`

```python
def _calc_slot_value(spell, slot, level) -> int
def _check_148_blocks(blocker, candidate) -> bool
def _check_149_overwrites(overwriter, existing, level) -> bool
def _controlled_slots(spell) -> set[int]
def _compare_pair(spell_a, spell_b, level) -> tuple[int, int] | None
def find_conflict_in_pool(candidate, pool, level) -> int | None
def check_stacking(spells, level) -> tuple[list[dict], dict[int, int]]
```

- `find_conflict_in_pool` : pour l'UI prédictive — retourne l'id du gagnant si le candidat serait perdant contre un sort du pool, sinon `None`
- `check_stacking` : pour les calculs de stats — retourne (sorts effectifs, dict conflits)

### `core/item_database.py`

`get_beneficial_spells_by_class(class_name, level)` retourne maintenant aussi `targettype` pour permettre le filtrage self-only.

### `ui/spells_tab.py`

- `_EXPANSION_LEVEL_CAP` : dict extension → niveau max
- `_on_class_selected` : filtre self-only (targettype=6) selon si la classe browsée == classe du personnage ; passe le level cap de l'extension à la DB
- `_rebuild_right_panel` : utilise `find_conflict_in_pool` pour colorer en prédictif (rouge barré) les sorts non cochés qui seraient bloqués
- `_refresh_stats` : utilise `check_stacking` + niveau caster (cap extension) sauf pour self-only

### `tests/test_spell_stacking.py`

Tests couverts :
1. Deux sorts sans conflit (SPAs différents) → stackent
2. Conflit slot 1, faible perd
3. Conflit slot N > 0
4. Trois sorts, deux perdants
5. Effectid 254 ignoré
6. Tie → premier gagne
7. SPA 148 : tier supérieur bloque tier inférieur
8. SPA 148 : tier supérieur n'est PAS bloqué par tier inférieur
9. SPA 149 : overwrite si valeur ≤ threshold
10. SPA 149 : ne overwrite PAS si valeur > threshold
11. Premier slot conflictuel décide (les suivants ignorés)

---

## UI — Indicateur visuel

| État | Style |
|------|-------|
| Sélectionné + bloqué | `color: #aa4444; text-decoration: line-through;` + ⚠ dans label, checkbox activé (peut décocher) |
| Non sélectionné + perdant (prédictif) | même style, checkbox désactivé |
| Non sélectionné + disponible | style normal, activé si < cap |
| Non sélectionné + gagnant (overwrite) | style normal, activé |

---

## Caps de résistances (Quarm)

| Extension | Cap résistances |
|-----------|----------------|
| Classic   | 200 |
| Kunark    | 200 |
| Velious   | 200 |
| Luclin    | 500 |
| PoP       | 500 |

---

## Fichiers modifiés

| Fichier | Modification |
|---------|-------------|
| `core/spell_stacking.py` | `check_stacking`, `find_conflict_in_pool`, `_compare_pair`, `_check_148_blocks`, `_check_149_overwrites`, `_controlled_slots` |
| `core/item_database.py` | `get_beneficial_spells_by_class` : ajoute `targettype` |
| `core/stat_caps.py` | `RESIST_CAPS` Luclin/PoP → 500 |
| `ui/spells_tab.py` | `_EXPANSION_LEVEL_CAP`, filtrage self-only, UI prédictive, niveau caster |
| `main.py` | Splash screen `QSplashScreen` au lancement |
| `tests/test_spell_stacking.py` | 11 tests (SPA 148/149 inclus) |
| `tests/test_stats_calculator.py` | Caps résistances mis à jour (500) |
