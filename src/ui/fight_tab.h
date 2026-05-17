#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase; class ItemDatabase;
class FightTab : public QWidget {
    Q_OBJECT
public:
    FightTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p=nullptr)
        : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};
