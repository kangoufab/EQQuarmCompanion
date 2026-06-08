#pragma once
#include <QComboBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <string>

class Config;

class InfosTab : public QWidget {
    Q_OBJECT
public:
    explicit InfosTab(Config* config, QWidget* parent = nullptr);

signals:
    void expansionChanged();

private slots:
    void onExpansionChanged();

private:
    void refreshContent();
    QWidget* buildResistSection(const std::string& resist, int cap, int expIdx);

    Config*      _config;
    QComboBox*   _expCombo;
    QHBoxLayout* _contentLayout{};
    QWidget*     _contentWidget{};
};
