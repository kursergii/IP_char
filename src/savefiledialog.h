#ifndef SAVEFILEDIALOG_H
#define SAVEFILEDIALOG_H

#include <QDialog>

class QLineEdit;

// Touch-friendly replacement for QFileDialog::getSaveFileName: a filename
// field plus an attached VirtualKeyboard, since the kiosk touchscreen has
// no physical keyboard. Saves into the app's working directory.
class SaveFileDialog : public QDialog
{
    Q_OBJECT
public:
    // Mirrors QFileDialog::getSaveFileName's static convenience API.
    static QString getSaveFileName(QWidget *parent, const QString &title,
                                    const QString &defaultName);

private:
    explicit SaveFileDialog(const QString &title, const QString &defaultName,
                             QWidget *parent = nullptr);

    QLineEdit *m_nameEdit = nullptr;
};

#endif // SAVEFILEDIALOG_H
