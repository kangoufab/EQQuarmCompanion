#include "ui/infos_tab.h"
#include "ui/infos_data.h"
#include "ui/ui_helpers.h"
#include "core/config.h"
#include <nlohmann/json.hpp>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

InfosTab::InfosTab(Config* cfg, QWidget* parent)
    : QWidget(parent), _config(cfg)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    hdr->addWidget(new QLabel("Expansion :"));
    _expSelector = new QComboBox;
    for (const auto& [exp, _] : kExpCaps)
        _expSelector->addItem(QString::fromStdString(exp));

    QString savedExp = QString::fromStdString(cfg->get("current_expansion"));
    int idx = _expSelector->findText(savedExp);
    if (idx >= 0) _expSelector->setCurrentIndex(idx);

    hdr->addWidget(_expSelector);
    hdr->addStretch();
    outer->addLayout(hdr);

    _content = new QScrollArea;
    _content->setWidgetResizable(true);
    _content->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(_content, 1);

    connect(_expSelector, &QComboBox::currentTextChanged,
            this, &InfosTab::onExpansionChanged);

    onExpansionChanged(_expSelector->currentText());
}

void InfosTab::onExpansionChanged(const QString& expansion) {
    _config->set("current_expansion", nlohmann::json(expansion.toStdString()));
    _config->save();
    _content->setWidget(buildExpansionPage(expansion));
}

QWidget* InfosTab::buildExpansionPage(const QString& expansion) {
    auto* page = new QWidget;
    auto* vl   = new QVBoxLayout(page);
    vl->setContentsMargins(6, 6, 6, 6);
    vl->setSpacing(8);
    vl->setAlignment(Qt::AlignTop);

    // Header — debuffs disponibles dans cette expansion
    auto debuffs = getResistDebuffs(expansion.toStdString());
    if (!debuffs.empty()) {
        auto [fDebuffs, flDebuffs] = sectionFrame("#ef5350");
        flDebuffs->addWidget(sectionLabel("Debuffs resist max disponibles", "#ef5350"));
        std::vector<std::pair<QString,QString>> pairs;
        for (const auto& [r, v] : debuffs) {
            pairs.push_back({QString::fromStdString(r),
                             QString::number(v)});
        }
        flDebuffs->addWidget(gridWidget(pairs, 5));
        vl->addWidget(fDebuffs);
    }

    // Groupes par type de résistance
    for (const char* resistType : {"MR", "FR", "CR", "PR", "DR"})
        vl->addWidget(buildResistGroup(resistType, expansion));

    vl->addStretch();
    return page;
}

QWidget* InfosTab::buildResistGroup(const QString& resistType,
                                     const QString& expansion)
{
    auto [frame, fl] = sectionFrame("#64b5f6");
    fl->addWidget(sectionLabel(resistType + " Debuffs", "#64b5f6"));

    auto it = kResistGroups.find(resistType.toStdString());
    if (it == kResistGroups.end()) return frame;

    auto debuffs = getResistDebuffs(expansion.toStdString());
    int debuffVal = 0;
    auto dIt = debuffs.find(resistType.toStdString());
    if (dIt != debuffs.end()) debuffVal = dIt->second;

    for (const auto& group : it->second) {
        // Afficher le groupe avec les IDs de sorts disponibles
        QString spellNames;
        for (int id : group.spell_ids)
            spellNames += QString::number(id) + " ";

        auto* lbl = new QLabel(
            QString("  %1 [%2]").arg(QString::fromStdString(group.label)).arg(spellNames.trimmed()));
        lbl->setStyleSheet("color:#888888;background:transparent;font-size:10px;");
        fl->addWidget(lbl);
    }

    if (debuffVal != 0) {
        auto* total = new QLabel(
            QString("  Total: <span style='color:#ef5350'>%1</span>").arg(debuffVal));
        total->setTextFormat(Qt::RichText);
        total->setStyleSheet("background:transparent;font-size:10px;");
        fl->addWidget(total);
    }

    return frame;
}
