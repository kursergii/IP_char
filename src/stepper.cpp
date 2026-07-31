#include "stepper.h"
#include "numberinputdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <algorithm>

Stepper::Stepper(const QString &label, int from, int to, int step, int value,
                 QWidget *parent)
    : QWidget(parent), m_label(label), m_from(from), m_to(to), m_step(step),
      m_value(std::clamp(value, from, to))
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto *name = new QLabel(label, this);
    name->setMinimumWidth(50);
    QFont bold = name->font(); bold.setBold(true); name->setFont(bold);
    lay->addWidget(name);

    auto *minus = new QPushButton("−", this);   // −
    auto *plus  = new QPushButton("+", this);
    for (QPushButton *b : {minus, plus}) {
        b->setAutoRepeat(true);
        b->setFixedSize(46, 40);
        QFont bf = b->font(); bf.setPointSize(16); bf.setBold(true); b->setFont(bf);
    }
    lay->addWidget(minus);

    m_valueBtn = new QPushButton(this);
    m_valueBtn->setFocusPolicy(Qt::NoFocus);
    m_valueBtn->setMinimumHeight(40);
    m_valueBtn->setStyleSheet(
        "QPushButton { background:#fafafa; border:1px solid #bbbbbb; border-radius:4px; }"
        "QPushButton:pressed { background:#eeeeee; }");
    QFont vf = m_valueBtn->font(); vf.setPointSize(16); vf.setBold(true);
    m_valueBtn->setFont(vf);
    lay->addWidget(m_valueBtn, 1);

    lay->addWidget(plus);

    connect(minus, &QPushButton::clicked, this, [this] { setValue(m_value - m_step); });
    connect(plus,  &QPushButton::clicked, this, [this] { setValue(m_value + m_step); });
    connect(m_valueBtn, &QPushButton::clicked, this, &Stepper::openNumberPad);

    updateLabel();
}

void Stepper::setValue(int v)
{
    v = std::clamp(v, m_from, m_to);
    if (v == m_value)
        return;
    m_value = v;
    updateLabel();
    emit valueChanged(m_value);
}

void Stepper::updateLabel()
{
    m_valueBtn->setText(QString::number(m_value));
}

void Stepper::openNumberPad()
{
    bool ok = false;
    int v = NumberInputDialog::getInt(this, m_label, "Value", m_value, m_from, m_to, &ok);
    if (ok)
        setValue(v);
}
