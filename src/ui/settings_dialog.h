#pragma once
#include <QDialog>
class Config;
class SettingsDialog : public QDialog {
    Q_OBJECT
public: explicit SettingsDialog(Config*, QWidget* p=nullptr):QDialog(p){}
};
