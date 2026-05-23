# Évolutions Fight + Infos — Plan A

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Améliorer l'onglet Fight (plage DPS, visibilité sorts NPC) et l'onglet Infos (total debuff proéminent).

**Architecture:** Modifications purement display — aucune nouvelle dépendance, aucun changement de DB. On étend `IncomingDamageResult` pour transporter min/max DPS, puis on ajuste le rendu dans `fight_tab.cpp` et `infos_tab.cpp`.

**Tech Stack:** C++17, Qt6 Widgets, `src/core/npc_analysis.{h,cpp}`, `src/ui/fight_tab.cpp`, `src/ui/infos_tab.cpp`

---

## Fichiers modifiés

| Fichier | Rôle |
|---------|------|
| `src/core/types.h` | Ajouter `min_dps`, `max_dps` à `IncomingDamageResult` |
| `src/core/npc_analysis.cpp` | Calculer min/max DPS dans `incomingDamage()` |
| `src/ui/fight_tab.cpp` | Afficher range DPS dans la table + DPS sorts dans l'en-tête |
| `src/ui/infos_tab.cpp` | Rendre le total debuff plus visible |

---

## Task 1 : Vérification attack_count = -1 (documentation)

**Files:**
- Read: `src/core/npc_analysis.cpp:47`

- [x] **Step 1 : Confirmer le comportement serveur**

Dans `EQMacEmu/zone/mob_ai.cpp` `DoMainHandRound()` :
```cpp
int16 n_atk = CastToNPC()->GetNumberOfAttacks(); // retourne attack_count
if (n_atk <= 1)
    Attack(victim, ...);   // 1 attaque si attack_count ≤ 1, y compris -1
```
`GetNumberOfAttacks()` retourne `attack_count` brut (npc.h:169). Donc -1 ≤ 1 → 1 attaque. ✓

Notre code : `float base = static_cast<float>(std::max(1, npc.attack_count));` est correct.

- [x] **Step 2 : Ajouter un commentaire explicatif dans npc_analysis.cpp**

Dans `src/core/npc_analysis.cpp`, ligne 47, modifier :
```cpp
// attack_count = -1 in DB means "default" = 1 attack (mob_ai.cpp:1184: n_atk <= 1 → single Attack())
float base = static_cast<float>(std::max(1, npc.attack_count));
```

- [x] **Step 3 : Commit**
```bash
git add src/core/npc_analysis.cpp
git commit -m "docs(fight): attack_count=-1 signifie 1 attaque — commentaire ajouté"
```

---

## Task 2 : Ajouter min_dps / max_dps à IncomingDamageResult

**Files:**
- Modify: `src/core/types.h:87-91`
- Modify: `src/core/npc_analysis.cpp:125-180`

- [x] **Step 1 : Étendre IncomingDamageResult dans types.h**

Remplacer :
```cpp
struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, mitigation_pct{}, disc_mult{1.f};
    int   npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};
```
Par :
```cpp
struct IncomingDamageResult {
    float avg_hit{}, est_dps{}, min_dps{}, max_dps{}, mitigation_pct{}, disc_mult{1.f};
    int   npc_offense{}, player_mit{}, exp_roll{};
    std::string disc_note;
};
```

- [x] **Step 2 : Calculer min/max dans incomingDamage() (npc_analysis.cpp)**

Après la ligne `r.est_dps = baseDps * discMult;`, ajouter :
```cpp
// Min DPS : roll = 1, hit = min_hit
float minHit = (r.avg_hit > 0.f) ? std::max(1.f, static_cast<float>(npc.min_hit)) : 0.f;
// Max DPS : roll = 20, hit = max_hit
float maxHit = (r.avg_hit > 0.f) ? static_cast<float>(npc.max_hit) : 0.f;
r.min_dps = minHit * attacks * baseHC / delaySec * discMult;
r.max_dps = maxHit * attacks * baseHC / delaySec * discMult;
```

- [x] **Step 3 : Build debug**
```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
```
Attendu : 0 erreur.

- [x] **Step 4 : Commit**
```bash
git add src/core/types.h src/core/npc_analysis.cpp
git commit -m "feat(fight): IncomingDamageResult + min_dps/max_dps calcules"
```

---

## Task 3 : Afficher la plage DPS dans la table Fight

La table `buildDpsSlowTable()` affiche actuellement `~X/s · ~Ys`. On ajoute `[min–max]` sur une deuxième ligne dans chaque cellule.

**Files:**
- Modify: `src/ui/fight_tab.cpp:771-776`

- [x] **Step 1 : Étendre la signature de buildDpsSlowTable**

Dans `fight_tab.h`, la signature :
```cpp
QWidget* buildDpsSlowTable(
    const std::vector<std::pair<QString,IncomingDamageResult>>& disciplines,
    const std::vector<std::tuple<QString,std::optional<float>,float>>& slowScenarios,
    float spDps, int hp);
```
Ne change pas — `IncomingDamageResult` contient maintenant `min_dps` et `max_dps`.

- [x] **Step 2 : Modifier le rendu de chaque cellule (fight_tab.cpp)**

Dans `buildDpsSlowTable`, remplacer le bloc de création de cellule (ligne ~771) :
```cpp
auto* cell = new QLabel(
    QString("<span style='color:%1'>~%2/s</span><span style='color:%3'> · %4</span>%5")
    .arg(dpsC).arg(total, 0, 'f', 0).arg(sc).arg(survStr).arg(chLine));
```
Par :
```cpp
float minTotal = dmg.min_dps * (atkSpeed / 100.f) + spDps;
float maxTotal = dmg.max_dps * (atkSpeed / 100.f) + spDps;
QString rangeStr = (minTotal > 0.f && maxTotal > minTotal)
    ? QString("<br><span style='color:#555555;font-size:12px'>[%1–%2/s]</span>")
        .arg(minTotal, 0, 'f', 0).arg(maxTotal, 0, 'f', 0)
    : QString();

auto* cell = new QLabel(
    QString("<span style='color:%1'>~%2/s</span><span style='color:%3'> · %4</span>%5%6")
    .arg(dpsC).arg(total, 0, 'f', 0).arg(sc).arg(survStr).arg(chLine).arg(rangeStr));
```

- [x] **Step 3 : Build + vérification visuelle**
```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
```
Lancer l'app, onglet Fight, sélectionner un NPC. Vérifier que chaque cellule montre `~X/s · ~Ys` + `[min–max/s]` en gris en dessous.

- [x] **Step 4 : Commit**
```bash
git add src/ui/fight_tab.cpp
git commit -m "feat(fight): plage DPS min-max affichée dans chaque cellule"
```

---

## Task 4 : Afficher le DPS sorts NPC dans l'en-tête du panneau droit

Actuellement le DPS sorts est affiché comme note sous la table (`+ ~X/s spell DPS (constant)`). On le monte dans le bloc "Incoming Damage" comme ligne distincte.

**Files:**
- Modify: `src/ui/fight_tab.cpp:560-588`

- [x] **Step 1 : Ajouter une ligne spellDPS dans le bloc Incoming Damage**

Dans `buildRightPanel()`, le bloc `gridWidget` pour "Incoming Damage" (ligne ~565) :

Remplacer :
```cpp
flDmg->addWidget(gridWidget({
    {"Avg hit",   QString("~%1  (%2-%3)").arg(dmg.avg_hit,0,'f',0).arg(npc.min_hit).arg(npc.max_hit)},
    {"NPC Power", QString::number(dmg.npc_offense)},
    {"Your Mit.", QString("%1  (d20->%2/20)").arg(dmg.player_mit).arg(dmg.exp_roll)},
    {"Mit. down", QString("~%1%").arg(dmg.mitigation_pct,0,'f',0)},
}, 1));
```
Par :
```cpp
std::vector<std::pair<QString,QString>> dmgRows = {
    {"Avg hit",   QString("~%1  (%2-%3)").arg(dmg.avg_hit,0,'f',0).arg(npc.min_hit).arg(npc.max_hit)},
    {"NPC Power", QString::number(dmg.npc_offense)},
    {"Your Mit.", QString("%1  (d20→%2/20)").arg(dmg.player_mit).arg(dmg.exp_roll)},
    {"Mit. down", QString("~%1%").arg(dmg.mitigation_pct,0,'f',0)},
};
if (spDps > 0.f)
    dmgRows.push_back({"Spell DPS",
        QString("<span style='color:#ba68c8'>~%1/s</span>").arg(spDps,0,'f',0)});
flDmg->addWidget(gridWidget(dmgRows, 1));
```

- [x] **Step 2 : Supprimer la note "spell DPS" en bas de table**

Dans `buildDpsSlowTable`, supprimer le bloc (lignes ~780-787) :
```cpp
if (spDps > 0.f) {
    auto* note = new QLabel(
        QString("<span style='color:#555555;font-size:13px'>+ ~%1/s spell DPS (constant)</span>")
        .arg(spDps, 0, 'f', 0));
    ...
    g->addWidget(note, ...);
}
```

- [x] **Step 3 : Build + vérification visuelle**
```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
```
Lancer l'app. Vérifier : "Spell DPS" apparaît dans le bloc Incoming Damage en violet si le NPC a des sorts.

- [x] **Step 4 : Commit**
```bash
git add src/ui/fight_tab.cpp
git commit -m "feat(fight): Spell DPS NPC dans l en-tete Incoming Damage"
```

---

## Task 5 : Total debuff proéminent dans l'onglet Infos

Le total par résistance existe déjà (`totalVal`) mais est affiché en petite police grise. On le rend visible comme objectif de groupe.

**Files:**
- Modify: `src/ui/infos_tab.cpp:251-262`

- [x] **Step 1 : Remplacer le label total par un affichage proéminent**

Dans `buildResistSection()`, remplacer la fin (lignes ~251-261) :
```cpp
if (totalVal < 0) {
    auto* totLbl = new QLabel(
        QString("<span style='color:#555555'>Total cumulable : </span>"
                "<b><span style='color:#81c784'>%1</span></b>").arg(totalVal));
    totLbl->setTextFormat(Qt::RichText);
    totLbl->setStyleSheet("background:transparent;border:none;font-size:13px;padding-top:4px;");
    vl->addWidget(totLbl);
}
```
Par :
```cpp
if (totalVal < 0) {
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#2a2a3a;");
    vl->addWidget(sep);

    auto* totRow = new QHBoxLayout;
    totRow->setContentsMargins(0, 2, 0, 0);
    auto* totLblKey = new QLabel(QString("Debuff total (%1)").arg(resist.c_str()));
    totLblKey->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;border:none;")
                             .arg(color));
    auto* totLblVal = new QLabel(QString("<b>%1</b>").arg(totalVal));
    totLblVal->setTextFormat(Qt::RichText);
    totLblVal->setStyleSheet("color:#81c784;font-size:18px;background:transparent;border:none;");
    totRow->addWidget(totLblKey);
    totRow->addStretch();
    totRow->addWidget(totLblVal);
    auto* totW = new QWidget; totW->setStyleSheet("background:transparent;");
    totW->setLayout(totRow);
    vl->addWidget(totW);
}
```

- [x] **Step 2 : Build + vérification visuelle**
```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
```
Lancer l'app, onglet Infos. Le total de chaque résistance doit être lisible en grand en bas de chaque section.

- [x] **Step 3 : Commit**
```bash
git add src/ui/infos_tab.cpp
git commit -m "design(infos): total debuff proéminent en bas de chaque section"
```

---

## Résumé des commits attendus

1. `docs(fight): attack_count=-1 signifie 1 attaque — commentaire ajouté`
2. `feat(fight): IncomingDamageResult + min_dps/max_dps calcules`
3. `feat(fight): plage DPS min-max affichée dans chaque cellule`
4. `feat(fight): Spell DPS NPC dans l en-tete Incoming Damage`
5. `design(infos): total debuff proéminent en bas de chaque section`

---

## Hors scope (plans séparés)

- **Plan B — Stuff tab** : slot filter, affichage item équipé, bouton Equiper, score par classe/expansion
- **Plan C — Buffs tab + applyWornStats** : groupement sorts + résolution effets focus/worn (requiert accès DB sorts dans `eq_core`)
