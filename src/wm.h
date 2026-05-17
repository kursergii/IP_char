#ifndef WM_H
#define WM_H

#include <QMainWindow>
#include <QTimer>
#include <QThread>
#include "qcustomplot.h"

#include "oscilloscope.h"
#include "powermeter.h"
#include "pulser.h"

namespace Ui { class WM; }

// Main window. Coordinates three hardware devices:
//   Pulser    — serial, runs in pulserThread via moveToThread
//   Osc       — PicoScope acquisition, runs in its own QThread
//   Powermeter — PM100A via USBTMC, runs in its own QThread
//
// The Start button runs a timed HV sweep: every 1 s the timer fires,
// reads the latest current (from Osc) and power (from Powermeter),
// plots the point, then steps the HV to the next value.
class WM : public QMainWindow
{
    Q_OBJECT

public:
    explicit WM(QWidget *parent = nullptr);
    ~WM();

signals:
    // Forwarded to Pulser via queued connection (runs in pulserThread)
    void requestOpen(const QString &port);
    void requestLaserOn();
    void requestLaserOff();
    void requestSetHV(int n, int b);

public slots:
    // Device connection
    void startPulser();
    void onPulserConnected(bool success);
    void startOscilloscope();
    void onOscConnected(bool success);
    void startPowermeter();
    void onPmConnected(bool success);

    // Measurement control
    void onStartClicked();     // toggles Start → Pause → Resume
    void startMeasurement();
    void stopMeasurement();
    void onMeasurementStep();  // called by measureTimer every 1 s

    // Data receivers (called via queued connections from worker threads)
    void read_values(const double &current_V);
    void read_power(double watts);

    void laser_ON();
    void save_data();
    void refreshPorts();

private:
    QCustomPlot *customPlot;

    // Sweep data: DATA[0] = current (V), DATA[1] = power (mW)
    // Indices match the measurement sequence — point i was taken at
    // n = spinBox_HV_n_from + i × spinBox_HV_n_step.
    QVector<QVector<double>> DATA;

    Osc        osc;
    Powermeter pm;
    Pulser     pulser;
    QThread    pulserThread;
    QTimer     measureTimer;

    int    sweep_n   = 0;      // current HV DAC value during a sweep
    bool   m_paused  = false;

    // Latest snapshots from background threads (written on main thread via queued slots)
    double latest_current = 0; // volts from oscilloscope (multiply by 1/R_sense for amps)
    double latest_power_W = 0; // watts from PM100A

    // Running axis bounds — updated incrementally to avoid O(n) scan per point
    double xMin = 0, xMax = 0;
    double yMin = 0, yMax = 0;

    void connects();
    void checkAllConnected();

    bool pulser_connected = false;
    bool osc_connected    = false;
    bool pm_connected     = false;

    Ui::WM *ui;
};

#endif // WM_H
