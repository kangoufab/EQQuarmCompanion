#pragma once
#include "core/types.h"
#include <QWidget>
class Config; class NpcDatabase; class ItemDatabase;
class CharacterTab : public QWidget {
    Q_OBJECT
public:
    CharacterTab(Config*, NpcDatabase*, ItemDatabase*, QWidget* p=nullptr)
        : QWidget(p) {}
    void setCharacter(CharacterInfo*, PlayerTotals*) {}
};
