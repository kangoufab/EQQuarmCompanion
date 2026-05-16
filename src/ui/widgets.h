#pragma once
#include <QComboBox>
class SearchComboBox : public QComboBox {
    Q_OBJECT
public: explicit SearchComboBox(QWidget* p=nullptr):QComboBox(p){}
signals: void popup_requested();
};
