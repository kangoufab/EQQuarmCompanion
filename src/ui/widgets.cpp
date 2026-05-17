#include "ui/widgets.h"

SearchComboBox::SearchComboBox(QWidget* parent) : QComboBox(parent) {
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
}

void SearchComboBox::keyPressEvent(QKeyEvent* event) {
    QComboBox::keyPressEvent(event);
    if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Escape)
        emit popup_requested();
}

void SearchComboBox::showPopup() {
    emit popup_requested();
    QComboBox::showPopup();
}
