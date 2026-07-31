#include "virtualkeyboard.h"

#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

VirtualKeyboard::VirtualKeyboard(QLineEdit *target, QWidget *parent)
    : QWidget(parent), m_target(target)
{
    auto *outer = new QVBoxLayout(this);
    outer->setSpacing(6);

    addLetterRow("1234567890");
    addLetterRow("qwertyuiop");
    addLetterRow("asdfghjkl", 1);

    auto *row4 = new QHBoxLayout;
    row4->setSpacing(6);
    outer->addLayout(row4);
    auto *shiftBtn = addKey(row4, "Shift", 72);
    shiftBtn->setCheckable(true);
    connect(shiftBtn, &QPushButton::clicked, this, &VirtualKeyboard::toggleShift);
    for (QChar c : QString("zxcvbnm")) {
        auto *key = addKey(row4, QString(c));
        m_letterKeys << key;
        connect(key, &QPushButton::clicked, this, [this, key] { m_target->insert(key->text()); });
    }
    auto *backBtn = addKey(row4, "⌫", 72);   // ⌫
    connect(backBtn, &QPushButton::clicked, this, [this] { m_target->backspace(); });

    auto *row5 = new QHBoxLayout;
    row5->setSpacing(6);
    outer->addLayout(row5);
    for (QChar c : QString("-_.")) {
        auto *key = addKey(row5, QString(c));
        connect(key, &QPushButton::clicked, this, [this, c] { m_target->insert(c); });
    }
    auto *spaceBtn = addKey(row5, "Space", 220);
    connect(spaceBtn, &QPushButton::clicked, this, [this] { m_target->insert(" "); });
    auto *clearBtn = addKey(row5, "Clear", 72);
    connect(clearBtn, &QPushButton::clicked, this, [this] { m_target->clear(); });
    auto *enterBtn = addKey(row5, "Enter", 90);
    connect(enterBtn, &QPushButton::clicked, this, &VirtualKeyboard::enterPressed);
}

QPushButton *VirtualKeyboard::addKey(QBoxLayout *row, const QString &label, int width)
{
    auto *btn = new QPushButton(label, this);
    btn->setFixedSize(width, 44);
    QFont f = btn->font();
    f.setPointSize(13);
    btn->setFont(f);
    row->addWidget(btn);
    return btn;
}

void VirtualKeyboard::addLetterRow(const QString &letters, int leadingStretch)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    static_cast<QVBoxLayout *>(layout())->addLayout(row);
    if (leadingStretch)
        row->addStretch(leadingStretch);
    for (QChar c : letters) {
        auto *key = addKey(row, QString(c));
        if (c.isLetter())
            m_letterKeys << key;
        connect(key, &QPushButton::clicked, this, [this, key] { m_target->insert(key->text()); });
    }
    if (leadingStretch)
        row->addStretch(leadingStretch);
}

void VirtualKeyboard::toggleShift()
{
    m_shift = !m_shift;
    for (QPushButton *key : m_letterKeys)
        key->setText(m_shift ? key->text().toUpper() : key->text().toLower());
}
