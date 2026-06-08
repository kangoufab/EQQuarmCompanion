# BIS Highlight — Design Spec

**Date:** 2026-06-08
**Status:** Approved

## Objectif

Marquer visuellement les items qui font partie de la BIS list du personnage (classe + extension en cours) dans 5 surfaces de l'app : grille équipée, lignes de sacs, dropdown de recherche, item cards, et liste de loot de l'onglet Fight.

---

## 1. Source des données BIS

### Pipeline

1. **Classe** : `config.getCharacterClass(charName)` — classe du personnage actif.
2. **Extension** : `config.get("current_expansion")` — chaîne parmi `{"Classic", "Kunark", "Velious", "Luclin", "Planes of Power"}`.
3. **Fetch/cache** : `BisScaper::fetchBis(className, expansion, callback)` gère :
   - Cache JSON local (`%APPDATA%/EqQuarmCompanion/bis_cache/<Class>_<Expansion>.json`, TTL 7 jours)
   - Si cache valide → callback synchrone immédiate avec le set chargé
   - Sinon → fetch async `eqprogression.com`, écriture du cache, callback async
4. **Résultat** : `QSet<QString>` de tous les noms d'items BIS, tous slots confondus. Sert uniquement à `contains(itemName)`.

### Extension du BisScaper

**Avant** : `fetchBis(className, callback)` — expansion "classic" en dur.
**Après** : `fetchBis(className, expansion, callback)` — mapping expansion → slug URL :

| Expansion | Slug |
|---|---|
| Classic | `classic` |
| Kunark | `ruins-of-kunark` |
| Velious | `scars-of-velious` |
| Luclin | `shadows-of-luclin` |
| Planes of Power | `planes-of-power` |

La structure intermédiaire `BisEntry` et `QList<BisEntry>` sont conservées pour le parsing HTML. Le callback public expose le `QSet<QString>` aplati (nom de chaque item, tous slots).

---

## 2. Gestion d'état dans MainWindow

### Nouvelles données membres

```cpp
BisScaper     _bisScaper;   // instance unique, parent = MainWindow
QSet<QString> _bisNames;    // noms BIS courants; vide = pas encore chargé
```

### Nouvelle méthode

```cpp
void MainWindow::refreshBis();
```

Séquence :
1. Lire `className = config.getCharacterClass(charName)` et `expansion = config.get("current_expansion")`
2. Si l'un ou l'autre est vide → `_bisNames.clear()`, propager, return
3. Appeler `_bisScaper.fetchBis(className, expansion, [this](QSet<QString> names) { ... })`
4. Dans le callback : `_bisNames = names`, appeler `_charTab->setBisNames(&_bisNames)`, `_fightTab->setBisNames(&_bisNames)`, déclencher un refresh UI des panels concernés

### Triggers

`refreshBis()` est appelé :
- À la fin de `onCharacterChanged()` (la classe peut avoir changé)
- Quand `InfosTab::onExpansionChanged()` a écrit dans la config (signal existant ou slot connecté)

---

## 3. Propagation aux tabs

### Nouvelle méthode sur CharacterTab et FightTab

```cpp
void setBisNames(const QSet<QString>* bisNames);
```

Stocke `_bisNames = bisNames` (pointeur, non-owning). Les rebuilds suivants utilisent ce pointeur. Après `setBisNames()`, `MainWindow` déclenche `rebuildInventoryPanel()` sur CharacterTab et `refreshStats()` sur FightTab pour que les surfaces se re-rendent avec le nouveau set.

---

## 4. Traitement visuel par surface

**Token couleur** : `kAccentGold = "#ffc947"` (déjà dans `palette.h`).

### Surface 1 — Lignes sacs (`addRow` dans `rebuildInventoryPanel`)

Quand `_bisNames && _bisNames->contains(QString::fromStdString(item->name))` :
- Nom du bouton : `"⭐ " + itemName`
- Couleur normale du bouton : `kAccentGold` (au lieu de `kTextBase`)
- Couleur hover : `kGreen` (inchangée)

### Surface 2 — Dropdown de recherche (QCompleter)

Dans `onSearchPopup()`, quand on ajoute le nom à `names` :
- Si BIS : `names << "⭐ " + QString::fromStdString(item.name)`
- `onItemSelected(index)` retrouve l'item par index dans `_searchResults` — pas besoin de stripper le préfixe.

### Surface 3 — Loot Fight tab (lignes `btn` dans `refreshStats`/build loot)

Quand l'item est BIS et utilisable par le personnage (`charCanEquip`) :
- Texte du bouton : `"⭐ " + name + "  N%"`
- Couleur : `kAccentGold` (au lieu de `kTextPrimary`)

### Surface 4 — Grille équipée (`makeEquipCell`)

Items affichés comme icônes → pas de préfixe texte. À la place, quand l'item est BIS :
- CSS border de la cellule : `border: 1px solid %1` avec `kAccentGold` (au lieu de `kBorderCard`)
- CSS hover : `border-color: #ffd97d` (gold légèrement éclairci)

### Surface 5 — Item card (`item_card.cpp`)

`makeItemCard` reçoit un nouveau paramètre `bool isBis = false`. Quand `isBis` est vrai :
- Dans le header, à côté du `nameLbl`, ajouter un `QLabel("⭐ BiS")` stylé :
  ```css
  color: #ffc947; font-size: 11px; font-weight: bold; background: transparent;
  ```
- Le layout du header devient un `QHBoxLayout` : `[nameLbl (stretch)] [bisLbl]`

**Calcul de `isBis` aux call sites** :
```cpp
bool isBis = _bisNames && _bisNames->contains(QString::fromStdString(item.name));
```
- `CharacterTab::showComparison()` → passe à `makeItemCard`
- `FightTab::showLootForSlot()` → passe à `makeItemCard`

---

## 5. Fichiers modifiés

| Fichier | Changement |
|---|---|
| `src/db/bis_scraper.h` | Nouveau paramètre `expansion`, callback `QSet<QString>` |
| `src/db/bis_scraper.cpp` | Mapping expansion→slug, cache JSON local TTL 7 jours |
| `src/ui/main_window.h` | `_bisScaper`, `_bisNames`, `refreshBis()` |
| `src/ui/main_window.cpp` | `refreshBis()`, triggers sur changement classe/extension |
| `src/ui/character_tab.h` | `_bisNames*`, `setBisNames()` |
| `src/ui/character_tab.cpp` | Surfaces 1, 2, 4 : étoile dans `addRow`, `onSearchPopup`, `makeEquipCell` |
| `src/ui/fight_tab.h` | `_bisNames*`, `setBisNames()` |
| `src/ui/fight_tab.cpp` | Surface 3 : étoile dans rendu loot rows |
| `src/ui/item_card.h` | Param `bool isBis = false` |
| `src/ui/item_card.cpp` | Surface 5 : badge `"⭐ BiS"` dans header |

---

## 6. Comportement aux cas limites

- **Pas de personnage chargé** : `_bisNames` vide, aucun marqueur affiché.
- **Classe ou extension inconnue** : `_bisNames` vide, aucun marqueur.
- **Réseau indisponible, pas de cache** : fetch échoue silencieusement (`onReplyFinished` vérifie `reply->error()`), `_bisNames` reste vide, pas de crash.
- **Premier lancement (pas de cache)** : panels s'affichent sans étoiles, puis `rebuildInventoryPanel()` est re-déclenché quand le fetch async arrive (~1-2s).
- **Item dans BIS mais slot ambigu** (ex: bague → Left/Right Finger) : le set contient le nom une seule fois, la correspondance par nom fonctionne dans les deux slots.
- **Item en sac non équipable** : `isBis` peut être vrai même si l'item ne peut pas être équipé par le perso — l'étoile reste pertinente (c'est un BIS à aller chercher).
