#ifndef NUMBERINPUTDIALOG_H
#define NUMBERINPUTDIALOG_H

#include <QDialog>

class QLineEdit;

// Touch-friendly integer entry: a value field plus an attached
// VirtualNumberPad, for picking exact from/to/step values (the measurement
// diapason) without hunting through +/- taps on the Stepper widget.
class NumberInputDialog : public QDialog
{
    Q_OBJECT
public:
    // Mirrors QInputDialog::getInt's static convenience API. Returns the
    // chosen value clamped to [minValue, maxValue]; *ok is false on cancel.
    static int getInt(QWidget *parent, const QString &title, const QString &label,
                       int value, int minValue, int maxValue, bool *ok = nullptr);

private:
    NumberInputDialog(const QString &title, const QString &label,
                       int value, int minValue, int maxValue, QWidget *parent = nullptr);

    int m_min, m_max;
    QLineEdit *m_valueEdit = nullptr;
};

#endif // NUMBERINPUTDIALOG_H
