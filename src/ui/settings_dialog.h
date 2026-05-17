#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
class Config;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Config* config, QWidget* parent = nullptr);

private slots:
    void onAccepted();

private:
    Config*    _config;
    QLineEdit* _eqFilesDir;
    QLineEdit* _dbHost;
    QSpinBox*  _dbPort;
    QLineEdit* _dbUser;
    QLineEdit* _dbPassword;
    QLineEdit* _dbName;
};
