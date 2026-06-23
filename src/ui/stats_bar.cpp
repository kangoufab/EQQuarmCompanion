#include "ui/stats_bar.h"
#include "ui/stat_categories.h"
#include "ui/spell_tooltip.h"
#include "core/spell_stats.h"
#include "core/stats_calculator.h"
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QScrollBar>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// formatSpellTooltip is defined in spell_tooltip.cpp

// ── makeStatTooltip ───────────────────────────────────────────────────────

static QString makeStatTooltip(const std::string& statKey,
                                const std::string& statLabel,
                                int cappedVal, int rawVal,
                                std::optional<int> cap,
                                const std::string& suffix,
                                const StatInfo& info,
                                const char* accent)
{
    // En-tête : nom + valeur / cap
    QString sfxQ = QString::fromStdString(suffix);
    QString valStr = QString("<span style='color:%1;font-weight:bold;font-size:14px;'>%2%3</span>")
        .arg(accent).arg(cappedVal).arg(sfxQ);
    if (cap.has_value())
        valStr += QString("<span style='color:%1;'> / %2%3</span>").arg(kTextMuted).arg(*cap).arg(sfxQ);
    if (rawVal != cappedVal)
        valStr += QString(" <span style='color:%1;font-size:14px;'>(brut %2)</span>").arg(kTextSecondary).arg(rawVal);

    QString rows;
    auto row = [&](const QString& label, const QString& val, const char* color) {
        rows += QString("<tr><td style='padding-left:12px;color:%1;'>%2</td>"
                        "<td align='right' style='color:%3;'>%4</td></tr>")
                .arg(kHtmlLabel).arg(label).arg(color).arg(val);
    };
    auto sectionHeader = [&](const QString& title, const char* color) {
        rows += QString("<tr><td colspan='2' style='color:%1;font-size:13px;"
                        "font-weight:bold;padding-top:4px;'>─ %2</td></tr>")
                .arg(color).arg(title);
    };

    if (info.base_val != 0) {
        sectionHeader("BASE", kAccentBlue);
        row("Race / Classe / Niveau",
            QString("%1%2").arg(info.base_val).arg(sfxQ), kAccentBlue);
    }
    if (!info.item_sources.empty()) {
        sectionHeader("ITEMS", kGreen);
        for (auto& [iname, ival] : info.item_sources) {
            QString sign = ival >= 0 ? "+" : "";
            row(QString::fromStdString(iname),
                sign + QString::number(ival) + sfxQ, kGreen);
        }
    }
    if (!info.aa_sources.empty()) {
        sectionHeader("AA", kAccentGold);
        for (auto& [aaname, aval] : info.aa_sources) {
            QString sign = aval >= 0 ? "+" : "";
            row(QString::fromStdString(aaname),
                sign + QString::number(aval) + sfxQ, kAccentGold);
        }
    }
    if (!info.spell_sources.empty()) {
        sectionHeader("SORTS", kAccentPurple);
        for (auto& [sname, sval] : info.spell_sources) {
            QString sign = sval >= 0 ? "+" : "";
            row(QString::fromStdString(sname),
                sign + QString::number(sval) + sfxQ, kAccentPurple);
        }
    }
    if (!info.formula.empty()) {
        sectionHeader("FORMULE", kOrange);

        int spellTotal = 0;
        for (auto& [n, v] : info.spell_sources) spellTotal += v;

        for (int fi = 0; fi < (int)info.formula.size(); ++fi) {
            auto& [lbl, val] = info.formula[fi];
            bool isLast = (fi == (int)info.formula.size() - 1);

            if (isLast && spellTotal != 0) {
                // Ligne sorts avant le total
                QString sign = spellTotal >= 0 ? "+" : "";
                row("Sorts", sign + QString::number(spellTotal) + sfxQ, kOrange);
                // Ligne total mise à jour avec la valeur réelle affichée
                row(QString::fromStdString(lbl),
                    QString::number(cappedVal) + sfxQ, kOrange);
            } else {
                row(QString::fromStdString(lbl),
                    QString::fromStdString(val), kOrange);
            }
        }
    }

    QString table = QString(
        "<table cellspacing='0' cellpadding='1'>"
        "<tr><td style='color:%1;font-weight:bold;font-size:14px;'>%2</td>"
        "<td align='right' style='font-size:14px;'>%3</td></tr>"
        "%4</table>")
        .arg(accent)
        .arg(QString::fromStdString(statLabel))
        .arg(valStr)
        .arg(rows);
    return table;
}

// ── Constantes ────────────────────────────────────────────────────────────


static bool isAttrS(const std::string& s) {
    return s=="astr"||s=="asta"||s=="adex"||s=="aagi"||s=="awis"||s=="aint"||s=="acha";
}
static bool isResistS(const std::string& s) {
    return s=="mr"||s=="fr"||s=="cr"||s=="dr"||s=="pr";
}

// ── Flow layout pour les effets (retour à la ligne sans couper un effet) ──

class EffectsFlowLayout : public QLayout {
public:
    EffectsFlowLayout(QWidget* parent, int hSpacing = 4, int vSpacing = 2)
        : QLayout(parent), _hSpace(hSpacing), _vSpace(vSpacing) {}
    ~EffectsFlowLayout() override { qDeleteAll(_items); }

    void addItem(QLayoutItem* item) override { _items.append(item); }
    int  count() const override { return _items.size(); }
    QLayoutItem* itemAt(int i) const override { return _items.value(i); }
    QLayoutItem* takeAt(int i) override {
        return (i >= 0 && i < _items.size()) ? _items.takeAt(i) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int  heightForWidth(int w) const override { return doLayout(QRect(0,0,w,0), true); }
    QSize minimumSize() const override {
        QSize s;
        for (auto* it : _items) s = s.expandedTo(it->minimumSize());
        auto m = contentsMargins();
        return s + QSize(m.left() + m.right(), m.top() + m.bottom());
    }
    QSize sizeHint() const override { return minimumSize(); }
    void setGeometry(const QRect& r) override { QLayout::setGeometry(r); doLayout(r, false); }

private:
    int doLayout(const QRect& rect, bool testOnly) const {
        auto m = contentsMargins();
        QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = eff.x(), y = eff.y(), lineH = 0;
        for (auto* it : _items) {
            QSize sh = it->sizeHint();
            int nextX = x + sh.width();
            if (nextX > eff.right() + 1 && lineH > 0) {
                x = eff.x(); y += lineH + _vSpace; lineH = 0;
            }
            if (!testOnly) it->setGeometry(QRect(QPoint(x, y), sh));
            x += sh.width() + _hSpace;
            lineH = qMax(lineH, sh.height());
        }
        return y + lineH - rect.y() + m.bottom();
    }
    QList<QLayoutItem*> _items;
    int _hSpace, _vSpace;
};

// ── Helpers effets worn/focus ─────────────────────────────────────────────

static QWidget* makeEffectsWidget(const std::vector<EffectEntry>& effects,
                                   const char* color,
                                   const char* prefix,
                                   const std::map<int, SpellData>& spellDetails,
                                   int tooltipLevel,
                                   const SpellNameResolver& nameResolver = {})
{
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* layout = new EffectsFlowLayout(w, 4, 2);
    layout->setContentsMargins(0, 2, 0, 0);

    if (prefix && prefix[0]) {
        auto* pl = new QLabel(QString::fromUtf8(prefix));
        pl->setStyleSheet(
            QString("color: %1; font-size: 13px; border: none; background: transparent;")
            .arg(color));
        layout->addWidget(pl);
    }

    for (int i = 0; i < (int)effects.size(); ++i) {
        auto& [name, spellId, itemName] = effects[i];
        // Séparateur · fusionné dans le label pour ne pas être coupé en bout de ligne
        QString text = QString::fromStdString(name);
        if (i < (int)effects.size() - 1) text += " \xc2\xb7";
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(
            QString("color: %1; font-size: 13px; font-style: italic; "
                    "border: none; background: transparent;").arg(color));
        if (spellId == -1) {
            // Flowing Thought agrégé : itemName contient les sources séparées par '\n'
            QString tip = QString("<b>Flowing Thought</b><br>"
                                  "<span style='color:%1;'>Mana Regen +%2/tick</span>")
                          .arg(kHtmlLabel)
                          .arg(name.size() > 17 ? QString::fromStdString(name).mid(17) : "?");
            if (!itemName.empty()) {
                tip += "<br><br>";
                for (auto& src : QString::fromStdString(itemName).split('\n', Qt::SkipEmptyParts))
                    tip += QString("<span style='color:%1;'>%2</span><br>").arg(kTextSecondary, src);
            }
            lbl->setToolTip(tip);
        } else if (spellId > 0) {
            auto it = spellDetails.find(spellId);
            if (it != spellDetails.end()) {
                QString tip = formatSpellTooltip(it->second, tooltipLevel, {}, QString(color), nameResolver);
                if (!itemName.empty())
                    tip = QString("<i style='color:%1;'>%2</i><br>")
                          .arg(kTextSecondary, QString::fromStdString(itemName)) + tip;
                lbl->setToolTip(tip);
            }
        }
        layout->addWidget(lbl);
    }
    return w;
}

// ── makePlayerStatsBar ────────────────────────────────────────────────────

QFrame* makePlayerStatsBar(
    const PlayerTotals& totals,
    const std::string& className,
    const std::string& expansion,
    const PlayerTotalsExtra& extra,
    const std::vector<EffectEntry>& wornEffects,
    const std::vector<EffectEntry>& focusEffects,
    const std::map<int, SpellData>& spellDetails,
    const SpellNameResolver& nameResolver)
{
    // Catégories selon la classe
    std::vector<std::pair<std::string, std::vector<std::string>>> cats;
    auto classIt = CLASS_CATEGORIES.find(className);
    if (classIt != CLASS_CATEGORIES.end()) {
        for (auto& [name, stats] : STAT_CATEGORIES)
            if (classIt->second.count(name))
                cats.push_back({name, stats});
    }
    if (cats.empty()) cats = STAT_CATEGORIES;

    // Defense en premier
    auto defIt = std::find_if(cats.begin(), cats.end(),
                              [](const auto& p){ return p.first == "Defense"; });
    if (defIt != cats.end() && defIt != cats.begin())
        std::rotate(cats.begin(), defIt, defIt + 1);

    // Catégorie cible pour les effects : préférer Sorts, sinon Defense
    std::string effectsTarget;
    for (const char* cn : {"Sorts", "Defense"}) {
        auto it = std::find_if(cats.begin(), cats.end(),
                               [cn](const auto& p){ return p.first == cn; });
        if (it != cats.end()) { effectsTarget = cn; break; }
    }

    // Caps d'expansion
    bool oldExp = (expansion == "Classic" || expansion == "Kunark" || expansion == "Velious");
    int attrCap   = oldExp ? 200 : 255;
    int resistCap = oldExp ? 200 : 500;

    auto* frame = new QFrame;
    frame->setStyleSheet(QString("QFrame { background: %1; border: none; }").arg(kBgMain));
    auto* outer = new QVBoxLayout(frame);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);

    auto* scrollContent = new QWidget;
    scrollContent->setStyleSheet("background: transparent;");
    auto* hbox = new QHBoxLayout(scrollContent);
    hbox->setSpacing(6);
    hbox->setContentsMargins(0, 0, 0, 0);

    for (int idx = 0; idx < (int)cats.size(); ++idx) {
        auto& [catName, catStats] = cats[idx];

        auto colIt = CAT_COLORS.find(catName);
        if (colIt == CAT_COLORS.end()) continue;
        auto& [bg, border, accent] = colIt->second;

        auto* panel = new QFrame;
        panel->setStyleSheet(
            QString("QFrame { background: %1; border-radius: 4px; border: 1px solid %2; }")
            .arg(bg).arg(border));
        auto* panelL = new QVBoxLayout(panel);
        panelL->setContentsMargins(6, 4, 6, 6);
        panelL->setSpacing(3);

        auto lblIt = CAT_LABELS.find(catName);
        const char* catStr = (lblIt != CAT_LABELS.end()) ? lblIt->second : catName.c_str();
        auto* catLbl = new QLabel(QString::fromUtf8(catStr));
        catLbl->setStyleSheet(
            QString("font-size: 14px; color: %1; font-variant: small-caps; "
                    "font-weight: bold; border: none; background: transparent;").arg(accent));
        panelL->addWidget(catLbl);

        auto* tilesW = new QWidget;
        tilesW->setStyleSheet("background: transparent;");
        auto* tilesL = new QHBoxLayout(tilesW);
        tilesL->setSpacing(3);
        tilesL->setContentsMargins(0, 0, 0, 0);

        for (auto& stat : catStats) {
            bool hasCap = isAttrS(stat) || isResistS(stat);
            int capVal  = isAttrS(stat) ? attrCap : (isResistS(stat) ? resistCap : 0);
            int dispVal = playerTotalStat(stat, totals);
            int rawFromExtra = extra.get(stat).raw ? extra.get(stat).raw : dispVal;
            // Pour les stats cappées, utiliser raw de l'extra si disponible
            if (hasCap) dispVal = std::min(dispVal, capVal);
            bool atCap  = hasCap && dispVal >= capVal;

            const char *tileBg, *tileFg;
            if (!hasCap)    { tileBg = kBgTileNoLimit; tileFg = kGreen; }
            else if (atCap) { tileBg = kBgTileAtCap;  tileFg = kAccentAtCap; }
            else             { tileBg = kBgTile;       tileFg = kTextBase; }

            auto* tile = new QFrame;
            tile->setStyleSheet(
                QString("QFrame { background: %1; border-radius: 3px; "
                        "border: 1px solid rgba(255,255,255,0.06); }").arg(tileBg));
            auto* tileL = new QVBoxLayout(tile);
            tileL->setContentsMargins(4, 3, 4, 3);
            tileL->setSpacing(1);

            auto slbIt = STAT_LABELS.find(stat);
            auto sufIt = STAT_SUFFIX.find(stat);
            std::string slabelStr = slbIt != STAT_LABELS.end() ? slbIt->second : stat;
            std::string suffixStr = sufIt != STAT_SUFFIX.end() ? sufIt->second : "";
            QString slabel = QString::fromStdString(slabelStr);
            QString suffix = QString::fromStdString(suffixStr);

            auto* nameLbl = new QLabel(slabel);
            nameLbl->setStyleSheet(
                QString("font-size: 14px; color: %1; border: none; background: transparent;")
                .arg(kTextTileKey));
            nameLbl->setAlignment(Qt::AlignCenter);

            auto* valLbl = new QLabel(QString::number(dispVal) + suffix);
            valLbl->setStyleSheet(
                QString("font-size: 13px; font-weight: bold; color: %1; "
                        "border: none; background: transparent;").arg(tileFg));
            valLbl->setAlignment(Qt::AlignCenter);

            tileL->addWidget(nameLbl);
            tileL->addWidget(valLbl);

            // Tooltip sur la tuile
            const StatInfo& si = extra.get(stat);
            std::optional<int> capOpt = hasCap ? std::optional<int>(capVal) : std::nullopt;
            int rawForTip = si.raw ? si.raw : dispVal;
            tile->setToolTip(makeStatTooltip(stat, slabelStr, dispVal,
                                             rawForTip, capOpt, suffixStr,
                                             si, tileFg));

            tilesL->addWidget(tile);
        }

        panelL->addWidget(tilesW);

        // Worn/focus effects dans la catégorie cible
        if (!effectsTarget.empty() && catName == effectsTarget) {
            if (!wornEffects.empty())
                panelL->addWidget(makeEffectsWidget(wornEffects,  kAccentWorn, "",        spellDetails, 0, nameResolver));
            if (!focusEffects.empty())
                panelL->addWidget(makeEffectsWidget(focusEffects, kAccentFocus, "Focus: ", spellDetails, 0, nameResolver));
        }

        panelL->addStretch();
        hbox->addWidget(panel, 0, Qt::AlignTop);
    }
    hbox->addStretch();

    auto* scrollArea = new QScrollArea;
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        QString(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:horizontal {"
            "  height: 6px; background: %1;"
            "  border-radius: 3px; margin: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "  background: %2; border-radius: 3px; min-width: 20px;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "  width: 0px;"
            "}").arg(kBgScrollTrack, kBorderCard));
    scrollArea->setWidget(scrollContent);
    outer->addWidget(scrollArea);

    // Fallback : si aucune catégorie cible n'est présente, on affiche sous le grid
    if (effectsTarget.empty()) {
        if (!wornEffects.empty())
            outer->addWidget(makeEffectsWidget(wornEffects,  kAccentWorn, "",        spellDetails, 0, nameResolver));
        if (!focusEffects.empty())
            outer->addWidget(makeEffectsWidget(focusEffects, kAccentFocus, "Focus: ", spellDetails, 0, nameResolver));
    }

    return frame;
}
