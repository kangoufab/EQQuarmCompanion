#pragma once
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSlider>
class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccepted();

private:
    QWidget* buildFilesTab();
    QWidget* buildWeightsTab();
    QWidget* buildClassWeightsWidget(const QString& className);

    Config*    _config;

    // Onglet Fichiers
    QLineEdit* _eqFilesDir;
    QComboBox* _expansion;

    // Onglet Poids de stats : className → statName → slider
    QMap<QString, QMap<QString, QSlider*>> _weightSliders;
};
