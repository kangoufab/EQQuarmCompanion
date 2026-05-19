#pragma once
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSlider>
#include <QSpinBox>
class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccepted();
    void onTestConnection();

private:
    QWidget* buildDbTab();
    QWidget* buildFilesTab();
    QWidget* buildWeightsTab();
    QWidget* buildClassWeightsWidget(const QString& className);

    Config*    _config;

    // Onglet DB
    QLineEdit* _dbHost;
    QSpinBox*  _dbPort;
    QLineEdit* _dbUser;
    QLineEdit* _dbPassword;
    QLineEdit* _dbName;

    // Onglet Fichiers
    QLineEdit* _eqFilesDir;
    QComboBox* _expansion;

    // Onglet Poids de stats : className → statName → slider
    QMap<QString, QMap<QString, QSlider*>> _weightSliders;
};
