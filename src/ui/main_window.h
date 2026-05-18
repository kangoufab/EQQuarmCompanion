#pragma once
#include "core/character_parser.h"
#include "core/stats_calculator.h"
#include "core/types.h"
#include <QComboBox>
#include <QLabel>
#include <QMainWindow>
#include <QTabWidget>
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
    void openSettings();

private:
    void loadCharacterFiles();
    void refreshAllTabs();
    void recalculateTotals();

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;

    QComboBox*    _charSelector;
    QTabWidget*   _tabs;
    CharacterTab* _charTab;
    FightTab*     _fightTab;
    SpellsTab*    _spellsTab;
    InfosTab*     _infosTab;

    std::vector<CharacterInfo>      _characters;
    CharacterInfo                   _currentChar;
    PlayerTotals                    _playerTotals;
    std::map<std::string, ItemData> _equippedItems;
};
