#pragma once
#include "core/types.h"
#include <QFutureWatcher>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <optional>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;

class FightTab : public QWidget {
    Q_OBJECT
public:
    FightTab(Config* config, NpcDatabase* npcDb,
             ItemDatabase* itemDb, QWidget* parent = nullptr);

    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);
    void refreshStats();

signals:
    void itemSelected(const QString& itemName);

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
};
