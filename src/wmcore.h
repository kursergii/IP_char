#ifndef WMCORE_H
#define WMCORE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QVector>
#include <QString>

#include "oscilloscope.h"
#include "powermeter.h"
#include "pulser.h"

// Headless (no-GUI) equivalent of WM: owns the same three devices and runs
// the same timed HV sweep, but driven by text commands instead of widgets.
// This is the Raspberry Pi server half of the Bluetooth remote-control split
// (see the "bluetooth-remote" branch) — for now the commands come from
// stdin/stdout; swapping that for a Bluetooth-PAN QTcpSocket later shouldn't
// need any changes here, only in the transport layer around it.
//
// Deliberately a separate class from WM rather than a shared base, to avoid
// touching the working GUI app for this: the two currently duplicate the
// sweep/validation logic. Worth unifying (WM wrapping a WMCore) if the two
// drift, but not attempted here.
class WMCore : public QObject
{
    Q_OBJECT
public:
    explicit WMCore(bool simulate = false, QObject *parent = nullptr);
    ~WMCore();

public slots:
    // Commands — each returns a one-line human-readable result for the
    // caller to relay back to the client (stdin/stdout today, a socket
    // later). No destructive-action confirmation happens here: that's a
    // client-side concern (the eventual PC GUI owns the confirm dialogs).
    QString cmdPorts();
    QString cmdConnect(const QString &port);
    QString cmdStart(int from, int to, int step, int b);
    QString cmdPause();
    QString cmdResume();
    QString cmdStop();
    QString cmdLaser(bool on);
    QString cmdClear();
    QString cmdSave(const QString &fileName);
    QString cmdStatus();

signals:
    void requestOpen(const QString &port);
    void requestLaserOn();
    void requestLaserOff();
    void requestSetHV(int n, int b);

    // Async push notifications — the client learns about sweep progress this
    // way rather than polling cmdStatus() every tick.
    void pointCollected(int index, double current_V, double power_avg_mW,
                        double power_peak_mW, int hv);
    void sweepStopped(const QString &reason, int totalPoints);
    void log(const QString &message);
    // device: "pulser" | "osc" | "pm" | "all". Structured alternative to
    // parsing log() text, so a client doesn't need to pattern-match strings
    // to know when to light up a status indicator.
    void deviceStatus(const QString &device, bool connected);

private slots:
    void onPulserConnected(bool success);
    void onOscConnected(bool success);
    void onPmConnected(bool success);
    void read_values(double current_V);
    void read_power(double watts);
    void onMeasurementStep();

private:
    void applyHV(int n, int b);
    void applySimHV(int n, int b);
    void checkAllConnected();
    void clearPlotData();
    void doStop(const QString &reason);

    Osc        osc;
    Powermeter pm;
    Pulser     pulser;
    QThread    pulserThread;
    QTimer     measureTimer;

    QVector<double> dataCurrent;   // volts
    QVector<double> dataPower;     // mW (average)

    int  sweep_n  = 0;
    int  m_bValue = 1;
    int  m_from = 0, m_to = 0, m_step = 0;
    bool m_paused = false;
    bool m_sim    = false;

    bool pulser_connected = false;
    bool osc_connected    = false;
    bool pm_connected     = false;
    bool m_allConnected   = false;

    double latest_current = 0;
    double latest_power_W = 0;

    // Same stale-reading watchdog as WM — see wm.h.
    static constexpr qint64 kStaleTimeoutMs = 5000;
    QElapsedTimer m_currentUpdateTimer;
    QElapsedTimer m_powerUpdateTimer;

    // Same peak-power duty-cycle derivation as WM — see wm.h.
    static constexpr double kPulseDutyCycle =
        (double(Pulser::kPulseWidth50ns) / Pulser::kPulsePeriod50ns) *
        (double(Pulser::kModWs) / Pulser::kModPs);
};

#endif // WMCORE_H
