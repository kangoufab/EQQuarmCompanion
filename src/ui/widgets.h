#pragma once
#include <QComboBox>
#include <QKeyEvent>

class SearchComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit SearchComboBox(QWidget* parent = nullptr);

signals:
    void popup_requested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showPopup() override;
};
