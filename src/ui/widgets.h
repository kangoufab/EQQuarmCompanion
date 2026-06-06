#pragma once
#include <QComboBox>

class SearchComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit SearchComboBox(QWidget* parent = nullptr);

signals:
    void popup_requested();

public:
    void showPopup() override;
};
