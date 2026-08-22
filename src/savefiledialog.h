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
    // If directory is empty, saves into the app's current working directory.
    static QString getSaveFileName(QWidget *parent, const QString &title,
                                    const QString &defaultName,
                                    const QString &directory = QString());

    // Root path of a mounted, writable removable drive (e.g. a USB stick),
    // or an empty string if none is present.
    static QString findRemovableDrive();

private:
    explicit SaveFileDialog(const QString &title, const QString &defaultName,
                             const QString &directory, QWidget *parent = nullptr);

    QLineEdit *m_nameEdit = nullptr;
    QString m_directory;
};

#endif // SAVEFILEDIALOG_H
