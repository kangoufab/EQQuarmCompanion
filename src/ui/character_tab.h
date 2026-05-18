#pragma once
#include "core/types.h"
#include <QWidget>
#include <QList>
#include <map>
#include <string>
#include <vector>

class Config;
class NpcDatabase;
class ItemDatabase;
class SearchComboBox;
class QScrollArea;
class QLabel;
class QVBoxLayout;
class QFrame;

class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p = nullptr);
    void setCharacter(CharacterInfo*, PlayerTotals*,
                      const std::map<std::string, ItemData>&);

private slots:
    void onSearchPopup();
    void onItemSelected(int index);

private:
    void buildUi();
    QFrame* makeStatsBar(const QString& label,
                         const PlayerTotals& totals,
                         int attrCap, int resistCap,
                         const PlayerTotals* refTotals = nullptr);
    QFrame* makeItemCard(const ItemData* item, const ItemData* refItem,
                         const QString& title, bool showDeltas);
    void refreshStats();
    void showComparison(const ItemData& newItem, const QString& slot,
                        const std::vector<QString>& allSlots = {});
    void clearComparison();
    std::vector<QString> detectSlots(const ItemData& item) const;
    bool canEquip(const ItemData& item) const;
    std::pair<int,int> expansionCaps() const;
    QString buildStatTooltip(const std::string& stat,
                              int dispVal, bool hasCap, int cap,
                              const PlayerTotals& totals) const;

    Config*       _config;
    NpcDatabase*  _npcDb;
    ItemDatabase* _itemDb;
    CharacterInfo*  _charInfo{nullptr};
    PlayerTotals*   _totals{nullptr};
    std::map<std::string, ItemData> _equippedItems;

    QLabel*         _lblHeader{nullptr};
    QVBoxLayout*    _statsLayout{nullptr};   // unique bandeau stats (avant ou après)
    SearchComboBox* _searchCombo{nullptr};
    QWidget*        _comparisonArea{nullptr};
    QVBoxLayout*    _comparisonLayout{nullptr};

    QList<ItemData> _searchResults;
};
