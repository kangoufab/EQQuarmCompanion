# Formules de calcul des statistiques et règles de stacking

**Référence :** EQMacEmu (`client_mods.cpp`, `mob.cpp`, `bonuses.cpp`, `buffstacking.cpp`)  
**Serveur :** Project Quarm (base Planes of Power)  
**Mis à jour :** 2026-05-11

---

## Sommaire

1. [Attributs — STR, STA, AGI, DEX, INT, WIS, CHA](#1-attributs)
2. [HP](#2-hp)
3. [Mana](#3-mana)
4. [AC](#4-ac)
5. [ATK](#5-atk)
6. [Haste](#6-haste)
7. [HP/tick (régénération HP)](#7-hptick)
8. [Mana/tick (régénération mana)](#8-manatick)
9. [Résistances — MR, FR, CR, DR, PR](#9-résistances)
10. [Formules de sort (CalcSpellEffectValue)](#10-formules-de-sort)
11. [Stacking des buffs](#11-stacking-des-buffs)

---

## 1. Attributs

**STR · STA · AGI · DEX · INT · WIS · CHA**

### Sources

| Source | Comportement |
|--------|-------------|
| Items équipés | Somme simple |
| Buffs de sorts | Somme simple (s'ajoutent aux items) |
| AAs | Somme simple |

### Cap par extension

| Extension | Cap attributs |
|-----------|--------------|
| Classic | 200 |
| Kunark | 200 |
| Velious | 200 |
| Luclin | 255 |
| Planes of Power | 255 |

> Au-delà du cap, les points d'attribut n'ont aucun effet sur les stats secondaires (HP, Mana, AC…), sauf cas particulier de STA pour HP (voir § 2).

### SPA sorts liés

| SPA | Attribut |
|-----|----------|
| 4 | STR |
| 5 | DEX |
| 6 | AGI |
| 7 | STA |
| 8 | INT |
| 9 | WIS |
| 10 | CHA |
| 159 | Tous les attributs (+value à chacun) |

---

## 2. HP

### Formule complète

```
HP = base_formule(classe, niveau, STA) + items_HP × ND% + other_HP + 5
```

où `other_HP` = AAs HP (ex. Natural Durability bonus plat) + sorts HP (SPA 69).

### Base formule

```python
clf = class_level_factor(classe, niveau)   # voir tableau ci-dessous
lm  = clf // 10
level_hp = niveau × lm
sta_eff  = STA si STA ≤ 255, sinon 255 + (STA - 255) // 2
base_formule = level_hp × sta_eff // 300 + level_hp
```

**Class Level Factor (clf) par classe et niveau :**

| Classe | L<20 | L<30 | L<40 | L<53 | L<57 | L<60 | L<70 | L≥70 |
|--------|------|------|------|------|------|------|------|------|
| Warrior | 220 | 230 | 250 | 270 | 280 | 290 | 300 | 311 |
| Paladin / SK | 210 | 210 | 210 | 220¹ | 230² | 240³ | 250⁴ | 270 |
| Ranger | — | — | — | — | — | — | 200 | 220 |
| Monk / Bard / Rogue / Beastlord | — | — | — | — | — | 180 | 190⁵ | 210 |
| Druid / Cleric / Shaman | — | — | — | — | — | — | 150 | 157 |
| Mage / Wiz / Necro / Enc | — | — | — | — | — | — | 120 | 127 |

¹ L≥35 ; ² L≥45 ; ³ L≥51 puis L≥56=240 ; ⁴ L≥60 ; ⁵ L≥58

> **Note STA :** La formule base utilise STA plafonné à 255 (hardcode EQMacEmu `GetMaxStat()`), indépendamment du cap d'extension affiché. Au-delà de 255, les points supplémentaires sont divisés par 2 (`255 + (STA-255)/2`).

### Natural Durability (AA)

Le multiplicateur ND s'applique sur `(base_formule + items_HP)` uniquement, avant les AAs plats et les sorts.

| Rang ND | Bonus | +Physical Enhancement (rang ≥ 1) |
|---------|-------|----------------------------------|
| 1 | +2% | +2% supplémentaire |
| 2 | +5% | +2% supplémentaire |
| 3 | +10% | +2% supplémentaire |

### +5 fixe

EQMacEmu ajoute toujours +5 HP (CharacterClass::CalcMaxHP, `+5` en fin de formule).

---

## 3. Mana

### Classes concernées

Cleric, Druid, Shaman, Paladin, Ranger, Shadowknight, Wizard, Magician, Enchanter, Necromancer, Bard, Beastlord.  
Les classes purement mêlée (Warrior, Monk, Rogue) n'ont pas de mana.

### Stat primaire

| Type de caster | Stat utilisée |
|----------------|--------------|
| INT-casters (Wiz, Mage, Enc, Necro) | INT |
| WIS-casters (Cler, Dru, Sha, Pal, Ranger, Beastlord) | WIS |
| Autres (Bard, SK) | Aucune (pas de base mana calculée) |

### Formule base mana

```python
stat = INT ou WIS (selon classe), capped au cap d'extension

lesser     = max(0, (stat - 199) // 2)
mind_factor = stat - lesser

if stat > 100:
    base_mana = ((5 × (mind_factor + 20)) // 2) × 3 × niveau // 40
else:
    base_mana = ((5 × (mind_factor + 200)) // 2) × 3 × niveau // 100
```

### Mana items et sorts

Mana des items (SPA 97) et buffs s'ajoutent à `base_mana`. Aucun cap séparé.

---

## 4. AC

### Formule affichée

```
AC_affiché = (avoidance + mitigation) × 1000 / 847
```

### Mitigation

```python
mitigation = sum(AC items)
if classe ≠ pure caster:
    mitigation = mitigation × 4 // 3    # +33% pour toutes les classes sauf Wiz/Mage/Enc/Necro
if niveau < 50 and mitigation > niveau × 6 + 25:
    mitigation = niveau × 6 + 25        # plafond basses niveaux
```

### Avoidance

```python
def_skill = min(niveau, 60) × defense_max_skill // 60   # skill défense estimé
avoidance = max(1, def_skill × 400 // 225 + agi_bonus(AGI, niveau))
```

**Defense skill max par classe :**

| 252 | Warrior, Paladin, SK, Monk, Bard, Rogue |
| 240 | Ranger, Beastlord |
| 200 | Cleric, Druid, Shaman |
| 145 | Necro, Wiz, Mage, Enc |

**Bonus AGI sur l'avoidance :**

| AGI | Formule |
|-----|---------|
| < 40 | `(25 × (AGI - 40)) // 40` (négatif) |
| 40–74 | ~0 (zone neutre) |
| ≥ 75 | `(2 × (adj - (200 - AGI) // 5)) // 3` |

où `adj` = 35/55/70/80 selon niveau (<7 / <20 / <40 / ≥40).

---

## 5. ATK

### Formule affichée

```
ATK_affiché = (toHit + offense) × 1000 / 744
```

avec :
```
toHit   = 7 + skill_offense + skill_arme
offense = skill_arme + item_ATK_cappé + sort_ATK + bonus_STR
```

### Caps et détails

| Composant | Règle |
|-----------|-------|
| Items ATK (SPA 2) | Cappé à **250** (RuleI `ItemATKCap`) |
| Sorts ATK (SPA 2) | **Sans cap** (bypasse ItemATKCap) |
| Bonus STR | `(2 × STR - 150) // 3` si STR ≥ 75, sinon 0 |
| Ranger L55+ | bonus offense += `niveau × 4 - 216` |

**Skills estimés :**
```
skill = min(cap_classe, cap_classe × min(niveau, 60) // 60)
```

---

## 6. Haste

Le haste EQMacEmu est en **% de bonus** (non additionné à 100). Trois pools distincts :

### V1 — Haste normal (SPA 11)

- Encodage DB : valeur `100 + pct` (ex. 140 = 40% haste)
- Stacking : **max-wins** séparément pour items et sorts, puis **additifs** entre eux
- `V1_total = max(items_V1) + max(sorts_V1)`

### V2 — Bard overhaste (SPA 98)

- Même encodage `100 + pct`
- Actif uniquement si `niveau ≥ 50`
- Cappé à **10%** (`V2 = min(V2_total, 10)`)
- S'additionne à V1 avant l'application du hard cap

### Hard cap (V1 + V2)

```python
if niveau ≥ 60: cap = 100%     # Quarm/PoP
elif niveau ≥ 51: cap = 85%
else: cap = niveau + 25
```

`(V1 + V2)` est plafonné à ce cap avant l'ajout de V3.

### V3 — Overcap (SPA 119)

- Encodage DB : valeur **directe** en % (ex. 10 = 10%)
- Stacking : max-wins
- Cap interne : `niveau > 50 → 25%`, sinon `10%`
- Ajouté **après** le hard cap → peut dépasser le cap normal
- `V3 = min(sort_V3, cap_v3)`

### Résultat affiché

```
Haste_affiché = min(V1 + V2, hard_cap) + V3
```

---

## 7. HP/tick

### Formule complète

```
HP/tick = base_sitting(niveau, race) + min(items_HP_regen, cap_items) + sorts_AAs_HP_regen
```

### Base sitting

| Niveau | Bonus cumulatif |
|--------|----------------|
| Base | +1 (debout) +1 (assis) = 2 |
| ≥ 20 | +1 |
| ≥ 50 | +1 |
| ≥ 51 | +1 |
| ≥ 56 | +1 |
| ≥ 60 | +1 |
| ≥ 61 | +1 |
| ≥ 63 | +1 |
| ≥ 65 | +1 |

**Races Troll/Iksar :** bonus supplémentaires aux paliers 51 et 56, puis **total × 2**.

### Cap items

```python
cap_items = 30 si niveau ≤ 60, sinon niveau - 30
```

> Les sorts et AAs de regen ne sont **pas** soumis à ce cap.

### SPA liés

| SPA | Effet |
|-----|-------|
| 0 | HP regen (bard healing songs, SE_CurrentHP) |
| 100 | HP regen (SE_CurrentHPOnce / Flowing Health) |

---

## 8. Mana/tick

### Formule complète

```
Mana/tick = base_meditate(classe, niveau) + min(Flowing Thought items, 15) + sorts_AAs_mana_regen
```

### Base meditate

```python
skill_meditate = min(niveau × 4, 235)   # estimation

if classe == Bard:
    base = 2 + bonus_haut_niveau
elif classe ∈ INT/WIS-casters:
    base = 4 + skill_meditate // 15 + bonus_haut_niveau
else:
    base = 2 + bonus_haut_niveau    # demi-casters (Pal, Ranger, SK, Beastlord)
```

`bonus_haut_niveau` = +1 si niveau > 61, +1 si niveau > 63.

### Cap items (Flowing Thought)

- Items : cappé à **15** (RuleI `ItemManaRegenCap`)
- Sorts et AAs : non cappés

### SPA lié

| SPA | Effet |
|-----|-------|
| 15 | Mana regen (SE_CurrentMana) |

---

## 9. Résistances

### Sources

| Source | Comportement |
|--------|-------------|
| Race | Résistances de base raciales (voir tableau) |
| Classe + niveau | Bonus class/level-scaling (voir tableau) |
| Items | Somme simple |
| Sorts | Somme simple |
| AAs | Somme simple |

### Base raciale (mr, fr, dr, pr, cr)

| Race | MR | FR | DR | PR | CR |
|------|----|----|----|----|-----|
| Human | 25 | 25 | 15 | 15 | 25 |
| Barbarian | 25 | 25 | 15 | 15 | 35 |
| Erudite | 30 | 25 | 10 | 15 | 25 |
| Wood Elf | 25 | 25 | 15 | 15 | 25 |
| High Elf | 25 | 25 | 15 | 15 | 25 |
| Dark Elf | 25 | 25 | 15 | 15 | 25 |
| Half Elf | 25 | 25 | 15 | 15 | 25 |
| Dwarf | 30 | 25 | 15 | 20 | 25 |
| Troll | 25 | 5 | 15 | 15 | 25 |
| Ogre | 25 | 25 | 15 | 15 | 25 |
| Halfling | 25 | 25 | 20 | 20 | 25 |
| Gnome | 25 | 25 | 15 | 15 | 25 |
| Iksar | 25 | 30 | 15 | 15 | 15 |
| Vah Shir | 25 | 25 | 15 | 15 | 25 |

### Bonus classe/niveau

| Classe | Résist | Bonus |
|--------|--------|-------|
| Warrior | MR | +niveau // 2 |
| Ranger | FR | +4 + max(0, niveau − 49) |
| Monk | FR | +8 + max(0, niveau − 49) |
| Paladin | DR | +8 + max(0, niveau − 49) |
| Shadowknight | DR | +4 + max(0, niveau − 49) |
| Beastlord | DR | +4 + max(0, niveau − 49) |
| Monk | DR | +max(0, niveau − 50) |
| Rogue | PR | +8 + max(0, niveau − 49) |
| Shadowknight | PR | +4 + max(0, niveau − 49) |
| Monk | PR | +max(0, niveau − 50) |
| Ranger | CR | +4 + max(0, niveau − 49) |
| Beastlord | CR | +4 + max(0, niveau − 49) |

### Cap par extension (Quarm)

| Extension | Cap résistances |
|-----------|----------------|
| Classic | 200 |
| Kunark | 200 |
| Velious | 200 |
| Luclin | **500** |
| Planes of Power | **500** |

### SPA liés

| SPA | Résistance |
|-----|-----------|
| 46 | FR |
| 47 | CR |
| 48 | PR |
| 49 | DR |
| 50 | MR |
| 111 | Toutes les résistances (+value à chacune) |

---

## 10. Formules de sort (CalcSpellEffectValue)

Les sorts stockent pour chaque slot : `base`, `max`, `formula`.

```
valeur = calc(base, max, formula, niveau_caster)
si max ≠ 0 :
    si base ≥ 0 : valeur = min(valeur, max)
    si base < 0 : valeur = max(valeur, max)
```

| Formula | Calcul |
|---------|--------|
| 0 ou 100 | `base` (valeur fixe) |
| 1–99 | `base + niveau × formula` |
| 101 | `base + niveau // 2` |
| 102 | `base + niveau` |
| 103 | `base + niveau × 2` |
| 104 | `base + niveau × 3` |
| 105 | `base + niveau × 4` |
| 109 | `base + niveau // 4` |
| 110 | `base + niveau // 6` |
| 119 | `base + niveau // 8` |
| 121 | `base + niveau // 3` |
| autre | `base` |

> **Niveau utilisé :** Pour les sorts reçus d'un autre joueur, le niveau scalant est celui du **caster** (cap de l'extension, soit 60 pour Luclin sur Quarm). Exception : les sorts self-only (targettype=6) utilisent le niveau du personnage lui-même.

---

## 11. Stacking des buffs

Référence : `buffstacking.cpp` — EQMacEmu.

### Règle générale

Pour chaque paire de sorts actifs, on applique les règles dans cet ordre de priorité :

```
1. SPA 148 — SE_StackingCommand_Block  (vérification dans les deux sens)
2. SPA 149 — SE_StackingCommand_Overwrite
3. Comparaison slot-par-slot (fallback)
```

Dès qu'une règle détermine un gagnant et un perdant, les suivantes ne s'appliquent pas.

---

### SPA 148 — Blocage explicite

```
Pour chaque slot du bloqueur avec SPA = 148 :
  blocked_spa  = base[slot]          → SPA ciblé dans le candidat
  target_slot  = formula[slot] - 201 → slot 0-based ciblé dans le candidat
  threshold    = abs(max[slot])      → seuil de valeur

  Si candidat.effectid[target_slot] == blocked_spa :
    cand_value = max ou base du candidat à ce slot
    (si max ≠ 0, utiliser max ; sinon utiliser base)

    Vérification "même ligne" (same-line upgrade) :
      Si le candidat possède aussi SPA 148 avec même base et formula
      → substituer abs(candidat.max[slot_148]) comme cand_value
      → permet à un tier supérieur de la même ligne de remplacer un tier inférieur

    Si cand_value ≤ threshold → candidat BLOQUÉ (ne peut pas coexister)
```

**Exemple :** Aegolism (threshold=2250) bloque Symbol of Transal (HP=1000 ≤ 2250).  
Symbol ne bloque pas Aegolism (HP=2250 > threshold=1000 de Symbol).

---

### SPA 149 — Overwrite forcé

```
Pour chaque slot du overwriter avec SPA = 149 :
  target_spa  = base[slot]
  target_slot = formula[slot] - 201
  threshold   = max[slot]

  Si existing.effectid[target_slot] == target_spa :
    old_value = calc_spell_value(existing, target_slot, niveau_caster)
    Si old_value ≤ threshold → existing RETIRÉ (remplacé par le overwriter)
```

---

### Comparaison slot-par-slot (fallback)

Utilisé quand SPA 148/149 n'ont pas décidé.

**Slots exclus :**
- SPA 254 (SE_Blank — slot vide)
- SPA 148 et 149 eux-mêmes
- Les *controlled slots* : tout slot ciblé par un SPA 148 ou 149 dans **l'un ou l'autre** sort → exclus pour éviter une double-décision

```
Pour chaque slot N (0–11), sauf ceux exclus ci-dessus :
  Si effectid_A[N] == effectid_B[N] :
    Si val_A == 0 et val_B == 0 → ignorer (placeholder sans effet)
    Si val_A ≥ val_B → A gagne, B est le perdant
    Sinon         → B gagne, A est le perdant
    Arrêt (premier slot conflictuel décide)

  Si aucun slot ne conflit → les deux sorts stackent normalement
```

---

### Résultats et affichage

| Résultat | Affichage UI |
|----------|-------------|
| Sort sélectionné + bloqué par un autre sort sélectionné | Barré rouge ⚠, tooltip "Bloqué par X", checkbox actif (décocher possible) |
| Sort non sélectionné qui serait perdant (prédiction) | Barré rouge ⚠, checkbox désactivé |
| Sort non sélectionné qui serait gagnant (overwrite) | Affiché normalement, sélectionnable |
| Aucun conflit | Affiché normalement |

> **Calcul des stats :** Seuls les sorts **gagnants** (non bloqués) contribuent aux totaux de stats. Les perdants sont exclus de `compute_totals`.

---

### Cas particuliers vérifiés

| Cas | Comportement |
|-----|-------------|
| Torpor + Shield of the Magi | Stackent (SPA CHA=0 dans les deux → skip val_a==val_b==0) |
| Blessed Armor of the Risen + Aegolism | Stackent (SPA 79 de Blessed Armor est un controlled slot d'Aegolism) |
| Ancient:Gift of Aegolim + Aegolism | Aegolism bloqué (SPA 148 same-line : Ancient tier supérieur) |
| Sort niveau 24 reçu d'un caster level 60 | Valeur calculée au niveau 60, pas 24 |
