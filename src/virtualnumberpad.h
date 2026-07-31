#ifndef VIRTUALNUMBERPAD_H
#define VIRTUALNUMBERPAD_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QBoxLayout;

// Touch-friendly on-screen numeric keypad, wired directly to a QLineEdit.
// Digits only (the ranges it's used for are non-negative), matches
// VirtualKeyboard's key sizing so both look at home in the same dialogs.
class VirtualNumberPad : public QWidget
{
    Q_OBJECT
public:
    explicit VirtualNumberPad(QLineEdit *target, QWidget *parent = nullptr);

signals:
    void enterPressed();

private:
    QPushButton *addKey(QBoxLayout *row, const QString &label, int width = 64);

    QLineEdit *m_target;
};

#endif // VIRTUALNUMBERPAD_H
