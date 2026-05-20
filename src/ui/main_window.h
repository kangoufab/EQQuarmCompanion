#pragma once
#include "core/character_parser.h"
#include "core/stats_calculator.h"
#include "core/types.h"
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
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
    PlayerTotalsExtra               _playerExtra;
    AaStats                         _aaStats;
    std::map<std::string, ItemData> _equippedItems;
};
