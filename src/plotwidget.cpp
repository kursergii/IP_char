#include "plotwidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
}

void PlotWidget::addPoint(double x, double y)
{
    m_pts.append(QPointF(x, y));
    updateBounds();
    update();
}

void PlotWidget::setData(const QVector<double> &xs, const QVector<double> &ys)
{
    m_pts.clear();
    const int n = std::min(xs.size(), ys.size());
    for (int i = 0; i < n; ++i)
        m_pts.append(QPointF(xs[i], ys[i]));
    updateBounds();
    update();
}

void PlotWidget::clear()
{
    m_pts.clear();
    m_xMin = m_yMin = 0;
    m_xMax = m_yMax = 1;
    update();
}

void PlotWidget::setAxisTitles(const QString &x, const QString &y)
{
    m_xTitle = x;
    m_yTitle = y;
    update();
}

void PlotWidget::updateBounds()
{
    if (m_pts.isEmpty()) {
        m_xMin = m_yMin = 0; m_xMax = m_yMax = 1;
        return;
    }
    double xlo = m_pts[0].x(), xhi = xlo, ylo = m_pts[0].y(), yhi = ylo;
    for (const QPointF &p : m_pts) {
        xlo = std::min(xlo, p.x()); xhi = std::max(xhi, p.x());
        ylo = std::min(ylo, p.y()); yhi = std::max(yhi, p.y());
    }
    // 8% margin; guard against zero span
    double xs = (xhi - xlo < 1e-12) ? 1.0 : (xhi - xlo) * 0.08;
    double ys = (yhi - ylo < 1e-12) ? 1.0 : (yhi - ylo) * 0.08;
    m_xMin = xlo - xs; m_xMax = xhi + xs;
    m_yMin = ylo - ys; m_yMax = yhi + ys;
}

double PlotWidget::niceNum(double range, bool round)
{
    if (range <= 0) return 1.0;
    double expv = std::floor(std::log10(range));
    double f = range / std::pow(10, expv);   // 1..10
    double nf;
    if (round) {
        if      (f < 1.5) nf = 1;
        else if (f < 3)   nf = 2;
        else if (f < 7)   nf = 5;
        else              nf = 10;
    } else {
        if      (f <= 1) nf = 1;
        else if (f <= 2) nf = 2;
        else if (f <= 5) nf = 5;
        else             nf = 10;
    }
    return nf * std::pow(10, expv);
}

void PlotWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int L = 66, R = 16, T = 14, B = 44;   // margins for axes/labels
    QRectF plot(L, T, width() - L - R, height() - T - B);
    if (plot.width() < 20 || plot.height() < 20)
        return;

    // Plot area background + frame
    p.fillRect(plot, QColor("#fafafa"));
    p.setPen(QColor("#bbbbbb"));
    p.drawRect(plot);

    // Axis ranges (fall back to a unit box when there is no data yet)
    double xlo = m_xMin, xhi = m_xMax, ylo = m_yMin, yhi = m_yMax;
    if (xhi - xlo < 1e-12) { xlo -= 0.5; xhi += 0.5; }
    if (yhi - ylo < 1e-12) { ylo -= 0.5; yhi += 0.5; }

    auto mapX = [&](double x) { return plot.left()   + (x - xlo) / (xhi - xlo) * plot.width(); };
    auto mapY = [&](double y) { return plot.bottom() - (y - ylo) / (yhi - ylo) * plot.height(); };

    QFont f = p.font();
    f.setPointSizeF(9);
    p.setFont(f);
    QFontMetrics fm(f);

    // ── X grid + tick labels ────────────────────────────────────────────────
    double xrange = niceNum(xhi - xlo, false);
    double xstep  = niceNum(xrange / 5.0, true);
    double xstart = std::ceil(xlo / xstep) * xstep;
    for (double xv = xstart; xv <= xhi + xstep * 1e-6; xv += xstep) {
        double px = mapX(xv);
        p.setPen(QColor("#e6e6e6"));
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
        p.setPen(QColor("#555555"));
        QString lbl = QString::number(xv, 'g', 4);
        p.drawText(QRectF(px - 40, plot.bottom() + 4, 80, 16),
                   Qt::AlignHCenter | Qt::AlignTop, lbl);
    }

    // ── Y grid + tick labels ────────────────────────────────────────────────
    double yrange = niceNum(yhi - ylo, false);
    double ystep  = niceNum(yrange / 5.0, true);
    double ystart = std::ceil(ylo / ystep) * ystep;
    for (double yv = ystart; yv <= yhi + ystep * 1e-6; yv += ystep) {
        double py = mapY(yv);
        p.setPen(QColor("#e6e6e6"));
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
        p.setPen(QColor("#555555"));
        QString lbl = QString::number(yv, 'g', 4);
        p.drawText(QRectF(plot.left() - 62, py - 8, 56, 16),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
    }

    // ── Axis titles ─────────────────────────────────────────────────────────
    p.setPen(Qt::black);
    QFont tf = f; tf.setBold(true); p.setFont(tf);
    p.drawText(QRectF(plot.left(), height() - 20, plot.width(), 18),
               Qt::AlignHCenter, m_xTitle);
    p.save();
    p.translate(16, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2, -8, plot.height(), 16),
               Qt::AlignHCenter, m_yTitle);
    p.restore();
    p.setFont(f);

    // ── Scatter points ──────────────────────────────────────────────────────
    p.setPen(QPen(QColor("#10456e"), 1));
    p.setBrush(QColor("#1f77b4"));
    for (const QPointF &pt : m_pts) {
        double px = mapX(pt.x()), py = mapY(pt.y());
        if (px >= plot.left() - 6 && px <= plot.right() + 6 &&
            py >= plot.top() - 6  && py <= plot.bottom() + 6)
            p.drawEllipse(QPointF(px, py), 5, 5);
    }
}
