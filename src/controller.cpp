#include "controller.h"

#include <QSerialPortInfo>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <cmath>

Controller::Controller(QObject *parent) : QObject(parent)
{
    DATA.resize(2);  // DATA[0] = current (V), DATA[1] = power (mW)

    connect(&measureTimer, &QTimer::timeout, this, &Controller::onMeasurementStep);

    // Pulser runs in its own thread — serial I/O never blocks the UI.
    // Command signals cross threads as queued connections automatically.
    pulser.moveToThread(&pulserThread);
    connect(this,    &Controller::requestOpen,     &pulser, &Pulser::open);
    connect(this,    &Controller::requestLaserOn,  &pulser, &Pulser::laserOn);
    connect(this,    &Controller::requestLaserOff, &pulser, &Pulser::laserOff);
    connect(this,    &Controller::requestSetHV,    &pulser, &Pulser::setHV);
    connect(&pulser, &Pulser::connectionResult,    this,    &Controller::onPulserConnected);
    pulserThread.start();

    // Osc and Powermeter are QThreads that emit data via queued connections.
    connect(&osc, &Osc::sendData,               this, &Controller::read_values);
    connect(&osc, &Osc::connectionResult,        this, &Controller::onOscConnected);
    connect(&pm,  &Powermeter::sendPower,        this, &Controller::read_power);
    connect(&pm,  &Powermeter::connectionResult, this, &Controller::onPmConnected);

    refreshPorts();
}

Controller::~Controller()
{
    // Laser off and serial closed from the pulser's own thread before quitting it
    QMetaObject::invokeMethod(&pulser, "close", Qt::BlockingQueuedConnection);
    pulserThread.quit();
    pulserThread.wait();

    osc.stop();
    osc.wait();
    pm.stop();
    pm.wait();
}

// ── Device connection ───────────────────────────────────────────────────────────

void Controller::refreshPorts()
{
    m_ports.clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        m_ports.append(info.portName());
    emit portsChanged();
}

void Controller::connectDevices(const QString &port)
{
    if (!m_pulserConnected) {
        if (port.isEmpty()) {
            emit warning("Pulser", "No serial port selected.");
        } else {
            emit requestOpen(port);
        }
    }
    if (!m_oscConnected) osc.start();
    if (!m_pmConnected)  pm.start();
}

void Controller::onPulserConnected(bool success)
{
    if (success) {
        m_pulserConnected = true;
        emit pulserConnectedChanged();
        emit allConnectedChanged();
    } else {
        emit warning("Pulser",
            "Failed to connect to laser pulser.\nCheck the USB-serial connection and try again.");
    }
}

void Controller::onOscConnected(bool success)
{
    if (success) {
        m_oscConnected = true;
        emit oscConnectedChanged();
        emit allConnectedChanged();
    } else {
        emit warning("Oscilloscope",
            "Failed to connect to PicoScope.\nCheck the USB connection and try again.");
    }
}

void Controller::onPmConnected(bool success)
{
    if (success) {
        m_pmConnected = true;
        emit pmConnectedChanged();
        emit allConnectedChanged();
    } else {
        emit warning("Powermeter",
            "Failed to connect to PM100A.\nCheck the USB connection and try again.");
    }
}

// ── Live readings (queued from worker threads) ───────────────────────────────────

void Controller::read_values(double current_V)
{
    latest_current = current_V;
    m_currentValue = current_V;
    emit currentChanged();
}

void Controller::read_power(double watts)
{
    latest_power_W = watts;
    m_powerMW = watts * 1000.0;
    emit powerChanged();
}

// ── Laser manual control ─────────────────────────────────────────────────────────

void Controller::setLaser(bool on)
{
    if (on) emit requestLaserOn();
    else    emit requestLaserOff();
    setLaserState(on);
}

void Controller::setLaserState(bool on)
{
    if (m_laserOn != on) {
        m_laserOn = on;
        emit laserOnChanged();
    }
}

// ── Measurement sweep ────────────────────────────────────────────────────────────

void Controller::startOrPause(int nFrom, int nTo, int nStep, int b)
{
    if (!m_measuring && !m_paused) {
        startMeasurement(nFrom, nTo, nStep, b);
    } else if (m_measuring) {
        // Pause: freeze the timer but keep the laser on and HV unchanged
        measureTimer.stop();
        setMeasuring(false);
        setPaused(true);
    } else {
        // Resume from pause
        measureTimer.start(1000);
        setMeasuring(true);
        setPaused(false);
    }
}

void Controller::startMeasurement(int nFrom, int nTo, int nStep, int b)
{
    if (nFrom >= nTo) {
        emit warning("Invalid range", "HV 'from' must be less than 'to'.");
        return;
    }
    if (nStep >= (nTo - nFrom)) {
        emit warning("Invalid range", "Step must be smaller than the range (to − from).");
        return;
    }

    clearData();

    m_nTo   = nTo;
    m_nStep = nStep;
    m_b     = b;
    sweep_n = nFrom;

    emit requestSetHV(sweep_n, m_b);
    m_hvLabel = QString("HV: n=%1, b=%2").arg(sweep_n).arg(m_b);
    emit hvLabelChanged();

    emit requestLaserOn();
    setLaserState(true);
    setMeasuring(true);
    setPaused(false);

    // First timer fire is the first measurement (1 s settling time for the laser)
    measureTimer.start(1000);
}

void Controller::stop()
{
    measureTimer.stop();
    setMeasuring(false);
    setPaused(false);
    emit requestLaserOff();
    setLaserState(false);
    m_hvLabel = "HV: —";
    emit hvLabelChanged();
    qInfo() << "Measurement stopped," << pointCount() << "points collected";
}

void Controller::onMeasurementStep()
{
    if (!std::isfinite(latest_current) || !std::isfinite(latest_power_W))
        return;

    double x = latest_current;
    double y = latest_power_W * 1000.0;  // W → mW

    DATA[0].push_back(x);
    DATA[1].push_back(y);
    emit pointAdded(x, y);
    emit pointCountChanged();

    // Advance to next HV step
    sweep_n += m_nStep;

    if (sweep_n > m_nTo) {
        finishSweep();
        qInfo() << "Measurement complete," << pointCount() << "points";
    } else {
        emit requestSetHV(sweep_n, m_b);
        m_hvLabel = QString("HV: n=%1, b=%2").arg(sweep_n).arg(m_b);
        emit hvLabelChanged();
    }
}

void Controller::finishSweep()
{
    measureTimer.stop();
    setMeasuring(false);
    setPaused(false);
    emit requestLaserOff();
    setLaserState(false);
    m_hvLabel = "HV: —";
    emit hvLabelChanged();
}

void Controller::clearData()
{
    DATA[0].clear();
    DATA[1].clear();
    emit dataCleared();
    emit pointCountChanged();
}

// ── Data export ──────────────────────────────────────────────────────────────────

QString Controller::suggestedFileName() const
{
    return "IV-" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".csv";
}

bool Controller::saveData(const QUrl &fileUrl)
{
    if (DATA[0].isEmpty()) {
        emit warning("Save data", "No data to save.");
        return false;
    }

    QString fileName = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    if (fileName.isEmpty())
        return false;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit warning("Save data", "Cannot open file:\n" + file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << "# IP characterisation — " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# Points: " << DATA[0].size() << "\n";
    out << "Current_V,Power_mW\n";
    for (int i = 0; i < DATA[0].size(); ++i)
        out << DATA[0][i] << "," << DATA[1][i] << "\n";

    file.close();
    qInfo() << "Saved" << DATA[0].size() << "points to" << fileName;
    return true;
}

// ── Property setters with change notification ────────────────────────────────────

void Controller::setMeasuring(bool m)
{
    if (m_measuring != m) {
        m_measuring = m;
        emit measuringChanged();
    }
}

void Controller::setPaused(bool p)
{
    if (m_paused != p) {
        m_paused = p;
        emit pausedChanged();
    }
}
