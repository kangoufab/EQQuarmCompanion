#pragma once
#include <QMainWindow>

class Config;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Config* config, QWidget* parent = nullptr);
};
