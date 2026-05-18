#pragma once
#include "core/types.h"
#include <QWidget>
#include <map>
#include <string>
#include <vector>

class Config;
class ItemDatabase;
class QLabel;
class QListWidget;
class QVBoxLayout;
class QScrollArea;
class QFrame;

// Un buff actif : sort + classe qui l'a casté + niveau minimum
struct ActiveBuff {
    SpellData   spell;
    std::string buffClass;
    int         minLevel{};
};

class SpellsTab : public QWidget {
    Q_OBJECT
public:
    SpellsTab(Config* config, ItemDatabase* itemDb, QWidget* parent = nullptr);
    void setCharacter(CharacterInfo* charInfo, PlayerTotals* baseTotals,
                      const std::map<std::string, ItemData>& equipped);

private slots:
    void onClassSelected(int row);

private:
    void buildUi();
    void loadSpellsForClass(const std::string& className);
    void rebuildRightPanel();
    void rebuildClassList();
    void refreshStats();
    int  expansionMaxLevel() const;

    Config*        _config;
    ItemDatabase*  _itemDb;
    CharacterInfo* _charInfo{nullptr};
    PlayerTotals*  _baseTotals{nullptr};
    std::map<std::string, ItemData> _equippedItems;

    QListWidget*  _classList{nullptr};
    QWidget*      _rightInner{nullptr};
    QVBoxLayout*  _rightLayout{nullptr};
    QWidget*      _statsHolder{nullptr};
    QVBoxLayout*  _statsLayout{nullptr};
    QLabel*       _headerLabel{nullptr};

    std::vector<ActiveBuff>  _activeBuffs;
    std::map<int, int>       _conflicts;       // loser_id → winner_id
    std::vector<SpellData>   _currentClassSpells;
    std::string              _currentClass;
};
