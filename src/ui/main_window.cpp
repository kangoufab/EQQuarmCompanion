#include "ui/main_window.h"
#include "ui/character_tab.h"
#include "ui/fight_tab.h"
#include "ui/spells_tab.h"
#include "ui/infos_tab.h"
#include "ui/settings_dialog.h"
#include "ui/stats_bar.h"
#include "core/config.h"
#include "db/npc_database.h"
#include "db/item_database.h"
#include <filesystem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(Config* config, NpcDatabase* npcDb,
                        ItemDatabase* itemDb, QWidget* parent)
    : QMainWindow(parent), _config(config), _npcDb(npcDb), _itemDb(itemDb)
{
    setWindowTitle("EQ Quarm Companion");
    resize(1280, 800);

    // ── Toolbar ──────────────────────────────────────────────────────────────
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

    // ── Widget central ───────────────────────────────────────────────────────
    auto* central = new QWidget;
    central->setStyleSheet("background: #0f1624;");
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(8, 8, 8, 8);
    centralLayout->setSpacing(6);

    // En-tête personnage (nom + classe + niveau)
    {
        auto* frame = new QWidget;
        frame->setStyleSheet(
            "background: #1a2236; border-radius: 4px; border: 1px solid #3a4a6a;");
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        _charHeaderLabel = new QLabel(QString::fromUtf8("\xe2\x80\x94"));
        _charHeaderLabel->setStyleSheet(
            "font-weight: bold; font-size: 12px; color: #e0e0e0; "
            "border: none; background: transparent;");
        fl->addWidget(_charHeaderLabel);
        centralLayout->addWidget(frame);
    }

    // Bandeau stats global
    {
        auto* holder = new QWidget;
        holder->setStyleSheet("background: transparent;");
        _globalStatsLayout = new QVBoxLayout(holder);
        _globalStatsLayout->setContentsMargins(0, 0, 0, 0);
        centralLayout->addWidget(holder);
    }

    // Onglets
    _charTab   = new CharacterTab(config, npcDb, itemDb);
    _fightTab  = new FightTab(config, npcDb, itemDb);
    _spellsTab = new SpellsTab(config, itemDb);
    _infosTab  = new InfosTab(config);

    _tabs = new QTabWidget;
    _tabs->addTab(_charTab,   "Stuff");
    _tabs->addTab(_spellsTab, "Buffs");
    _tabs->addTab(_fightTab,  "Fight");
    _tabs->addTab(_infosTab,  "Infos");
    centralLayout->addWidget(_tabs, 1);

    setCentralWidget(central);

    // Signaux des onglets
    connect(_charTab,   &CharacterTab::statsChanged, this, &MainWindow::onStatsChanged);
    connect(_spellsTab, &SpellsTab::statsChanged,    this, &MainWindow::onStatsChanged);
    connect(_tabs, &QTabWidget::currentChanged,      this, &MainWindow::onTabChanged);

    loadCharacterFiles();
}

// ── Stats bar globale ─────────────────────────────────────────────────────

void MainWindow::rebuildGlobalStatsBar(const PlayerTotals& totals) {
    while (_globalStatsLayout->count()) {
        auto* child = _globalStatsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (_currentChar.level > 0)
        _globalStatsLayout->addWidget(
            makePlayerStatsBar(totals, _currentChar.class_name,
                               _config->get("current_expansion")));
}

void MainWindow::onStatsChanged(PlayerTotals totals) {
    rebuildGlobalStatsBar(totals);
}

void MainWindow::onTabChanged(int /*index*/) {
    // Retour aux stats de base quand on change d'onglet
    rebuildGlobalStatsBar(_playerTotals);
}

// ── Chargement des personnages ────────────────────────────────────────────

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
    _currentChar = _characters[index];
    _equippedItems.clear();

    for (const auto& [slot, itemId] : _currentChar.equipped) {
        auto item = _itemDb->getItemById(itemId);
        if (item)
            _equippedItems[slot] = *item;
    }

    recalculateTotals();
    refreshAllTabs();
}

void MainWindow::recalculateTotals() {
    std::vector<ItemData> items;
    items.reserve(_equippedItems.size());
    for (const auto& kv : _equippedItems)
        items.push_back(kv.second);

    int primaryItemtype = 0;
    auto pit = _equippedItems.find("Primary");
    if (pit != _equippedItems.end())
        primaryItemtype = pit->second.itemtype;

    _playerTotals = calculateTotals(_currentChar, items, primaryItemtype);
}

void MainWindow::refreshAllTabs() {
    // En-tête personnage
    if (!_currentChar.name.empty()) {
        _charHeaderLabel->setText(
            QString::fromStdString(_currentChar.name) + "  \xe2\x80\x94  " +
            QString::fromStdString(_currentChar.class_name) +
            "  niv. " + QString::number(_currentChar.level));
    } else {
        _charHeaderLabel->setText(QString::fromUtf8("\xe2\x80\x94"));
    }

    // Stats de base
    rebuildGlobalStatsBar(_playerTotals);

    _charTab->setCharacter(&_currentChar, &_playerTotals, _equippedItems);
    _fightTab->setCharacter(&_currentChar, &_playerTotals);
    _spellsTab->setCharacter(&_currentChar, &_playerTotals, _equippedItems);
}

void MainWindow::openSettings() {
    SettingsDialog dlg(_config, this);
    if (dlg.exec() == QDialog::Accepted)
        loadCharacterFiles();
}
