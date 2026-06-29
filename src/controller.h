#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include "oscilloscope.h"
#include "powermeter.h"
#include "pulser.h"

// Bridge between the QML UI and the device backends (Osc / Powermeter / Pulser).
// Replaces the old QWidget-based WM class: exposes device state as Q_PROPERTYs,
// actions as Q_INVOKABLEs, and streams measurement points to QML via signals.
// The device classes themselves are reused unchanged.
class Controller : public QObject
{
    Q_OBJECT

    // Serial ports available for the pulser
    Q_PROPERTY(QStringList ports READ ports NOTIFY portsChanged)

    // Live readings
    Q_PROPERTY(double currentValue READ currentValue NOTIFY currentChanged)
    Q_PROPERTY(double powerMW       READ powerMW      NOTIFY powerChanged)
    Q_PROPERTY(QString hvLabel      READ hvLabel      NOTIFY hvLabelChanged)

    // Per-device and aggregate connection state
    Q_PROPERTY(bool pulserConnected READ pulserConnected NOTIFY pulserConnectedChanged)
    Q_PROPERTY(bool oscConnected    READ oscConnected    NOTIFY oscConnectedChanged)
    Q_PROPERTY(bool pmConnected     READ pmConnected     NOTIFY pmConnectedChanged)
    Q_PROPERTY(bool allConnected    READ allConnected    NOTIFY allConnectedChanged)

    // Measurement run state
    Q_PROPERTY(bool measuring READ measuring NOTIFY measuringChanged)
    Q_PROPERTY(bool paused    READ paused    NOTIFY pausedChanged)
    Q_PROPERTY(bool laserOn   READ laserOn   NOTIFY laserOnChanged)
    Q_PROPERTY(int  pointCount READ pointCount NOTIFY pointCountChanged)
    Q_PROPERTY(bool simulated  READ simulated  CONSTANT)

public:
    explicit Controller(bool simulate = false, QObject *parent = nullptr);
    ~Controller();

    bool simulated() const           { return m_sim; }

    QStringList ports() const        { return m_ports; }
    double currentValue() const      { return m_currentValue; }
    double powerMW() const           { return m_powerMW; }
    QString hvLabel() const          { return m_hvLabel; }
    bool pulserConnected() const     { return m_pulserConnected; }
    bool oscConnected() const        { return m_oscConnected; }
    bool pmConnected() const         { return m_pmConnected; }
    bool allConnected() const        { return m_pulserConnected && m_oscConnected && m_pmConnected; }
    bool measuring() const           { return m_measuring; }
    bool paused() const              { return m_paused; }
    bool laserOn() const             { return m_laserOn; }
    int pointCount() const           { return DATA.isEmpty() ? 0 : DATA[0].size(); }

public slots:
    // Device control (called from QML)
    void refreshPorts();
    void connectDevices(const QString &port);
    void setLaser(bool on);

    // Measurement sweep (called from QML; sweep params come from the UI spin boxes)
    void startOrPause(int nFrom, int nTo, int nStep, int b);
    void stop();
    void clearData();

    // Data export
    QString suggestedFileName() const;
    bool saveData(const QUrl &fileUrl);

signals:
    // Cross-thread requests to the pulser (which lives in pulserThread)
    void requestOpen(const QString &port);
    void requestLaserOn();
    void requestLaserOff();
    void requestSetHV(int n, int b);

    // Plot updates consumed by QML
    void pointAdded(double current, double powerMW);
    void dataCleared();

    // UI feedback
    void warning(const QString &title, const QString &text);

    // Property notifications
    void portsChanged();
    void currentChanged();
    void powerChanged();
    void hvLabelChanged();
    void pulserConnectedChanged();
    void oscConnectedChanged();
    void pmConnectedChanged();
    void allConnectedChanged();
    void measuringChanged();
    void pausedChanged();
    void laserOnChanged();
    void pointCountChanged();

private slots:
    void onPulserConnected(bool success);
    void onOscConnected(bool success);
    void onPmConnected(bool success);
    void read_values(double current_V);
    void read_power(double watts);
    void onMeasurementStep();

private:
    void startMeasurement(int nFrom, int nTo, int nStep, int b);
    void applyHV(int n, int b);      // set HV label + drive hardware (or simulate)
    void applySimHV(int n, int b);   // compute simulated current/power for setpoint n,b
    void setMeasuring(bool m);
    void setPaused(bool p);
    void setLaserState(bool on);
    void finishSweep();

    Osc        osc;
    Powermeter pm;
    Pulser     pulser;
    QThread    pulserThread;
    QTimer     measureTimer;

    QVector<QVector<double>> DATA;  // DATA[0]=current (V), DATA[1]=power (mW)

    QStringList m_ports;
    double m_currentValue = 0;
    double m_powerMW      = 0;
    QString m_hvLabel     = "HV: —";

    bool m_sim             = false;  // simulation mode: synthesise data, no hardware
    bool m_pulserConnected = false;
    bool m_oscConnected    = false;
    bool m_pmConnected     = false;
    bool m_measuring       = false;
    bool m_paused          = false;
    bool m_laserOn         = false;

    double latest_current  = 0;
    double latest_power_W  = 0;

    // Active sweep parameters (captured when a run starts)
    int sweep_n = 0;
    int m_nTo   = 0;
    int m_nStep = 0;
    int m_b     = 1;
};

#endif // CONTROLLER_H
