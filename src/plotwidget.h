#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>

// Lightweight scatter plot (pure QPainter, no external dependency).
// Auto-ranges to the data, draws axes, grid, tick labels and axis titles.
// Replaces QCustomPlot for the I-P curve.
class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);

    void addPoint(double x, double y);
    void setData(const QVector<double> &xs, const QVector<double> &ys);
    void clear();
    void setAxisTitles(const QString &x, const QString &y);

    QSize sizeHint() const override { return QSize(600, 400); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<QPointF> m_pts;
    double m_xMin = 0, m_xMax = 1, m_yMin = 0, m_yMax = 1;
    QString m_xTitle = "Current, V";
    QString m_yTitle = "Power, mW";

    void updateBounds();
    // "nice" axis range + step for ~5 ticks (1/2/5 × 10^n)
    static double niceNum(double range, bool round);
};

#endif // PLOTWIDGET_H
