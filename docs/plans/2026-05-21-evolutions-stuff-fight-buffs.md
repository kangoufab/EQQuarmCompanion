# Évolutions Stuff / Fight / Buffs — Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Ajouter filtre slot + bouton Équiper (A), source item (B), skill mods (C), tags NPC raid/quest (E), zone type NPC (F), no-drop loot (G), avoidance/slow_mitigation NPC (H), barre recherche buffs (I), file watcher auto-reload (L).

**Architecture:** Trois couches touchées dans l'ordre : types.h → DB (item_database + npc_database) → UI (character_tab, fight_tab, spells_tab, main_window). Les tâches 1–4 sont des prérequis ; les tâches 5–9 sont indépendantes entre elles et exécutables dans n'importe quel ordre une fois 1–4 terminées.

**Tech Stack:** C++17, Qt6 Widgets, QtSql, QFileSystemWatcher, MinGW 13.1, CMake preset `windows-x64-debug`.

**Build command:** Créer `$env:TEMP\build_debug.ps1` avec :
```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug
```
Exécuter via `& "$env:TEMP\build_debug.ps1"`. Toujours écrire dans un `.ps1` temp — jamais inline.

---

## Fichiers modifiés

| Fichier | Tâches |
|---------|--------|
| `src/core/types.h` | T1 |
| `src/db/item_database.h` | T4 |
| `src/db/item_database.cpp` | T2, T4 |
| `src/db/npc_database.cpp` | T3, T7 |
| `src/ui/character_tab.h` | T5, T6 |
| `src/ui/character_tab.cpp` | T5, T6 |
| `src/ui/fight_tab.cpp` | T7 |
| `src/ui/spells_tab.h` | T8 |
| `src/ui/spells_tab.cpp` | T8 |
| `src/ui/main_window.h` | T9 |
| `src/ui/main_window.cpp` | T5 (signal connect), T9 |

---

## Task 1 — Étendre types.h (ItemData, NpcData, LootItem, NpcSourceData)

**Files:**
- Modify: `src/core/types.h`

Pas de tests unitaires — ces changements sont des prérequis purs sans logique.

- [x] **Step 1 : Ajouter champs à ItemData**

Dans `src/core/types.h`, après `std::string name, lore;` (ligne ~132), le struct `ItemData` devient :

```cpp
struct ItemData {
    int id{}, hp{}, mana{}, ac{}, atk{};
    int astr{}, asta{}, adex{}, aagi{}, aint{}, awis{}, acha{};
    int mr{}, fr{}, cr{}, dr{}, pr{};
    int damage{}, delay{}, itemtype{};
    int haste{}, hp_regen{}, mana_regen{};
    int item_slots{}, classes{65535}, reqlevel{};
    int worneffect{}, focuseffect{}, proceffect{};
    int skillmodtype{-1}, skillmodvalue{};  // C: skill modifier (skillmodtype=-1 = none)
    int nodrop{};                           // G: 1=no-drop, 0=droppable
    std::string name, lore;
    std::string worneffect_name, focuseffect_name, proceffect_name;
    int wornlevel{};
    int atk_base{}, atk_formula{100}, atk_max{};
    int haste_base{}, haste_formula{100}, haste_max{};
    int hp_regen_base{}, hp_regen_formula{100}, hp_regen_max{};
    int mana_regen_base{}, mana_regen_formula{100}, mana_regen_max{};
};
```

- [x] **Step 2 : Ajouter champs à NpcData**

Dans `struct NpcData`, remplacer la définition actuelle par :

```cpp
struct NpcData {
    int  id{}, level{}, hp{}, ac{}, atk{}, accuracy{};
    int  min_hit{}, max_hit{}, attack_delay{10}, attack_count{1};
    int  mr{}, fr{}, cr{}, dr{}, pr{};
    int  npc_spells_id{}, loottable_id{}, race{}, npc_class{};
    int  avoidance{}, slow_mitigation{};          // H
    bool raid_target{}, is_quest{}, encounter{};  // E
    int  zone_type{-1};                           // F (-1=unknown, 0=dungeon, 1=outdoor, 2=city)
    std::string name, special_abilities, zone_long_name;
};
```

- [x] **Step 3 : Ajouter champ nodrop à LootItem**

```cpp
struct LootItem { int item_id{}, item_slots{}; float chance{}; bool nodrop{}; std::string name; };
```

- [x] **Step 4 : Ajouter struct NpcSourceData**

Après la définition de `LootItem` :

```cpp
struct NpcSourceData {
    int id{}, level{};
    float drop_chance{};
    std::string name, zone_long_name;
};
```

- [x] **Step 5 : Build pour vérifier aucun brisage**

```powershell
$s = "$env:TEMP\build_t1.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```
Attendu : 0 erreur.

- [x] **Step 6 : Commit**

```bash
git add src/core/types.h
git commit -m "feat(types): NpcData+ItemData+LootItem+NpcSourceData — champs E/F/G/H/A/B/C"
```

---

## Task 2 — DB items : charger skillmodtype, skillmodvalue, nodrop

**Files:**
- Modify: `src/db/item_database.cpp` (fonctions `buildCols()` et `rowToItemData()`)

- [x] **Step 1 : Ajouter colonnes à buildCols()**

Dans `buildCols()`, après `"i.wornlevel, " +`, ajouter avant les spaCase :
```cpp
"i.skillmodtype, i.skillmodvalue, i.nodrop, "
```

La fonction complète commence :
```cpp
static QString buildCols() {
    return
        "i.id, i.Name AS name, i.ac, i.hp, i.mana, i.damage, i.delay, i.itemtype, "
        "i.range AS itemrange, "
        "i.astr, i.asta, i.aagi, i.adex, i.awis, i.aint, i.acha, "
        "i.cr, i.fr, i.dr, i.mr, i.pr, i.slots, i.classes, i.races, i.reqlevel, "
        "i.worneffect, COALESCE(sw.name, '') AS worneffect_name, "
        "i.focuseffect, COALESCE(sf.name, '') AS focuseffect_name, "
        "i.proceffect, COALESCE(sp.name, '') AS proceffect_name, "
        "i.wornlevel, i.skillmodtype, i.skillmodvalue, i.nodrop, " +
        spaCase(2,  "effect_base_value")    + " AS atk_base, " +
        // ...reste inchangé
```

- [x] **Step 2 : Mapper dans rowToItemData()**

Après `item.reqlevel = q.value("reqlevel").toInt();` ajouter :

```cpp
item.skillmodtype  = q.value("skillmodtype").isNull() ? -1 : q.value("skillmodtype").toInt();
item.skillmodvalue = q.value("skillmodvalue").toInt();
item.nodrop        = q.value("nodrop").toInt();
```

- [x] **Step 3 : Build**

```powershell
$s = "$env:TEMP\build_t2.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 4 : Commit**

```bash
git add src/db/item_database.cpp
git commit -m "feat(db): charger skillmodtype/skillmodvalue/nodrop depuis items"
```

---

## Task 3 — DB NPCs : charger avoidance, slow_mitigation, raid_target, is_quest, encounter, zone_type

**Files:**
- Modify: `src/db/npc_database.cpp` (fonction `getNpcById`)

- [x] **Step 1 : Étendre la requête SELECT dans getNpcById**

Dans `getNpcById`, remplacer le début de la chaîne SQL :

```cpp
q.prepare(
    "SELECT id, name, level, maxlevel, hp, mana, race, `class` AS npc_class, bodytype,"
    " AC AS ac, ATK AS atk, mindmg AS min_hit, maxdmg AS max_hit,"
    " attack_delay, attack_count, Accuracy AS accuracy,"
    " STR AS str_v, STA AS sta, DEX AS dex, AGI AS agi,"
    " _INT AS int_v, WIS AS wis, CHA AS cha,"
    " MR AS mr, CR AS cr, DR AS dr, FR AS fr, PR AS pr,"
    " hp_regen_rate, mana_regen_rate, runspeed,"
    " avoidance, slow_mitigation,"
    " raid_target, isquest AS is_quest, encounter,"
    " special_abilities, npc_spells_id, loottable_id"
    " FROM npc_types WHERE id = :id LIMIT 1"
);
```

- [x] **Step 2 : Mapper les nouveaux champs**

Après `npc.loottable_id = q.value("loottable_id").toInt();` ajouter :

```cpp
npc.avoidance       = q.value("avoidance").toInt();
npc.slow_mitigation = q.value("slow_mitigation").toInt();
npc.raid_target     = q.value("raid_target").toBool();
npc.is_quest        = q.value("is_quest").toBool();
npc.encounter       = q.value("encounter").toBool();
```

- [x] **Step 3 : Récupérer zone_type dans le zone lookup**

Dans la zone lookup après `getNpcById`, étendre la requête `zq` :

```cpp
zq.prepare(
    "SELECT MIN(z.long_name) AS zone_long_name, MIN(z.type) AS zone_type"
    " FROM spawnentry se"
    " JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
    " JOIN zone z ON z.short_name = s2.zone"
    " WHERE se.npcID = :id"
);
zq.bindValue(":id", id);
if (zq.exec() && zq.next()) {
    zone = zq.value("zone_long_name").toString();
    npc.zone_type = zq.value("zone_type").isNull() ? -1 : zq.value("zone_type").toInt();
}
```

- [x] **Step 4 : Build**

```powershell
$s = "$env:TEMP\build_t3.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 5 : Commit**

```bash
git add src/db/npc_database.cpp
git commit -m "feat(db): NPC avoidance/slow_mitigation/raid_target/is_quest/zone_type"
```

---

## Task 4 — DB items : getNpcSources + searchItems slot filter

**Files:**
- Modify: `src/db/item_database.h`
- Modify: `src/db/item_database.cpp`

- [x] **Step 1 : Déclarer dans item_database.h**

Ajouter après `searchItems` :

```cpp
// Retourne les NPCs qui droppent cet item (loottable → npc_types), max 30.
[[nodiscard]] QList<NpcSourceData> getNpcSources(int itemId);
```

Modifier la signature de `searchItems` :

```cpp
[[nodiscard]] QList<ItemData> searchItems(const QString& nameFragment,
                                           int limit = 30,
                                           int slotFilter = 0);
```

- [x] **Step 2 : Implémenter getNpcSources dans item_database.cpp**

Après `searchItems`, ajouter :

```cpp
QList<NpcSourceData> ItemDatabase::getNpcSources(int itemId) {
    QList<NpcSourceData> result;
    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        "SELECT nt.id, nt.name, nt.level,"
        " lde.chance AS item_chance, lte.probability AS table_prob,"
        " MIN(z.long_name) AS zone_long_name"
        " FROM lootdrop_entries lde"
        " JOIN loottable_entries lte ON lte.lootdrop_id = lde.lootdrop_id"
        " JOIN npc_types nt ON nt.loottable_id = lte.loottable_id"
        " LEFT JOIN spawnentry se ON se.npcID = nt.id"
        " LEFT JOIN spawn2 s2 ON s2.spawngroupID = se.spawngroupID"
        " LEFT JOIN zone z ON z.short_name = s2.zone"
        " WHERE lde.item_id = :item_id"
        " GROUP BY nt.id, nt.name, nt.level, lde.chance, lte.probability"
        " ORDER BY (lte.probability * lde.chance) DESC"
        " LIMIT 30"
    );
    q.bindValue(":item_id", itemId);
    if (!q.exec()) {
        qWarning() << "getNpcSources failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        NpcSourceData s;
        s.id           = q.value("id").toInt();
        s.name         = q.value("name").toString().toStdString();
        s.level        = q.value("level").toInt();
        double ip      = q.value("item_chance").toDouble();
        double tp      = q.value("table_prob").toDouble();
        s.drop_chance  = static_cast<float>(tp / 100.0 * ip / 100.0 * 100.0);
        s.zone_long_name = q.value("zone_long_name").toString().toStdString();
        result.append(s);
    }
    return result;
}
```

- [x] **Step 3 : Modifier searchItems pour accepter slotFilter**

Remplacer la définition de `searchItems` :

```cpp
QList<ItemData> ItemDatabase::searchItems(const QString& nameFragment, int limit, int slotFilter) {
    QList<ItemData> result;
    QString cols = buildCols();
    QString from = buildFrom();

    QString slotCond = (slotFilter > 0)
        ? QString(" AND (i.slots & %1) != 0").arg(slotFilter)
        : QString();

    QSqlQuery q(DbConnection::instance().db());
    q.prepare(
        QString("SELECT %1 FROM %2 WHERE i.Name LIKE :name%3"
                " ORDER BY CHAR_LENGTH(i.Name) ASC LIMIT :lim")
            .arg(cols).arg(from).arg(slotCond)
    );
    q.bindValue(":name", QString("%%1%").arg(nameFragment));
    q.bindValue(":lim", limit);
    if (!q.exec()) {
        qWarning() << "searchItems failed:" << q.lastError().text();
        return result;
    }
    while (q.next())
        result.append(rowToItemData(q));
    return result;
}
```

Note: le slot bitmask est interpolé directement dans la chaîne (valeur numérique entière, pas d'injection possible).

- [x] **Step 4 : Build**

```powershell
$s = "$env:TEMP\build_t4.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 5 : Commit**

```bash
git add src/db/item_database.h src/db/item_database.cpp
git commit -m "feat(db): getNpcSources + searchItems slot filter"
```

---

## Task 5 — Onglet Stuff : filtre slot + bouton Équiper

**Files:**
- Modify: `src/ui/character_tab.h`
- Modify: `src/ui/character_tab.cpp`
- Modify: `src/ui/main_window.cpp` (ajouter connect sur le nouveau signal)

### character_tab.h

- [x] **Step 1 : Ajouter signal + membre dans character_tab.h**

Ajouter dans `signals:` :

```cpp
void equipRequested(std::string slot, ItemData item);
```

Ajouter dans `private:` après `QList<ItemData> _searchResults;` :

```cpp
QComboBox* _slotFilter{nullptr};
```

### character_tab.cpp — buildUi

- [x] **Step 2 : Ajouter le filtre slot dans buildUi()**

Dans `buildUi()`, avant la création de `_searchCombo`, insérer le bloc suivant dans le layout de la barre de recherche :

```cpp
// Filtre par slot
_slotFilter = new QComboBox;
_slotFilter->addItem("Tous slots", 0);
static const std::vector<std::pair<const char*, int>> SLOT_FILTER_ITEMS = {
    {"Charm",1},{"Left Ear",2},{"Head",4},{"Face",8},{"Right Ear",16},
    {"Neck",32},{"Shoulders",64},{"Arms",128},{"Back",256},
    {"Left Wrist",512},{"Right Wrist",1024},{"Range",2048},{"Hands",4096},
    {"Primary",8192},{"Secondary",16384},{"Left Finger",32768},
    {"Right Finger",65536},{"Chest",131072},{"Legs",262144},
    {"Feet",524288},{"Waist",1048576},{"Ammo",2097152},
};
for (auto& [name, bit] : SLOT_FILTER_ITEMS)
    _slotFilter->addItem(name, bit);
_slotFilter->setStyleSheet(
    "QComboBox { background: #1a2236; border: 1px solid #3a4a6a; "
    "border-radius: 3px; color: #c0c0c0; padding: 3px 6px; font-size: 13px; }"
    "QComboBox:hover { border-color: #64b5f6; }"
    "QComboBox QAbstractItemView { background: #1a2236; border: 1px solid #3a4a6a; "
    "color: #c0c0c0; selection-background-color: #2a3a5a; }");
row->addWidget(_slotFilter);
```

(Placer `row->addWidget(_slotFilter)` avant `row->addWidget(_searchCombo)`)

### character_tab.cpp — onSearchPopup

- [x] **Step 3 : Passer le slot filter à searchItems**

Dans `onSearchPopup()`, remplacer :

```cpp
auto results = _itemDb->searchItems(query, 50);
```

Par :

```cpp
int slotBit = _slotFilter ? _slotFilter->currentData().toInt() : 0;
auto results = _itemDb->searchItems(query, 50, slotBit);
```

### character_tab.cpp — showComparison

- [x] **Step 4 : Ajouter le bouton "Équiper" dans showComparison()**

Dans `showComparison()`, après la construction du `sumFrame` (après `_comparisonLayout->addWidget(sumFrame);`), ajouter :

```cpp
// Bouton Équiper
{
    auto* equipBtn = new QPushButton(
        QString::fromUtf8("\xe2\x9c\x93  \xc3\x89quiper dans ") + slot);
    equipBtn->setStyleSheet(
        "QPushButton { background: #1e3a1e; border: 1px solid #4caf50; "
        "border-radius: 3px; color: #4caf50; padding: 4px 12px; font-size: 13px; }"
        "QPushButton:hover { background: #4caf50; color: #0f1624; }");
    connect(equipBtn, &QPushButton::clicked,
            [this, slot=slot.toStdString(), item=newItem]() {
                emit equipRequested(slot, item);
            });
    _comparisonLayout->addWidget(equipBtn);
    _comparisonArea->setVisible(true);
}
```

### main_window.cpp — connecter equipRequested

- [x] **Step 5 : Connecter le signal dans MainWindow constructor**

Dans `main_window.cpp`, dans le constructeur après les connexions existantes des onglets (après `connect(_spellsTab, ...)`), ajouter :

```cpp
connect(_charTab, &CharacterTab::equipRequested,
        this, [this](std::string slot, ItemData item) {
    _equippedItems[slot] = item;
    recalculateTotals();
    refreshAllTabs();   // réinitialise aussi clearComparison
});
```

- [x] **Step 6 : Build**

```powershell
$s = "$env:TEMP\build_t5.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 7 : Test manuel**

Lancer l'app. Onglet Stuff :
- Sélectionner "Head" dans le filtre slot → la recherche ne renvoie que des casques.
- Comparer un item → cliquer "Équiper" → l'item devient l'équipé dans le slot, le bandeau stats se met à jour, la comparaison est réinitialisée.

- [x] **Step 8 : Commit**

```bash
git add src/ui/character_tab.h src/ui/character_tab.cpp src/ui/main_window.cpp
git commit -m "feat(stuff): filtre slot + bouton Équiper (signal equipRequested)"
```

---

## Task 6 — Onglet Stuff : affichage skill mods (C) + bouton Source (B)

**Files:**
- Modify: `src/ui/character_tab.cpp` (fonctions `makeItemCard` et `showComparison`)

### Skill Mods (C)

- [x] **Step 1 : Ajouter la map SKILL_NAMES en static**

Dans `character_tab.cpp`, après les constantes existantes (vers ligne 110), ajouter :

```cpp
static const std::map<int, const char*> SKILL_NAMES = {
    {0,"1H Blunt"},{1,"1H Slash"},{2,"2H Blunt"},{3,"2H Slash"},
    {4,"Abjuration"},{5,"Alteration"},{6,"Apply Poison"},{7,"Archery"},
    {8,"Backstab"},{9,"Bind Wound"},{10,"Bash"},{11,"Block"},
    {12,"Brass Inst."},{13,"Channeling"},{14,"Conjuration"},
    {15,"Defense"},{16,"Disarm"},{17,"Disarm Traps"},{18,"Divination"},
    {19,"Dodge"},{20,"Double Attack"},{21,"Dragon Punch"},{22,"Dual Wield"},
    {23,"Eagle Strike"},{24,"Evocation"},{25,"Feign Death"},
    {27,"Flying Kick"},{29,"Hand to Hand"},{30,"Hide"},{31,"Kick"},
    {32,"Meditate"},{34,"Offense"},{35,"Parry"},{36,"Pick Lock"},
    {37,"Piercing"},{38,"Riposte"},{39,"Round Kick"},{40,"Safe Fall"},
    {42,"Singing"},{43,"Sneak"},{49,"Pick Pockets"},{51,"Swimming"},
    {52,"Throwing"},{53,"Tiger Claw"},{54,"Tracking"},
};
```

- [x] **Step 2 : Afficher le skill mod dans makeItemCard()**

Dans `makeItemCard()`, après le bloc des effets (après `addEffect(item->proceffect, ...)` et avant `bodyL->addStretch()`), ajouter :

```cpp
// Skill mod
if (item->skillmodtype >= 0 && item->skillmodvalue != 0) {
    auto skillIt = SKILL_NAMES.find(item->skillmodtype);
    QString skillName = skillIt != SKILL_NAMES.end()
        ? skillIt->second
        : QString("Skill %1").arg(item->skillmodtype);
    addSeparator(bodyL);
    auto* lbl = new QLabel(
        QString("<span style='color:#ffb74d;font-weight:bold;'>Skill</span>"
                "<span style='color:#aaa;'> %1 %2%3</span>")
        .arg(skillName)
        .arg(item->skillmodvalue > 0 ? "+" : "")
        .arg(item->skillmodvalue));
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet("font-size: 13px; border: none; background: transparent;");
    bodyL->addWidget(lbl);
}
```

### Bouton Source (B)

- [x] **Step 3 : Ajouter bouton "Source" dans showComparison()**

Dans `showComparison()`, après le bouton "Équiper" (ou juste avant `_comparisonArea->setVisible(true)`), ajouter :

```cpp
// Bouton Source
{
    auto* srcBtn = new QPushButton(
        QString::fromUtf8("Qui droppe cet item ?"));
    srcBtn->setStyleSheet(
        "QPushButton { background: #1e2a3e; border: 1px solid #3a4a6a; "
        "border-radius: 3px; color: #888; padding: 3px 10px; font-size: 13px; }"
        "QPushButton:hover { border-color: #64b5f6; color: #64b5f6; }");
    connect(srcBtn, &QPushButton::clicked,
            [this, itemId=newItem.id, itemName=QString::fromStdString(newItem.name)]() {
                onShowSources(itemId, itemName);
            });
    _comparisonLayout->addWidget(srcBtn);
}
```

- [x] **Step 4 : Déclarer onShowSources dans character_tab.h**

Dans `private slots:` ajouter :

```cpp
void onShowSources(int itemId, const QString& itemName);
```

- [x] **Step 5 : Implémenter onShowSources dans character_tab.cpp**

Après `clearComparison()`, ajouter :

```cpp
void CharacterTab::onShowSources(int itemId, const QString& itemName)
{
    auto sources = _itemDb->getNpcSources(itemId);

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString::fromUtf8("Sources — ") + itemName);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(480, 360);
    dlg->setStyleSheet("background: #0f1624; color: #c0c0c0;");

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    if (sources.isEmpty()) {
        auto* lbl = new QLabel(
            QString::fromUtf8("Aucun NPC ne droppe cet item dans la base de données."));
        lbl->setStyleSheet("color: #888; font-size: 13px;");
        layout->addWidget(lbl);
    } else {
        auto* header = new QLabel(
            QString::fromUtf8("NPC droppant <b>%1</b> :").arg(itemName));
        header->setTextFormat(Qt::RichText);
        header->setStyleSheet("color: #64b5f6; font-size: 14px;");
        layout->addWidget(header);

        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea { border: 1px solid #3a4a6a; background: #141428; }");
        auto* inner = new QWidget;
        inner->setStyleSheet("background: transparent;");
        auto* innerL = new QVBoxLayout(inner);
        innerL->setContentsMargins(6, 6, 6, 6);
        innerL->setSpacing(3);

        for (const auto& src : sources) {
            QString zone = src.zone_long_name.empty()
                ? "?"
                : QString::fromStdString(src.zone_long_name);
            QString text = QString("<b>%1</b>  <span style='color:#888;font-size:13px;'>"
                                   "niv.%2 — %3 — ~%4%</span>")
                .arg(QString::fromStdString(src.name).replace('_', ' '))
                .arg(src.level)
                .arg(zone)
                .arg(src.drop_chance, 0, 'f', 1);
            auto* lbl = new QLabel(text);
            lbl->setTextFormat(Qt::RichText);
            lbl->setStyleSheet("border: none; background: transparent; font-size: 13px;");
            innerL->addWidget(lbl);
        }
        innerL->addStretch();
        scroll->setWidget(inner);
        layout->addWidget(scroll, 1);
    }

    auto* closeBtn = new QPushButton("Fermer");
    closeBtn->setStyleSheet(
        "QPushButton { background: #2a3a5a; border: 1px solid #3a4a6a; "
        "border-radius: 3px; color: #c0c0c0; padding: 4px 16px; font-size: 13px; }"
        "QPushButton:hover { border-color: #64b5f6; }");
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg->exec();
}
```

Note : ajouter `#include <QDialog>` et `#include <QScrollArea>` si non présents (QScrollArea est déjà inclus, QDialog peut l'être via d'autres headers Qt — vérifier à la compilation).

- [x] **Step 6 : Build**

```powershell
$s = "$env:TEMP\build_t6.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 7 : Test manuel**

- Chercher "Shield of the Slain Unicorn" → la fiche affiche "Skill Defense +10".
- Cliquer "Qui droppe cet item ?" → dialog listant les NPCs sources.

- [x] **Step 8 : Commit**

```bash
git add src/ui/character_tab.h src/ui/character_tab.cpp
git commit -m "feat(stuff): affichage skill mods (C) + popup source NPC (B)"
```

---

## Task 7 — Onglet Fight : tags NPC (E), zone type (F), no-drop loot (G), avoidance/slow_mit (H)

**Files:**
- Modify: `src/ui/fight_tab.cpp` (fonction `buildLeftPanel`)
- Modify: `src/db/npc_database.cpp` (query `getNpcLoot` — ajouter `i.nodrop`)

### G — nodrop dans getNpcLoot

- [x] **Step 1 : Ajouter i.nodrop à la requête getNpcLoot**

Dans `getNpcLoot`, dans la chaîne SQL, remplacer :

```cpp
" i.slots"
" FROM loottable_entries lte"
```

Par :

```cpp
" i.slots, i.nodrop"
" FROM loottable_entries lte"
```

Et dans le parsing des `RawRow`, ajouter le champ :

```cpp
struct RawRow {
    int    item_id{}, slots{};
    double item_chance{}, table_probability{}, table_multiplier{};
    int    droplimit{}, mindrop{}, lootdrop_id{};
    bool   nodrop{};
    QString name;
};
```

Puis dans la boucle `while (q.next())` : `r.nodrop = q.value("nodrop").toBool();`

Et lors de la construction de `LootItem` : `li.nodrop = r.nodrop;`

### E, F, H — buildLeftPanel dans fight_tab.cpp

- [x] **Step 2 : Ajouter tags [RAID]/[QUEST] après le nom NPC**

Dans `buildLeftPanel`, après `layout->addWidget(name);`, remplacer le bloc `sub` :

```cpp
// Tags E : RAID / QUEST / ENCOUNTER
{
    auto* tagsRow = new QHBoxLayout;
    tagsRow->setSpacing(4);
    tagsRow->setContentsMargins(0, 0, 0, 0);
    auto addTag = [&](const char* label, const char* color) {
        auto* t = new QLabel(label);
        t->setStyleSheet(
            QString("color:%1;font-size:12px;font-weight:bold;border:1px solid %1;"
                    "border-radius:3px;padding:1px 5px;background:transparent;").arg(color));
        tagsRow->addWidget(t);
    };
    if (npc.raid_target) addTag("RAID",      "#ef5350");
    if (npc.is_quest)    addTag("QUEST",     "#ffb74d");
    if (npc.encounter)   addTag("ENCOUNTER", "#ba68c8");
    tagsRow->addStretch();
    if (npc.raid_target || npc.is_quest || npc.encounter) {
        auto* tagsW = new QWidget; tagsW->setStyleSheet("background:transparent;");
        tagsW->setLayout(tagsRow);
        layout->addWidget(tagsW);
    }
}

// Zone F : zone_long_name + type tag
{
    static const std::map<int, const char*> ZONE_TYPE_LABELS = {
        {0, "Donjon"}, {1, "Ext\xc3\xa9rieur"}, {2, "Ville"},
    };
    QString zoneTxt = QString::fromStdString(npc.zone_long_name);
    if (npc.zone_type >= 0) {
        auto zt = ZONE_TYPE_LABELS.find(npc.zone_type);
        if (zt != ZONE_TYPE_LABELS.end())
            zoneTxt += QString("  <span style='color:#555555;font-size:12px;'>[%1]</span>")
                        .arg(QString::fromUtf8(zt->second));
    }
    QString sub_text = zoneTxt + QString("  *  Race %1  *  Class %2")
                        .arg(npc.race).arg(npc.npc_class);
    auto* sub = new QLabel(sub_text);
    sub->setTextFormat(Qt::RichText);
    sub->setStyleSheet("color:#888888;font-size:13px;");
    layout->addWidget(sub);
}
```

(Supprimer l'ancien bloc `auto* sub = new QLabel(...)` qui suit le nom NPC.)

- [x] **Step 3 : Ajouter avoidance + slow_mitigation au tileGrid Combat (H)**

Dans `tileGrid` du panneau Combat (dans `buildLeftPanel`), ajouter deux tiles à la fin :

```cpp
flCombat->addWidget(tileGrid({
    {"Level",    QString::number(npc.level)},
    {"HP",       QLocale(QLocale::English).toString(npc.hp)},
    {"AC",       QString::number(npc.ac)},
    {"ATK",      QString::number(npc.atk)},
    {"Accuracy", QString::number(npc.accuracy)},
    {"Min hit",  QString::number(npc.min_hit)},
    {"Max hit",  QString::number(npc.max_hit)},
    {"Delay",    QString("%1s").arg(npc.attack_delay / 10.0, 0, 'f', 1)},
    {"Hits/rnd", QString::number(std::max(1, npc.attack_count))},
    {"Avoidance",  npc.avoidance  > 0 ? QString("+%1").arg(npc.avoidance) : "—"},
    {"Slow Mit.",  npc.slow_mitigation > 0 ? QString("%1%").arg(npc.slow_mitigation) : "—"},
}, 4));
```

- [x] **Step 4 : Afficher "(NO DROP)" en rouge dans la liste loot (G)**

Dans la boucle d'affichage du loot (après `for (const auto& item : loot)`), remplacer la construction du bouton :

```cpp
const char* nameColor = equippable ? "#e0e0e0" : "#555555";
QString nodropSuffix = item.nodrop
    ? QString("  <span style='color:#ef5350;font-size:12px;'>NO DROP</span>")
    : QString();
auto* btn = new QPushButton;
btn->setFlat(true);
btn->setStyleSheet(
    QString("QPushButton{text-align:left;color:%1;background:transparent;border:none;padding:0;}"
            "QPushButton:hover{color:#81c784;}").arg(nameColor));
btn->setText(
    QString("%1  %2%%3")
        .arg(QString::fromStdString(item.name))
        .arg(item.chance, 0, 'f', 0)
        .arg(nodropSuffix));
btn->setTextFormat(Qt::RichText);  // QPushButton n'a pas setTextFormat — utiliser QLabel
```

Attention : `QPushButton` ne supporte pas RichText. Remplacer par un `QLabel` cliquable ou utiliser `QLabel` avec `setTextInteractionFlags`. La solution la plus simple :

```cpp
const char* nameColor = equippable ? "#e0e0e0" : "#555555";
QString nodropBadge = item.nodrop
    ? QString("  <span style='color:#ef5350;font-size:11px;font-weight:bold;'>NO DROP</span>")
    : QString();
QString btnText = QString("<span style='color:%1'>%2  %3%</span>%4")
    .arg(nameColor)
    .arg(QString::fromStdString(item.name))
    .arg(item.chance, 0, 'f', 0)
    .arg(nodropBadge);

auto* lbl = new QLabel(btnText);
lbl->setTextFormat(Qt::RichText);
lbl->setStyleSheet("background:transparent;border:none;font-size:13px;padding:0;");
lbl->setCursor(Qt::PointingHandCursor);
if (item.item_id > 0) {
    int lid = item.item_id;
    connect(lbl, &QLabel::linkActivated, []{});  // non utilisé
    // Clic via installEventFilter ou MousePressEvent — plus simple via un wrapper
}
flLoot->addWidget(lbl);
```

Remarque : puisque l'ancien code utilise un `QPushButton` avec signal clicked pour `onLootClicked`, il faut conserver le QPushButton et ajouter le badge NO DROP via un layout horizontal séparé. Solution finale :

```cpp
const char* nameColor = (item.item_slots > 0) ? "#e0e0e0" : "#555555";
auto* row = new QWidget; row->setStyleSheet("background:transparent;");
auto* rowL = new QHBoxLayout(row);
rowL->setContentsMargins(0,0,0,0); rowL->setSpacing(4);

auto* btn = new QPushButton(
    QString("%1  %2%")
        .arg(QString::fromStdString(item.name))
        .arg(item.chance, 0, 'f', 0));
btn->setFlat(true);
btn->setStyleSheet(
    QString("QPushButton{text-align:left;color:%1;background:transparent;border:none;padding:0;}"
            "QPushButton:hover{color:#81c784;}").arg(nameColor));
if (item.item_id > 0)
    connect(btn, &QPushButton::clicked,
            [this, id=item.item_id]{ onLootClicked(id); });
rowL->addWidget(btn);

if (item.nodrop) {
    auto* ndLbl = new QLabel("NO DROP");
    ndLbl->setStyleSheet(
        "color:#ef5350;font-size:11px;font-weight:bold;"
        "background:transparent;border:none;");
    rowL->addWidget(ndLbl);
}
rowL->addStretch();
flLoot->addWidget(row);
```

(Supprimer l'ancien bloc de construction du `btn` et son `flLoot->addWidget(btn)` existant.)

- [x] **Step 5 : Build**

```powershell
$s = "$env:TEMP\build_t7.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 6 : Test manuel**

Onglet Fight :
- Chercher "Fennin Ro" → tag [RAID] visible.
- Chercher un NPC avec slow_mitigation > 0 (Planes of Time mobs) → "Slow Mit. 25%" visible.
- Loot d'un NPC → items NO DROP affichés en rouge.

- [x] **Step 7 : Commit**

```bash
git add src/db/npc_database.cpp src/ui/fight_tab.cpp
git commit -m "feat(fight): tags NPC raid/quest (E), zone type (F), avoidance/slow_mit (H), loot no-drop (G)"
```

---

## Task 8 — Onglet Buffs : barre de recherche (I)

**Files:**
- Modify: `src/ui/spells_tab.h`
- Modify: `src/ui/spells_tab.cpp`

- [x] **Step 1 : Déclarer _spellSearch dans spells_tab.h**

Dans `private:`, après `QListWidget* _classList{nullptr};` ajouter :

```cpp
QLineEdit* _spellSearch{nullptr};
```

- [x] **Step 2 : Ajouter la barre de recherche dans buildUi()**

Dans `buildUi()`, dans le bloc DROITE (après `rightL->addWidget(sortsTitre);` et avant `auto* panels = new QHBoxLayout;`), insérer :

```cpp
// Barre de recherche sort
_spellSearch = new QLineEdit;
_spellSearch->setPlaceholderText(QString::fromUtf8("Filtrer les sorts\xe2\x80\xa6"));
_spellSearch->setClearButtonEnabled(true);
_spellSearch->setStyleSheet(
    "QLineEdit { background: #141428; border: 1px solid #3a4a6a; "
    "border-radius: 3px; color: #c0c0c0; padding: 3px 6px; font-size: 13px; }"
    "QLineEdit:focus { border-color: #64b5f6; }");
rightL->addWidget(_spellSearch);
connect(_spellSearch, &QLineEdit::textChanged,
        this, [this](const QString&) { rebuildRightPanel(); });
```

- [x] **Step 3 : Filtrer par nom dans rebuildRightPanel()**

Dans `rebuildRightPanel()`, au début de la fonction, après avoir récupéré les spells (ou juste avant la boucle qui les affiche), ajouter :

```cpp
QString searchTxt = _spellSearch ? _spellSearch->text().trimmed() : QString();
```

Puis dans la boucle qui construit les checkboxes (chercher le pattern qui itère sur `_currentClassSpells`), envelopper l'ajout du widget par :

```cpp
if (!searchTxt.isEmpty() &&
    !QString::fromStdString(sd.name).contains(searchTxt, Qt::CaseInsensitive))
    continue;
```

Note : `sd` est la variable de la boucle sur `_currentClassSpells`. Trouver la boucle exacte dans `rebuildRightPanel()` et y insérer la condition.

- [x] **Step 4 : Build**

```powershell
$s = "$env:TEMP\build_t8.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 5 : Test manuel**

Onglet Buffs, sélectionner Cleric → taper "symb" dans la barre de recherche → seuls les sorts contenant "symb" (Symbol of Naltron, etc.) s'affichent.

- [x] **Step 6 : Commit**

```bash
git add src/ui/spells_tab.h src/ui/spells_tab.cpp
git commit -m "feat(buffs): barre de recherche pour filtrer les sorts (I)"
```

---

## Task 9 — File watcher auto-reload (L)

**Files:**
- Modify: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`

- [x] **Step 1 : Ajouter QFileSystemWatcher dans main_window.h**

Ajouter l'include et le membre :

```cpp
#include <QFileSystemWatcher>
// Dans private:
QFileSystemWatcher* _fileWatcher{nullptr};
```

- [x] **Step 2 : Créer le watcher dans le constructeur de MainWindow**

Dans `MainWindow::MainWindow()`, avant `loadCharacterFiles()` (vers la fin du constructeur), ajouter :

```cpp
_fileWatcher = new QFileSystemWatcher(this);
connect(_fileWatcher, &QFileSystemWatcher::fileChanged,
        this, [this](const QString& /*path*/) {
    // Debounce 500ms : EQ peut écrire le fichier en deux passes
    QTimer::singleShot(500, this, &MainWindow::loadCharacterFiles);
});
```

- [x] **Step 3 : Modifier loadCharacterFiles pour enregistrer les fichiers**

Dans `loadCharacterFiles()`, après `auto files = findCharacterFiles(eqDir);`, ajouter :

```cpp
// Retire les anciens watches
if (_fileWatcher && !_fileWatcher->files().isEmpty())
    _fileWatcher->removePaths(_fileWatcher->files());
```

Et à la fin de la boucle `for (auto& f : files)`, après `_charSelector->addItem(...)` :

```cpp
if (_fileWatcher)
    _fileWatcher->addPath(QString::fromStdWString(f.wstring()));
```

Note : `f` est un `std::filesystem::path`. `f.wstring()` donne le chemin complet. Vérifier que le type de `files` est `std::vector<std::filesystem::path>` (oui, c'est le type retourné par `findCharacterFiles`).

- [x] **Step 4 : Re-enregistrer après fileChanged (Windows)**

Sur Windows, `QFileSystemWatcher` supprime le watch quand le fichier est modifié (si l'inode change). Pour pallier, dans le lambda `fileChanged`, re-ajouter le fichier après le singleShot :

```cpp
connect(_fileWatcher, &QFileSystemWatcher::fileChanged,
        this, [this](const QString& path) {
    // Re-watch immédiatement (Windows peut supprimer le watch sur write)
    if (!path.isEmpty() && QFile::exists(path))
        _fileWatcher->addPath(path);
    QTimer::singleShot(500, this, &MainWindow::loadCharacterFiles);
});
```

- [x] **Step 5 : Build**

```powershell
$s = "$env:TEMP\build_t9.ps1"
Set-Content $s '$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug'
& $s; Remove-Item $s
```

- [x] **Step 6 : Test manuel**

Lancer l'app avec un personnage chargé. Dans EQ, utiliser `/charinfo` ou faire une action qui modifie le `.quarmy`. Vérifier que l'app se recharge automatiquement après ~500ms.

- [x] **Step 7 : Commit**

```bash
git add src/ui/main_window.h src/ui/main_window.cpp
git commit -m "feat(main): file watcher QFileSystemWatcher auto-reload .quarmy (L)"
```

---

## Résumé des commits attendus

1. `feat(types): NpcData+ItemData+LootItem+NpcSourceData — champs E/F/G/H/A/B/C`
2. `feat(db): charger skillmodtype/skillmodvalue/nodrop depuis items`
3. `feat(db): NPC avoidance/slow_mitigation/raid_target/is_quest/zone_type`
4. `feat(db): getNpcSources + searchItems slot filter`
5. `feat(stuff): filtre slot + bouton Équiper (signal equipRequested)`
6. `feat(stuff): affichage skill mods (C) + popup source NPC (B)`
7. `feat(fight): tags NPC raid/quest (E), zone type (F), avoidance/slow_mit (H), loot no-drop (G)`
8. `feat(buffs): barre de recherche pour filtrer les sorts (I)`
9. `feat(main): file watcher QFileSystemWatcher auto-reload .quarmy (L)`
