#ifndef STEPPER_H
#define STEPPER_H

#include <QWidget>
#include <QString>

class QPushButton;

// Touch-friendly numeric stepper: [label]  [ − ]  [ value ]  [ + ]
// Big buttons with auto-repeat; clamps to [from, to]. Mirrors the QML
// LabeledStepper component so the Widgets UI matches the QML one. Tapping
// the value opens a virtual numpad to type an exact value directly.
class Stepper : public QWidget
{
    Q_OBJECT
public:
    Stepper(const QString &label, int from, int to, int step, int value,
            QWidget *parent = nullptr);

    int value() const { return m_value; }
    void setValue(int v);

signals:
    void valueChanged(int value);

private:
    void updateLabel();
    void openNumberPad();

    QString m_label;
    int m_from, m_to, m_step, m_value;
    QPushButton *m_valueBtn = nullptr;
};

#endif // STEPPER_H
