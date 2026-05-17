#pragma once
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>
#include <vector>

inline const char* kGreen  = "#81c784";
inline const char* kOrange = "#ffb74d";
inline const char* kRed    = "#ef5350";

inline std::pair<const char*, const char*> sectionTheme(const char* accent) {
    if (QLatin1String(accent) == "#64b5f6") return {"#1a2236", "#3a4a6a"};
    if (QLatin1String(accent) == "#ffb74d") return {"#2a241a", "#5a4a3a"};
    if (QLatin1String(accent) == "#81c784") return {"#1a2a1e", "#3a5a4a"};
    if (QLatin1String(accent) == "#ef5350") return {"#2a1a1a", "#5a3a3a"};
    if (QLatin1String(accent) == "#ba68c8") return {"#241a2a", "#4a3a5a"};
    return {"#1a1a2e", "#3a3a5a"};
}

inline QLabel* sectionLabel(const QString& text, const char* accent = "#888888") {
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet(QString(
        "font-size:10px;color:%1;font-variant:small-caps;"
        "font-weight:bold;border:none;background:transparent;").arg(accent));
    return lbl;
}

inline std::pair<QFrame*, QVBoxLayout*> sectionFrame(const char* accent = "#64b5f6") {
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
        auto* kl = new QLabel(QString("<span style='color:#888888'>%1</span>")
                              .arg(pairs[i].first));
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
