#ifndef WM_H
#define WM_H

#include <QMainWindow>
#include <QTimer>
#include <QThread>
#include <QVector>
#include <QElapsedTimer>

#include "oscilloscope.h"
#include "powermeter.h"
#include "pulser.h"

class PlotWidget;
class Stepper;
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QFrame;

// Main window. Same layout as the QML version, built with Qt Widgets and a
// lightweight PlotWidget (no QCustomPlot, no QtGraphs). Coordinates three
// devices (Pulser / Osc / Powermeter) and runs a timed HV sweep.
class WM : public QMainWindow
{
    Q_OBJECT

public:
    explicit WM(bool simulate = false, QWidget *parent = nullptr);
    ~WM();

signals:
    void requestOpen(const QString &port);
    void requestLaserOn();
    void requestLaserOff();
    void requestSetHV(int n, int b);

public slots:
    void connectAll();
    void onPulserConnected(bool success);
    void onOscConnected(bool success);
    void onPmConnected(bool success);

    void onStartClicked();
    void startMeasurement();
    void stopMeasurement();
    void onMeasurementStep();

    void read_values(double current_V);
    void read_power(double watts);

    void laser_ON();
    void save_data();
    void clear_data();
    void refreshPorts();

private:
    void buildUi();
    void checkAllConnected();
    void clearPlotData();
    void connectPulser();
    void connectOscilloscope();
    void connectPowermeter();
    void applyHV(int n, int b);      // set HV label + drive hardware (or simulate)
    void applySimHV(int n, int b);
    void updateControls();           // Start/Stop text + enabled state
    void setPill(QLabel *pill, bool on);
    void setBValue(int b);

    // ── UI ──────────────────────────────────────────────────────────────────
    PlotWidget  *plot        = nullptr;
    QComboBox   *portCombo   = nullptr;
    QPushButton *connectBtn  = nullptr;
    QPushButton *refreshBtn  = nullptr;
    QPushButton *clearBtn    = nullptr;
    QLabel      *pillPulser  = nullptr;
    QLabel      *pillOsc     = nullptr;
    QLabel      *pillPm      = nullptr;

    QFrame      *panel       = nullptr;
    QPushButton *startBtn    = nullptr;
    QPushButton *stopBtn     = nullptr;
    Stepper     *sFrom       = nullptr;
    Stepper     *sTo         = nullptr;
    Stepper     *sStep       = nullptr;
    QPushButton *bBtn1       = nullptr;
    QPushButton *bBtn2       = nullptr;
    QLabel      *currentVal  = nullptr;
    QLabel      *powerVal    = nullptr;
    QLabel      *peakPowerVal = nullptr;
    QLabel      *hvVal       = nullptr;
    QCheckBox   *laserCheck  = nullptr;
    QLabel      *pointsLabel = nullptr;
    QPushButton *saveBtn     = nullptr;

    // ── Backend ─────────────────────────────────────────────────────────────
    Osc        osc;
    Powermeter pm;
    Pulser     pulser;
    QThread    pulserThread;
    QTimer     measureTimer;

    QVector<QVector<double>> DATA;   // DATA[0]=current(V), DATA[1]=power(mW)

    int    sweep_n = 0;
    bool   m_paused = false;
    int    m_bValue = 1;

    double latest_current = 0;
    double latest_power_W = 0;

    // Watchdog: reset whenever read_values()/read_power() delivers a fresh
    // reading. If a sweep step finds either stale beyond kStaleTimeoutMs, the
    // device stopped responding (e.g. USB dropout) — the sweep aborts instead
    // of silently re-plotting the last known value as if it were current.
    static constexpr qint64 kStaleTimeoutMs = 5000;
    QElapsedTimer m_currentUpdateTimer;
    QElapsedTimer m_powerUpdateTimer;

    bool m_sim            = false;
    bool pulser_connected = false;
    bool osc_connected    = false;
    bool pm_connected     = false;

    // PM101A reads *average* power; a pulsed laser's peak power during each
    // pulse is much higher. Peak = average / duty_cycle. Derived from Pulser's
    // fixed default pulse settings (width/period/gating), not a live query —
    // goes stale if the pulser is ever reprogrammed to different timing.
    static constexpr double kPulseDutyCycle =
        (double(Pulser::kPulseWidth50ns) / Pulser::kPulsePeriod50ns) *
        (double(Pulser::kModWs) / Pulser::kModPs);
};

#endif // WM_H
