#include "stepper.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <algorithm>

Stepper::Stepper(const QString &label, int from, int to, int step, int value,
                 QWidget *parent)
    : QWidget(parent), m_from(from), m_to(to), m_step(step),
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

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setMinimumHeight(40);
    m_valueLabel->setStyleSheet(
        "background:#fafafa; border:1px solid #bbbbbb; border-radius:4px;");
    QFont vf = m_valueLabel->font(); vf.setPointSize(16); vf.setBold(true);
    m_valueLabel->setFont(vf);
    lay->addWidget(m_valueLabel, 1);

    lay->addWidget(plus);

    connect(minus, &QPushButton::clicked, this, [this] { setValue(m_value - m_step); });
    connect(plus,  &QPushButton::clicked, this, [this] { setValue(m_value + m_step); });

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
    m_valueLabel->setText(QString::number(m_value));
}
