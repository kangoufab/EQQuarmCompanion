#pragma once
#include <QWidget>
class Config;
class InfosTab : public QWidget {
    Q_OBJECT
public:
    InfosTab(Config*, QWidget* p=nullptr) : QWidget(p) {}
};
