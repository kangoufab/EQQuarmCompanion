#pragma once
#include "core/types.h"
#include <QFrame>
#include <QScrollArea>
#include <QWidget>

class Config;
class NpcDatabase;

class SpellsTab : public QWidget {
    Q_OBJECT
public:
    SpellsTab(Config* config, NpcDatabase* npcDb, QWidget* parent = nullptr);
    void setCharacter(CharacterInfo* charInfo, PlayerTotals* totals);

private:
    void buildUi();
    QWidget* buildSpellList();
    QWidget* buildSpellRow(const SpellData& spell, int index);

    Config*        _config;
    NpcDatabase*   _npcDb;
    CharacterInfo* _charInfo{};
    PlayerTotals*  _totals{};
    QScrollArea*   _spellsScroll;
    QScrollArea*   _analysisScroll;
};
