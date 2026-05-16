#include "ui/main_window.h"
#include "core/config.h"
#include <QLabel>

MainWindow::MainWindow(Config* config, QWidget* parent)
    : QMainWindow(parent)
{
    Q_UNUSED(config)
    setWindowTitle("EQ Quarm Companion");
    resize(1280, 800);
    auto* lbl = new QLabel("EQ Quarm Companion \xe2\x80\x94 chargement en cours\xe2\x80\xa6", this);
    lbl->setAlignment(Qt::AlignCenter);
    setCentralWidget(lbl);
}
