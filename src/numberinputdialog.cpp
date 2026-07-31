#include "numberinputdialog.h"
#include "virtualnumberpad.h"

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QFont>
#include <algorithm>

NumberInputDialog::NumberInputDialog(const QString &title, const QString &label,
                                     int value, int minValue, int maxValue,
                                     QWidget *parent)
    : QDialog(parent), m_min(minValue), m_max(maxValue)
{
    setWindowTitle(title);

    auto *outer = new QVBoxLayout(this);

    auto *rangeLabel = new QLabel(
        QString("%1 (%2 – %3)").arg(label).arg(minValue).arg(maxValue), this);
    rangeLabel->setStyleSheet("color:#666;");
    outer->addWidget(rangeLabel);

    m_valueEdit = new QLineEdit(QString::number(value), this);
    m_valueEdit->setValidator(new QIntValidator(minValue, maxValue, m_valueEdit));
    m_valueEdit->setAlignment(Qt::AlignCenter);
    m_valueEdit->setMinimumHeight(40);
    QFont ef = m_valueEdit->font();
    ef.setPointSize(16);
    ef.setBold(true);
    m_valueEdit->setFont(ef);
    m_valueEdit->selectAll();
    outer->addWidget(m_valueEdit);

    auto *numpad = new VirtualNumberPad(m_valueEdit, this);
    outer->addWidget(numpad);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *cancelBtn = new QPushButton("Cancel", this);
    auto *okBtn = new QPushButton("OK", this);
    for (QPushButton *b : {cancelBtn, okBtn})
        b->setFixedSize(100, 40);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    outer->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto tryAccept = [this] {
        if (!m_valueEdit->text().trimmed().isEmpty())
            accept();
    };
    connect(okBtn, &QPushButton::clicked, this, tryAccept);
    connect(numpad, &VirtualNumberPad::enterPressed, this, tryAccept);

    resize(480, 460);
}

int NumberInputDialog::getInt(QWidget *parent, const QString &title, const QString &label,
                              int value, int minValue, int maxValue, bool *ok)
{
    NumberInputDialog dlg(title, label, value, minValue, maxValue, parent);
    bool accepted = dlg.exec() == QDialog::Accepted;
    if (ok)
        *ok = accepted;
    if (!accepted)
        return value;
    return std::clamp(dlg.m_valueEdit->text().trimmed().toInt(), minValue, maxValue);
}
