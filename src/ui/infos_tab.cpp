#include "ui/infos_tab.h"
#include "ui/infos_data.h"
#include "ui/infos_spell_data.h"
#include "ui/ui_helpers.h"
#include "core/config.h"
#include <nlohmann/json.hpp>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <set>

static const std::map<std::string, const char*> kResistColors = {
    {"MR","#ba68c8"},{"FR","#ef5350"},{"CR","#64b5f6"},{"PR","#81c784"},{"DR","#ffb74d"},
};
static const std::map<std::string, const char*> kResistNames = {
    {"MR","Magic Resist"},{"FR","Fire Resist"},{"CR","Cold Resist"},
    {"PR","Poison Resist"},{"DR","Disease Resist"},
};
static const std::map<std::string, std::pair<const char*,const char*>> kResistThemes = {
    {"MR",{"#241a2a","#4a3a5a"}},{"FR",{"#2a1a1a","#5a3a3a"}},
    {"CR",{"#1a2236","#3a4a6a"}},{"PR",{"#1a2a1e","#3a5a4a"}},{"DR",{"#2a241a","#5a4a3a"}},
};

// ── Constructor ───────────────────────────────────────────────────────────────

InfosTab::InfosTab(Config* cfg, QWidget* parent) : QWidget(parent), _config(cfg) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // Header row
    auto* hdr = new QHBoxLayout;
    auto* title = new QLabel("Debuffs de Résistances — combinaisons optimales par extension");
    title->setStyleSheet("font-size:15px;font-weight:bold;color:#e0e0e0;");
    hdr->addWidget(title);
    hdr->addStretch();
    _expCombo = new QComboBox;
    for (const auto& [exp, _] : kExpIndex)
        _expCombo->addItem(QString::fromStdString(exp));
    QString saved = QString::fromStdString(cfg->get("current_expansion"));
    if (saved.isEmpty()) saved = "Luclin";
    int idx = _expCombo->findText(saved);
    if (idx < 0) idx = _expCombo->findText("Luclin");
    if (idx >= 0) _expCombo->setCurrentIndex(idx);
    _expCombo->setFixedWidth(160);
    hdr->addWidget(_expCombo);
    outer->addLayout(hdr);

    // Legend
    auto* legend = new QLabel(
        "<span style='color:#9cbe9c'>■ Bard songs</span>"
        " <span style='color:#555555'>— stackent toujours avec les debuffs non-bard"
        " (buffstacking.cpp: caster bard → goto STACK_OK)</span>");
    legend->setTextFormat(Qt::RichText);
    legend->setStyleSheet("font-size:11px;background:transparent;border:none;padding:0;");
    outer->addWidget(legend);

    // Scroll area with 2-column content
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll, 1);

    _contentWidget = new QWidget;
    _contentLayout = new QHBoxLayout(_contentWidget);
    _contentLayout->setContentsMargins(4, 4, 4, 4);
    _contentLayout->setSpacing(10);
    _contentLayout->setAlignment(Qt::AlignTop);
    scroll->setWidget(_contentWidget);

    connect(_expCombo, &QComboBox::currentIndexChanged, this, &InfosTab::onExpansionChanged);
    refreshContent();
}

void InfosTab::onExpansionChanged() {
    std::string exp = _expCombo->currentText().toStdString();
    _config->set("current_expansion", nlohmann::json(exp));
    _config->save();
    refreshContent();
}

void InfosTab::refreshContent() {
    while (_contentLayout->count()) {
        auto* item = _contentLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    std::string exp = _expCombo->currentText().toStdString();
    auto capIt = kExpIndex.find(exp);
    int expIdx = (capIt != kExpIndex.end()) ? capIt->second : 3;
    auto capIt2 = kExpCaps.find(exp);
    int cap = (capIt2 != kExpCaps.end()) ? capIt2->second : 60;

    auto* leftW  = new QWidget; auto* leftV  = new QVBoxLayout(leftW);
    auto* rightW = new QWidget; auto* rightV = new QVBoxLayout(rightW);
    leftV->setSpacing(10);  leftV->setAlignment(Qt::AlignTop);
    rightV->setSpacing(10); rightV->setAlignment(Qt::AlignTop);

    // Order: MR+CR on left, FR+PR+DR on right
    const char* resistOrder[] = {"MR","CR","FR","PR","DR"};
    for (int i = 0; i < 5; ++i) {
        auto* section = buildResistSection(resistOrder[i], cap, expIdx);
        (i < 2 ? leftV : rightV)->addWidget(section);
    }

    _contentLayout->addWidget(leftW, 1);
    _contentLayout->addWidget(rightW, 1);
}

// ── Resist section ────────────────────────────────────────────────────────────

QWidget* InfosTab::buildResistSection(const std::string& resist, int cap, int expIdx) {
    auto colorIt = kResistColors.find(resist);
    auto nameIt  = kResistNames.find(resist);
    auto themeIt = kResistThemes.find(resist);
    const char* color  = colorIt  != kResistColors.end() ? colorIt->second  : "#888888";
    const char* rname  = nameIt   != kResistNames.end()  ? nameIt->second   : resist.c_str();
    auto [bg, border]  = themeIt  != kResistThemes.end() ? themeIt->second
                                                          : std::pair<const char*,const char*>{"#1a1a2e","#3a3a5a"};

    auto* frame = new QFrame;
    frame->setStyleSheet(QString("QFrame{background:%1;border-radius:4px;border:1px solid %2;}")
                         .arg(bg).arg(border));
    auto* vl = new QVBoxLayout(frame);
    vl->setContentsMargins(8, 6, 8, 8);
    vl->setSpacing(4);

    auto* titleLbl = new QLabel(QString("%1 (%2)").arg(rname).arg(resist.c_str()));
    titleLbl->setStyleSheet(QString(
        "font-size:13px;color:%1;font-variant:small-caps;"
        "font-weight:bold;border:none;background:transparent;").arg(color));
    vl->addWidget(titleLbl);

    // Grid
    auto* gridW = new QWidget; gridW->setStyleSheet("background:transparent;");
    auto* grid  = new QGridLayout(gridW);
    grid->setContentsMargins(0, 2, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(3);

    // Column headers (sans colonne Groupe)
    const char* headers[] = {"Meilleur sort","Classes","Val.","Cible"};
    int colWidths[]       = {280,             155,      48,    65};
    for (int ci = 0; ci < 4; ++ci) {
        auto* lbl = new QLabel(QString("<span style='color:#444444'>%1</span>").arg(headers[ci]));
        lbl->setTextFormat(Qt::RichText);
        lbl->setStyleSheet("background:transparent;border:none;font-size:11px;");
        lbl->setMinimumWidth(colWidths[ci]);
        grid->addWidget(lbl, 0, ci);
    }

    // Collect group order
    std::vector<std::string> groupOrder;
    auto& rgOrder = getResistGroupOrder();
    auto& rgBard  = getResistBardGroups();
    {
        auto nbit = rgOrder.find(resist);
        if (nbit != rgOrder.end()) groupOrder.insert(groupOrder.end(), nbit->second.begin(), nbit->second.end());
        auto bit  = rgBard.find(resist);
        if (bit  != rgBard.end())  groupOrder.insert(groupOrder.end(), bit->second.begin(),  bit->second.end());
    }

    // Build group lookup
    std::map<std::string, const InfoGroup*> groupById;
    for (auto& g : getInfoGroups()) groupById[g.id] = &g;

    // Pre-pass: which groups have a best spell?
    std::set<std::string> activeGroups;
    for (const auto& gid : groupOrder) {
        auto git = groupById.find(gid);
        if (git == groupById.end()) continue;
        const InfoGroup& grp = *git->second;
        if (bestInGroup(grp.spell_ids, resist.c_str(), cap, grp.is_bard, expIdx))
            activeGroups.insert(gid);
    }
    // Groups blocked by a stronger/broader active group
    auto& conflicts = getCrossConflicts();
    std::set<std::string> blockedGroups;
    for (auto& [bid, cpair] : conflicts)
        if (activeGroups.count(bid) && activeGroups.count(cpair.first))
            blockedGroups.insert(bid);

    int rowI = 0;
    int totalVal = 0;
    std::set<std::string> seenGroups;

    for (const auto& gid : groupOrder) {
        if (seenGroups.count(gid)) continue;
        if (blockedGroups.count(gid)) continue;
        auto git = groupById.find(gid);
        if (git == groupById.end()) continue;
        seenGroups.insert(gid);
        const InfoGroup& grp = *git->second;

        const InfoSpell* best = bestInGroup(grp.spell_ids, resist.c_str(), cap, grp.is_bard, expIdx);
        if (!best) continue;

        int val = spellResistVal(*best, resist.c_str(), cap);

        int r = rowI + 1;
        QString rowBg = (rowI % 2 == 0) ? "#18181e" : "transparent";

        // Spell name (col 0)
        const char* nameColor = grp.is_bard ? "#b8d8b8" : "#e0e0e0";
        auto* nameLbl = new QLabel(QString("<span style='color:%1'>%2</span>")
                                   .arg(nameColor).arg(best->name));
        nameLbl->setTextFormat(Qt::RichText);
        nameLbl->setStyleSheet(QString("background:%1;border:none;font-size:12px;padding:2px 3px;").arg(rowBg));
        grid->addWidget(nameLbl, r, 0);

        // Classes (col 1)
        auto* clsLbl = new QLabel(QString("<span style='color:#777777'>%1</span>")
                                  .arg(QString::fromStdString(classesStr(*best))));
        clsLbl->setTextFormat(Qt::RichText);
        clsLbl->setStyleSheet(QString("background:%1;border:none;font-size:11px;padding:2px 3px;").arg(rowBg));
        grid->addWidget(clsLbl, r, 1);

        // Value (col 2)
        const char* vc = val <= -40 ? "#81c784" : val <= -20 ? "#ffb74d" : "#aaaaaa";
        auto* valLbl = new QLabel(QString("<b><span style='color:%1'>%2</span></b>").arg(vc).arg(val));
        valLbl->setTextFormat(Qt::RichText);
        valLbl->setStyleSheet(QString("background:%1;border:none;font-size:12px;padding:2px 3px;").arg(rowBg));
        grid->addWidget(valLbl, r, 2);

        // Target type (col 3)
        auto* ttLbl = new QLabel(QString("<span style='color:#666666'>%1</span>")
                                 .arg(targetLabel(best->targettype)));
        ttLbl->setTextFormat(Qt::RichText);
        ttLbl->setStyleSheet(QString("background:%1;border:none;font-size:11px;padding:2px 3px;").arg(rowBg));
        grid->addWidget(ttLbl, r, 3);

        totalVal += val;

        ++rowI;
    }

    vl->addWidget(gridW);

    if (totalVal < 0) {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color:#2a2a3a;");
        vl->addWidget(sep);

        auto* totRow = new QHBoxLayout;
        totRow->setContentsMargins(0, 2, 0, 0);
        auto* totLblKey = new QLabel(QString("Debuff total (%1)").arg(resist.c_str()));
        totLblKey->setStyleSheet(QString("color:%1;font-size:11px;background:transparent;border:none;")
                                 .arg(color));
        auto* totLblVal = new QLabel(QString("<b>%1</b>").arg(totalVal));
        totLblVal->setTextFormat(Qt::RichText);
        totLblVal->setStyleSheet("color:#81c784;font-size:18px;background:transparent;border:none;");
        totRow->addWidget(totLblKey);
        totRow->addStretch();
        totRow->addWidget(totLblVal);
        auto* totW = new QWidget; totW->setStyleSheet("background:transparent;");
        totW->setLayout(totRow);
        vl->addWidget(totW);
    }

    return frame;
}
