#include "ui/settings_dialog.h"
#include "core/config.h"
#include <nlohmann/json.hpp>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

static const QStringList EXPANSIONS = {
    "Classic", "Kunark", "Velious", "Luclin", "Planes of Power"
};
static const QStringList EQ_CLASSES = {
    "Warrior","Cleric","Paladin","Ranger","Shadowknight",
    "Druid","Monk","Bard","Rogue","Shaman",
    "Necromancer","Wizard","Magician","Enchanter","Beastlord",
};
static const QStringList SCORED_STATS = {
    "ac","hp","mana","damage","delay",
    "astr","asta","aagi","adex","awis","aint","acha"
};

SettingsDialog::SettingsDialog(Config* cfg, QWidget* parent)
    : QDialog(parent), _config(cfg)
{
    setWindowTitle(QString::fromUtf8("Paramètres"));
    setMinimumWidth(520);

    auto* tabs = new QTabWidget;
    tabs->addTab(buildFilesTab(),   "Fichiers EQ");
    tabs->addTab(buildWeightsTab(), "Poids de stats");

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* vl = new QVBoxLayout(this);
    vl->addWidget(tabs);
    vl->addWidget(btns);
}

// ── Onglet Fichiers EQ ────────────────────────────────────────────────────

QWidget* SettingsDialog::buildFilesTab()
{
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    auto* form = new QFormLayout;

    _eqFilesDir = new QLineEdit(QString::fromStdString(_config->get("eq_files_dir")));
    auto* browseBtn = new QPushButton(QString::fromUtf8("Parcourir..."));
    connect(browseBtn, &QPushButton::clicked, [this] {
        auto dir = QFileDialog::getExistingDirectory(
            this, QString::fromUtf8("Répertoire des fichiers EQ"));
        if (!dir.isEmpty()) _eqFilesDir->setText(dir);
    });
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(_eqFilesDir);
    dirRow->addWidget(browseBtn);
    auto* dirW = new QWidget; dirW->setLayout(dirRow);
    form->addRow(QString::fromUtf8("Répertoire EQ :"), dirW);

    _expansion = new QComboBox;
    _expansion->addItems(EXPANSIONS);
    QString curExp = QString::fromStdString(_config->get("current_expansion"));
    int idx = _expansion->findText(curExp);
    if (idx >= 0) _expansion->setCurrentIndex(idx);
    form->addRow(QString::fromUtf8("Extension courante :"), _expansion);

    layout->addLayout(form);
    layout->addStretch();
    return w;
}

// ── Onglet Poids de stats ─────────────────────────────────────────────────

QWidget* SettingsDialog::buildWeightsTab()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* container = new QWidget;
    auto* layout = new QVBoxLayout(container);

    auto* classTabs = new QTabWidget;
    for (const auto& cls : EQ_CLASSES)
        classTabs->addTab(buildClassWeightsWidget(cls), cls);

    layout->addWidget(classTabs);
    scroll->setWidget(container);
    return scroll;
}

QWidget* SettingsDialog::buildClassWeightsWidget(const QString& className)
{
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);

    auto weights = _config->getClassWeights(className.toStdString());
    _weightSliders[className] = {};

    for (const auto& stat : SCORED_STATS) {
        auto* row = new QHBoxLayout;

        auto* lbl = new QLabel(stat);
        lbl->setFixedWidth(60);

        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(-5, 5);
        float wval = weights.count(stat.toStdString())
                     ? weights.at(stat.toStdString()) : 0.f;
        slider->setValue(static_cast<int>(wval));

        auto* valLbl = new QLabel(QString::number(slider->value()));
        valLbl->setFixedWidth(25);
        connect(slider, &QSlider::valueChanged,
                [valLbl](int v){ valLbl->setText(QString::number(v)); });

        row->addWidget(lbl);
        row->addWidget(slider);
        row->addWidget(valLbl);
        layout->addLayout(row);

        _weightSliders[className][stat] = slider;
    }
    layout->addStretch();
    return w;
}

// ── Slots ─────────────────────────────────────────────────────────────────

void SettingsDialog::onAccepted()
{
    _config->set("eq_files_dir",
                 nlohmann::json(_eqFilesDir->text().toStdString()));
    _config->set("current_expansion",
                 nlohmann::json(_expansion->currentText().toStdString()));

    for (const auto& cls : EQ_CLASSES) {
        std::map<std::string, float> w;
        auto clsIt = _weightSliders.find(cls);
        if (clsIt != _weightSliders.end()) {
            for (const auto& stat : SCORED_STATS) {
                auto slIt = clsIt->find(stat);
                if (slIt != clsIt->end())
                    w[stat.toStdString()] = static_cast<float>(slIt.value()->value());
            }
        }
        _config->setClassWeights(cls.toStdString(), w);
    }

    _config->save();
    accept();
}
