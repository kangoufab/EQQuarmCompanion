#include "ui/character_tab.h"
#include "ui/widgets.h"
#include "core/config.h"
#include "db/npc_database.h"
#include "db/item_database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>
#include <QString>

// ── Palette de couleurs ───────────────────────────────────────────────────────
static const char* kBlue   = "#64b5f6";
static const char* kOrange = "#ffb74d";
static const char* kGreen  = "#81c784";
static const char* kRed    = "#ef5350";
static const char* kGrey   = "#888888";

// ── Slots EQ standards (22 slots) ────────────────────────────────────────────
static const QStringList kEqSlots = {
    "Head", "Face", "Neck", "Shoulders", "Arms", "Back",
    "Left Wrist", "Right Wrist", "Range", "Hands",
    "Primary", "Secondary",
    "Left Finger", "Right Finger",
    "Chest", "Legs", "Feet", "Waist",
    "Left Ear", "Right Ear",
    "Charm", "Ammo"
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static QFrame* makeSectionFrame()
{
    auto* frame = new QFrame;
    frame->setStyleSheet(
        "QFrame{background:#1a2236;border-radius:4px;border:1px solid #3a4a6a;}");
    return frame;
}

static QLabel* makeSectionTitle(const QString& text, const char* color)
{
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("font-weight:bold;color:%1;font-size:11px;").arg(color));
    return lbl;
}

static QLabel* makeKeyLabel(const QString& text)
{
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString("color:%1;font-size:10px;").arg(kGrey));
    return lbl;
}

static QLabel* makeValueLabel(const QString& text, const char* color = nullptr)
{
    auto* lbl = new QLabel(text);
    QString style = "font-weight:bold;font-size:10px;";
    if (color)
        style += QString("color:%1;").arg(color);
    lbl->setStyleSheet(style);
    return lbl;
}

// ── Constructor ───────────────────────────────────────────────────────────────

CharacterTab::CharacterTab(Config* config, NpcDatabase* npcDb,
                           ItemDatabase* itemDb, QWidget* p)
    : QWidget(p), _config(config), _npcDb(npcDb), _itemDb(itemDb)
{
    buildUi();
}

// ── setCharacter ──────────────────────────────────────────────────────────────

void CharacterTab::setCharacter(CharacterInfo* charInfo, PlayerTotals* totals)
{
    _charInfo = charInfo;
    _totals   = totals;
    refreshStats();
}

// ── buildUi ───────────────────────────────────────────────────────────────────

void CharacterTab::buildUi()
{
    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(4, 4, 4, 4);
    outerLayout->setSpacing(6);

    // ── Panneau gauche : Stats ──────────────────────────────────────────────
    _statsScroll = new QScrollArea;
    _statsScroll->setWidgetResizable(true);
    _statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _statsScroll->setFixedWidth(220);
    _statsScroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    auto* statsContainer = new QWidget;
    statsContainer->setStyleSheet("background:#0f1624;");
    buildStatsPanel(statsContainer);
    _statsScroll->setWidget(statsContainer);

    // ── Panneau droit : Équipement ──────────────────────────────────────────
    _equipScroll = new QScrollArea;
    _equipScroll->setWidgetResizable(true);
    _equipScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _equipScroll->setStyleSheet("QScrollArea{border:none;background:transparent;}");

    auto* equipContainer = new QWidget;
    equipContainer->setStyleSheet("background:#0f1624;");
    buildEquipPanel(equipContainer);
    _equipScroll->setWidget(equipContainer);

    outerLayout->addWidget(_statsScroll);
    outerLayout->addWidget(_equipScroll, 1);
}

// ── buildStatsPanel ───────────────────────────────────────────────────────────

void CharacterTab::buildStatsPanel(QWidget* container)
{
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(8);

    // ── Section Header ──────────────────────────────────────────────────────
    {
        auto* frame = makeSectionFrame();
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        fl->setSpacing(4);

        _lblName = new QLabel("—");
        _lblName->setStyleSheet("font-weight:bold;font-size:12px;color:#e0e0e0;");
        _lblClassLevel = new QLabel("—");
        _lblClassLevel->setStyleSheet("font-size:10px;color:#b0b0b0;");

        fl->addWidget(_lblName);
        fl->addWidget(_lblClassLevel);
        vl->addWidget(frame);
    }

    // ── Section Combat (bleu) ───────────────────────────────────────────────
    {
        auto* frame = makeSectionFrame();
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        fl->setSpacing(4);
        fl->addWidget(makeSectionTitle("COMBAT", kBlue));

        auto* gw = new QWidget;
        gw->setStyleSheet("background:transparent;");
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);

        g->addWidget(makeKeyLabel("HP"),   0, 0);
        _lblHp   = makeValueLabel("0", kGreen);
        g->addWidget(_lblHp, 0, 1);

        g->addWidget(makeKeyLabel("Mana"), 1, 0);
        _lblMana = makeValueLabel("0", kGreen);
        g->addWidget(_lblMana, 1, 1);

        g->addWidget(makeKeyLabel("ATK"),  2, 0);
        _lblAtk  = makeValueLabel("0", kOrange);
        g->addWidget(_lblAtk, 2, 1);

        g->addWidget(makeKeyLabel("AC"),   3, 0);
        _lblAc   = makeValueLabel("0", kBlue);
        g->addWidget(_lblAc, 3, 1);

        fl->addWidget(gw);
        vl->addWidget(frame);
    }

    // ── Section Stats (orange) ──────────────────────────────────────────────
    {
        auto* frame = makeSectionFrame();
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        fl->setSpacing(4);
        fl->addWidget(makeSectionTitle("STATS", kOrange));

        auto* gw = new QWidget;
        gw->setStyleSheet("background:transparent;");
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);

        struct { const char* key; QLabel** lbl; } rows[] = {
            {"STR", &_lblStr}, {"STA", &_lblSta}, {"DEX", &_lblDex},
            {"AGI", &_lblAgi}, {"INT", &_lblInt}, {"WIS", &_lblWis},
            {"CHA", &_lblCha},
        };
        for (int i = 0; i < 7; ++i) {
            g->addWidget(makeKeyLabel(rows[i].key), i, 0);
            *rows[i].lbl = makeValueLabel("0");
            g->addWidget(*rows[i].lbl, i, 1);
        }

        fl->addWidget(gw);
        vl->addWidget(frame);
    }

    // ── Section Resists (rouge) ─────────────────────────────────────────────
    {
        auto* frame = makeSectionFrame();
        auto* fl = new QVBoxLayout(frame);
        fl->setContentsMargins(8, 6, 8, 6);
        fl->setSpacing(4);
        fl->addWidget(makeSectionTitle("RESISTS", kRed));

        auto* gw = new QWidget;
        gw->setStyleSheet("background:transparent;");
        auto* g = new QGridLayout(gw);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(4);

        struct { const char* key; QLabel** lbl; } rows[] = {
            {"MR", &_lblMr}, {"FR", &_lblFr}, {"CR", &_lblCr},
            {"DR", &_lblDr}, {"PR", &_lblPr},
        };
        for (int i = 0; i < 5; ++i) {
            g->addWidget(makeKeyLabel(rows[i].key), i, 0);
            *rows[i].lbl = makeValueLabel("0");
            g->addWidget(*rows[i].lbl, i, 1);
        }

        fl->addWidget(gw);
        vl->addWidget(frame);
    }

    vl->addStretch();
}

// ── buildEquipPanel ───────────────────────────────────────────────────────────

void CharacterTab::buildEquipPanel(QWidget* container)
{
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(4);

    // Titre
    auto* title = new QLabel("Équipement");
    title->setStyleSheet(
        QString("font-weight:bold;font-size:12px;color:%1;").arg(kBlue));
    vl->addWidget(title);

    // Grille des 22 slots
    auto* gridWidget = new QWidget;
    gridWidget->setStyleSheet("background:transparent;");
    auto* grid = new QGridLayout(gridWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);

    for (int i = 0; i < kEqSlots.size(); ++i) {
        const QString& slotName = kEqSlots[i];

        // Label du slot
        auto* slotLabel = new QLabel(slotName + " :");
        slotLabel->setStyleSheet(
            QString("color:%1;font-size:10px;min-width:90px;").arg(kGrey));
        grid->addWidget(slotLabel, i, 0);

        // SearchComboBox pour rechercher un item
        auto* combo = new SearchComboBox;
        combo->setMinimumWidth(200);
        combo->lineEdit()->setPlaceholderText("— vide —");
        combo->setStyleSheet(
            "QComboBox{background:#1a2236;border:1px solid #3a4a6a;"
            "border-radius:3px;color:#c0c0c0;padding:1px 4px;font-size:10px;}"
            "QComboBox:hover{border-color:#64b5f6;}"
            "QComboBox QAbstractItemView{background:#1a2236;border:1px solid #3a4a6a;"
            "color:#c0c0c0;selection-background-color:#2a3a5a;}");

        // Capture slotName par valeur pour les lambdas
        connect(combo, &SearchComboBox::popup_requested,
                [this, combo, slotName]() {
                    // Recherche d'items selon le texte saisi
                    QString query = combo->lineEdit()->text().trimmed();
                    if (query.length() < 2) return;

                    auto results = _itemDb->searchItems(query, 30);
                    combo->blockSignals(true);
                    combo->clear();

                    for (const auto& item : results) {
                        combo->addItem(QString::fromStdString(item.name),
                                       QVariant(item.id));
                    }
                    combo->lineEdit()->setText(query);
                    combo->blockSignals(false);
                });

        connect(combo, qOverload<int>(&QComboBox::activated),
                [this, combo, slotName](int index) {
                    if (index < 0) return;
                    QVariant data = combo->itemData(index);
                    if (data.isValid()) {
                        emit equippedItemChanged(slotName, data.toInt());
                    }
                });

        grid->addWidget(combo, i, 1);
    }

    vl->addWidget(gridWidget);
    vl->addStretch();
}

// ── refreshStats ─────────────────────────────────────────────────────────────

void CharacterTab::refreshStats()
{
    if (!_charInfo) {
        _lblName->setText("—");
        _lblClassLevel->setText("—");
    } else {
        _lblName->setText(QString::fromStdString(_charInfo->name));
        _lblClassLevel->setText(
            QString::fromStdString(_charInfo->class_name)
            + "  niv. " + QString::number(_charInfo->level));
    }

    if (!_totals) {
        // Reset all stat labels
        for (auto* lbl : { _lblHp, _lblMana, _lblAtk, _lblAc,
                            _lblStr, _lblSta, _lblDex, _lblAgi,
                            _lblInt, _lblWis, _lblCha,
                            _lblMr, _lblFr, _lblCr, _lblDr, _lblPr }) {
            lbl->setText("0");
        }
        return;
    }

    const PlayerTotals& t = *_totals;

    _lblHp->setText(QString::number(t.hp.capped));
    _lblMana->setText(QString::number(t.mana.capped));
    _lblAtk->setText(QString::number(t.atk));
    _lblAc->setText(QString::number(t.mitigation));

    _lblStr->setText(QString::number(t.str_v));
    _lblSta->setText(QString::number(t.sta));
    _lblDex->setText(QString::number(t.dex));
    _lblAgi->setText(QString::number(t.agi));
    _lblInt->setText(QString::number(t.int_v));
    _lblWis->setText(QString::number(t.wis));
    _lblCha->setText(QString::number(t.cha));

    _lblMr->setText(QString::number(t.mr));
    _lblFr->setText(QString::number(t.fr));
    _lblCr->setText(QString::number(t.cr));
    _lblDr->setText(QString::number(t.dr));
    _lblPr->setText(QString::number(t.pr));
}
