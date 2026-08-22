#include "pcclient.h"
#include "plotwidget.h"
#include "stepper.h"

#include <QWidget>
#include <QFrame>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTcpSocket>
#include <QMap>

static QMap<QString, QString> parseKV(const QString &s)
{
    QMap<QString, QString> m;
    for (const QString &tok : s.split(' ', Qt::SkipEmptyParts)) {
        int eq = tok.indexOf('=');
        if (eq > 0)
            m[tok.left(eq)] = tok.mid(eq + 1);
    }
    return m;
}

PcClient::PcClient(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("WM Remote — I-P Characterisation (PC client)");
    buildUi();

    sock = new QTcpSocket(this);
    connect(sock, &QTcpSocket::connected,    this, &PcClient::onSocketConnected);
    connect(sock, &QTcpSocket::disconnected, this, &PcClient::onSocketDisconnected);
    connect(sock, &QTcpSocket::errorOccurred, this, &PcClient::onSocketError);
    connect(sock, &QTcpSocket::readyRead,    this, &PcClient::onReadyRead);

    setLinked(false);
    resize(1024, 600);
    setMinimumSize(880, 480);
}

// ── UI construction (mirrors WM::buildUi) ────────────────────────────────────

void PcClient::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    // ── Link bar: TCP connection to wm_headless ────────────────────────────
    auto *linkRow = new QHBoxLayout;
    linkRow->setSpacing(8);
    linkRow->addWidget(new QLabel("Pi:"));
    hostEdit = new QLineEdit("raspberrypi.local");
    hostEdit->setMinimumWidth(180);
    linkRow->addWidget(hostEdit);
    linkRow->addWidget(new QLabel("Port:"));
    portSpin = new QSpinBox;
    portSpin->setRange(1, 65535);
    portSpin->setValue(8765);
    linkRow->addWidget(portSpin);
    linkBtn = new QPushButton("Connect");
    linkBtn->setFixedWidth(110);
    connect(linkBtn, &QPushButton::clicked, this, &PcClient::onLinkClicked);
    linkRow->addWidget(linkBtn);
    linkPill = new QLabel("Link");
    linkRow->addWidget(linkPill);
    linkRow->addStretch(1);
    outer->addLayout(linkRow);

    // ── Device bar: hardware on the Pi, once linked ────────────────────────
    auto *devRow = new QHBoxLayout;
    devRow->setSpacing(8);
    devicePortCombo = new QComboBox;
    devicePortCombo->setMinimumWidth(160);
    devRow->addWidget(devicePortCombo);
    refreshBtn = new QPushButton("↺");
    refreshBtn->setFixedWidth(40);
    refreshBtn->setToolTip("Refresh the Pi's serial port list");
    connect(refreshBtn, &QPushButton::clicked, this, &PcClient::onRefreshPorts);
    devRow->addWidget(refreshBtn);
    devConnectBtn = new QPushButton("Connect devices");
    connect(devConnectBtn, &QPushButton::clicked, this, &PcClient::onConnectDevices);
    devRow->addWidget(devConnectBtn);

    pillPulser = new QLabel("Pulser");
    pillOsc    = new QLabel("Osc");
    pillPm     = new QLabel("PM");
    for (QLabel *p : {pillPulser, pillOsc, pillPm}) { setPill(p, false); devRow->addWidget(p); }

    devRow->addStretch(1);
    clearBtn = new QPushButton("Clear data");
    connect(clearBtn, &QPushButton::clicked, this, &PcClient::onClear);
    devRow->addWidget(clearBtn);
    outer->addLayout(devRow);

    // ── Plot + control panel ────────────────────────────────────────────────
    auto *content = new QHBoxLayout;
    content->setSpacing(12);

    plot = new PlotWidget;
    plot->setAxisTitles("Current, V", "Power, mW");
    content->addWidget(plot, 1);

    panel = new QFrame;
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setFixedWidth(290);
    auto *pl = new QVBoxLayout(panel);
    pl->setContentsMargins(8, 8, 8, 8);
    pl->setSpacing(8);

    auto *runRow = new QHBoxLayout;
    runRow->setSpacing(10);
    startBtn = new QPushButton("Start");
    stopBtn  = new QPushButton("Stop");
    for (QPushButton *b : {startBtn, stopBtn}) {
        b->setMinimumHeight(44);
        QFont f = b->font(); f.setPointSize(15); f.setBold(true); b->setFont(f);
    }
    stopBtn->setEnabled(false);
    connect(startBtn, &QPushButton::clicked, this, &PcClient::onStartClicked);
    connect(stopBtn,  &QPushButton::clicked, this, &PcClient::onStop);
    runRow->addWidget(startBtn);
    runRow->addWidget(stopBtn);
    pl->addLayout(runRow);

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
    auto styleB = [this](QPushButton *sel, QPushButton *unsel) {
        sel->setStyleSheet("background:#1f77b4; color:white; font-weight:bold;");
        unsel->setStyleSheet("font-weight:bold;");
    };
    connect(bBtn1, &QPushButton::clicked, this, [this, styleB] { m_bValue = 1; styleB(bBtn1, bBtn2); });
    connect(bBtn2, &QPushButton::clicked, this, [this, styleB] { m_bValue = 2; styleB(bBtn2, bBtn1); });
    styleB(bBtn1, bBtn2);
    hvl->addLayout(bRow);
    pl->addWidget(hvBox);

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
    rg->addWidget(new QLabel("Current, V"),       0, 0);  rg->addWidget(currentVal,   0, 1);
    rg->addWidget(new QLabel("Power (avg), mW"),  1, 0);  rg->addWidget(powerVal,     1, 1);
    rg->addWidget(new QLabel("Power (peak), mW"), 2, 0);  rg->addWidget(peakPowerVal, 2, 1);
    rg->addWidget(new QLabel("HV"),               3, 0);  rg->addWidget(hvVal,        3, 1);
    pl->addWidget(rBox);

    laserCheck = new QCheckBox("Laser ON");
    { QFont f = laserCheck->font(); f.setBold(true); laserCheck->setFont(f); }
    connect(laserCheck, &QCheckBox::toggled, this, &PcClient::onLaserToggled);
    pl->addWidget(laserCheck);

    pl->addStretch(1);

    pointsLabel = new QLabel("0 points collected");
    pointsLabel->setAlignment(Qt::AlignHCenter);
    pointsLabel->setStyleSheet("color:#888;");
    pl->addWidget(pointsLabel);

    saveBtn = new QPushButton("Save data");
    saveBtn->setMinimumHeight(44);
    { QFont f = saveBtn->font(); f.setPointSize(14); f.setBold(true); saveBtn->setFont(f); }
    connect(saveBtn, &QPushButton::clicked, this, &PcClient::onSave);
    pl->addWidget(saveBtn);

    content->addWidget(panel);
    outer->addLayout(content, 1);

    setLinked(false);   // disables devRow/panel until the TCP link is up
    statusBar()->showMessage("Not connected to a Pi.");
}

void PcClient::setPill(QLabel *pill, bool on)
{
    pill->setStyleSheet(QString(
        "QLabel { background:%1; color:white; border-radius:13px;"
        " padding:3px 12px; font-weight:bold; }")
        .arg(on ? "#2e7d32" : "#9e9e9e"));
}

void PcClient::updateControls()
{
    startBtn->setText(m_running ? "Pause" : (m_paused ? "Resume" : "Start"));
    stopBtn->setEnabled(m_running || m_paused);
}

void PcClient::setLinked(bool linked)
{
    devicePortCombo->setEnabled(linked);
    refreshBtn->setEnabled(linked);
    devConnectBtn->setEnabled(linked);
    panel->setEnabled(linked);
    setPill(linkPill, linked);
    if (!linked) {
        setPill(pillPulser, false);
        setPill(pillOsc, false);
        setPill(pillPm, false);
        m_running = m_paused = false;
        updateControls();
    }
}

// ── TCP link ─────────────────────────────────────────────────────────────────

void PcClient::onLinkClicked()
{
    if (sock->state() == QAbstractSocket::UnconnectedState) {
        linkBtn->setEnabled(false);
        statusBar()->showMessage("Connecting to " + hostEdit->text() + "...");
        sock->connectToHost(hostEdit->text(), quint16(portSpin->value()));
    } else {
        sock->disconnectFromHost();
    }
}

void PcClient::onSocketConnected()
{
    linkBtn->setText("Disconnect");
    linkBtn->setEnabled(true);
    setLinked(true);
    statusBar()->showMessage("Linked to " + hostEdit->text(), 3000);
    sendCmd("status");
}

void PcClient::onSocketDisconnected()
{
    linkBtn->setText("Connect");
    linkBtn->setEnabled(true);
    setLinked(false);
    statusBar()->showMessage("Disconnected.");
}

void PcClient::onSocketError()
{
    linkBtn->setText("Connect");
    linkBtn->setEnabled(true);
    statusBar()->showMessage("Link error: " + sock->errorString(), 6000);
}

void PcClient::onReadyRead()
{
    m_recvBuf += sock->readAll();
    while (true) {
        int nl = m_recvBuf.indexOf('\n');
        if (nl < 0) break;
        QString line = QString::fromUtf8(m_recvBuf.left(nl)).trimmed();
        m_recvBuf.remove(0, nl + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void PcClient::sendCmd(const QString &cmd)
{
    if (sock->state() == QAbstractSocket::ConnectedState)
        sock->write((cmd + "\n").toUtf8());
}

// ── Protocol ─────────────────────────────────────────────────────────────────

void PcClient::handleLine(const QString &line)
{
    if (line.startsWith("READY")) {
        statusBar()->showMessage(line, 3000);
        sendCmd("ports");

    } else if (line.startsWith("PORTS")) {
        QString rest = line.mid(5).trimmed();
        QString cur = devicePortCombo->currentText();
        devicePortCombo->clear();
        if (!rest.isEmpty())
            devicePortCombo->addItems(rest.split(',', Qt::SkipEmptyParts));
        int idx = devicePortCombo->findText(cur);
        if (idx >= 0) devicePortCombo->setCurrentIndex(idx);

    } else if (line.startsWith("DEVICE ")) {
        QMap<QString, QString> kv = parseKV(line.mid(7));
        QString name = kv.value("name");
        bool ok = kv.value("ok") == "1";
        if      (name == "pulser") setPill(pillPulser, ok);
        else if (name == "osc")    setPill(pillOsc, ok);
        else if (name == "pm")     setPill(pillPm, ok);

    } else if (line.startsWith("POINT ")) {
        QMap<QString, QString> kv = parseKV(line.mid(6));
        double cur  = kv.value("current").toDouble();
        double avg  = kv.value("power_avg").toDouble();
        double peak = kv.value("power_peak").toDouble();
        int    hv   = kv.value("hv").toInt();

        dataCurrent   << cur;
        dataPowerAvg  << avg;
        dataPowerPeak << peak;
        dataHv        << hv;

        plot->addPoint(cur, avg);
        currentVal->setText(QString::number(cur, 'f', 4));
        powerVal->setText(QString::number(avg, 'f', 3));
        peakPowerVal->setText(QString::number(peak, 'f', 1));
        hvVal->setText(QString("n=%1, b=%2").arg(hv).arg(m_bValue));
        pointsLabel->setText(QString::number(dataCurrent.size()) + " points collected");

    } else if (line.startsWith("DONE ")) {
        m_running = false;
        m_paused  = false;
        updateControls();
        hvVal->setText("—");
        statusBar()->showMessage(line, 5000);

    } else if (line.startsWith("STATUS ")) {
        QMap<QString, QString> kv = parseKV(line.mid(7));
        setPill(pillPulser, kv.value("pulser") == "1");
        setPill(pillOsc,    kv.value("osc")    == "1");
        setPill(pillPm,     kv.value("pm")     == "1");
        m_running = kv.value("running") == "1";
        m_paused  = kv.value("paused")  == "1";
        updateControls();

    } else if (line.startsWith("LOG ") || line.startsWith("OK ") || line.startsWith("ERROR ")) {
        statusBar()->showMessage(line, 4000);
    }
}

// ── Commands ─────────────────────────────────────────────────────────────────

void PcClient::onRefreshPorts()   { sendCmd("ports"); }
void PcClient::onConnectDevices() { sendCmd("connect " + devicePortCombo->currentText()); }

void PcClient::onStartClicked()
{
    if (!m_running && !m_paused) {
        sendCmd(QString("start %1 %2 %3 %4")
            .arg(sFrom->value()).arg(sTo->value()).arg(sStep->value()).arg(m_bValue));
        dataCurrent.clear(); dataPowerAvg.clear(); dataPowerPeak.clear(); dataHv.clear();
        plot->clear();
        pointsLabel->setText("0 points collected");
        m_running = true;
    } else if (m_running) {
        sendCmd("pause");
        m_running = false;
        m_paused  = true;
    } else {
        sendCmd("resume");
        m_running = true;
        m_paused  = false;
    }
    updateControls();
}

void PcClient::onStop()
{
    sendCmd("stop");
    m_running = false;
    m_paused  = false;
    updateControls();
}

void PcClient::onLaserToggled(bool on)
{
    sendCmd(on ? "laser on" : "laser off");
}

void PcClient::onClear()
{
    if (!dataCurrent.isEmpty()) {
        auto reply = QMessageBox::question(this, "Clear data",
            "Discard " + QString::number(dataCurrent.size()) + " unsaved point(s)?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }
    dataCurrent.clear(); dataPowerAvg.clear(); dataPowerPeak.clear(); dataHv.clear();
    plot->clear();
    pointsLabel->setText("0 points collected");
    sendCmd("clear");
}

void PcClient::onSave()
{
    if (dataCurrent.isEmpty()) {
        QMessageBox::information(this, "Save data", "No data to save.");
        return;
    }

    QString defaultName = "IV-" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".csv";
    QString fileName = QFileDialog::getSaveFileName(this, "Save data", defaultName,
                                                     "CSV (*.csv);;All files (*)");
    if (fileName.isEmpty())
        return;
    if (!fileName.contains('.'))
        fileName += ".csv";

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save data", "Cannot open file:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out << "# IP characterisation — " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# HV n_from=" << sFrom->value() << " n_step=" << sStep->value()
        << " n_to=" << sTo->value() << " b=" << m_bValue << "\n";
    out << "# Points: " << dataCurrent.size() << "\n";
    out << "HV_n,Current_V,Power_avg_mW,Power_peak_mW\n";
    for (int i = 0; i < dataCurrent.size(); ++i) {
        out << dataHv[i] << "," << dataCurrent[i] << "," << dataPowerAvg[i]
            << "," << dataPowerPeak[i] << "\n";
    }
    file.close();
    statusBar()->showMessage("Saved " + QString::number(dataCurrent.size()) + " points to " + fileName, 4000);
}
