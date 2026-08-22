#include "wmcore.h"

#include <QSerialPortInfo>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

WMCore::WMCore(bool simulate, QObject *parent)
    : QObject(parent), m_sim(simulate)
{
    connect(&measureTimer, &QTimer::timeout, this, &WMCore::onMeasurementStep);

    // Pulser runs in its own thread — serial I/O never blocks the command loop.
    pulser.moveToThread(&pulserThread);
    connect(this,    &WMCore::requestOpen,          &pulser, &Pulser::open);
    connect(this,    &WMCore::requestLaserOn,       &pulser, &Pulser::laserOn);
    connect(this,    &WMCore::requestLaserOff,      &pulser, &Pulser::laserOff);
    connect(this,    &WMCore::requestSetHV,         &pulser, &Pulser::setHV);
    connect(&pulser, &Pulser::connectionResult,     this,    &WMCore::onPulserConnected);
    pulserThread.start();

    connect(&osc, &Osc::sendData,               this, &WMCore::read_values);
    connect(&osc, &Osc::connectionResult,        this, &WMCore::onOscConnected);
    connect(&pm,  &Powermeter::sendPower,        this, &WMCore::read_power);
    connect(&pm,  &Powermeter::connectionResult, this, &WMCore::onPmConnected);
}

WMCore::~WMCore()
{
    QMetaObject::invokeMethod(&pulser, "close", Qt::BlockingQueuedConnection);
    pulserThread.quit();
    pulserThread.wait();
    osc.stop(); osc.wait();
    pm.stop();  pm.wait();
}

// ── Commands ─────────────────────────────────────────────────────────────────

QString WMCore::cmdPorts()
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        names << info.portName();
    return names.isEmpty() ? "PORTS" : "PORTS " + names.join(",");
}

QString WMCore::cmdConnect(const QString &port)
{
    if (m_sim) {
        pulser_connected = osc_connected = pm_connected = true;
        checkAllConnected();
        return "OK connect: simulated, all devices connected";
    }
    if (!pulser_connected) {
        if (port.trimmed().isEmpty())
            emit log("pulser: no port given, skipping (try: connect <port>)");
        else
            emit requestOpen(port.trimmed());
    }
    if (!osc_connected) osc.start();
    if (!pm_connected)  pm.start();
    return "OK connect: attempting — watch for LOG lines";
}

QString WMCore::cmdStart(int from, int to, int step, int b)
{
    if (!m_allConnected)
        return "ERROR start: not all devices connected";
    if (measureTimer.isActive())
        return "ERROR start: sweep already running";
    if (from >= to)
        return "ERROR start: 'from' must be less than 'to'";
    if (step >= (to - from))
        return "ERROR start: step must be smaller than the range (to - from)";

    clearPlotData();
    m_from = from; m_to = to; m_step = step; m_bValue = b;
    sweep_n   = from;
    m_paused  = false;
    m_currentUpdateTimer.start();
    m_powerUpdateTimer.start();
    applyHV(sweep_n, m_bValue);
    if (!m_sim) emit requestLaserOn();

    // Real hardware needs the full 1 s for the PM101A's averaged reading to
    // settle; simulation has no such constraint, so it runs faster for testing.
    measureTimer.start(m_sim ? 250 : 1000);
    return "OK start: sweep running";
}

QString WMCore::cmdPause()
{
    if (!measureTimer.isActive())
        return "ERROR pause: not running";
    measureTimer.stop();
    m_paused = true;
    return "OK pause";
}

QString WMCore::cmdResume()
{
    if (!m_paused)
        return "ERROR resume: not paused";
    m_paused = false;
    measureTimer.start(m_sim ? 250 : 1000);
    return "OK resume";
}

QString WMCore::cmdStop()
{
    if (!measureTimer.isActive() && !m_paused)
        return "ERROR stop: not running";
    doStop("stopped by operator");
    return "OK stop";
}

QString WMCore::cmdLaser(bool on)
{
    if (m_sim) return "OK laser: simulated (no-op)";
    if (on) emit requestLaserOn(); else emit requestLaserOff();
    return on ? "OK laser on" : "OK laser off";
}

QString WMCore::cmdClear()
{
    if (measureTimer.isActive())
        return "ERROR clear: stop the sweep first";
    int n = dataCurrent.size();
    clearPlotData();
    return "OK clear: discarded " + QString::number(n) + " point(s)";
}

QString WMCore::cmdSave(const QString &fileName)
{
    if (dataCurrent.isEmpty())
        return "ERROR save: no data to save";
    QString name = fileName.trimmed();
    if (name.isEmpty())
        return "ERROR save: empty filename";
    if (!name.contains('.'))
        name += ".csv";

    QFile file(name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return "ERROR save: cannot open file: " + file.errorString();

    QTextStream out(&file);
    out << "# IP characterisation — " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# HV n_from=" << m_from << " n_step=" << m_step << " n_to=" << m_to
        << " b=" << m_bValue << "\n";
    out << "# Points: " << dataCurrent.size() << "\n";
    out << "HV_n,Current_V,Power_mW\n";

    int n = m_from;
    for (int i = 0; i < dataCurrent.size(); ++i) {
        out << n << "," << dataCurrent[i] << "," << dataPower[i] << "\n";
        n += m_step;
    }
    file.close();
    return "OK save: " + QString::number(dataCurrent.size()) + " points -> "
           + QFileInfo(name).absoluteFilePath();
}

QString WMCore::cmdStatus()
{
    return QString("STATUS pulser=%1 osc=%2 pm=%3 running=%4 paused=%5 "
                    "current=%6 power_avg=%7 power_peak=%8 hv=%9 points=%10")
        .arg(pulser_connected ? 1 : 0)
        .arg(osc_connected    ? 1 : 0)
        .arg(pm_connected     ? 1 : 0)
        .arg(measureTimer.isActive() ? 1 : 0)
        .arg(m_paused ? 1 : 0)
        .arg(latest_current, 0, 'f', 4)
        .arg(latest_power_W * 1000.0, 0, 'f', 3)
        .arg(latest_power_W * 1000.0 / kPulseDutyCycle, 0, 'f', 1)
        .arg(sweep_n)
        .arg(dataCurrent.size());
}

// ── Device connection ───────────────────────────────────────────────────────

void WMCore::onPulserConnected(bool success)
{
    if (success) { pulser_connected = true; checkAllConnected(); emit log("pulser connected"); }
    else emit log("ERROR pulser: failed to connect — check the USB-serial connection");
    emit deviceStatus("pulser", success);
}

void WMCore::onOscConnected(bool success)
{
    if (success) { osc_connected = true; checkAllConnected(); emit log("oscilloscope connected"); }
    else emit log("ERROR oscilloscope: failed to connect — check the USB connection");
    emit deviceStatus("osc", success);
}

void WMCore::onPmConnected(bool success)
{
    if (success) { pm_connected = true; checkAllConnected(); emit log("power meter connected"); }
    else emit log("ERROR power meter: failed to connect — check the USB connection");
    emit deviceStatus("pm", success);
}

void WMCore::checkAllConnected()
{
    if (pulser_connected && osc_connected && pm_connected) {
        m_allConnected = true;
        emit log("all devices connected — ready to start");
        emit deviceStatus("all", true);
    }
}

// ── Data receivers ──────────────────────────────────────────────────────────

void WMCore::read_values(double current_V)
{
    latest_current = current_V;
    m_currentUpdateTimer.restart();
}

void WMCore::read_power(double watts)
{
    latest_power_W = watts;
    m_powerUpdateTimer.restart();
}

// ── Measurement sweep ───────────────────────────────────────────────────────

void WMCore::clearPlotData()
{
    dataCurrent.clear();
    dataPower.clear();
}

void WMCore::doStop(const QString &reason)
{
    measureTimer.stop();
    m_paused = false;
    if (!m_sim) emit requestLaserOff();
    emit sweepStopped(reason, dataCurrent.size());
}

void WMCore::onMeasurementStep()
{
    if (m_currentUpdateTimer.elapsed() > kStaleTimeoutMs ||
        m_powerUpdateTimer.elapsed() > kStaleTimeoutMs) {
        doStop(QString("stalled: no fresh reading for %1 s").arg(kStaleTimeoutMs / 1000));
        return;
    }

    if (!std::isfinite(latest_current) || !std::isfinite(latest_power_W))
        return;

    double x    = latest_current;
    double y    = latest_power_W * 1000.0;       // W → mW (average)
    double peak = y / kPulseDutyCycle;            // mW (peak, see kPulseDutyCycle)

    dataCurrent.push_back(x);
    dataPower.push_back(y);
    emit pointCollected(dataCurrent.size(), x, y, peak, sweep_n);

    sweep_n += m_step;

    if (sweep_n > m_to) {
        measureTimer.stop();
        m_paused = false;
        if (!m_sim) emit requestLaserOff();
        emit sweepStopped("complete", dataCurrent.size());
    } else {
        applyHV(sweep_n, m_bValue);
    }
}

// ── HV setpoint: hardware or simulation ─────────────────────────────────────

void WMCore::applyHV(int n, int b)
{
    if (m_sim) applySimHV(n, b);
    else       emit requestSetHV(n, b);
}

void WMCore::applySimHV(int n, int b)
{
    // Synthetic laser-diode L-I curve (threshold + slope) for testing without
    // hardware — mirrors WM::applySimHV so both entry points behave the same way.
    auto noise = [](double frac) {
        return 1.0 + frac * (QRandomGenerator::global()->generateDouble() - 0.5);
    };
    double drive     = (n / 4096.0) * b;
    double current_V = 0.05 * drive * noise(0.02);
    const double I_th = 0.012;
    double power_W = current_V > I_th ? 0.8 * (current_V - I_th) : 0.0;
    power_W = std::max(0.0, power_W * noise(0.04));

    read_values(current_V);
    read_power(power_W);
}
