#pragma once
#include "ui/palette.h"
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>
#include <vector>

// kGreen / kOrange / kRed / all other color tokens come from palette.h

// Shared stylesheet for QComboBox widgets (dark card style).
inline const QString kComboStyle = QString(
    "QComboBox { background: %1; border: 1px solid %2; "
    "border-radius: 3px; color: %3; padding: 3px 6px; font-size: 13px; }"
    "QComboBox:hover { border-color: %4; }"
    "QComboBox QAbstractItemView { background: %1; border: 1px solid %2; "
    "color: %3; selection-background-color: %5; }")
    .arg(kBgCard, kBorderCard, kTextBase, kAccentBlue, kBorderMid);

inline std::pair<const char*, const char*> sectionTheme(const char* accent) {
    if (QLatin1String(accent) == kAccentBlue)   return {kBgCard,        kBorderCard};
    if (QLatin1String(accent) == kOrange)        return {"#2a241a",       "#5a4a3a"};
    if (QLatin1String(accent) == kGreen)         return {"#1a2a1e",       "#3a5a4a"};
    if (QLatin1String(accent) == kRed)           return {"#2a1a1a",       "#5a3a3a"};
    if (QLatin1String(accent) == kAccentPurple)  return {"#241a2a",       "#4a3a5a"};
    return {kSurfaceDark, "#3a3a5a"};
}

inline QLabel* sectionLabel(const QString& text, const char* accent = kTextSecondary) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
        "font-size:10px;color:%1;font-variant:small-caps;"
        "font-weight:bold;border:none;background:transparent;").arg(accent));
    return lbl;
}

// Primary section headers — one step larger for data-critical sections.
inline QLabel* sectionLabelPrimary(const QString& text, const char* accent = kTextSecondary) {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
        "font-size:11px;color:%1;font-variant:small-caps;"
        "font-weight:bold;border:none;background:transparent;").arg(accent));
    return lbl;
}

inline std::pair<QFrame*, QVBoxLayout*> sectionFrame(const char* accent = kAccentBlue) {
    auto [bg, border] = sectionTheme(accent);
    auto* frame = new QFrame;
    frame->setStyleSheet(
        QString("QFrame{background:%1;border-radius:4px;border:1px solid %2;}")
            .arg(bg).arg(border));
    auto* inner = new QVBoxLayout(frame);
    inner->setContentsMargins(8, 6, 8, 6);
    inner->setSpacing(4);
    return {frame, inner};
}

inline QWidget* gridWidget(
    const std::vector<std::pair<QString,QString>>& pairs, int cols = 2)
{
    auto* w = new QWidget;
    w->setStyleSheet("background:transparent;");
    auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(4);
    for (int i = 0; i < static_cast<int>(pairs.size()); ++i) {
        int row = i / cols, col = i % cols;
        auto* kl = new QLabel(QString("<span style='color:%1'>%2</span>")
                              .arg(kTextSecondary).arg(pairs[i].first));
        kl->setTextFormat(Qt::RichText);
        kl->setStyleSheet("background:transparent;");
        auto* vl2 = new QLabel(QString("<b>%1</b>").arg(pairs[i].second));
        vl2->setTextFormat(Qt::RichText);
        vl2->setStyleSheet("background:transparent;");
        g->addWidget(kl,  row, col*2);
        g->addWidget(vl2, row, col*2+1);
    }
    return w;
}
