#include "ui/spells_tab.h"
#include "ui/ui_helpers.h"
#include "core/config.h"
#include "core/spell_stacking.h"
#include "db/npc_database.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

SpellsTab::SpellsTab(Config* cfg, NpcDatabase* npcDb, QWidget* parent)
    : QWidget(parent), _config(cfg), _npcDb(npcDb)
{
    buildUi();
}

void SpellsTab::buildUi() {
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    _spellsScroll   = new QScrollArea;
    _analysisScroll = new QScrollArea;
    _spellsScroll->setWidgetResizable(true);
    _analysisScroll->setWidgetResizable(true);
    _spellsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _analysisScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lbl  = new QLabel("Selectionner un personnage");
    lbl->setAlignment(Qt::AlignCenter);
    _spellsScroll->setWidget(lbl);

    auto* lbl2 = new QLabel("Selectionner un sort");
    lbl2->setAlignment(Qt::AlignCenter);
    _analysisScroll->setWidget(lbl2);

    outer->addWidget(_spellsScroll, 1);
    outer->addWidget(_analysisScroll, 1);
}

void SpellsTab::setCharacter(CharacterInfo* ci, PlayerTotals* totals) {
    _charInfo = ci;
    _totals   = totals;
    _spellsScroll->setWidget(buildSpellList());
}

QWidget* SpellsTab::buildSpellList() {
    auto* w  = new QWidget;
    auto* vl = new QVBoxLayout(w);
    vl->setAlignment(Qt::AlignTop);

    if (!_charInfo) return w;

    auto* name = new QLabel(
        QString("Sorts de %1 (%2)")
            .arg(QString::fromStdString(_charInfo->name))
            .arg(QString::fromStdString(_charInfo->class_name)));
    name->setStyleSheet("font-size:13px;font-weight:bold;color:#e0e0e0;");
    vl->addWidget(name);

    // Placeholder — porter depuis spells_tab.py::_build_spell_list()
    auto* placeholder = new QLabel("(Liste des sorts a porter depuis spells_tab.py)");
    placeholder->setStyleSheet("color:#555555;");
    vl->addWidget(placeholder);

    vl->addStretch();
    return w;
}

QWidget* SpellsTab::buildSpellRow(const SpellData& spell, int index) {
    auto* frame = new QFrame;
    frame->setStyleSheet(
        index % 2 == 0 ? "background:#16161e;border-radius:3px;"
                       : "background:#1e1e2c;border-radius:3px;");
    auto* inner = new QVBoxLayout(frame);
    inner->setContentsMargins(8, 4, 8, 4);
    auto* lbl = new QLabel(QString::fromStdString(spell.name));
    lbl->setStyleSheet("color:#e0e0e0;background:transparent;");
    inner->addWidget(lbl);
    return frame;
}
