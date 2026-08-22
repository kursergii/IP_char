#include "savefiledialog.h"
#include "virtualkeyboard.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QMessageBox>
#include <QStorageInfo>

// Default to .csv if the operator didn't type an extension.
static QString withDefaultExtension(QString name)
{
    if (!name.contains('.'))
        name += ".csv";
    return name;
}

QString SaveFileDialog::findRemovableDrive()
{
    for (const QStorageInfo &info : QStorageInfo::mountedVolumes()) {
        if (!info.isValid() || !info.isReady() || info.isReadOnly())
            continue;
        // udisks/gvfs auto-mount removable media under one of these prefixes
        // on Linux; that's a much better signal here than QStorageInfo's own
        // device-type bits, which don't reliably flag USB flash drives.
        const QString root = info.rootPath();
        if (root.startsWith("/media/") || root.startsWith("/run/media/"))
            return root;
    }
    return QString();
}

SaveFileDialog::SaveFileDialog(const QString &title, const QString &defaultName,
                               const QString &directory, QWidget *parent)
    : QDialog(parent)
    , m_directory(directory.isEmpty() ? QDir::current().absolutePath() : directory)
{
    setWindowTitle(title);

    auto *outer = new QVBoxLayout(this);

    auto *pathLabel = new QLabel("Saving to: " + m_directory, this);
    pathLabel->setStyleSheet("color:#666;");
    outer->addWidget(pathLabel);

    m_nameEdit = new QLineEdit(defaultName, this);
    m_nameEdit->setMinimumHeight(40);
    QFont ef = m_nameEdit->font();
    ef.setPointSize(14);
    m_nameEdit->setFont(ef);
    m_nameEdit->selectAll();
    outer->addWidget(m_nameEdit);

    auto *keyboard = new VirtualKeyboard(m_nameEdit, this);
    outer->addWidget(keyboard);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *cancelBtn = new QPushButton("Cancel", this);
    auto *saveBtn = new QPushButton("Save", this);
    for (QPushButton *b : {cancelBtn, saveBtn})
        b->setFixedSize(100, 40);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    outer->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto tryAccept = [this] {
        QString name = m_nameEdit->text().trimmed();
        if (name.isEmpty())
            return;
        name = withDefaultExtension(name);
        if (QFile::exists(QDir(m_directory).filePath(name))) {
            auto reply = QMessageBox::question(this, "Overwrite file?",
                "\"" + name + "\" already exists. Overwrite it?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply != QMessageBox::Yes)
                return;
        }
        accept();
    };
    connect(saveBtn, &QPushButton::clicked, this, tryAccept);
    connect(keyboard, &VirtualKeyboard::enterPressed, this, tryAccept);

    resize(760, 420);
}

QString SaveFileDialog::getSaveFileName(QWidget *parent, const QString &title,
                                         const QString &defaultName,
                                         const QString &directory)
{
    SaveFileDialog dlg(title, defaultName, directory, parent);
    if (dlg.exec() != QDialog::Accepted)
        return QString();
    return QDir(dlg.m_directory).filePath(withDefaultExtension(dlg.m_nameEdit->text().trimmed()));
}
