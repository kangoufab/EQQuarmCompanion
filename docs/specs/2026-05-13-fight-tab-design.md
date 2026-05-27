# Onglet Fight — NPC Combat Analysis — Design Spec

**Date :** 2026-05-13

---

## Objectif

Ajouter un troisième onglet "Fight" à `CharacterTab` (aux côtés de "Stuff" et "Buffs"). L'onglet permet de rechercher un NPC par nom, d'afficher sa fiche de combat complète, et d'analyser comment il se comporte face au personnage sélectionné.

---

## Layout

Les deux panneaux sont dans un `QScrollArea` pour accommoder les loot tables et listes de sorts longues.

```
┌─────────────────────────────────────────────────────────────────┐
│ Fight                                                            │
├─────────────────────────────────────────────────────────────────┤
│ 🔍 Emperor Ssraeshza (Ssraeshza's Sanctum)          ▼          │
├────────────────────────────┬────────────────────────────────────┤
│  Emperor Ssraeshza         │  Analysis vs Laraliel (WAR 65)     │
│  Ssraeshza's Sanctum       │                                     │
│  Undead · Shadow Knight    │  Incoming Damage                    │
│                            │  Avg hit  530 (200–860)             │
│  Combat                    │  Raw DPS  883/s (3×/1.8s)          │
│  Level  70    HP  300 000  │  Your AC  1 850  mit. ↓ ~38%       │
│  AC  1 200    ATK  1 450   │  Est. DPS  ~547/s                   │
│  Min hit  200  Max  860    │  Survival  ~43s unhealed            │
│  Delay  1.8s  Hits/rnd  3  │                                     │
│                            │  Your Resists (vs lv.70 spells)     │
│  Attributes                │  MR  320  [GOOD  ≈76%]             │
│  STR  350  STA  310        │  FR  180  [MEDIUM ≈52%]            │
│  DEX  280  AGI  260        │  CR   80  [LOW   ≈18%]             │
│  INT  240  WIS  120        │  DR  220  [GOOD  ≈62%]             │
│  CHA  100                  │  PR  150  [MEDIUM ≈41%]            │
│                            │                                     │
│  Resists                   │  NPC Spells                         │
│  MR 200 FR 150 CR 100      │  Quill of Ssra (MR) nuke           │
│  DR  50 PR  80             │  Curse of Ssraeshza (PR) debuff     │
│                            │  Ward of Ssraeshza (—) buff         │
│  Special Abilities         │                                     │
│  🛡 Immune to Fear         │  Your Offense (vs NPC resists)      │
│  🛡 Immune to Mez          │  ATK  1350  [MEDIUM vs AC 1200]    │
│  🛡 Immune to Slow         │  Magic   [HARD  — NPC MR 200]      │
│  ⚠ Summon <50%            │  Fire    [MEDIUM — NPC FR 150]     │
│  ⚠ Enrage <10%            │                                     │
│  ⚠ Rampage                │                                     │
│                            │                                     │
│  Loot                      │                                     │
│  Ssraeshza's Helm  100%    │                                     │
│  Vex Thal Key       80%    │                                     │
│  Crystalline Silk   45%    │                                     │
│  …                         │                                     │
└────────────────────────────┴────────────────────────────────────┘
```

---

## Fichiers

### `core/npc_database.py` (nouveau)

```python
class NpcDatabase:
    def __init__(self, db_config: dict): ...

    def search_npcs(self, name: str) -> list[dict]:
        """Retourne jusqu'à 50 NPCs dont le nom contient `name`.
        Chaque entrée : {id, name, level, zone_long_name}.
        JOIN npc_types → spawnentry → spawn2 → zone (GROUP BY npc_types.id).
        Si un NPC spawne dans plusieurs zones, retourne la première zone trouvée.
        """

    def get_npc_by_id(self, npc_id: int) -> dict | None:
        """Retourne tous les champs de npc_types pour ce NPC."""
```

**Colonnes sélectionnées depuis `npc_types` :**
`id, name, level, maxlevel, hp, mana, race, class AS npc_class, bodytype, ac, atk,`
(alias `npc_class` car `class` est un mot réservé Python)
`min_hit, max_hit, attack_delay, attack_count,`
`STR, STA, DEX, AGI, INT, WIS, CHA,`
`MR, CR, DR, FR, PR,`
`hp_regen_rate, mana_regen_rate, run_speed, special_abilities,`
`npc_spells_id, loottable_id`

Deux méthodes supplémentaires :

```python
    def get_npc_spells(self, npc_spells_id: int) -> list[dict]:
        """Retourne les sorts castés par le NPC.
        Chaque entrée : { id, name, resist_type, spell_type, recast_delay }.
        JOIN npc_spells_entries → spells_new.
        resist_type : 0=unresistable, 1=MR, 2=FR, 3=CR, 4=PR, 5=DR.
        spell_type  : entier EQMacEmu (npc_spells_entries.type).
        Triés par priority ASC.
        """

    def get_npc_loot(self, loottable_id: int) -> list[dict]:
        """Retourne les items de la loot table du NPC.
        Chaque entrée : { item_id, name, chance, equip_item }.
        JOIN loottable_entries → lootdrop_entries → items.
        Triés par chance DESC.
        """
```

**Mapping `resist_type` → libellé :**

| resist_type | Affiché |
|-------------|---------|
| 0 | — (unresistable) |
| 1 | MR |
| 2 | FR |
| 3 | CR |
| 4 | PR |
| 5 | DR |

**Mapping `spell_type` → catégorie :** les types EQMacEmu les plus courants affichés comme `nuke`, `heal`, `buff`, `debuff`, `root`, `mez`, `fear`, `slow`, `dot`, `charm`, `lifetap`. Types inconnus → `spell`.

### `core/npc_analysis.py` (nouveau)

Calculs purs, sans accès DB, sans PyQt6.

```python
def incoming_damage(npc: dict, player_totals: dict) -> dict:
    """
    Retourne:
      avg_hit, raw_dps, mitigation_pct, est_dps, survival_s
    """

def resist_ratings(npc: dict, player_totals: dict) -> dict[str, dict]:
    """
    Pour chaque resist (MR, FR, CR, DR, PR) :
      { "value": int, "pct": float, "rating": "GOOD"|"MEDIUM"|"LOW" }
    Formule resist_pct = player_resist / (player_resist + npc_level × 2) × 100
    GOOD ≥ 65%, MEDIUM ≥ 35%, LOW < 35%
    """

def offense_ratings(npc: dict, player_totals: dict) -> dict:
    """
    ATK (mêlée) : compare player ATK vs npc.AC
      EASY  si npc.AC / player.ATK < 0.6
      MEDIUM si < 0.9
      HARD   sinon
    Sorts par école (MR, FR, CR, DR, PR) :
      npc resist EASY ≤ 100 / MEDIUM 101–200 / HARD > 200
    """

def decode_special_abilities(raw: str) -> list[dict]:
    """
    Parse "1,50^3,10^8,1" → liste de { tag: str, severity: "red"|"orange"|"grey" }
    """
```

**Table de décodage `special_abilities` :**

| ID | Tag | Sévérité |
|----|-----|----------|
| 1  | `Summon <VALUE%` | orange |
| 3  | `Enrage <VALUE%` | orange |
| 4  | `Rampage` | orange |
| 5  | `AE Rampage` | rouge |
| 6  | `Immune to Fleeing` | rouge |
| 7  | `Calls for Help` | orange |
| 8  | `Immune to Fear` | rouge |
| 9  | `Immune to Mez` | rouge |
| 10 | `Immune to Charm` | rouge |
| 11 | `Immune to Slow` | rouge |
| 12 | `Immune to Snare` | rouge |
| 13 | `Immune to DoT` | rouge |
| 14 | `Immune to Nuke` | rouge |
| 16 | `Immune to Stun` | rouge |
| 19 | `Immune to Taunt` | orange |
| 20 | `Immune to Dispel` | orange |
| ??  | `Unknown(ID)` | gris |

### `ui/fight_tab.py` (nouveau)

```python
class FightTab(QWidget):
    def __init__(self, char_info, player_totals, config, parent=None): ...
```

- **Barre de recherche** : `SearchComboBox` (réutilisé depuis `ui/widgets.py`)
  - Chaque item affiché : `"Emperor Ssraeshza (Ssraeshza's Sanctum)"`
  - `popup_requested` → `NpcDatabase.search_npcs(text)`
  - `activated` → `_on_npc_selected(npc_id)`

- **Panneau gauche** (`QScrollArea` → `QFrame`, `QVBoxLayout`) — sections séparées par `QFrame.HLine` :
  - Header : nom + zone + race/classe NPC
  - Combat : grid 2 colonnes (Level, HP, AC, ATK, Min hit, Max hit, Delay, Hits/round)
  - Attributes : grid 4 colonnes (STR, STA, DEX, AGI, INT, WIS, CHA)
  - Resists : grid 5 colonnes (MR, FR, CR, DR, PR), valeurs colorées par intensité
  - Special Abilities : `QLabel` wrappable avec tags colorés (rouge/orange/gris)
  - Loot : liste `"Nom item  XX%"` triée par chance DESC ; si `npc.loottable_id == 0`, afficher `"(no loot)"`.

- **Panneau droit** (`QScrollArea` → `QFrame`, `QVBoxLayout`) — sections séparées par `QFrame.HLine` :
  - Header : `"Analysis vs <char_name> (<class> <level>)"`
  - Incoming Damage : grid 2 colonnes
  - Your Resists : grid 3 colonnes (resist, value, rating badge)
  - NPC Spells : liste `"Nom sort  (resist_type)  catégorie"` ; si `npc.npc_spells_id == 0`, afficher `"(no spells)"`.
  - Your Offense : grid 3 colonnes (school, _, rating badge)

- État initial (aucun NPC sélectionné) : les deux panneaux affichent un `QLabel` centré `"Recherchez un NPC pour commencer"`.

### `ui/character_tab.py` (modification)

Dans `_build_ui()`, ajouter après l'onglet "Buffs" :

```python
from ui.fight_tab import FightTab
tabs.addTab(
    FightTab(self._char_info, self._totals_before, self._config),
    "Fight",
)
```

`player_totals` = `self._totals_before` (stats actuelles du personnage, avec AA).

---

## Formules de calcul

### Dégâts reçus (NPC DPS)

Le DPS NPC est calculé dans `core/npc_analysis.py::incoming_damage()` en tenant compte de toutes les mécaniques d'attaque EQ :

```
avg_hit           = (npc.min_hit + npc.max_hit) / 2
delay_s           = npc.attack_delay / 10

# Attaques par round (double attack, triple, dual wield, flurry)
da_chance         = _npc_double_attack_chance(level)   # ~62% à lv60
triple_chance     = 0.135 si (Warrior/Monk et lv≥60) sinon 0
dw                = SA 7 dans special_abilities → dual wield actif
flurry_rate       = SA 5 param[0] / 100  (défaut 0.20)

attacks_per_round = base_count × (1 + da_chance + triple_chance)
                  × (2 si dw sinon 1)
                  + flurry_rate × base_count

hit_chance        = _avoidance_hit_chance(npc_th, base_avoidance, disc_mult)
base_dps          = avg_hit × attacks_per_round × hit_chance / delay_s

mit_ac            = player_totals.get("_mitigation") or 0
denom             = mit_ac + npc.ATK
est_dps           = base_dps × (1 - mit_ac/denom)  si denom > 0
hp                = player_totals["hp"]["capped"]
survival_s        = hp / total_dps   # total_dps = est_dps + spell_dps
```

### Double attack NPC

```python
def _npc_double_attack_chance(level):
    if level <= 7: return 0.0
    da_skill = 250 if level > 50 else min(level * 5, 210)
    effective = da_skill + (level if level > 35 else 0)
    return min(1.0, effective / 500.0)
```

### Tableau 2D Disciplines × Slow

Incoming Damage est affiché sous forme de tableau 2D :
- **Lignes** : disciplines (—, Evasive, Defensive) — Warrior lv52+/55+
- **Colonnes** : scénarios de slow (No slow, Turgur's Insects, Plague of Insects)

Chaque cellule contient :
1. `~DPS/s` — DPS estimé (mêlée × atk_speed/100 + spell_dps)
2. `~Xs` — temps de survie coloré (≥120s vert, ≥60s orange, <60s rouge)
3. `/pause N · Xclr` — CH rotation optimale (bleu)

Règles de filtrage des colonnes slow :
- NPC immune au slow (SA 12) → colonne exclue
- Chance d'atterrissage < 10% → colonne exclue
- La chance intègre les debuffs de résistance de l'onglet Infos (expansion sélectionnée)

### Formule chance de slow

Source : `CheckResistSpell` dans EQMacEmu.

```
resist_check = npc_resist + level_mod
resist_chance = max(4, min(198, resist_check × 3/2))
land_pct      = 100 - resist_chance/200×100
```

Sorts de slow utilisés :
- **Turgur's Insects** (MR, atk_speed→25, 75% slow) — affiché si land_pct ≥ 10%
- **Plague of Insects** (DR, atk_speed→75, 25% slow) — affiché si land_pct ≥ 10%

DPS espéré avec slow (pour CH rotation) :
```
exp_dps = base_dps × (1 − land_pct/100 × (1 − atk_speed/100))
```

### CH Rotation /pause

Intégrée dans chaque cellule du tableau. En EQ, tous les clerics utilisent le **même** `/pause` :

```
safe_pause  = floor(hp × 0.70 × 10 / dps)   # pause max (dixièmes) avant que HP < 30%
# N minimum = premier N (1..12) tel que ceil(100/N) ≤ safe_pause
# P affiché  = safe_pause   (pause maximale garantissant HP ≥ 30% au pire DPS)
```

`safe_pause` est la limite exacte : au-delà HP descend sous 30%. `ceil(100/N)` est la
pause minimale pour que la rotation soit soutenable (chaque clerc a ≥ 10s entre ses lancers).

Affichage : `/pause P · Nclr` — rouge si >12 clerics requis.

### Rating résistances

```
resist_pct = player_resist / (player_resist + npc.level × 2) × 100
GOOD   ≥ 65%
MEDIUM ≥ 35%
LOW    <  35%
```

### Rating offense mêlée

```
ratio = npc.AC / player.ATK
EASY   ratio < 0.6
MEDIUM ratio < 0.9
HARD   ratio ≥ 0.9
```

### Rating sorts (par école)

```
npc_resist ≤ 100  → EASY
npc_resist ≤ 200  → MEDIUM
npc_resist >  200 → HARD
```

La section "Your Offense - Spells" est masquée pour les classes sans sorts offensifs :
`Warrior`, `Monk`, `Rogue`, `Berserker`.

---

## Hors scope

- Historique des NPCs consultés
- Comparaison entre plusieurs NPCs
- Filtrage par zone dans la recherche
- Détail des effets de chaque sort NPC (formules, durées)
