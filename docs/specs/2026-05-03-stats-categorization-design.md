# Stats par Catégories — Design Spec

**Date :** 2026-05-03

---

## Objectif

Réorganiser l'affichage des statistiques en **4 catégories thématiques** sous forme d'une **grille 2×2 colorée**, pour remplacer la disposition actuelle (lignes empilées avec en-têtes de catégorie). Améliorer la lisibilité par regroupement sémantique et code couleur.

---

## Catégories

| Catégorie | Stats | Couleur |
|-----------|-------|---------|
| **Mêlée** ⚔ | STR · ATK · Haste | Rouge |
| **Distance** 🏹 | DEX | Ambre |
| **Défense** 🛡 | STA · AGI · HP · AC · HP/tick · MR · FR · CR · DR · PR | Bleu |
| **Sorts** ✨ | INT · WIS · CHA · Mana · Mana/tick | Violet |

Les 19 stats sont conservés à l'identique. Aucun stat n'est ajouté ni retiré.

---

## Layout visuel

### Onglet Sorts (`spells_tab`)

Une seule grille 2×2 affichée par `make_stats_bar` :

```
┌──────────────────────┬──────────────────────┐
│ ⚔ MÊLÉE              │ 🏹 DISTANCE          │
│ [STR][ATK][Haste]    │ [DEX]                │
├──────────────────────┼──────────────────────┤
│ 🛡 DÉFENSE           │ ✨ SORTS             │
│ [STA][AGI][HP][AC]   │ [INT][WIS][CHA]      │
│ [HP/t][MR][FR][CR]   │ [Mana][Mana/t]       │
│ [DR][PR]             │                      │
└──────────────────────┴──────────────────────┘
```

### Onglet Équipement (`character_tab`)

Deux grilles 2×2 **empilées verticalement** :

```
[Stats actuelles]
  [grille 2×2]
  [worn effects actuels]

[Si équipé (Waist)]   ← apparaît quand un item est sélectionné
  [grille 2×2 avec deltas verts/rouges sur les tiles]
  [worn effects si équipé]
```

Les deltas (+15 vert, -5 rouge) apparaissent à droite de chaque valeur quand `ref_totals` est fourni à `make_stats_bar`.

---

## Style des panneaux

Chaque panneau de catégorie :
- **Fond** : teinte sombre thématique
- **Bordure** : 1px de la teinte plus claire
- **En-tête** : label small-caps en couleur d'accent (ex. `⚔ MÊLÉE` en rouge clair)
- **Tiles internes** : style existant conservé (vert pour uncapped, bleu pour at-cap, gris sinon)

Couleurs (hex) :
- Mêlée : `bg #2a1a1a`, `border #5a3a3a`, `accent #e57373`
- Distance : `bg #2a241a`, `border #5a4a3a`, `accent #ffb74d`
- Défense : `bg #1a2236`, `border #3a4a6a`, `accent #64b5f6`
- Sorts : `bg #241a2a`, `border #4a3a5a`, `accent #ba68c8`

---

## Modifications fichiers

### `core/stat_caps.py`
- Ajouter `STAT_CATEGORY_COLORS: dict[str, tuple[str, str, str]]` mappant chaque nom de catégorie à `(bg, border, accent)`
- `STAT_CATEGORIES` reste inchangé (déjà créé en commit précédent)

### `ui/widgets.py`
- Refactorer `make_stats_bar` : remplacer le `QHBoxLayout` actuel des catégories par un `QGridLayout` 2×2
- Chaque panneau utilise les couleurs de `STAT_CATEGORY_COLORS`
- Le paramètre `vertical` est supprimé (non utilisé, plus pertinent avec ce layout)
- Comportement `ref_totals` (deltas) conservé à l'identique
- **Position des panneaux dans la grille** : l'ordre suit `STAT_CATEGORIES` (Mêlée=row 0 col 0, Distance=row 0 col 1, Défense=row 1 col 0, Sorts=row 1 col 1)

### `ui/character_tab.py`
- **Supprimer** l'implémentation custom `_build_stats_section` actuelle (QGridLayout avec lignes par stat + colonnes before/after)
- Remplacer par deux appels à `make_stats_bar` :
  - Premier : `make_stats_bar("Stats actuelles", self._totals_before, ...)` toujours visible
  - Second : `make_stats_bar("Si équipé (...)", totals_after, ref_totals=self._totals_before, ...)` créé dynamiquement dans `_refresh_stats_after`
- Les worn effects (avant/après) restent affichés sous chaque grille via `_populate_effects` existant
- Le mécanisme `_inject_spell_levels` est conservé tel quel

### `ui/spells_tab.py`
Aucun changement nécessaire — utilise déjà `make_stats_bar` qui sera refactoré.

---

## Choix techniques

- **2×2 plutôt que 1×4 horizontal** : meilleur usage de l'espace vertical, lisibilité accrue avec moins de tiles par ligne (Défense a 10 stats — trop large en 1 ligne)
- **Empilé plutôt que côte-à-côte pour la comparaison** : la fenêtre est typiquement étroite, deux grilles 2×2 côte-à-côte seraient compressées
- **Suppression du QGridLayout custom dans character_tab** : élimine la duplication de code (avant : ~100 lignes pour le grid custom ; après : 2 appels à `make_stats_bar`)

---

## Filtrage des catégories par classe (ajout post-spec)

Implémenté dans `core/stat_caps.py` : `CLASS_CATEGORIES` et `categories_for_class()` permettent d'afficher seulement les catégories pertinentes pour chaque classe.

```python
CLASS_CATEGORIES: dict[str, set[str]] = {
    "Warrior":      {"Mêlée", "Défense"},
    "Cleric":       {"Défense", "Sorts"},
    "Paladin":      {"Mêlée", "Défense", "Sorts"},
    "Ranger":       {"Mêlée", "Distance", "Défense", "Sorts"},
    ...
}

def categories_for_class(class_name: str) -> list[tuple[str, list[str]]]:
    ...
```

`make_stats_bar` accepte un paramètre `categories: list[tuple[str, list[str]]] | None` — si `None`, toutes les catégories sont affichées. `CharacterTab` et `SpellsTab` passent `categories=categories_for_class(self._class_name)`.

Règles de filtrage :
- **Distance** incluse seulement pour Ranger, Rogue, Bard, Monk (arcs/lancer)
- Classes sans entrée dans `CLASS_CATEGORIES` → 4 catégories complètes
- `DEX`, `ATK`, `Haste` apparaissent dans **Mêlée ET Distance** (partagés)

---

## Hors scope

- Animations de transition lors de l'apparition de "Si équipé"
- Personnalisation des couleurs par utilisateur
- Réorganisation des stats à l'intérieur des catégories (l'ordre actuel est conservé)
