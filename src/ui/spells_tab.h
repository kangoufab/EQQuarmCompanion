#pragma once
#include "core/types.h"
#include <QWidget>
#include <map>
#include <string>
#include <vector>

class Config;
class ItemDatabase;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
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
                      const std::map<std::string, ItemData>& equipped,
                      const AaStats& aaStats);

signals:
    void statsChanged(PlayerTotals totals, PlayerTotalsExtra spellExtra);

private slots:
    void onClassSelected(int row);
    void onSaveSet();
    void onLoadSet();
    void onDeleteSet();

private:
    void buildUi();
    void loadSpellsForClass(const std::string& className);
    void rebuildRightPanel();
    void rebuildClassList();
    void refreshStats();
    void refreshSetsCombo();
    void rebuildActiveBuffsList();
    int  expansionMaxLevel() const;

    Config*        _config;
    ItemDatabase*  _itemDb;
    CharacterInfo* _charInfo{nullptr};
    PlayerTotals*  _baseTotals{nullptr};
    AaStats        _aaStats;
    std::map<std::string, ItemData> _equippedItems;

    QListWidget*  _classList{nullptr};
    QLineEdit*    _spellSearch{nullptr};
    QWidget*      _rightInner{nullptr};
    QVBoxLayout*  _rightLayout{nullptr};
    QLabel*       _headerLabel{nullptr};
    QComboBox*    _setsCombo{nullptr};
    QPushButton*  _loadBtn{nullptr};
    QPushButton*  _deleteBtn{nullptr};

    // Panneau gauche : liste des buffs actifs
    QWidget*      _activeBuffsInner{nullptr};
    QVBoxLayout*  _activeBuffsLayout{nullptr};

    std::vector<ActiveBuff>  _activeBuffs;
    std::map<int, int>       _conflicts;       // loser_id → winner_id
    std::vector<SpellData>   _currentClassSpells;
    std::string              _currentClass;
};
