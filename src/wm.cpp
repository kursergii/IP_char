#include "wm.h"
#include "plotwidget.h"
#include "stepper.h"

#include <QWidget>
#include <QFrame>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include "savefiledialog.h"
#include <QSerialPortInfo>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

// ── Construction ────────────────────────────────────────────────────────────────

WM::WM(bool simulate, QWidget *parent)
    : QMainWindow(parent), m_sim(simulate)
{
    setWindowTitle(m_sim ? "WM — I-P Characterisation  [SIMULATION]"
                         : "WM — I-P Characterisation");
    if (m_sim)
        qInfo() << "WM: SIMULATION mode — no hardware will be accessed";

    DATA.resize(2);
    buildUi();

    connect(&measureTimer, &QTimer::timeout, this, &WM::onMeasurementStep);

    // Pulser runs in its own thread — serial I/O never blocks the UI.
    pulser.moveToThread(&pulserThread);
    connect(this,    &WM::requestOpen,           &pulser, &Pulser::open);
    connect(this,    &WM::requestLaserOn,        &pulser, &Pulser::laserOn);
    connect(this,    &WM::requestLaserOff,       &pulser, &Pulser::laserOff);
    connect(this,    &WM::requestSetHV,          &pulser, &Pulser::setHV);
    connect(&pulser, &Pulser::connectionResult,  this,    &WM::onPulserConnected);
    pulserThread.start();

    connect(&osc, &Osc::sendData,               this, &WM::read_values);
    connect(&osc, &Osc::connectionResult,        this, &WM::onOscConnected);
    connect(&pm,  &Powermeter::sendPower,        this, &WM::read_power);
    connect(&pm,  &Powermeter::connectionResult, this, &WM::onPmConnected);

    refreshPorts();
}

WM::~WM()
{
    QMetaObject::invokeMethod(&pulser, "close", Qt::BlockingQueuedConnection);
    pulserThread.quit();
    pulserThread.wait();
    osc.stop(); osc.wait();
    pm.stop();  pm.wait();
}

// ── UI construction (mirrors Main.qml) ──────────────────────────────────────────

void WM::setPill(QLabel *pill, bool on)
{
    pill->setStyleSheet(QString(
        "QLabel { background:%1; color:white; border-radius:13px;"
        " padding:3px 12px; font-weight:bold; }")
        .arg(on ? "#2e7d32" : "#9e9e9e"));
}

void WM::setBValue(int b)
{
    m_bValue = b;
    auto style = [](QPushButton *btn, bool sel) {
        btn->setStyleSheet(sel
            ? "background:#1f77b4; color:white; font-weight:bold;"
            : "font-weight:bold;");
    };
    style(bBtn1, b == 1);
    style(bBtn2, b == 2);
}

void WM::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    // ── Top bar ─────────────────────────────────────────────────────────────
    auto *top = new QHBoxLayout;
    top->setSpacing(8);

    connectBtn = new QPushButton("Connect");
    connectBtn->setFixedWidth(110);
    connect(connectBtn, &QPushButton::clicked, this, &WM::connectAll);
    top->addWidget(connectBtn);

    portCombo = new QComboBox;
    portCombo->setMinimumWidth(200);
    top->addWidget(portCombo);

    refreshBtn = new QPushButton("↺");
    refreshBtn->setFixedWidth(40);
    refreshBtn->setToolTip("Refresh serial port list");
    connect(refreshBtn, &QPushButton::clicked, this, &WM::refreshPorts);
    top->addWidget(refreshBtn);

    pillPulser = new QLabel("Pulser");
    pillOsc    = new QLabel("Osc");
    pillPm     = new QLabel("PM");
    for (QLabel *p : {pillPulser, pillOsc, pillPm}) { setPill(p, false); top->addWidget(p); }

    top->addStretch(1);

    clearBtn = new QPushButton("Clear data");
    connect(clearBtn, &QPushButton::clicked, this, &WM::clear_data);
    top->addWidget(clearBtn);

    outer->addLayout(top);

    // ── Plot + control panel ────────────────────────────────────────────────
    auto *content = new QHBoxLayout;
    content->setSpacing(12);

    plot = new PlotWidget;
    plot->setAxisTitles("Current, V", "Power, mW");
    content->addWidget(plot, 1);

    panel = new QFrame;
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setFixedWidth(290);
    panel->setEnabled(false);   // until all devices connected
    auto *pl = new QVBoxLayout(panel);
    pl->setContentsMargins(8, 8, 8, 8);
    pl->setSpacing(8);

    // Run / Stop
    auto *runRow = new QHBoxLayout;
    runRow->setSpacing(10);
    startBtn = new QPushButton("Start");
    stopBtn  = new QPushButton("Stop");
    for (QPushButton *b : {startBtn, stopBtn}) {
        b->setMinimumHeight(44);
        QFont f = b->font(); f.setPointSize(15); f.setBold(true); b->setFont(f);
    }
    stopBtn->setEnabled(false);
    connect(startBtn, &QPushButton::clicked, this, &WM::onStartClicked);
    connect(stopBtn,  &QPushButton::clicked, this, &WM::stopMeasurement);
    runRow->addWidget(startBtn);
    runRow->addWidget(stopBtn);
    pl->addLayout(runRow);

    // HV sweep
    auto *hvBox = new QGroupBox("HV sweep (n)");
    auto *hvl = new QVBoxLayout(hvBox);
    hvl->setContentsMargins(8, 6, 8, 6);
    hvl->setSpacing(6);
    sFrom = new Stepper("from", 1, 4095, 25, 1);
    sTo   = new Stepper("to",   1, 4095, 25, 4095);
    sStep = new Stepper("step", 1, 500,  5,  100);
    hvl->addWidget(sFrom);
    hvl->addWidget(sTo);
    hvl->addWidget(sStep);

    auto *bRow = new QHBoxLayout;
    bRow->setSpacing(6);
    auto *bLbl = new QLabel("b");
    bLbl->setMinimumWidth(50);
    { QFont f = bLbl->font(); f.setBold(true); bLbl->setFont(f); }
    bRow->addWidget(bLbl);
    bBtn1 = new QPushButton("×1");
    bBtn2 = new QPushButton("×2");
    for (QPushButton *b : {bBtn1, bBtn2}) {
        b->setMinimumHeight(40);
        QFont f = b->font(); f.setPointSize(14); f.setBold(true); b->setFont(f);
        bRow->addWidget(b, 1);
    }
    connect(bBtn1, &QPushButton::clicked, this, [this] { setBValue(1); });
    connect(bBtn2, &QPushButton::clicked, this, [this] { setBValue(2); });
    hvl->addLayout(bRow);
    pl->addWidget(hvBox);

    // Readings
    auto *rBox = new QGroupBox("Readings");
    auto *rg = new QGridLayout(rBox);
    rg->setContentsMargins(8, 4, 8, 4);
    rg->setHorizontalSpacing(12);
    rg->setVerticalSpacing(4);
    auto bigVal = [](QLabel *l) {
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont f = l->font(); f.setPointSize(14); f.setBold(true); l->setFont(f);
    };
    currentVal   = new QLabel("0.0000");
    powerVal     = new QLabel("0.000");
    peakPowerVal = new QLabel("0.000");
    hvVal        = new QLabel("—");
    bigVal(currentVal); bigVal(powerVal); bigVal(peakPowerVal);
    { QFont f = hvVal->font(); f.setBold(true); hvVal->setFont(f);
      hvVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter); }
    rg->addWidget(new QLabel("Current, V"),      0, 0);  rg->addWidget(currentVal,   0, 1);
    rg->addWidget(new QLabel("Power (avg), mW"), 1, 0);  rg->addWidget(powerVal,     1, 1);
    rg->addWidget(new QLabel("Power (peak), mW"),2, 0);  rg->addWidget(peakPowerVal, 2, 1);
    rg->addWidget(new QLabel("HV"),              3, 0);  rg->addWidget(hvVal,        3, 1);
    pl->addWidget(rBox);

    laserCheck = new QCheckBox("Laser ON");
    { QFont f = laserCheck->font(); f.setBold(true); laserCheck->setFont(f); }
    connect(laserCheck, &QCheckBox::clicked, this, &WM::laser_ON);
    pl->addWidget(laserCheck);

    pl->addStretch(1);

    pointsLabel = new QLabel("0 points collected");
    pointsLabel->setAlignment(Qt::AlignHCenter);
    pointsLabel->setStyleSheet("color:#888;");
    pl->addWidget(pointsLabel);

    saveBtn = new QPushButton("Save data");
    saveBtn->setMinimumHeight(44);
    { QFont f = saveBtn->font(); f.setPointSize(14); f.setBold(true); saveBtn->setFont(f); }
    connect(saveBtn, &QPushButton::clicked, this, &WM::save_data);
    pl->addWidget(saveBtn);

    content->addWidget(panel);
    outer->addLayout(content, 1);

    setBValue(1);
    resize(1024, 568);
    setMinimumSize(880, 480);
}

// ── Device connection ───────────────────────────────────────────────────────────

void WM::connectAll()
{
    if (m_sim) {
        pulser_connected = osc_connected = pm_connected = true;
        setPill(pillPulser, true); setPill(pillOsc, true); setPill(pillPm, true);
        checkAllConnected();
        return;
    }
    if (!pulser_connected) connectPulser();
    if (!osc_connected)    connectOscilloscope();
    if (!pm_connected)     connectPowermeter();
}

void WM::connectPulser()
{
    if (portCombo->currentText().isEmpty()) {
        QMessageBox::warning(this, "Pulser", "No serial port selected.");
        return;
    }
    emit requestOpen(portCombo->currentText());
}

void WM::onPulserConnected(bool success)
{
    if (success) {
        pulser_connected = true;
        setPill(pillPulser, true);
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Pulser",
            "Failed to connect to laser pulser.\nCheck the USB-serial connection and try again.");
    }
}

void WM::connectOscilloscope() { osc.start(); }

void WM::onOscConnected(bool success)
{
    if (success) {
        osc_connected = true;
        setPill(pillOsc, true);
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Oscilloscope",
            "Failed to connect to PicoScope.\nCheck the USB connection and try again.");
    }
}

void WM::connectPowermeter() { pm.start(); }

void WM::onPmConnected(bool success)
{
    if (success) {
        pm_connected = true;
        setPill(pillPm, true);
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Powermeter",
            "Failed to connect to PM100A.\nCheck the USB connection and try again.");
    }
}

void WM::checkAllConnected()
{
    if (pulser_connected && osc_connected && pm_connected) {
        panel->setEnabled(true);
        connectBtn->setEnabled(false);
        connectBtn->setText("Connected");
    }
}

// ── Data receivers ──────────────────────────────────────────────────────────────

void WM::read_values(double current_V)
{
    latest_current = current_V;
    currentVal->setText(QString::number(current_V, 'f', 4));
    m_currentUpdateTimer.restart();
}

void WM::read_power(double watts)
{
    latest_power_W = watts;
    powerVal->setText(QString::number(watts * 1000, 'f', 3));
    // PM101A reads average power; the pulsed laser's actual peak power during
    // each 150 ns pulse is far higher — see kPulseDutyCycle.
    peakPowerVal->setText(QString::number(watts * 1000 / kPulseDutyCycle, 'f', 1));
    m_powerUpdateTimer.restart();
}

// ── Ports / laser ───────────────────────────────────────────────────────────────

void WM::refreshPorts()
{
    QString current = portCombo->currentText();
    portCombo->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        portCombo->addItem(info.portName());
    int idx = portCombo->findText(current);
    if (idx >= 0)
        portCombo->setCurrentIndex(idx);
}

void WM::laser_ON()
{
    if (m_sim) return;
    if (laserCheck->isChecked()) emit requestLaserOn();
    else                         emit requestLaserOff();
}

// ── Measurement sweep ───────────────────────────────────────────────────────────

void WM::updateControls()
{
    bool running = measureTimer.isActive();
    startBtn->setText(running ? "Pause" : (m_paused ? "Resume" : "Start"));
    stopBtn->setEnabled(running || m_paused);
}

void WM::clearPlotData()
{
    DATA[0].clear();
    DATA[1].clear();
    plot->clear();
    pointsLabel->setText("0 points collected");
}

void WM::onStartClicked()
{
    if (!measureTimer.isActive() && !m_paused) {
        startMeasurement();
    } else if (measureTimer.isActive()) {
        measureTimer.stop();
        m_paused = true;
    } else {
        measureTimer.start(m_sim ? 250 : 1000);
        m_paused = false;
    }
    updateControls();
}

void WM::startMeasurement()
{
    int n_from = sFrom->value();
    int n_to   = sTo->value();
    int n_step = sStep->value();

    if (n_from >= n_to) {
        QMessageBox::warning(this, "Invalid range", "HV 'from' must be less than 'to'.");
        return;
    }
    if (n_step >= (n_to - n_from)) {
        QMessageBox::warning(this, "Invalid range", "Step must be smaller than the range (to − from).");
        return;
    }

    clearPlotData();

    sweep_n = n_from;
    m_currentUpdateTimer.start();
    m_powerUpdateTimer.start();
    applyHV(sweep_n, m_bValue);
    if (!m_sim) emit requestLaserOn();
    laserCheck->setChecked(true);

    // Real hardware needs the full 1 s for the PM101A's averaged reading to
    // settle; simulation has no such constraint, so it runs faster for UI testing.
    measureTimer.start(m_sim ? 250 : 1000);
    updateControls();
}

void WM::stopMeasurement()
{
    measureTimer.stop();
    m_paused = false;
    if (!m_sim) emit requestLaserOff();
    laserCheck->setChecked(false);
    hvVal->setText("—");
    updateControls();
    qInfo() << "Measurement stopped," << DATA[0].size() << "points collected";
}

void WM::onMeasurementStep()
{
    if (m_currentUpdateTimer.elapsed() > kStaleTimeoutMs ||
        m_powerUpdateTimer.elapsed() > kStaleTimeoutMs) {
        stopMeasurement();
        QMessageBox::warning(this, "Measurement stalled",
            QString("No new reading from the oscilloscope or power meter for %1 s "
                    "— sweep stopped so stale data isn't recorded as if it were fresh.\n\n"
                    "Check the USB connections, then Start again.")
                .arg(kStaleTimeoutMs / 1000));
        return;
    }

    if (!std::isfinite(latest_current) || !std::isfinite(latest_power_W))
        return;

    double x = latest_current;
    double y = latest_power_W * 1000.0;  // W → mW

    DATA[0].push_back(x);
    DATA[1].push_back(y);
    plot->addPoint(x, y);
    pointsLabel->setText(QString::number(DATA[0].size()) + " points collected");

    sweep_n += sStep->value();

    if (sweep_n > sTo->value()) {
        measureTimer.stop();
        m_paused = false;
        if (!m_sim) emit requestLaserOff();
        laserCheck->setChecked(false);
        hvVal->setText("—");
        updateControls();
        qInfo() << "Measurement complete," << DATA[0].size() << "points";
    } else {
        applyHV(sweep_n, m_bValue);
    }
}

// ── HV setpoint: hardware or simulation ─────────────────────────────────────────

void WM::applyHV(int n, int b)
{
    hvVal->setText(QString("n=%1, b=%2").arg(n).arg(b));
    if (m_sim) applySimHV(n, b);
    else       emit requestSetHV(n, b);
}

void WM::applySimHV(int n, int b)
{
    // Synthetic laser-diode L-I curve (threshold + slope) for UI testing.
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

// ── Data export ─────────────────────────────────────────────────────────────────

void WM::save_data()
{
    if (DATA[0].isEmpty()) {
        QMessageBox::information(this, "Save data", "No data to save.");
        return;
    }

    QString defaultName = "IV-" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".csv";
    QString usbDir = SaveFileDialog::findRemovableDrive();
    QString fileName = SaveFileDialog::getSaveFileName(this, "Save data", defaultName, usbDir);
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save data", "Cannot open file:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << "# IP characterisation — " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# HV n_from=" << sFrom->value()
        << " n_step="     << sStep->value()
        << " n_to="       << sTo->value()
        << " b="          << m_bValue << "\n";
    out << "# Points: " << DATA[0].size() << "\n";
    out << "HV_n,Current_V,Power_mW\n";

    int n    = sFrom->value();
    int step = sStep->value();
    for (int i = 0; i < DATA[0].size(); ++i) {
        out << n << "," << DATA[0][i] << "," << DATA[1][i] << "\n";
        n += step;
    }
    file.close();
    qInfo() << "Saved" << DATA[0].size() << "points to" << fileName;
}

void WM::clear_data()
{
    if (!DATA[0].isEmpty()) {
        auto reply = QMessageBox::question(this, "Clear data",
            "Discard " + QString::number(DATA[0].size()) + " unsaved point(s)?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }
    clearPlotData();
}
