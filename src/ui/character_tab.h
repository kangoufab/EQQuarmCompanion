#pragma once
#include "core/types.h"
#include "ui/item_card.h"
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

    QList<ItemData> _searchResults;
    QComboBox*      _slotFilter{nullptr};

    QScrollArea*    _inventoryScroll{nullptr};
    QWidget*        _inventoryContent{nullptr};
    QList<std::pair<int,ItemData>> _bagItems;  // {bag_number, item}
    void rebuildInventoryPanel();
};
