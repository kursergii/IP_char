#include "virtualnumberpad.h"

#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

VirtualNumberPad::VirtualNumberPad(QLineEdit *target, QWidget *parent)
    : QWidget(parent), m_target(target)
{
    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(6);

    static const char *rows[] = { "789", "456", "123" };
    for (const char *digits : rows) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        outer->addLayout(row);
        for (QChar c : QString(digits)) {
            auto *key = addKey(row, QString(c));
            connect(key, &QPushButton::clicked, this, [this, c] { m_target->insert(c); });
        }
    }

    auto *lastRow = new QHBoxLayout;
    lastRow->setSpacing(6);
    outer->addLayout(lastRow);
    auto *clearBtn = addKey(lastRow, "Clear");
    connect(clearBtn, &QPushButton::clicked, this, [this] { m_target->clear(); });
    auto *zeroBtn = addKey(lastRow, "0");
    connect(zeroBtn, &QPushButton::clicked, this, [this] { m_target->insert("0"); });
    auto *backBtn = addKey(lastRow, "⌫");   // ⌫
    connect(backBtn, &QPushButton::clicked, this, [this] { m_target->backspace(); });

    auto *enterRow = new QHBoxLayout;
    enterRow->setSpacing(6);
    outer->addLayout(enterRow);
    auto *okBtn = addKey(enterRow, "Enter", 208);
    connect(okBtn, &QPushButton::clicked, this, &VirtualNumberPad::enterPressed);
}

QPushButton *VirtualNumberPad::addKey(QBoxLayout *row, const QString &label, int width)
{
    auto *btn = new QPushButton(label, this);
    btn->setFixedSize(width, 44);
    QFont f = btn->font();
    f.setPointSize(14);
    f.setBold(true);
    btn->setFont(f);
    if (row)
        row->addWidget(btn);
    return btn;
}
