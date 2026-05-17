#pragma once
#include "core/types.h"
#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QGridLayout>
#include <QMap>

class Config;
class NpcDatabase;
class ItemDatabase;
class SearchComboBox;

class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p = nullptr);
    void setCharacter(CharacterInfo*, PlayerTotals*);

signals:
    void equippedItemChanged(const QString& slotName, int itemId);

private:
    void buildUi();
    void buildStatsPanel(QWidget* container);
    void buildEquipPanel(QWidget* container);
    void refreshStats();

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;
    CharacterInfo* _charInfo{nullptr};
    PlayerTotals*  _totals{nullptr};

    QScrollArea* _statsScroll{nullptr};
    QScrollArea* _equipScroll{nullptr};

    // Stat labels (updated by refreshStats)
    QLabel* _lblName{nullptr};
    QLabel* _lblClassLevel{nullptr};
    QLabel* _lblHp{nullptr};
    QLabel* _lblMana{nullptr};
    QLabel* _lblAtk{nullptr};
    QLabel* _lblAc{nullptr};
    QLabel* _lblStr{nullptr};
    QLabel* _lblSta{nullptr};
    QLabel* _lblDex{nullptr};
    QLabel* _lblAgi{nullptr};
    QLabel* _lblInt{nullptr};
    QLabel* _lblWis{nullptr};
    QLabel* _lblCha{nullptr};
    QLabel* _lblMr{nullptr};
    QLabel* _lblFr{nullptr};
    QLabel* _lblCr{nullptr};
    QLabel* _lblDr{nullptr};
    QLabel* _lblPr{nullptr};
};
