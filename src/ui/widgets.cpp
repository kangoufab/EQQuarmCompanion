#include "ui/widgets.h"
#include <QLineEdit>

SearchComboBox::SearchComboBox(QWidget* parent) : QComboBox(parent) {
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    // textEdited fires for all user input (including spaces) without QComboBox interception
    connect(lineEdit(), &QLineEdit::textEdited, this, [this](const QString&) {
        emit popup_requested();
    });
}

void SearchComboBox::showPopup() {
    QComboBox::showPopup();
}
