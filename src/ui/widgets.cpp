#include "ui/widgets.h"
#include <QLineEdit>
#include <QTimer>

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
    // Qt's popup steals focus asynchronously — restore it to the line edit
    // on the next event loop iteration so the user can keep typing.
    if (lineEdit()) {
        int pos = lineEdit()->cursorPosition();
        QTimer::singleShot(0, this, [this, pos]() {
            if (lineEdit()) {
                lineEdit()->setFocus(Qt::OtherFocusReason);
                lineEdit()->setCursorPosition(pos);
            }
        });
    }
}
