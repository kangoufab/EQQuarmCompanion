#include "ui/main_window.h"
#include "ui/character_tab.h"
#include "ui/fight_tab.h"
#include "ui/spells_tab.h"
#include "ui/infos_tab.h"
#include "ui/settings_dialog.h"
#include "core/config.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include <filesystem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QWidget>

MainWindow::MainWindow(Config* config, NpcDatabase* npcDb,
                        ItemDatabase* itemDb, QWidget* parent)
    : QMainWindow(parent), _config(config), _npcDb(npcDb), _itemDb(itemDb)
{
    setWindowTitle("EQ Quarm Companion");
    resize(1280, 800);

    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    _charSelector = new QComboBox;
    _charSelector->setMinimumWidth(200);
    toolbar->addWidget(new QLabel("  Personnage : "));
    toolbar->addWidget(_charSelector);
    toolbar->addSeparator();
    auto* settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99"));
    settingsBtn->setFlat(true);
    toolbar->addWidget(settingsBtn);

    connect(_charSelector, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCharacterChanged);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::openSettings);

    _charTab   = new CharacterTab(config, npcDb, itemDb);
    _fightTab  = new FightTab(config, npcDb, itemDb);
    _spellsTab = new SpellsTab(config, npcDb);
    _infosTab  = new InfosTab(config);

    _tabs = new QTabWidget;
    _tabs->addTab(_charTab,   "Stuff");
    _tabs->addTab(_fightTab,  "Fight");
    _tabs->addTab(_spellsTab, "Spells");
    _tabs->addTab(_infosTab,  "Infos");

    setCentralWidget(_tabs);
    loadCharacterFiles();
}

void MainWindow::loadCharacterFiles() {
    auto eqDir = std::filesystem::path(
        QString::fromStdString(_config->get("eq_files_dir")).toStdWString());
    auto files = findCharacterFiles(eqDir);

    _charSelector->blockSignals(true);
    _charSelector->clear();
    _characters.clear();
    for (auto& f : files) {
        auto ci = parseCharacterFile(f);
        if (!ci) continue;
        _characters.push_back(*ci);
        _charSelector->addItem(
            QString::fromStdString(ci->name + " (" + ci->class_name + ")"));
    }
    _charSelector->blockSignals(false);
    if (!_characters.empty()) onCharacterChanged(0);
}

void MainWindow::onCharacterChanged(int index) {
    if (index < 0 || index >= static_cast<int>(_characters.size())) return;
    _currentChar  = _characters[index];
    _playerTotals = calculateTotals(_currentChar, {});
    refreshAllTabs();
}

void MainWindow::refreshAllTabs() {
    _charTab->setCharacter(&_currentChar, &_playerTotals);
    _fightTab->setCharacter(&_currentChar, &_playerTotals);
    _spellsTab->setCharacter(&_currentChar, &_playerTotals);
}

void MainWindow::openSettings() {
    SettingsDialog dlg(_config, this);
    if (dlg.exec() == QDialog::Accepted)
        loadCharacterFiles();
}
