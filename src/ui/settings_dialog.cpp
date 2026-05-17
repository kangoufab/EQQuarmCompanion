#include "ui/settings_dialog.h"
#include "core/config.h"
#include <nlohmann/json.hpp>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

SettingsDialog::SettingsDialog(Config* cfg, QWidget* parent)
    : QDialog(parent), _config(cfg)
{
    setWindowTitle("Parametres");
    setMinimumWidth(420);

    auto* form = new QFormLayout;

    _eqFilesDir = new QLineEdit(QString::fromStdString(cfg->get("eq_files_dir")));
    auto* browseBtn = new QPushButton("...");
    browseBtn->setFixedWidth(40);
    connect(browseBtn, &QPushButton::clicked, [this]{
        auto dir = QFileDialog::getExistingDirectory(this, "Dossier fichiers EQ");
        if (!dir.isEmpty()) _eqFilesDir->setText(dir);
    });
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(_eqFilesDir);
    dirRow->addWidget(browseBtn);
    auto* dirW = new QWidget; dirW->setLayout(dirRow);
    form->addRow("Dossier EQ :", dirW);

    auto dbCfg = cfg->getDbConfig();
    _dbHost     = new QLineEdit(QString::fromStdString(dbCfg.host));
    _dbPort     = new QSpinBox;
    _dbPort->setRange(1, 65535);
    _dbPort->setValue(dbCfg.port);
    _dbUser     = new QLineEdit(QString::fromStdString(dbCfg.user));
    _dbPassword = new QLineEdit(QString::fromStdString(dbCfg.password));
    _dbPassword->setEchoMode(QLineEdit::Password);
    _dbName     = new QLineEdit(QString::fromStdString(dbCfg.database));

    form->addRow("DB Host :",     _dbHost);
    form->addRow("DB Port :",     _dbPort);
    form->addRow("DB User :",     _dbUser);
    form->addRow("DB Password :", _dbPassword);
    form->addRow("DB Name :",     _dbName);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* vl = new QVBoxLayout(this);
    vl->addLayout(form);
    vl->addWidget(btns);
}

void SettingsDialog::onAccepted() {
    _config->set("eq_files_dir",
                 nlohmann::json(_eqFilesDir->text().toStdString()));
    nlohmann::json db;
    db["host"]     = _dbHost->text().toStdString();
    db["port"]     = _dbPort->value();
    db["user"]     = _dbUser->text().toStdString();
    db["password"] = _dbPassword->text().toStdString();
    db["database"] = _dbName->text().toStdString();
    _config->set("db", db);
    _config->save();
    accept();
}
