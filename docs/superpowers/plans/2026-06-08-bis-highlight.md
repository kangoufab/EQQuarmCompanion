# BIS Highlight Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Marquer visuellement d'une étoile ⭐ les items qui font partie de la BIS list du personnage (classe + extension en cours) dans 5 surfaces : grille équipée, lignes sacs, dropdown recherche, item cards, loot Fight tab.

**Architecture:** `BisScaper` est étendu (param expansion + cache JSON local 7 jours) et produit un `QSet<QString>` de noms BIS. `MainWindow` tient ce set et l'injecte dans `CharacterTab` et `FightTab` via `setBisNames()`; chaque surface l'interroge au moment du rendu.

**Tech Stack:** C++17, Qt6 (Widgets, Network, Core), CMake/Ninja, MinGW 13.1

---

## Fichiers modifiés

| Fichier | Rôle |
|---|---|
| `src/db/bis_scraper.h` | Nouvelle signature fetchBis + méthodes cache |
| `src/db/bis_scraper.cpp` | Mapping expansion→slug, cache JSON 7 jours, QSet callback |
| `src/ui/infos_tab.h` | Nouveau signal `expansionChanged()` |
| `src/ui/infos_tab.cpp` | Émettre le signal dans `onExpansionChanged()` |
| `src/ui/main_window.h` | Membres `_bisScaper`, `_bisNames`, slot `refreshBis()` |
| `src/ui/main_window.cpp` | Implémentation `refreshBis()`, wiring triggers |
| `src/ui/character_tab.h` | Membre `_bisNames*`, méthodes `setBisNames()` + `rebuildInventoryPanel()` publique |
| `src/ui/character_tab.cpp` | Surfaces 1/2/4 : addRow, makeEquipCell, onSearchPopup, call sites makeItemCard |
| `src/ui/fight_tab.h` | Membre `_bisNames*`, méthode `setBisNames()` |
| `src/ui/fight_tab.cpp` | Surface 3 : loot rows + isBis dans showLootForSlot |
| `src/ui/item_card.h` | Param `bool isBis = false` |
| `src/ui/item_card.cpp` | Badge ⭐ BiS dans header si isBis |

---

## Task 1 — Étendre BisScaper (expansion + cache JSON + QSet callback)

**Files:**
- Modify: `src/db/bis_scraper.h`
- Modify: `src/db/bis_scraper.cpp`

- [ ] **Step 1.1 — Remplacer `src/db/bis_scraper.h` intégralement**

```cpp
#pragma once
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <functional>

struct BisEntry {
    QString slotName;
    QString itemName;
    int     itemId{};
};

class BisScaper : public QObject {
    Q_OBJECT
public:
    explicit BisScaper(QObject* parent = nullptr);

    // Fetches BIS data for className + expansion (e.g. "Warrior", "Luclin").
    // Reads local JSON cache if valid (TTL 7 days). Fetches from network otherwise.
    // Calls callback with a flat QSet of all BIS item names across all slots.
    void fetchBis(const QString& className,
                  const QString& expansion,
                  std::function<void(QSet<QString>)> callback);

private slots:
    void onReplyFinished();

private:
    static QString cacheFilePath(const QString& className, const QString& expansion);
    static bool    cacheIsValid(const QString& path);
    static QSet<QString> loadFromCache(const QString& path);
    void           saveToCache(const QString& path, const QList<BisEntry>& entries) const;
    static QString expansionToSlug(const QString& expansion);
    static QString classToSlug(const QString& className);
    [[nodiscard]] QList<BisEntry> parseHtml(const QByteArray& html) const;
    static QSet<QString> entriesToSet(const QList<BisEntry>& entries);

    QNetworkAccessManager              _nam;
    std::function<void(QSet<QString>)> _callback;
    QString                            _cachePath;
};
```

- [ ] **Step 1.2 — Remplacer `src/db/bis_scraper.cpp` intégralement**

```cpp
#include "db/bis_scraper.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static constexpr qint64 kCacheTtlSeconds = 7 * 24 * 3600;

static const QMap<QString, QString> kExpSlug = {
    {"Classic",         "classic"},
    {"Kunark",          "ruins-of-kunark"},
    {"Velious",         "scars-of-velious"},
    {"Luclin",          "shadows-of-luclin"},
    {"Planes of Power", "planes-of-power"},
};

static const QMap<QString, QString> kClassSlug = {
    {"Shadowknight", "shadow-knight"},
};

BisScaper::BisScaper(QObject* parent) : QObject(parent) {}

QString BisScaper::classToSlug(const QString& className) {
    return kClassSlug.value(className, className.toLower());
}

QString BisScaper::expansionToSlug(const QString& expansion) {
    return kExpSlug.value(expansion, expansion.toLower().replace(' ', '-'));
}

QString BisScaper::cacheFilePath(const QString& className, const QString& expansion) {
    QString dir = QCoreApplication::applicationDirPath() + "/bis_cache";
    QDir().mkpath(dir);
    return dir + "/" + className + "_" + expansion + ".json";
}

bool BisScaper::cacheIsValid(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) return false;
    return fi.lastModified().secsTo(QDateTime::currentDateTime()) < kCacheTtlSeconds;
}

QSet<QString> BisScaper::loadFromCache(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return {};
    QSet<QString> names;
    for (auto it = doc.object().begin(); it != doc.object().end(); ++it)
        for (const QJsonValue& v : it.value().toArray())
            names.insert(v.toString());
    return names;
}

void BisScaper::saveToCache(const QString& path, const QList<BisEntry>& entries) const {
    QJsonObject obj;
    for (const BisEntry& e : entries) {
        QJsonArray arr = obj.value(e.slotName).toArray();
        if (!arr.contains(QJsonValue(e.itemName)))
            arr.append(e.itemName);
        obj[e.slotName] = arr;
    }
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

QSet<QString> BisScaper::entriesToSet(const QList<BisEntry>& entries) {
    QSet<QString> s;
    for (const BisEntry& e : entries)
        s.insert(e.itemName);
    return s;
}

void BisScaper::fetchBis(const QString& className,
                          const QString& expansion,
                          std::function<void(QSet<QString>)> cb)
{
    _callback  = std::move(cb);
    _cachePath = cacheFilePath(className, expansion);

    if (cacheIsValid(_cachePath)) {
        if (_callback) _callback(loadFromCache(_cachePath));
        return;
    }

    QString url = "https://www.eqprogression.com/"
                  + classToSlug(className)
                  + "-best-in-slot-bis-gearing-guide-"
                  + expansionToSlug(expansion) + "/";
    auto* reply = _nam.get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, &BisScaper::onReplyFinished);
}

void BisScaper::onReplyFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    reply->deleteLater();
    if (data.isEmpty()) {
        if (_callback) _callback({});
        return;
    }
    QList<BisEntry> entries = parseHtml(data);
    saveToCache(_cachePath, entries);
    if (_callback) _callback(entriesToSet(entries));
}

QList<BisEntry> BisScaper::parseHtml(const QByteArray& html) const {
    QList<BisEntry> result;
    QString page = QString::fromUtf8(html);

    static const QRegularExpression rowRe(
        R"(<tr[^>]*>(.*?)</tr>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression cellRe(
        R"(<td[^>]*>(.*?)</td>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression tooltipRe(
        R"(<span[^>]*data-tooltip[^>]*>\s*<span[^>]*>([^<]+)</span>)",
        QRegularExpression::DotMatchesEverythingOption |
        QRegularExpression::CaseInsensitiveOption);

    static const QMap<QString, QStringList> slotMap = {
        {"Ears",    {"Left Ear",    "Right Ear"}},
        {"Fingers", {"Left Finger", "Right Finger"}},
        {"Wrists",  {"Left Wrist",  "Right Wrist"}},
        {"Ranged",  {"Range"}},
    };

    auto rowIt = rowRe.globalMatch(page);
    while (rowIt.hasNext()) {
        auto rowMatch = rowIt.next();
        QString rowHtml = rowMatch.captured(1);

        QStringList cells;
        auto cellIt = cellRe.globalMatch(rowHtml);
        while (cellIt.hasNext())
            cells << cellIt.next().captured(1);
        if (cells.size() < 2) continue;

        QString rawSlot = cells[0];
        rawSlot.remove(QRegularExpression("<[^>]+>"));
        rawSlot = rawSlot.trimmed();
        if (rawSlot.isEmpty()) continue;

        QString itemsHtml = cells[1];
        auto tipIt = tooltipRe.globalMatch(itemsHtml);
        while (tipIt.hasNext()) {
            QString itemName = tipIt.next().captured(1).trimmed();
            if (itemName.isEmpty()) continue;
            for (const QString& eqSlot : slotMap.value(rawSlot, {rawSlot})) {
                BisEntry e;
                e.slotName = eqSlot;
                e.itemName = itemName;
                e.itemId   = 0;
                result.append(e);
            }
        }
    }
    return result;
}
```

- [ ] **Step 1.3 — Build de vérification**

```powershell
$s = "$env:TEMP\build_bis1.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& $cmake --preset windows-x64-debug
& $cmake --build build/debug 2>&1 | Select-String -Pattern "error:|warning:" | Select-Object -First 20
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : compilation sans erreur sur `bis_scraper.cpp`.

- [ ] **Step 1.4 — Commit**

```bash
git add src/db/bis_scraper.h src/db/bis_scraper.cpp
git commit -m "feat(db): BisScaper — expansion param, cache JSON 7j, QSet callback"
```

---

## Task 2 — Signal `expansionChanged` dans InfosTab

**Files:**
- Modify: `src/ui/infos_tab.h`
- Modify: `src/ui/infos_tab.cpp`

- [ ] **Step 2.1 — Ajouter le signal dans `infos_tab.h`**

Ajouter le bloc `signals:` après `Q_OBJECT` (avant `public:`), ou l'ajouter si le bloc n'existe pas :

```cpp
// Dans infos_tab.h, entre Q_OBJECT et public:
signals:
    void expansionChanged();
```

Fichier complet après modification :

```cpp
#pragma once
#include <QComboBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <string>

class Config;

class InfosTab : public QWidget {
    Q_OBJECT
public:
    explicit InfosTab(Config* config, QWidget* parent = nullptr);

signals:
    void expansionChanged();

private slots:
    void onExpansionChanged();

private:
    void refreshContent();
    QWidget* buildResistSection(const std::string& resist, int cap, int expIdx);

    Config*      _config;
    QComboBox*   _expCombo;
    QHBoxLayout* _contentLayout{};
    QWidget*     _contentWidget{};
};
```

- [ ] **Step 2.2 — Émettre le signal dans `infos_tab.cpp`**

Dans `InfosTab::onExpansionChanged()`, ajouter `emit expansionChanged();` après `_config->save()` :

```cpp
void InfosTab::onExpansionChanged() {
    std::string exp = _expCombo->currentText().toStdString();
    _config->set("current_expansion", nlohmann::json(exp));
    _config->save();
    emit expansionChanged();
    refreshContent();
}
```

- [ ] **Step 2.3 — Build de vérification**

```powershell
$s = "$env:TEMP\build_bis2.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug 2>&1 | Select-String "error:" | Select-Object -First 10
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : aucune erreur.

- [ ] **Step 2.4 — Commit**

```bash
git add src/ui/infos_tab.h src/ui/infos_tab.cpp
git commit -m "feat(ui): InfosTab émet expansionChanged() quand l'extension change"
```

---

## Task 3 — `refreshBis()` dans MainWindow

**Files:**
- Modify: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`

- [ ] **Step 3.1 — Mettre à jour `main_window.h`**

Ajouter les includes et membres. Fichier complet après modification :

```cpp
#pragma once
#include "core/character_parser.h"
#include "core/stats_calculator.h"
#include "core/types.h"
#include "db/bis_scraper.h"
#include <QComboBox>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMainWindow>
#include <QSet>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <map>
#include <string>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;
class CharacterTab;
class FightTab;
class SpellsTab;
class InfosTab;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, NpcDatabase* npcDb,
                        ItemDatabase* itemDb, QWidget* parent = nullptr);

private slots:
    void onCharacterChanged(int index);
    void onStatsChanged(PlayerTotals totals, std::map<std::string, ItemData> equippedItems);
    void onBuffStatsChanged(PlayerTotals totals, PlayerTotalsExtra spellExtra);
    void openSettings();
    void checkDbStatus();
    void refreshBis();

private:
    void loadCharacterFiles();
    void refreshAllTabs();
    void recalculateTotals();
    void rebuildGlobalStatsBar(const PlayerTotals& totals,
                                const PlayerTotalsExtra* extraOverride = nullptr,
                                const std::map<std::string, ItemData>* itemsOverride = nullptr);
    void updateDbBadge(bool connected);

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;

    QFileSystemWatcher* _fileWatcher{nullptr};

    QComboBox*    _charSelector;
    QLabel*       _dbBadge{nullptr};
    QTimer*       _dbTimer{nullptr};
    QLabel*       _charHeaderLabel{nullptr};
    QVBoxLayout*  _globalStatsLayout{nullptr};
    QTabWidget*   _tabs;
    CharacterTab* _charTab;
    FightTab*     _fightTab;
    SpellsTab*    _spellsTab;
    InfosTab*     _infosTab;

    std::vector<CharacterInfo>      _characters;
    CharacterInfo                   _currentChar;
    PlayerTotals                    _playerTotals;
    PlayerTotals                    _buffedTotals;
    PlayerTotalsExtra               _playerExtra;
    AaStats                         _aaStats;
    std::map<std::string, ItemData> _equippedItems;

    BisScaper     _bisScaper;
    QSet<QString> _bisNames;
};
```

- [ ] **Step 3.2 — Implémenter `refreshBis()` dans `main_window.cpp`**

Ajouter la méthode `refreshBis()` à la fin de `main_window.cpp`, avant la dernière accolade :

```cpp
void MainWindow::refreshBis() {
    QString className = QString::fromStdString(_currentChar.class_name);
    QString expansion = QString::fromStdString(_config->get("current_expansion"));

    if (className.isEmpty() || expansion.isEmpty() || _currentChar.name.empty()) {
        _bisNames.clear();
        _charTab->setBisNames(&_bisNames);
        _fightTab->setBisNames(&_bisNames);
        return;
    }

    _bisScaper.fetchBis(className, expansion, [this](QSet<QString> names) {
        _bisNames = std::move(names);
        _charTab->setBisNames(&_bisNames);
        _fightTab->setBisNames(&_bisNames);
        _charTab->rebuildInventoryPanel();
        _fightTab->refreshStats();
    });
}
```

- [ ] **Step 3.3 — Connecter les triggers dans le constructeur de `MainWindow`**

Dans `MainWindow::MainWindow`, après la ligne `connect(_fightTab, &FightTab::lootItemActivated, ...)`, ajouter :

```cpp
connect(_infosTab, &InfosTab::expansionChanged, this, &MainWindow::refreshBis);
```

- [ ] **Step 3.4 — Appeler `refreshBis()` à la fin de `onCharacterChanged()`**

À la fin de `MainWindow::onCharacterChanged()`, après `refreshAllTabs()` :

```cpp
void MainWindow::onCharacterChanged(int index) {
    if (index < 0 || index >= static_cast<int>(_characters.size())) return;
    _currentChar = _characters[index];
    _equippedItems.clear();

    for (const auto& [slot, itemId] : _currentChar.equipped) {
        auto item = _itemDb->getItemById(itemId);
        if (item) {
            applyWornStats(*item, _currentChar.level);
            _equippedItems[slot] = *item;
        }
    }

    _aaStats = _itemDb->getAaStats(_currentChar.aa_purchases);
    recalculateTotals();
    refreshAllTabs();
    refreshBis();   // ← ajouter cette ligne
}
```

- [ ] **Step 3.5 — Pré-requis : appliquer d'abord les étapes 4.1 et 5.1**

`refreshBis()` appelle `_charTab->rebuildInventoryPanel()` (rendue publique en 4.1) et `_charTab->setBisNames()` / `_fightTab->setBisNames()` (déclarées en 4.1 et 5.1). Appliquer les étapes 4.1 et 5.1 **avant** de builder, puis revenir ici.

- [ ] **Step 3.6 — Build de vérification (après 4.1 et 5.1)**

```powershell
$s = "$env:TEMP\build_bis3.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug 2>&1 | Select-String "error:" | Select-Object -First 10
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : aucune erreur. `refreshBis()` compilé, tabs déclarés avec `setBisNames()` et `rebuildInventoryPanel()` publique.

- [ ] **Step 3.7 — Commit**

```bash
git add src/ui/main_window.h src/ui/main_window.cpp
git commit -m "feat(ui): MainWindow — refreshBis() injecte QSet BIS dans CharacterTab et FightTab"
```

---

## Task 4 — CharacterTab : `setBisNames()` + 3 surfaces

**Files:**
- Modify: `src/ui/character_tab.h`
- Modify: `src/ui/character_tab.cpp`

- [ ] **Step 4.1 — Mettre à jour `character_tab.h`**

Ajouter include `<QSet>`, rendre `rebuildInventoryPanel()` public, ajouter `setBisNames()` et `_bisNames` :

```cpp
#pragma once
#include "core/types.h"
#include "ui/item_card.h"
#include <QSet>
#include <QWidget>
#include <QList>
#include <map>
#include <string>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;
class SearchComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QFrame;
class QComboBox;
class QScrollArea;
class QStringListModel;

class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p = nullptr);
    void setCharacter(CharacterInfo*, PlayerTotals*,
                      const std::map<std::string, ItemData>&);
    void loadItemIntoComparison(const ItemData& item);
    void setBisNames(const QSet<QString>* bisNames);
    void rebuildInventoryPanel();

signals:
    void statsChanged(PlayerTotals totals, std::map<std::string, ItemData> equippedItems);
    void equipRequested(std::string slot, ItemData item);

private slots:
    void onSearchPopup();
    void onItemSelected(int index);
    void onShowSources(int itemId, const QString& itemName);

private:
    void buildUi();
    QFrame* makeStatsBar(const QString& label,
                         const PlayerTotals& totals,
                         int attrCap, int resistCap,
                         const PlayerTotals* refTotals = nullptr);

    void showComparison(const ItemData& newItem, const QString& slot,
                        const std::vector<QString>& allSlots = {});
    void clearComparison(bool emitReset = true);
    void showItemPreview(const ItemData& item, const QString& slotTitle);
    void clearItemPreview();
    std::map<int,SpellData> loadItemSpells(const ItemData& item) const;
    std::map<int,QString> buildLimitSpellNames(const std::map<int,SpellData>& spells) const;
    std::vector<QString> detectSlots(const ItemData& item) const;
    bool canEquip(const ItemData& item) const;
    std::pair<int,int> expansionCaps() const;
    bool isSlotAvailable(const std::string& slotName) const;
    QString buildStatTooltip(const std::string& stat,
                              int dispVal, bool hasCap, int cap,
                              const PlayerTotals& totals) const;

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;
    CharacterInfo*  _charInfo{nullptr};
    PlayerTotals*   _totals{nullptr};
    std::map<std::string, ItemData> _equippedItems;

    SearchComboBox*  _searchCombo{nullptr};
    QStringListModel* _searchModel{nullptr};
    QPushButton*     _clearBtn{nullptr};
    QWidget*        _comparisonArea{nullptr};
    QVBoxLayout*    _comparisonLayout{nullptr};
    QWidget*        _itemPreviewArea{nullptr};
    QVBoxLayout*    _itemPreviewLayout{nullptr};

    QList<ItemData> _searchResults;
    QComboBox*      _slotFilter{nullptr};

    QScrollArea*    _inventoryScroll{nullptr};
    QWidget*        _inventoryContent{nullptr};
    QList<std::pair<int,ItemData>> _bagItems;

    const QSet<QString>* _bisNames{nullptr};
};
```

- [ ] **Step 4.2 — Implémenter `setBisNames()` dans `character_tab.cpp`**

Ajouter après la définition de `setCharacter()` :

```cpp
void CharacterTab::setBisNames(const QSet<QString>* bisNames) {
    _bisNames = bisNames;
}
```

- [ ] **Step 4.3 — Surface 1 : lignes sacs — modifier `addRow` dans `rebuildInventoryPanel()`**

Dans `addRow`, remplacer le bloc `} else {` (item non vide, QPushButton) de la façon suivante.

**Avant** (dans le lambda `addRow`, branche `!isEmpty`) :

```cpp
        } else {
            auto* nameBtn = new QPushButton(itemName);
            nameBtn->setFlat(true);
            nameBtn->setStyleSheet(
                QString("QPushButton { text-align: left; color: %1; background: transparent; "
                        "border: 1px solid transparent; font-size: 12px; padding: 0; }"
                        "QPushButton:hover { color: %2; }"
                        "QPushButton:focus { border: 1px solid %3; border-radius: 2px; }")
                .arg(kTextBase, kGreen, kBorderAccent));
            nameBtn->setToolTip(formatItemTooltip(*item, kTextPrimary));
            if (onClick) connect(nameBtn, &QPushButton::clicked, onClick);
            rl->addWidget(nameBtn, 1);
        }
```

**Après** :

```cpp
        } else {
            bool isBisItem = _bisNames && _bisNames->contains(itemName);
            QString displayName = isBisItem
                ? QString::fromUtf8("\xe2\xad\x90 ") + itemName
                : itemName;
            const char* nameColor = isBisItem ? kAccentGold : kTextBase;
            auto* nameBtn = new QPushButton(displayName);
            nameBtn->setFlat(true);
            nameBtn->setStyleSheet(
                QString("QPushButton { text-align: left; color: %1; background: transparent; "
                        "border: 1px solid transparent; font-size: 12px; padding: 0; }"
                        "QPushButton:hover { color: %2; }"
                        "QPushButton:focus { border: 1px solid %3; border-radius: 2px; }")
                .arg(nameColor, kGreen, kBorderAccent));
            nameBtn->setToolTip(formatItemTooltip(*item, kTextPrimary));
            if (onClick) connect(nameBtn, &QPushButton::clicked, onClick);
            rl->addWidget(nameBtn, 1);
        }
```

- [ ] **Step 4.4 — Surface 2 : grille équipée — modifier `makeEquipCell`**

Dans `makeEquipCell`, remplacer la construction de `cellCss` :

**Avant** :

```cpp
        QString cellCss = QString(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px; }")
            .arg(isEmpty ? kBgBase : kBgTile, kBorderCard);
        if (!isEmpty)
            cellCss += QString(
                "QPushButton:hover { border-color: %1; }"
                "QPushButton:focus { border-color: %1; }").arg(kBorderAccent);
        btn->setStyleSheet(cellCss);
```

**Après** :

```cpp
        bool isBisCell = !isEmpty && _bisNames &&
            _bisNames->contains(QString::fromStdString(eqIt->second.name));
        const char* borderColor = isBisCell ? kAccentGold : kBorderCard;
        QString cellCss = QString(
            "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px; }")
            .arg(isEmpty ? kBgBase : kBgTile, borderColor);
        if (!isEmpty) {
            const char* hoverColor = isBisCell ? kAccentGold : kBorderAccent;
            cellCss += QString(
                "QPushButton:hover { border-color: %1; }"
                "QPushButton:focus { border-color: %1; }").arg(hoverColor);
        }
        btn->setStyleSheet(cellCss);
```

- [ ] **Step 4.5 — Surface 3 : dropdown recherche — modifier `onSearchPopup()`**

Dans `onSearchPopup()`, remplacer la ligne qui ajoute le nom à `names` :

**Avant** :

```cpp
        names << QString::fromStdString(item.name);
```

**Après** :

```cpp
        QString qname = QString::fromStdString(item.name);
        bool isBis = _bisNames && _bisNames->contains(qname);
        names << (isBis ? QString::fromUtf8("\xe2\xad\x90 ") + qname : qname);
```

- [ ] **Step 4.6 — Build de vérification**

```powershell
$s = "$env:TEMP\build_bis4.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug 2>&1 | Select-String "error:" | Select-Object -First 10
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : aucune erreur.

- [ ] **Step 4.7 — Commit**

```bash
git add src/ui/character_tab.h src/ui/character_tab.cpp
git commit -m "feat(ui): CharacterTab — setBisNames(), étoile sacs/grille/recherche"
```

---

## Task 5 — FightTab : `setBisNames()` + loot rows

**Files:**
- Modify: `src/ui/fight_tab.h`
- Modify: `src/ui/fight_tab.cpp`

- [ ] **Step 5.1 — Mettre à jour `fight_tab.h`**

Ajouter include `<QSet>`, méthode `setBisNames()` et membre `_bisNames` :

```cpp
#pragma once
#include "core/types.h"
#include <QFutureWatcher>
#include <QScrollArea>
#include <QSet>
#include <QVBoxLayout>
#include <QWidget>
#include <optional>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;
class QStringListModel;

class FightTab : public QWidget {
    Q_OBJECT
public:
    FightTab(Config* config, NpcDatabase* npcDb,
             ItemDatabase* itemDb, QWidget* parent = nullptr);

    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);
    void refreshStats();
    void setBisNames(const QSet<QString>* bisNames);

signals:
    void lootItemActivated(ItemData item);

private slots:
    void doSearch();
    void onNpcSelected(int index);
    void onSearchDone();
    void onLootClicked(int itemId);
    void toggleLootSort();

private:
    void buildUi();
    void showLootForSlot(const QString& slot);
    QWidget* buildLeftPanel(const NpcData&);
    QWidget* buildRightPanel(const NpcData&);
    QWidget* buildDpsSlowTable(
        const std::vector<std::pair<QString,IncomingDamageResult>>& disciplines,
        const std::vector<std::tuple<QString,std::optional<float>,float>>& slowScenarios,
        float spDps, int hp);

    Config*        _config;
    NpcDatabase*   _npcDb;
    ItemDatabase*  _itemDb;
    CharacterInfo* _charInfo{};
    PlayerTotals*  _totals{};

    class SearchComboBox* _searchCombo;
    QStringListModel*     _searchModel{nullptr};
    QScrollArea*          _leftScroll;
    QScrollArea*          _rightScroll;
    QWidget*              _itemSection{};
    QVBoxLayout*          _itemSectionLayout{};

    NpcData              _currentNpc;
    bool                 _hasNpc{false};
    std::vector<NpcData> _searchResults;
    QString              _lootSort{"chance"};

    ItemData             _lootItem{};
    std::vector<QString> _lootSlots;

    QFutureWatcher<std::vector<NpcData>>* _searchWatcher;

    const QSet<QString>* _bisNames{nullptr};
};
```

- [ ] **Step 5.2 — Implémenter `setBisNames()` dans `fight_tab.cpp`**

Ajouter la méthode (par exemple après `refreshStats()`) :

```cpp
void FightTab::setBisNames(const QSet<QString>* bisNames) {
    _bisNames = bisNames;
}
```

- [ ] **Step 5.3 — Modifier le rendu des loot rows dans `buildRightPanel()`**

Repérer la boucle `for (const auto& item : loot)` (environ ligne 465). Remplacer le bloc de calcul de `nameColor` et de création du bouton :

**Avant** :

```cpp
                const char* nameColor;
                if (item.scrolleffect > 0) {
                    auto spell = _itemDb->getSpellById(item.scrolleffect);
                    bool usable = spell && charCanUseSpell(*spell, _charInfo);
                    nameColor = usable ? kTextPrimary : kTextSecondary;
                } else {
                    bool equippable = item.item_slots > 0;
                    bool usable = charCanEquip(item, _charInfo);
                    nameColor = usable ? kTextPrimary : (equippable ? kTextSecondary : kTextDim);
                }

                auto* row = new QWidget; row->setStyleSheet("background:transparent;");
                auto* rowL = new QHBoxLayout(row);
                rowL->setContentsMargins(0,0,0,0); rowL->setSpacing(4);

                auto* btn = new QPushButton(
                    QString("%1  %2%").arg(QString::fromStdString(item.name))
                                      .arg(item.chance, 0, 'f', 0));
```

**Après** :

```cpp
                const char* nameColor;
                bool isUsable;
                if (item.scrolleffect > 0) {
                    auto spell = _itemDb->getSpellById(item.scrolleffect);
                    isUsable  = spell && charCanUseSpell(*spell, _charInfo);
                    nameColor = isUsable ? kTextPrimary : kTextSecondary;
                } else {
                    bool equippable = item.item_slots > 0;
                    isUsable  = charCanEquip(item, _charInfo);
                    nameColor = isUsable ? kTextPrimary : (equippable ? kTextSecondary : kTextDim);
                }

                QString itemName = QString::fromStdString(item.name);
                bool isBis = isUsable && _bisNames && _bisNames->contains(itemName);
                if (isBis) nameColor = kAccentGold;
                QString btnText = (isBis ? QString::fromUtf8("\xe2\xad\x90 ") : QString())
                    + itemName
                    + QString("  %1%").arg(item.chance, 0, 'f', 0);

                auto* row = new QWidget; row->setStyleSheet("background:transparent;");
                auto* rowL = new QHBoxLayout(row);
                rowL->setContentsMargins(0,0,0,0); rowL->setSpacing(4);

                auto* btn = new QPushButton(btnText);
```

- [ ] **Step 5.4 — Build de vérification**

```powershell
$s = "$env:TEMP\build_bis5.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build/debug 2>&1 | Select-String "error:" | Select-Object -First 10
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : aucune erreur.

- [ ] **Step 5.5 — Commit**

```bash
git add src/ui/fight_tab.h src/ui/fight_tab.cpp
git commit -m "feat(ui): FightTab — setBisNames(), étoile loot rows"
```

---

## Task 6 — Badge ⭐ BiS dans `makeItemCard` + mise à jour call sites

**Files:**
- Modify: `src/ui/item_card.h`
- Modify: `src/ui/item_card.cpp`
- Modify: `src/ui/character_tab.cpp` (call sites)
- Modify: `src/ui/fight_tab.cpp` (call site)

- [ ] **Step 6.1 — Ajouter `bool isBis` à `item_card.h`**

```cpp
#pragma once
#include "core/types.h"
#include <QString>
#include <map>
class QFrame;

// Unified item card (Option B — compact card with header + grouped stats).
// item             : item to display. null = shows an empty slot card.
// ref              : reference item for Δ comparison. null = no delta column.
// spells           : pre-loaded SpellData keyed by spell_id.
// titleOverride    : if non-empty, overrides the header (use for slot names).
// limitSpellNames  : optional map of spell_id → name for SE_LimitSpell (139).
// highlightSlot    : if non-empty, bolds the matching slot entry in the slot list.
// isBis            : if true, shows a ⭐ BiS badge in the card header.
QFrame* makeItemCard(
    const ItemData*                  item,
    const ItemData*                  ref              = nullptr,
    const std::map<int,SpellData>*   spells           = nullptr,
    const QString&                   titleOverride    = {},
    const std::map<int,QString>*     limitSpellNames  = nullptr,
    const QString&                   highlightSlot    = {},
    bool                             isBis            = false
);
```

- [ ] **Step 6.2 — Ajouter le badge dans `item_card.cpp` — fonction `makeItemCard`**

Dans `makeItemCard`, repérer la signature :
```cpp
QFrame* makeItemCard(const ItemData* item, const ItemData* ref,
```
Changer pour accepter `bool isBis` :
```cpp
QFrame* makeItemCard(const ItemData* item, const ItemData* ref,
                     const std::map<int,SpellData>* spells,
                     const QString& titleOverride,
                     const std::map<int,QString>* limitSpellNames,
                     const QString& highlightSlot,
                     bool isBis)
```

Puis, dans la section header, remplacer le bloc qui ajoute `nameLbl` :

**Avant** :

```cpp
    auto* nameLbl = new QLabel(displayName);
    // PlainText : un nom d'item contenant '<', '>' ou '&' ne doit jamais être
    // interprété comme du HTML par l'heuristique Qt::AutoText par défaut.
    nameLbl->setTextFormat(Qt::PlainText);
    nameLbl->setStyleSheet(
        QString("font-weight: bold; font-size: 13px; color: %1; "
                "border: none; background: transparent;").arg(kTextPrimary));
    nameLbl->setWordWrap(true);
    hL->addWidget(nameLbl);
```

**Après** :

```cpp
    auto* nameLbl = new QLabel(displayName);
    // PlainText : un nom d'item contenant '<', '>' ou '&' ne doit jamais être
    // interprété comme du HTML par l'heuristique Qt::AutoText par défaut.
    nameLbl->setTextFormat(Qt::PlainText);
    nameLbl->setStyleSheet(
        QString("font-weight: bold; font-size: 13px; color: %1; "
                "border: none; background: transparent;").arg(kTextPrimary));
    nameLbl->setWordWrap(true);

    if (isBis) {
        auto* nameRow = new QWidget;
        nameRow->setStyleSheet("background: transparent;");
        auto* nameRowL = new QHBoxLayout(nameRow);
        nameRowL->setContentsMargins(0, 0, 0, 0);
        nameRowL->setSpacing(6);
        nameRowL->addWidget(nameLbl, 1);
        auto* bisLbl = new QLabel(QString::fromUtf8("\xe2\xad\x90 BiS"));
        bisLbl->setTextFormat(Qt::PlainText);
        bisLbl->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold; "
                    "background: transparent; border: none;").arg(kAccentGold));
        nameRowL->addWidget(bisLbl);
        hL->addWidget(nameRow);
    } else {
        hL->addWidget(nameLbl);
    }
```

- [ ] **Step 6.3 — Mettre à jour `buildSlotCard` dans `character_tab.cpp`**

Localiser la fonction `buildSlotCard` (vers ligne 691) et ajouter le paramètre `isBis` :

**Avant** :

```cpp
static QFrame* buildSlotCard(const ItemData* item, const ItemData* refItem,
                               const QString& slotTitle,
                               const std::map<int,SpellData>*  spells          = nullptr,
                               const std::map<int,QString>*    limitSpellNames = nullptr)
{
    return makeItemCard(item, refItem, spells, slotTitle, limitSpellNames);
}
```

**Après** :

```cpp
static QFrame* buildSlotCard(const ItemData* item, const ItemData* refItem,
                               const QString& slotTitle,
                               const std::map<int,SpellData>*  spells          = nullptr,
                               const std::map<int,QString>*    limitSpellNames = nullptr,
                               bool                            isBis           = false)
{
    return makeItemCard(item, refItem, spells, slotTitle, limitSpellNames, {}, isBis);
}
```

- [ ] **Step 6.4 — Mettre à jour `showComparison()` dans `character_tab.cpp`**

Juste avant les deux appels `buildSlotCard` / `makeItemCard` (vers ligne 867), ajouter le calcul de `isBis` et passer la valeur :

**Avant** :

```cpp
    colL->addWidget(buildSlotCard(curItem, nullptr, curName,
                                  curItem ? &curSpells : nullptr,
                                  curItem ? &curLimitNames : nullptr));
    colL->addWidget(makeItemCard(&newItem, curItem,
                                 newSpells.empty() ? nullptr : &newSpells,
                                 {},
                                 newLimitNames.empty() ? nullptr : &newLimitNames));
```

**Après** :

```cpp
    bool isBisCur = curItem && _bisNames &&
        _bisNames->contains(QString::fromStdString(curItem->name));
    bool isBisNew = _bisNames &&
        _bisNames->contains(QString::fromStdString(newItem.name));

    colL->addWidget(buildSlotCard(curItem, nullptr, curName,
                                  curItem ? &curSpells : nullptr,
                                  curItem ? &curLimitNames : nullptr,
                                  isBisCur));
    colL->addWidget(makeItemCard(&newItem, curItem,
                                 newSpells.empty() ? nullptr : &newSpells,
                                 {},
                                 newLimitNames.empty() ? nullptr : &newLimitNames,
                                 {},
                                 isBisNew));
```

- [ ] **Step 6.5 — Mettre à jour `showItemPreview()` dans `character_tab.cpp`**

**Avant** (vers ligne 976) :

```cpp
    _itemPreviewLayout->addWidget(makeItemCard(&item, nullptr,
                                                spells.empty() ? nullptr : &spells,
                                                {},
                                                limitNames.empty() ? nullptr : &limitNames,
                                                slotTitle));
```

**Après** :

```cpp
    bool isBisPreview = _bisNames &&
        _bisNames->contains(QString::fromStdString(item.name));
    _itemPreviewLayout->addWidget(makeItemCard(&item, nullptr,
                                                spells.empty() ? nullptr : &spells,
                                                {},
                                                limitNames.empty() ? nullptr : &limitNames,
                                                slotTitle,
                                                isBisPreview));
```

- [ ] **Step 6.6 — Mettre à jour `showLootForSlot()` dans `fight_tab.cpp`**

**Avant** (vers ligne 943) :

```cpp
    _itemSectionLayout->addWidget(
        makeItemCard(&_lootItem, equippedItem ? &*equippedItem : nullptr,
                     spells.empty() ? nullptr : &spells,
                     {},
                     limitNames.empty() ? nullptr : &limitNames));
```

**Après** :

```cpp
    bool isBisLoot = _bisNames &&
        _bisNames->contains(QString::fromStdString(_lootItem.name));
    _itemSectionLayout->addWidget(
        makeItemCard(&_lootItem, equippedItem ? &*equippedItem : nullptr,
                     spells.empty() ? nullptr : &spells,
                     {},
                     limitNames.empty() ? nullptr : &limitNames,
                     {},
                     isBisLoot));
```

- [ ] **Step 6.7 — Build complet debug + tests**

```powershell
$s = "$env:TEMP\build_bis6.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& $cmake --build build/debug
Set-Location build/debug
& ctest --output-on-failure
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : build sans erreur, 29 tests passent.

- [ ] **Step 6.8 — Commit**

```bash
git add src/ui/item_card.h src/ui/item_card.cpp src/ui/character_tab.cpp src/ui/fight_tab.cpp
git commit -m "feat(ui): badge BiS dans item cards, call sites mis à jour"
```

---

## Task 7 — Vérification manuelle + build release

- [ ] **Step 7.1 — Lancer l'app en debug et vérifier les 5 surfaces**

Lancer `build/debug/EqQuarmCompanion.exe`. Scénario de test :
1. Sélectionner un personnage avec classe et extension configurées (ex: Enchanter + Luclin)
2. **Grille équipée** : les cellules d'items BIS doivent avoir une bordure gold (`#ffc947`)
3. **Sacs** : les items BIS dans les bags doivent afficher `⭐ Item Name` en or
4. **Recherche** : taper un nom d'item BIS → il apparaît en tête ou marqué `⭐` dans la dropdown
5. **Item card** : cliquer un item BIS → badge `⭐ BiS` en or dans le header de la carte
6. **Fight tab loot** : sélectionner un NPC qui droppe un item BIS du perso → ligne marquée `⭐`
7. Changer d'extension (onglet Infos) → les marqueurs se mettent à jour (async, ~1-2s si pas de cache)
8. Re-lancer l'app → le cache JSON est chargé instantanément (pas d'appel réseau)

- [ ] **Step 7.2 — Build release**

```powershell
$s = "$env:TEMP\build_bis_rel.ps1"
@'
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
$cmake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
Set-Location "D:\Games\quarm\source\EqQuarmCompanion"
& $cmake --preset windows-x64-release
& $cmake --build build/release --target EqQuarmCompanion
'@ | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : `build/release/EqQuarmCompanion.exe` généré sans erreur.

- [ ] **Step 7.3 — Regénérer l'installeur**

```powershell
$s = "$env:TEMP\build_installer_bis.ps1"
'Set-Location "D:\Games\quarm\source\EqQuarmCompanion"; & ".\installer\build_installer.ps1"' | Out-File $s -Encoding utf8
& $s; Remove-Item $s
```

Résultat attendu : `installer/output/EqQuarmCompanion-Setup.exe` mis à jour.

- [ ] **Step 7.4 — Commit final + push**

```bash
git add .
git commit -m "feat: BIS highlight — étoile ⭐ sur items BIS dans toutes les surfaces UI"
git push
```
