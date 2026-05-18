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
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

// ── Collecte worn/focus effects ───────────────────────────────────────────

static int parseFlowingThoughtLevel(const std::string& name) {
    static const std::string prefix = "Flowing Thought ";
    if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
        return 0;
    std::string roman = name.substr(prefix.size());
    static const std::map<std::string, int> ROMAN = {
        {"I",1},{"II",2},{"III",3},{"IV",4},{"V",5},
        {"VI",6},{"VII",7},{"VIII",8},{"IX",9},{"X",10},
        {"XI",11},{"XII",12},{"XIII",13},{"XIV",14},{"XV",15},
    };
    auto it = ROMAN.find(roman);
    return it != ROMAN.end() ? it->second : 0;
}

// ID de sort synthétique pour la tooltip Flowing Thought agrégée
static constexpr int FT_SYNTHETIC_ID = -1;

static std::vector<EffectEntry> collectWornEffects(const std::map<std::string, ItemData>& items) {
    int ftTotal = 0;
    std::vector<std::string> ftSources; // noms des items qui contribuent au FT
    std::vector<EffectEntry> others;
    for (const auto& [slot, item] : items) {
        if (item.worneffect_name.empty()) continue;
        int ftLevel = parseFlowingThoughtLevel(item.worneffect_name);
        if (ftLevel > 0) {
            ftTotal += ftLevel;
            ftSources.push_back(item.name + " (" + item.worneffect_name + ")");
        } else {
            others.emplace_back(item.worneffect_name, item.worneffect, item.name);
        }
    }
    std::vector<EffectEntry> result;
    if (ftTotal > 0) {
        // On encode les sources dans le champ item_name (séparées par '\n')
        std::string sources;
        for (auto& s : ftSources) sources += s + "\n";
        result.emplace_back("Flowing Thought +" + std::to_string(ftTotal),
                             FT_SYNTHETIC_ID, sources);
    }
    result.insert(result.end(), others.begin(), others.end());
    return result;
}

static std::vector<EffectEntry> collectFocusEffects(const std::map<std::string, ItemData>& items) {
    std::vector<EffectEntry> result;
    for (const auto& [slot, item] : items)
        if (!item.focuseffect_name.empty())
            result.emplace_back(item.focuseffect_name, item.focuseffect, item.name);
    return result;
}

static std::map<int, SpellData> loadSpellDetails(
    const std::vector<EffectEntry>& worn,
    const std::vector<EffectEntry>& focus,
    ItemDatabase* db)
{
    std::map<int, SpellData> result;
    for (auto& [nm, id, src] : worn)  if (id > 0 && !result.count(id)) { auto s = db->getSpellById(id); if (s) result[id] = *s; }
    for (auto& [nm, id, src] : focus) if (id > 0 && !result.count(id)) { auto s = db->getSpellById(id); if (s) result[id] = *s; }
    return result;
}

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
    connect(_spellsTab, &SpellsTab::statsChanged,    this, &MainWindow::onBuffStatsChanged);
    connect(_tabs, &QTabWidget::currentChanged,      this, &MainWindow::onTabChanged);

    loadCharacterFiles();
}

// ── Stats bar globale ─────────────────────────────────────────────────────

void MainWindow::rebuildGlobalStatsBar(const PlayerTotals& totals,
                                        const PlayerTotalsExtra* extraOverride) {
    while (_globalStatsLayout->count()) {
        auto* child = _globalStatsLayout->takeAt(0);
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }
    if (_currentChar.level > 0) {
        const PlayerTotalsExtra& extra = extraOverride ? *extraOverride : _playerExtra;
        auto worn  = collectWornEffects(_equippedItems);
        auto focus = collectFocusEffects(_equippedItems);
        auto spellDetails = loadSpellDetails(worn, focus, _itemDb);
        _globalStatsLayout->addWidget(
            makePlayerStatsBar(totals, _currentChar.class_name,
                               _config->get("current_expansion"),
                               extra, worn, focus, spellDetails));
    }
}

void MainWindow::onStatsChanged(PlayerTotals totals) {
    rebuildGlobalStatsBar(totals);
}

void MainWindow::onBuffStatsChanged(PlayerTotals totals, PlayerTotalsExtra spellExtra) {
    // Fusionner _playerExtra (base+items+AA) avec les sources de sorts de SpellsTab
    PlayerTotalsExtra merged = _playerExtra;
    for (auto& [k, si] : spellExtra.stats)
        if (!si.spell_sources.empty())
            merged.stats[k].spell_sources = si.spell_sources;
    rebuildGlobalStatsBar(totals, &merged);
}

void MainWindow::onTabChanged(int /*index*/) {
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

    _aaStats = _itemDb->getAaStats(_currentChar.aa_purchases);
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

    _playerExtra  = {};
    _playerTotals = calculateTotals(_currentChar, items, primaryItemtype, &_playerExtra, &_aaStats);
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
