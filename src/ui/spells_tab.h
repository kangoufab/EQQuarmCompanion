#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase;
class SpellsTab : public QWidget {
    Q_OBJECT
public:
    SpellsTab(Config*, NpcDatabase*, QWidget* p=nullptr) : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};
