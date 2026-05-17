#pragma once
#include <QComboBox>
#include <QScrollArea>
#include <QWidget>

class Config;

class InfosTab : public QWidget {
    Q_OBJECT
public:
    explicit InfosTab(Config* config, QWidget* parent = nullptr);

private slots:
    void onExpansionChanged(const QString& expansion);

private:
    QWidget* buildExpansionPage(const QString& expansion);
    QWidget* buildResistGroup(const QString& resistType, const QString& expansion);

    Config*      _config;
    QComboBox*   _expSelector;
    QScrollArea* _content;
};
