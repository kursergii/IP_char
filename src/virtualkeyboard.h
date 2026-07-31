#ifndef VIRTUALKEYBOARD_H
#define VIRTUALKEYBOARD_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QBoxLayout;

// Touch-friendly on-screen QWERTY keyboard, wired directly to a QLineEdit.
// Built in-app (no qtvirtualkeyboard dependency) so it works with the
// stock Qt 6.4 the Widgets branch targets on the Pi.
class VirtualKeyboard : public QWidget
{
    Q_OBJECT
public:
    explicit VirtualKeyboard(QLineEdit *target, QWidget *parent = nullptr);

signals:
    void enterPressed();

private:
    QPushButton *addKey(QBoxLayout *row, const QString &label, int width = 44);
    void addLetterRow(const QString &letters, int leadingStretch = 0);
    void toggleShift();

    QLineEdit *m_target;
    QList<QPushButton *> m_letterKeys;
    bool m_shift = false;
};

#endif // VIRTUALKEYBOARD_H
