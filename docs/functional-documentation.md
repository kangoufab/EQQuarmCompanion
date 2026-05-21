# Documentation Fonctionnelle — EqQuarmCompanion

**Serveur :** Project Quarm (EQMacEmu, base Planes of Power)  
**Référence serveur :** `common/emu_constants.h`, `zone/client_mods.cpp`, `zone/attack.cpp`, `zone/buffstacking.cpp`, `zone/mob.cpp`  
**Mis à jour :** 2026-05-21 (révision 4)

---

## Sommaire

1. [Attributs — STR, STA, AGI, DEX, INT, WIS, CHA](#1-attributs)
2. [HP](#2-hp)
3. [Mana](#3-mana)
4. [AC](#4-ac)
5. [ATK](#5-atk)
6. [Haste](#6-haste)
7. [HP/tick](#7-hptick)
8. [Mana/tick](#8-manatick)
9. [Résistances](#9-résistances)
10. [Formules de sort — CalcSpellEffectValue](#10-formules-de-sort)
11. [Sorts → dictionnaire de stats](#11-sorts--dictionnaire-de-stats)
12. [Analyse combat NPC](#12-analyse-combat-npc)
    - [Attaques par round NPC](#121-attaques-par-round-npc)
    - [Offense NPC](#122-offense-npc-getoffense)
    - [To-Hit NPC](#123-to-hit-npc-gettohit)
    - [Avoidance joueur](#124-avoidance-joueur)
    - [Hit chance & DPS mêlée entrant](#125-hit-chance--dps-mêlée-entrant)
    - [Mitigation en %](#126-mitigation-en-)
    - [Taux de résistance joueur vs NPC](#127-taux-de-résistance-joueur-vs-npc)
    - [Slow land %](#128-slow-land-)
    - [Résistance sort joueur](#129-résistance-sort-joueur)
    - [Special Abilities](#1210-special-abilities)
    - [DPS sorts NPC](#1211-dps-sorts-npc)
    - [Type et effets des sorts NPC](#1212-type-et-effets-des-sorts-npc)
13. [Onglet Infos — Debuffs résistance](#13-onglet-infos--debuffs-résistance)
14. [Stacking des buffs](#14-stacking-des-buffs)

---

## 1. Attributs

**STR · STA · AGI · DEX · INT · WIS · CHA**

### Sources et accumulation

| Source | Comportement |
|--------|-------------|
| Base race/classe | Valeur fixe initiale |
| Items équipés | Somme simple (tous slots) |
| Sorts (SPA 4–10, 159) | Somme simple (ajoutés avant cap) |
| AAs | Somme simple |

### Cap par extension (Quarm = PoP)

| Extension | Cap attributs |
|-----------|--------------|
| Classic–Velious | 200 |
| Luclin–PoP | **255** |

Au-delà du cap, les points d'attribut n'ont plus d'effet sur les stats dérivées (HP, Mana, AC…), sauf STA dans la formule HP où la zone 255–∞ est divisée par 2.

### SPAs attributs

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
HP = calcBaseHp(classe, niveau, STA_eff)
   + itemsHP                        ← pas de cap
   + ND_bonus                       ← Natural Durability AA × (baseHp + itemsHP)
   + aaHP                           ← AAs HP plats (hors ND)
   + sortsHP                        ← SPA 69
   + 5                              ← fixe (CalcMaxHP ligne finale)
```

### calcBaseHp

```
clf        = classLevelFactor(classe, niveau)   ← voir tableau
lm         = clf / 10
level_hp   = niveau × lm
STA_eff    = STA ≤ 255 → STA
             STA >  255 → 255 + (STA − 255) / 2
base_hp    = level_hp × STA_eff / 300 + level_hp
```

### classLevelFactor

| Classe | L<20 | L<30 | L<40 | L<35¹ | L<45¹ | L<51¹ | L<53 | L<56¹ | L<57 | L<58² | L<60 | L<68¹ | L<70 | L≥70 |
|--------|------|------|------|-------|-------|-------|------|-------|------|-------|------|-------|------|------|
| Warrior | 220 | 230 | 250 | — | — | — | 270 | — | 280 | — | 290 | — | 300 | 311 |
| Paladin / SK | — | — | — | 210 | 220 | 230 | — | 240 | — | — | 250 | 260 | — | 270 |
| Ranger | — | — | — | — | — | — | — | — | — | 200 | — | — | 210 | 220 |
| Monk / Bard / Rogue / Beastlord | — | — | — | — | — | 180 | — | — | — | 190 | — | — | 200 | 210 |
| Druid / Cleric / Shaman | 150 (fixe jusqu'à L<70) | | | | | | | | | | | | | 157 |
| Mage / Wiz / Necro / Enc | 120 (fixe jusqu'à L<70) | | | | | | | | | | | | | 127 |

¹ Paladin/SK : L<35=210, L<45=220, L<51=230, L<56=240, L<60=250, L<68=260  
² Ranger : L<58=200, L<70=210  

### Natural Durability (AA)

ND s'applique sur `(baseHp + itemsHP)` avant d'ajouter les AAs plats et les sorts.

| Rang | Bonus ND | + Physical Enhancement (rang ≥ 1) |
|------|----------|----------------------------------|
| 1 | +2% | +2% |
| 2 | +5% | +2% |
| 3 | +10% | +2% |

### Caps

Quarm (EQMacEmu) n'a **pas** de cap sur les HP items — `itembonuses.HP` est sommé sans limite.

---

## 3. Mana

### Classes avec mana

Cleric, Druid, Shaman, Paladin, Ranger, Shadowknight, Wizard, Magician, Enchanter, Necromancer, Bard, Beastlord.

### Stat primaire

| Type | Stat |
|------|------|
| INT-casters (Wiz, Mage, Enc, Necro) | INT |
| WIS-casters (Clr, Dru, Sha, Pal, Rng, Bst) | WIS |
| Bard, SK | aucune base calculée |

### calcBaseMana

```
lesser      = max(0, (stat − 199) / 2)
mind_factor = stat − lesser

if stat > 100:
    base_mana = ((5 × (mind_factor + 20)) / 2) × 3 × niveau / 40
else:
    base_mana = ((5 × (mind_factor + 200)) / 2) × 3 × niveau / 100
```

où `stat` = INT ou WIS cappé au cap d'extension (255 sur PoP).

### Total Mana

```
Mana = base_mana + itemsMana + sortsMana    ← aucun cap
```

---

## 4. AC

### Formule affichée

```
AC_affiché = (avoidance + mitigation) × 1000 / 847
```

### Mitigation

```
mitigation = itemsAC + aaAC          ← somme items + AAs
if classe ≠ pure_caster:
    mitigation = mitigation × 4 / 3   ← scaling non-casters (+33%)
if niveau < 50 and mitigation > niveau × 6 + 25:
    mitigation = niveau × 6 + 25      ← anti-twink cap

// Iksar racial bonus (attack.cpp 5094-5108)
if race == Iksar:
    if niveau <  10 : mitigation += 10
    elif niveau > 35: mitigation += 35
    else             : mitigation += niveau

// Defense skill → mitigation
if pure_caster: mitigation += defSkill / 2
else           : mitigation += defSkill / 3

// Spell AC (SPA 1) divisé différemment (attack.cpp 5126-5131)
if pure_caster: mitigation += spellAC / 3
else           : mitigation += spellAC / 4

// AGI → mitigation
if AGI > 70: mitigation += AGI / 20
```

**Pure casters :** Necromancer, Wizard, Magician, Enchanter.

### Avoidance

```
defSkill  = min(niveau, 60) × defenseSkillMax(classe) / 60
avoidance = max(1, defSkill × 400 / 225 + agiAvoidance(AGI, niveau))
```

**defenseSkillMax par classe :**

| Max | Classes |
|-----|---------|
| 252 | Warrior, Paladin, SK, Monk, Bard, Rogue |
| 240 | Ranger, Beastlord |
| 200 | Cleric, Druid, Shaman |
| 145 | Necro, Wiz, Mage, Enc |

### agiAvoidance

```
AGI_cap = min(AGI, 200)

if AGI_cap < 40:
    agiAv = (25 × (AGI_cap − 40)) / 40       ← négatif (pénalité)
elif AGI_cap >= 75:
    adj = 35 (niv<7), 55 (niv<20), 70 (niv<40), 80 (niv≥40)
    agiAv = (2 × (adj − (200 − AGI_cap) / 5)) / 3
else:  // 40–74
    agiAv = 0
```

---

## 5. ATK

### Formule affichée

```
ATK_affiché = (toHit + offense) × 1000 / 744
```

avec :
```
itemATK_cappé = min(itemsATK, 250)           ← RuleI ItemATKCap
strBonus      = (STR ≥ 75) ? (2 × STR − 150) / 3 : 0
toHit         = 7 + skillOffense + skillArme
offense       = skillArme + itemATK_cappé + strBonus
```

> Les sorts ATK (SPA 2) sont ajoutés à `itemATK` **avant** le cap (ils le respectent donc aussi).  
> Ranger L>54 : `skillOffense += niveau × 4 − 216`.

### Skill estimé

```
skill = min(cap_classe, cap_classe × min(niveau, 60) / 60)
```

**Caps skill Offense (ID 33) et armes par classe :**

| Classe | Offense | 1HB | 1HS | 2HB | 2HS | 1HP | HTH |
|--------|---------|-----|-----|-----|-----|-----|-----|
| Warrior | 252 | 250 | 250 | 250 | 250 | 240 | 100 |
| Paladin/SK | 225 | 225 | 225 | 225 | 225 | 225 | 100 |
| Ranger | 252 | 250 | 250 | 250 | 250 | 250 | 100 |
| Monk | 252 | 252 | — | 252 | — | — | 252 |
| Bard/Rogue | 252 | 250 | 250 | — | — | 250 | 100 |
| Shaman | 200 | 200 | — | 200 | — | 200 | 75 |
| Necro/Wiz/Mag/Enc | 140 | 110 | — | 110 | — | 110 | 75 |
| Beastlord | 252 | 225 | — | 225 | — | 225 | 250 |
| Cleric | 200 | 175 | — | 175 | — | — | 75 |
| Druid | 200 | 175 | 175 | 175 | — | — | 75 |

**itemtype → skill arme :**

| itemtype | skill | Nom |
|----------|-------|-----|
| 0 | 1 | 1HS |
| 1 | 3 | 2HS |
| 2 | 36 | 1HP |
| 3 | 0 | 1HB |
| 4 | 2 | 2HB |
| 97 | 36 | 1HP (bow) |
| autre | 28 | HTH |

---

## 6. Haste

Le haste EQMacEmu est un **% de bonus de vitesse** (non basé sur 100). Trois pools indépendants.

### V1 — Haste normal (SPA 11)

- Encodage DB : `100 + pct` (ex. 140 = +40%)
- Valeur effective : `base_value − 100`
- Stacking : **max-wins** parmi tous les sorts/items de même pool
- Items haste : max-wins entre tous les items équipés

### V2 — Bard overhaste (SPA 98)

- Même encodage `100 + pct`
- Actif uniquement pour niveau ≥ 50

### Hard cap (V1 + V2 combinés)

```
cap = niveau ≥ 60 → 100%
      niveau ≥ 51 → 85%
      sinon       → niveau + 25
```

```
haste_normal = max(items_V1, sorts_V1 + sorts_V2)
haste_capped = min(haste_normal, cap)
```

### V3 — Overhaste (SPA 119)

- Encodage DB : valeur **directe** en % (pas de +100)
- Exemple : Battlecry of the Vah Shir (ID 2606) = +15%
- Cap interne : niveau ≥ 51 → 25%, sinon 10%
- **Ajouté après le hard cap** (peut dépasser le cap normal)

```
haste_final = haste_capped + min(sum_V3, cap_v3)
```

### Résultat

```
Haste_affiché = haste_final  (en %)
```

---

## 7. HP/tick

### Formule complète

```
HP/tick = baseHpRegen(niveau, race)
        + min(itemsHpRegen, cap_items)
        + sortsHpRegen     ← non soumis au cap items
        + aaHpRegen        ← non soumis au cap items
```

### baseHpRegen (assis + méditation)

| Seuil | Cumul |
|-------|-------|
| Base | +2 (1 debout + 1 assis) |
| niveau ≥ 20 | +1 |
| niveau ≥ 50 | +1 |
| niveau ≥ 51 | +1 |
| niveau ≥ 56 | +1 |
| niveau ≥ 60 | +1 |
| niveau ≥ 61 | +1 |
| niveau ≥ 63 | +1 |
| niveau ≥ 65 | +1 |

**Troll (race 9) / Iksar (race 128) :** bonus supplémentaires aux paliers 51 et 56, puis le total est **× 2**.

### Cap items

```
cap_items = niveau ≤ 60 → 30
            niveau >  60 → niveau − 30
```

### SPAs

| SPA | Effet |
|-----|-------|
| 0 | HP regen (SE_CurrentHP — bard healing songs) |
| 100 | HP regen (SE_CurrentHPOnce / Flowing Health) |

---

## 8. Mana/tick

### Formule complète

```
Mana/tick = baseManaRegen(classe, niveau)
          + min(itemsManaRegen, 15)     ← cap Flowing Thought
          + sortsManaRegen              ← non cappé
          + aaManaRegen                 ← non cappé
```

### baseManaRegen (méditation assise)

```
skill_med = min(niveau × 4, 235)

Bard / Shadowknight:
    base = 2 + bonusHautNiveau

Autres classes avec mana (INT/WIS + hybrids):
    base = 4 + skill_med / 15 + bonusHautNiveau

bonusHautNiveau = (niveau > 61 ? +1 : 0) + (niveau > 63 ? +1 : 0)
```

### Cap items

Flowing Thought items : cappé à **15** (RuleI `ItemManaRegenCap`).

### SPA

| SPA | Effet |
|-----|-------|
| 15 | Mana regen (SE_CurrentMana) |

---

## 9. Résistances

### Sources

| Source | Comportement |
|--------|-------------|
| Race | Valeurs de base raciales |
| Classe + niveau | Bonus scalants (voir tableau) |
| Items (SPA 46–50) | Somme simple |
| Sorts (SPA 46–50, 111) | Somme simple |
| AAs | Somme simple |

### Résistances de base raciales

| Race | MR | FR | CR | DR | PR |
|------|----|----|----|----|----|
| Human | 25 | 25 | 25 | 15 | 15 |
| Barbarian | 25 | 25 | 35 | 15 | 15 |
| Erudite | 30 | 25 | 25 | 10 | 15 |
| Wood Elf | 25 | 25 | 25 | 15 | 15 |
| High Elf | 25 | 25 | 25 | 15 | 15 |
| Dark Elf | 25 | 25 | 25 | 15 | 15 |
| Half Elf | 25 | 25 | 25 | 15 | 15 |
| Dwarf | 30 | 25 | 25 | 15 | 20 |
| Troll | 25 | 5 | 25 | 15 | 15 |
| Ogre | 25 | 25 | 25 | 15 | 15 |
| Halfling | 25 | 25 | 25 | 20 | 20 |
| Gnome | 25 | 25 | 25 | 15 | 15 |
| Iksar | 25 | 30 | 15 | 15 | 15 |
| Vah Shir | 25 | 25 | 25 | 15 | 15 |

### Bonus classe/niveau

```
lvlBonus49(b) = (niveau > 49) ? b + (niveau − 49) : b
lvlBonus50()  = (niveau > 50) ? niveau − 50 : 0
```

| Classe | Résist | Formule |
|--------|--------|---------|
| Warrior | MR | +niveau / 2 |
| Ranger | FR | +lvlBonus49(4) |
| Ranger | CR | +lvlBonus49(4) |
| Monk | FR | +lvlBonus49(8) |
| Monk | DR | +lvlBonus50() |
| Monk | PR | +lvlBonus50() |
| Paladin | DR | +lvlBonus49(8) |
| Shadowknight | DR | +lvlBonus49(4) |
| Shadowknight | PR | +lvlBonus49(4) |
| Beastlord | DR | +lvlBonus49(4) |
| Beastlord | CR | +lvlBonus49(4) |
| Rogue | PR | +lvlBonus49(8) |

### Cap (Quarm — PoP)

```
cap_résistances = 500
```

### SPAs résistances

| SPA | Résistance |
|-----|-----------|
| 46 | FR |
| 47 | CR |
| 48 | PR |
| 49 | DR |
| 50 | MR |
| 111 | Toutes (+value à chacune) |

---

## 10. Formules de sort

Référence : `CalcSpellEffectValue_formula` (EQMacEmu).

Chaque slot de sort stocke : `base`, `max`, `formula`.

```
valeur = calc(base, max, formula, niveau_caster)
puis cap :
  if max ≠ 0:
    if base ≥ 0: valeur = min(valeur, max)
    if base < 0: valeur = max(valeur, max)
```

### Table des formulas

| Formula | Calcul |
|---------|--------|
| 0 ou 100 | `base` (valeur fixe) |
| 1–99 | `base + niveau × formula` |
| 101 | `base + niveau / 2` |
| 102 | `base + niveau` |
| 103 | `base + niveau × 2` |
| 104 | `base + niveau × 3` |
| 105 | `base + niveau × 4` |
| 109 | `base + niveau / 4` |
| 110 | `base + niveau / 6` |
| 119 | `base + niveau / 8` |
| 121 | `base + niveau / 3` |
| autre | `base` |

> **Niveau utilisé :** Niveau du caster (cap expansion = 60 pour Luclin/PoP sur Quarm).  
> Exception : sorts self-only (targettype = 6) → niveau du personnage lui-même.

---

## 11. Sorts → dictionnaire de stats

`spellToStatDict(spell, casterLevel)` convertit un sort en map `stat_key → valeur`.

### Mapping SPA → clé stat

| SPA | Clé | Remarque |
|-----|-----|----------|
| 0 | `hp_regen` | SE_CurrentHP (bard songs) |
| 1 | `ac` | |
| 2 | `atk` | |
| 4 | `astr` | |
| 5 | `adex` | |
| 6 | `aagi` | |
| 7 | `asta` | |
| 8 | `aint` | |
| 9 | `awis` | |
| 10 | `acha` | |
| 11 | `haste` | V1 : `value − 100` |
| 15 | `mana_regen` | |
| 46 | `fr` | |
| 47 | `cr` | |
| 48 | `pr` | |
| 49 | `dr` | |
| 50 | `mr` | |
| 69 | `hp` | |
| 97 | `mana` | |
| 98 | `haste_v2` | V2 bard : `value − 100` |
| 100 | `hp_regen` | |
| 111 | `mr+fr+cr+dr+pr` | Toutes résistances |
| 119 | `haste_v3` | Overhaste : valeur directe |
| 159 | tous attributs | +value à chaque attribut |
| 254 | — | SE_Blank : arrêt du parsing |

---

## 12. Analyse combat NPC

### 12.1 Attaques par round NPC

Référence : `zone/mob_ai.cpp DoMainHandRound + Flurry`.

#### Double Attack

```
level = NPC level
daSkill   = level > 50 ? 250 : min(level × 5, 210)
effective = daSkill + (level > 35 ? level : 0)
da        = min(1.0, effective / 500.0)    ← probabilité 0–100%
```

> Pour level ≤ 7 : `da = 0` (pas de double attack).

#### Triple Attack

Uniquement pour les NPCs guerriers (npc_class = 1 ou 32) de niveau ≥ 60.

```
triple = da × 0.135    ← 13,5% des swings DA
```

#### Round principal (main hand)

```
mainAtk = attack_count + da + triple
```

#### Dual Wield (Special Ability 7)

L'off-hand effectue un round indépendant avec double attack propre.

```
offAtk = sa[7] présent ? (1.0 + da) : 0.0
```

#### Flurry (Special Ability 5)

Multiplie l'intégralité du round (main + off-hand) par `(1 + flurry%)`.

```
flurry_pct = sa[5].params[0] si présent et ≠ 0, sinon 20
total = (mainAtk + offAtk) × (1.0 + flurry_pct / 100.0)
```

#### Résumé

```
total_attacks = (attack_count + da + triple + offAtk) × flurry_mult
```

> **`attack_count = -1` en DB** : signifie "défaut = 1 attaque" (mob_ai.cpp:1184 — `n_atk <= 1 → single Attack()`). Notre code utilise `max(1, attack_count)`.

---

### 12.2 Offense NPC (GetOffense)

Référence : `attack.cpp:4691`.

```
level     = max(1, npc.level)
mobLevel  = (46 ≤ level ≤ 50) ? 45 : level

if mobLevel < 6:
    base   = mobLevel × 4
    strOff = mobLevel
else:
    base   = min(mobLevel × 55 / 10 − 4, 320)
    strOff = (mobLevel < 30) ? mobLevel / 2 + 1 : mobLevel × 2 − 40

offense = max(1, base + strOff + npc.atk)
```

---

### 12.3 To-Hit NPC (GetToHit)

Référence : `attack.cpp:4801`.

```
level     = max(1, npc.level)
weapSkill = level > 50 ? 250 : min(level × 5, 210)
toHit     = 7 + weapSkill + weapSkill + npc.accuracy
          = 7 + 2 × weapSkill + accuracy
```

---

### 12.4 Avoidance joueur

Référence : `Client::GetAvoidance` (attack.cpp).

```
defSkill  = min(niveau, 60) × defenseSkillMax(classe) / 60
avoidance = max(1, defSkill × 400 / 225 + agiAvoidance(AGI, niveau))
```

Voir § 4 pour `defenseSkillMax` et `agiAvoidance`.

#### Hit chance (AvoidanceCheck)

```
av = avoidance × (100 + avoidancePct) / 100    ← avoidancePct: +50 en Evasive
t  = npcToHit + 10
a  = av + 10

if t × 1.21 > a:
    hitChance = max(0, 1 − a / (t × 1.21 × 2))
else:
    hitChance = max(0, t × 1.21 / (a × 2))
```

#### Disciplines

| Discipline | Effet |
|------------|-------|
| Aucune | avoidancePct = 0 |
| Evasive | avoidancePct = +50 → hitChance réduite |
| Defensive | hitChance × 0.5 (dégâts réduits de 50%) |

---

### 12.5 Hit chance & DPS mêlée entrant

```
delaySec = max(0.1, npc.attack_delay / 10.0)    ← attack_delay en 1/10s

// Roll moyen issu de Mob::RollD20(offense, mitigation) — attack.cpp:369
// E[roll] = 1 + 10 × (3×offense − mitigation + 10) / (offense + mitigation + 10)
expRollF = clamp(1 + 10 × (3×npcOff − mit + 10) / (npcOff + mit + 10), 1, 20)

di       = (max_hit > min_hit) ? (max_hit − min_hit) / 19.0 : 0
effHit   = max(1, min_hit + (expRollF − 1) × di)

// DPS moyen / min / max
baseDPS      = effHit       × total_attacks × hitChance / delaySec
minDPS       = min_hit      × total_attacks × hitChance / delaySec   ← roll=1
maxDPS       = max_hit      × total_attacks × hitChance / delaySec   ← roll=20

// Avec discipline
DPS_final = baseDPS × discMult
min_dps   = minDPS  × discMult
max_dps   = maxDPS  × discMult
```

> La formule `expRollF` remplace l'ancienne `off×20/(off+mit)` — elle correspond à l'espérance réelle du dé biaisé `RollD20` du serveur (biais en faveur de l'attaquant quand offense > mitigation).

---

### 12.6 Mitigation en %

Exprime à quel point le joueur réduit la valeur effective du hit.

```
mitRatio     = max(0, 1 − effHit / avg_hit)
mitigation_pct = mitRatio × 100
```

où `avg_hit = (min_hit + max_hit) / 2`.

---

### 12.7 Taux de résistance joueur vs NPC

Évalue comment le joueur résiste aux sorts du NPC.

```
pct  = playerRes / (playerRes + npc.level × 2) × 100

Rating:
  ≥ 65% → GOOD   (vert)
  ≥ 35% → MEDIUM (orange)
  <  35% → LOW    (rouge)
```

---

### 12.8 Slow land %

Calcule la probabilité qu'un slow (MR ou DR) atterrisse sur le NPC.

```
si SA[12] présent → NPC immun au slow → retourner nullopt

npcRes     = (resistType == 1) ? npc.mr : npc.dr
npcRes     = max(0, npcRes + resistDiff)    ← resistDiff < 0 = debuff

levelMod   = (playerLevel − npc.level) × 2
check      = max(0, npcRes − levelMod)
resistChance = max(4, min(198, check × 3 / 2))

slowLand% = 100 − resistChance / 200 × 100
```

`resistDiff` est appliqué depuis les debuffs de l'onglet Infos (getResistDebuffs).

---

### 12.9 Résistance sort joueur

Calcule la probabilité que le joueur résiste à un sort NPC.

```
si spell.resist_type == 0 → pas de résistance → retourner nullopt

playerRes = selon resist_type:
    1 → mr, 2 → fr, 3 → cr, 4 → pr, 5 → dr

levelMod = (playerLevel − npcLevel) × 2
check    = playerRes + levelMod
rChance  = max(4, min(198, check × 3 / 2))

resistPct = 100 − rChance / 200 × 100
```

---

### 12.10 Special Abilities

Format dans la DB : `"id,enabled,param0,param1,..."` séparés par `^`.

- `enabled = 0` → entrée ignorée
- Les paramètres commencent à l'index 2 (index 0 = id, index 1 = enabled)

#### Table complète des IDs (EQMacEmu `emu_constants.h::SpecialAbility`)

| ID | Nom | Sévérité | Param |
|----|-----|----------|-------|
| 1 | Summon | orange | |
| 2 | Enrage | orange | params[0] = seuil HP% |
| 3 | Rampage | orange | |
| 4 | AE Rampage | red | |
| 5 | Flurry | orange | params[0] = % chance (défaut 20%) |
| 6 | Triple Attack | grey | |
| 7 | Dual Wield | grey | |
| 8 | Disallow Equip | grey | |
| 9 | Bane Attack | orange | |
| 10 | Magical Attack | orange | |
| 11 | Ranged Attack | grey | |
| 12 | Immune to Slow | red | |
| 13 | Immune to Mez | red | |
| 14 | Immune to Charm | red | |
| 15 | Immune to Stun | red | |
| 16 | Immune to Snare | red | |
| 17 | Immune to Fear | red | |
| 18 | Immune to Dispel | red | |
| 19 | Immune to Melee | red | |
| 20 | Immune to Magic | red | |
| 21 | Immune to Fleeing | red | |
| 22 | Immune to Melee (non-bane) | red | |
| 23 | Immune to Non-Magic Melee | red | |
| 24 | Immune to Aggro | orange | |
| 25 | Immune to Being Aggro | orange | |
| 26 | Belly-Caster Only | orange | |
| 27 | Immune to Feign Death | orange | |
| 28 | Immune to Taunt | orange | |
| 29 | Tunnel Vision | orange | |
| 30 | No Buff/Heal Allies | grey | |
| 31 | Immune to Pacify | orange | |
| 32 | Leashed | grey | |
| 33 | Tethered | grey | |
| 34 | Permaroot Flee | orange | |
| 35 | Immune to Player Damage | red | |
| 36 | Always Flees | orange | |
| 37 | Flees at HP% | orange | params[0] = seuil HP% |
| 38 | Allows Beneficial Spells | grey | |
| 39 | Melee Disabled | grey | |
| 40 | Chase Distance | grey | |
| 41 | Allowed to Tank | grey | |
| 42 | Proximity Aggro | orange | |
| 43 | Always Calls for Help | orange | |
| 44 | Warrior Skills | grey | |
| 45 | Flees on Low Con | orange | |
| 46 | No Loitering | grey | |
| 47 | Block Handin (Bad Faction) | grey | |
| 48 | PC Deathblow | orange | |
| 49 | Corpse Camper | grey | |
| 50 | Reverse Slow | orange | |
| 51 | Immune to Haste | orange | |
| 52 | Immune to Disarm | orange | |
| 53 | Immune to Riposte | orange | |
| 54 | Proximity Aggro 2 | orange | |

IDs hors table : affichés comme `SA#n` (gris).

---

### 12.11 DPS sorts NPC

`spellIncomingDps(npcSpells, player, playerLevel, npcLevel)` — `src/core/spell_stats.cpp`.

Pour chaque sort NPC :

```
// Dégâts
SPA 0  (SE_CurrentHP) avec base < 0 :
    si buffdurationformula > 0 et buffduration > 0 → DoT : dégâts par tick
    sinon                                           → Nuke : dégâts directs

SPA 79 (SE_CurrentHPOnce) avec base < 0 → Nuke : dégâts directs

// Recast effectif (résolu en DB)
recast_delay = nse.recast_delay > 0 ? nse.recast_delay : sn.recast_time  (ms)
castSec   = sn.cast_time / 1000   (0 pour la plupart des sorts NPC)
recastSec = recast_delay / 1000   (fallback 6s si absent)
cycleSec  = max(1, castSec + recastSec)

// Chance d'atterrissage
levelMod    = (playerLevel − npcLevel) × 2
check       = playerRes + levelMod        ← playerRes selon resist_type
resistChance = max(4, min(198, check × 3 / 2))
landChance  = 1 − resistChance / 200

// Contribution DPS
DPS_nuke = nukeDmg   / cycleSec × landChance
DPS_dot  = dotPerTick / 6.0     × landChance   ← steady-state, 1 tick EQ = 6s
```

Le DPS total sorts est ajouté au DPS mêlée dans chaque cellule de la table DPS×slow.

---

### 12.12 Type et effets des sorts NPC

`effectiveSpellType(spell)` détermine le type à partir des SPAs :

| SPAs détectés | Type retourné |
|---------------|---------------|
| SPA 0, base<0, sans durée | `nuke` |
| SPA 0, base<0, avec durée | `dot` |
| SPA 79, base<0 | `nuke` |
| SPA 0, base>0 | `heal` |
| SPA 11, base<100 | `slow` |
| SPA 11, base≥100 | `haste` |
| SPA 21 | `stun` |
| SPA 22 | `charm` |
| SPA 23 | `fear` |
| SPA 27 | `dispel` |
| SPA 31 | `mez` |
| SPA 50, base<0 | `snare` |
| SPA 96 | `root` |
| SPA 99, base<100 | `slow` |
| spell_type=0 sinon | `debuff` |
| spell_type=1 sinon | `buff` |

`formatSpellSummary(spell, npcLevel)` produit une ligne d'effets lisibles, ex. :
`Dmg 850 · Slow -40% · MR -50 · Stun 3s`

Les valeurs sont calculées au niveau du NPC via `calcSpellEffectValue`.

---

## 13. Onglet Infos — Debuffs résistance

### Structure

L'onglet affiche les meilleurs debuffs de résistance disponibles pour le groupe, organisés par résistance (MR, FR, CR, PR, DR). La sélection dépend de :

- L'expansion active (filtre les sorts non disponibles)
- Le niveau cap de la zone (filtre les sorts trop élevés)
- Le stacking entre groupes (certains groupes ne peuvent pas coexister)

### Groupes de sorts et résistances

| ID groupe | Label | Résistances affectées | Bard | Sorts (IDs) |
|-----------|-------|-----------------------|------|-------------|
| malo | Malo ♦ | MR, FR, CR, PR | non | 110, 111, 112, 1577, 1578, 1772 |
| tash | Tash | MR | non | 676, 677, 678, 1699, 1702, 1704 |
| scent | Scent ♦ | FR, PR, DR | non | 1511, 1512, 1513, 1716 |
| insidious | Insidious | DR | non | 526, 527, 1573 |
| druid_fr | Druide FR | FR | non | 1437, 29, 2518 |
| fire_aoe | Fire AoE ⚠ | CR | non | 433 |
| frost_z | Frost Zephyr | CR | non | 3695 |
| breath_ro | Breath of Ro | FR | non | 1600 |
| occl_multi | Occl./Harmony ♦ | FR, CR, MR | oui | 1451, 3375 |
| tuyen_fr | Tuyen Flamme/Feu | FR | oui | 743, 3367 |
| tuyen_cr | Tuyen Gel/Glace | CR | oui | 744, 3373 |
| tuyen_pr | Tuyen Venin | PR | oui | 3566, 3370 |
| tuyen_dr | Tuyen Fléau | DR | oui | 3567, 3363 |
| bard_mr2 | Bard MR (slows) | MR | oui | 725, 750 |
| bard_mr4 | Denon ⚠ | MR | oui | 1764 |
| bard_mr6 | Fufil's Chant | MR | oui | 707 |

> **Songs retirés (Mesmerize — SPA 31, inutilisables sur boss) :**
> 741 Crission's Pixie Strike, 868 Sionachie's Dreams, 1753 Song of Twilight.

### Conflits de stacking (cross-conflicts)

| Groupe | Conflit avec | Raison |
|--------|--------------|--------|
| druid_fr | scent | FR@slot2 conflicte avec Scent |
| fire_aoe | malo | CR@slot2 remplace Malo (attention!) |
| bard_mr4 | occl_multi | MR@slot3 invalide les −CR/−FR d'Occl./Harmony |

### Disponibilité par expansion

| Sort ID | Expansion min |
|---------|---------------|
| 29, 110, 111, 112, 433, 526, 527, 676, 677, 1511, 1512, 1513, 1699, 1704, 1772 | 0 (Classic) |
| 1577, 1578, 1600, 1702 | 1 (Luclin) |
| 1437, **1716** | 2 (PoP) |
| 2518 | 3 (Gates of Discord) |

Autres bard songs : pas dans la table → disponibles à toutes les expansions.

> **1716 Scent of Terris** : disponible uniquement à partir de Planes of Power (niveau 52 Necromancien).

### Algorithme bestInGroup

```
pour chaque spell_id du groupe:
    si minClassLevel(sort) > cap → exclure
    si non bard ET spell_zone_exp[sort] > expansion_idx → exclure
    valeur = spellResistVal(sort, resist, cap)
             = max(|calcSpellEffect(base, max, formula, cap)|)
               pour chaque slot avec le bon SPA et base < 0
    retenir le sort avec la valeur négative la plus grande (meilleur debuff)
```

### Ordre d'accumulation par résistance

Les groupes non-bard et bard sont cumulables mais distincts.

**Non-bard :**
| Résist | Ordre |
|--------|-------|
| MR | tash → malo |
| FR | druid_fr → scent → breath_ro → malo |
| CR | malo → fire_aoe → frost_z |
| PR | scent → malo |
| DR | insidious → scent |

**Bard :**
| Résist | Ordre |
|--------|-------|
| MR | occl_multi → bard_mr2 → bard_mr4 → bard_mr6 |
| FR | occl_multi → tuyen_fr |
| CR | occl_multi → tuyen_cr |
| PR | tuyen_pr |
| DR | tuyen_dr |

### Groupes bloqués masqués

Si un groupe est en conflit avec un groupe plus fort/large ET que ce dernier est actif (a un meilleur sort disponible), le groupe bloqué est entièrement masqué de l'affichage (pas de ligne rouge, juste absent).

| Groupe masqué | Condition |
|---------------|-----------|
| `druid_fr` | si `scent` actif |
| `fire_aoe` | si `malo` actif |
| `bard_mr4` | si `occl_multi` actif |

### Total debuff par résistance

Le total cumulable (somme des meilleurs sorts non-bard + bard actifs) est affiché en bas de chaque section résistance, en grand (18px vert), avec séparateur horizontal et libellé `Debuff total (MR/FR/…)`.

---

## 14. Stacking des buffs

Référence : `zone/buffstacking.cpp` — EQMacEmu.

### SPAs ignorés par le stacking

Ces SPAs ne causent jamais de conflit, quelle que soit leur valeur :

| SPA | Nom |
|-----|-----|
| 13 | SE_SeeInvis |
| 24 | SE_Stamina (Invigor) |
| 35 | SE_DiseaseCounter |
| 36 | SE_PoisonCounter |
| 57 | SE_Levitate |
| 65 | SE_InfraVision |
| 66 | SE_UltraVision |
| 79 | SE_CurrentHPOnce |
| 116 | SE_CurseCounter |
| 124–144 | Focus effects, LimitX, SpellMod |
| 254 | SE_Blank (fin de slot) |

### Règle générale

Pour chaque paire de sorts actifs, les règles s'appliquent dans cet ordre de priorité :

```
1. SPA 148 — SE_StackingCommand_Block  (vérification dans les deux sens)
2. SPA 149 — SE_StackingCommand_Overwrite
3. Comparaison slot-par-slot (fallback)
```

Dès qu'une règle détermine un gagnant, les suivantes sont ignorées.

### SPA 148 — Blocage explicite

```
Pour chaque slot du bloqueur avec SPA = 148 :
    blocked_spa  = base[slot]          ← SPA ciblé dans le candidat
    target_slot  = formula[slot] − 201 ← slot 0-based ciblé
    threshold    = abs(max[slot])      ← seuil de valeur

    si candidat.spa[target_slot] == blocked_spa :
        cand_val = max si max ≠ 0, sinon base

        // Vérification same-line upgrade :
        si candidat possède aussi SPA 148 avec même base et formula
            → cand_val = abs(candidat.max[slot_148])

        si cand_val ≤ threshold → candidat BLOQUÉ
```

**Exemple :** Aegolism (threshold=2250) bloque Symbol of Transal (HP=1000 ≤ 2250).

### SPA 149 — Overwrite forcé

```
Pour chaque slot du overwriter avec SPA = 149 :
    target_spa  = base[slot]
    target_slot = formula[slot] − 201
    threshold   = max[slot]

    si existing.spa[target_slot] == target_spa :
        old_val = calcSpellEffectValue(existing, target_slot, niveau_caster)
        si old_val ≤ threshold → existing RETIRÉ
```

### Comparaison slot-par-slot (fallback)

Utilisé quand SPA 148/149 n'ont pas décidé.

**Slots exclus :**
- SPA 254 (SE_Blank)
- SPA 148 et 149 eux-mêmes
- *Controlled slots* : tout slot ciblé par un SPA 148 ou 149 dans l'un ou l'autre sort

```
Pour chaque slot N (0–11) non exclu :
    si spa_A[N] == spa_B[N] et non dans IGNORED_SPAS :
        // Exception CHA spacer (SPA 10, base = 0)
        si spa == 10 et (base_A[N] == 0 ou base_B[N] == 0) → ignorer

        val_A = calcSlotValue(A, N, niveau)
        val_B = calcSlotValue(B, N, niveau)
        si val_A == 0 et val_B == 0 → ignorer

        si val_A ≥ val_B → A gagne, B perd (arrêt)
        sinon            → B gagne, A perd (arrêt)

    si aucun slot conflictuel → les deux sorts stackent
```

### Bard vs non-bard

Un sort bard (classes[7] ≠ 255) ne conflicte **jamais** avec un sort non-bard : ils stackent toujours.

### Cas particuliers vérifiés

| Cas | Comportement |
|-----|-------------|
| Torpor + Shield of the Magi | Stackent — SPA CHA=0 ignoré (spacer) |
| Blessed Armor + Aegolism | Stackent — SPA 79 de Blessed est un controlled slot |
| Ancient:Gift of Aegolim + Aegolism | Aegolism bloqué — SPA 148 same-line, Ancient tier sup. |
| Sort reçu d'un caster niveau 60 | Valeur calculée au niveau 60, pas au niveau du spell |

---

*Référence code C++ : `src/core/stats_calculator.cpp`, `src/core/npc_analysis.cpp`,  
`src/core/spell_stats.cpp`, `src/core/spell_stacking.cpp`, `src/ui/infos_spell_data.h`*
