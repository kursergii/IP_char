#include "savefiledialog.h"
#include "virtualkeyboard.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFont>

SaveFileDialog::SaveFileDialog(const QString &title, const QString &defaultName,
                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);

    auto *outer = new QVBoxLayout(this);

    auto *pathLabel = new QLabel("Saving to: " + QDir::current().absolutePath(), this);
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
        if (!m_nameEdit->text().trimmed().isEmpty())
            accept();
    };
    connect(saveBtn, &QPushButton::clicked, this, tryAccept);
    connect(keyboard, &VirtualKeyboard::enterPressed, this, tryAccept);

    resize(760, 420);
}

QString SaveFileDialog::getSaveFileName(QWidget *parent, const QString &title,
                                         const QString &defaultName)
{
    SaveFileDialog dlg(title, defaultName, parent);
    if (dlg.exec() != QDialog::Accepted)
        return QString();
    return QDir::current().filePath(dlg.m_nameEdit->text().trimmed());
}
