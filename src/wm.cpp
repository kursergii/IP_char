#include "wm.h"
#include "ui_wm.h"
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <algorithm>
#include <cmath>

WM::WM(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WM)
{
    ui->setupUi(this);

    customPlot = ui->customPlot;
    customPlot->addGraph();
    customPlot->yAxis->setLabel("Power, mW");
    customPlot->xAxis->setLabel("Current, V");  // volts until current-sense calibration applied
    customPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    customPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCrossCircle, 6));
    DATA.resize(2);  // DATA[0] = current (V), DATA[1] = power (mW)

    connect(&measureTimer, &QTimer::timeout, this, &WM::onMeasurementStep);

    // Pulser runs in pulserThread — serial I/O never blocks the UI.
    // All command signals use QueuedConnection automatically (different threads).
    pulser.moveToThread(&pulserThread);
    connect(this,    &WM::requestOpen,            &pulser, &Pulser::open);
    connect(this,    &WM::requestLaserOn,         &pulser, &Pulser::laserOn);
    connect(this,    &WM::requestLaserOff,        &pulser, &Pulser::laserOff);
    connect(this,    &WM::requestSetHV,           &pulser, &Pulser::setHV);
    connect(&pulser, &Pulser::connectionResult,   this,    &WM::onPulserConnected);
    pulserThread.start();

    // Osc and Powermeter connections wired once here, not in button slots,
    // to prevent duplicate connections if buttons are clicked more than once.
    connect(&osc, &Osc::sendData,               this, &WM::read_values);
    connect(&osc, &Osc::connectionResult,        this, &WM::onOscConnected);
    connect(&pm,  &Powermeter::sendPower,        this, &WM::read_power);
    connect(&pm,  &Powermeter::connectionResult, this, &WM::onPmConnected);

    // Populate serial port list at startup; user can restart if port appears later
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->comboBox_port->addItem(info.portName());

    connects();
}

WM::~WM()
{
    // Laser off and serial closed from the pulser's own thread before quitting it
    QMetaObject::invokeMethod(&pulser, "close", Qt::BlockingQueuedConnection);
    pulserThread.quit();
    pulserThread.wait();

    osc.stop();
    osc.wait();
    pm.stop();
    pm.wait();
    delete ui;
}

void WM::connects() {
    connect(ui->pushButton_connect,     &QPushButton::clicked, this, &WM::connectAll);
    connect(ui->pushButton_start,       &QPushButton::clicked, this, &WM::onStartClicked);
    connect(ui->pushButton_stop,        &QPushButton::clicked, this, &WM::stopMeasurement);
    connect(ui->pushButton_save_data,      &QPushButton::clicked, this, &WM::save_data);
    connect(ui->pushButton_clear_data,     &QPushButton::clicked, this, &WM::clear_data);
    connect(ui->pushButton_refresh_ports,  &QPushButton::clicked, this, &WM::refreshPorts);
    connect(ui->checkBox_laser_on,         &QCheckBox::clicked,   this, &WM::laser_ON);
}

// ── Device connection ──────────────────────────────────────────────────────────

void WM::connectAll() {
    if (!pulser_connected) connectPulser();
    if (!osc_connected)    connectOscilloscope();
    if (!pm_connected)     connectPowermeter();
}

void WM::connectPulser() {
    if (ui->comboBox_port->currentText().isEmpty()) {
        QMessageBox::warning(this, "Pulser", "No serial port selected.");
        return;
    }
    emit requestOpen(ui->comboBox_port->currentText());
}

void WM::onPulserConnected(bool success) {
    if (success) {
        pulser_connected = true;
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Pulser",
            "Failed to connect to laser pulser.\nCheck the USB-serial connection and try again.");
    }
}

void WM::connectOscilloscope() {
    osc.start();
}

void WM::onOscConnected(bool success) {
    if (success) {
        osc_connected = true;
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Oscilloscope",
            "Failed to connect to PicoScope.\nCheck the USB connection and try again.");
    }
}

void WM::connectPowermeter() {
    pm.start();
}

void WM::onPmConnected(bool success) {
    if (success) {
        pm_connected = true;
        checkAllConnected();
    } else {
        QMessageBox::warning(this, "Powermeter",
            "Failed to connect to PM100A.\nCheck the USB connection and try again.");
    }
}

void WM::checkAllConnected() {
    if (pulser_connected && osc_connected && pm_connected) {
        ui->groupBox->setEnabled(true);
        ui->pushButton_connect->setEnabled(false);
        ui->pushButton_connect->setText("Connected");
    }
}

// ── Data receivers (called via queued connections from worker threads) ─────────

void WM::read_values(double current_V) {
    latest_current = current_V;
    ui->label_current->setText(QString::number(current_V));
}

void WM::read_power(double watts) {
    latest_power_W = watts;
    ui->label_power->setText(QString::number(watts * 1000, 'f', 3) + " mW");
}

// ── Laser manual control ───────────────────────────────────────────────────────

void WM::refreshPorts() {
    QString current = ui->comboBox_port->currentText();
    ui->comboBox_port->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ui->comboBox_port->addItem(info.portName());
    // Restore previous selection if it still exists
    int idx = ui->comboBox_port->findText(current);
    if (idx >= 0)
        ui->comboBox_port->setCurrentIndex(idx);
}

void WM::laser_ON() {
    if (ui->checkBox_laser_on->isChecked())
        emit requestLaserOn();
    else
        emit requestLaserOff();
}

// ── Measurement sweep ──────────────────────────────────────────────────────────

void WM::clearPlotData() {
    DATA[0].clear();
    DATA[1].clear();
    customPlot->graph(0)->data()->clear();
    customPlot->replot();

    // Reset incremental axis bounds
    xMin = xMax = yMin = yMax = 0;
}

void WM::onStartClicked() {
    if (!measureTimer.isActive() && !m_paused) {
        startMeasurement();
    } else if (measureTimer.isActive()) {
        // Pause: freeze the timer but keep the laser on and HV unchanged
        measureTimer.stop();
        m_paused = true;
        ui->pushButton_start->setText("Resume");
    } else {
        // Resume from pause
        measureTimer.start(1000);
        m_paused = false;
        ui->pushButton_start->setText("Pause");
    }
}

void WM::startMeasurement() {
    int n_from = ui->spinBox_HV_n_from->value();
    int n_to   = ui->spinBox_HV_n_to->value();
    int n_step = ui->spinBox_HV_n_step->value();

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
    emit requestSetHV(sweep_n, ui->spinBox_HV_b->value());
    ui->label_hv->setText(QString("HV: n=%1, b=%2").arg(sweep_n).arg(ui->spinBox_HV_b->value()));
    emit requestLaserOn();
    ui->checkBox_laser_on->setChecked(true);
    ui->pushButton_start->setText("Pause");
    ui->pushButton_stop->setEnabled(true);

    // First timer fire is the first measurement (1 s settling time for the laser)
    measureTimer.start(1000);
}

void WM::stopMeasurement() {
    measureTimer.stop();
    m_paused = false;
    emit requestLaserOff();
    ui->checkBox_laser_on->setChecked(false);
    ui->pushButton_start->setText("Start");
    ui->pushButton_stop->setEnabled(false);
    ui->label_hv->setText("HV: —");
    qInfo() << "Measurement stopped," << DATA[0].size() << "points collected";
}

void WM::onMeasurementStep() {
    if (!std::isfinite(latest_current) || !std::isfinite(latest_power_W))
        return;

    double x = latest_current;
    double y = latest_power_W * 1000.0;  // W → mW

    DATA[0].push_back(x);
    DATA[1].push_back(y);

    customPlot->graph(0)->setData(DATA[0], DATA[1]);

    // Update axis bounds incrementally — avoids O(n) scan on every step
    if (DATA[0].size() == 1) {
        xMin = xMax = x;
        yMin = yMax = y;
    } else {
        xMin = std::min(xMin, x);  xMax = std::max(xMax, x);
        yMin = std::min(yMin, y);  yMax = std::max(yMax, y);
    }
    if (DATA[0].size() >= 2) {
        // Ensure non-zero span to avoid QCustomPlot pixel overflow on flat data
        double xs = (xMax - xMin < 1e-9) ? 1.0 : 0.0;
        double ys = (yMax - yMin < 1e-9) ? 1.0 : 0.0;
        customPlot->xAxis->setRange(xMin - xs, xMax + xs);
        customPlot->yAxis->setRange(yMin - ys, yMax + ys);
    }
    customPlot->replot();

    // Advance to next HV step
    sweep_n += ui->spinBox_HV_n_step->value();

    if (sweep_n > ui->spinBox_HV_n_to->value()) {
        measureTimer.stop();
        m_paused = false;
        emit requestLaserOff();
        ui->checkBox_laser_on->setChecked(false);
        ui->pushButton_start->setText("Start");
        ui->pushButton_stop->setEnabled(false);
        ui->label_hv->setText("HV: —");
        qInfo() << "Measurement complete," << DATA[0].size() << "points";
    } else {
        emit requestSetHV(sweep_n, ui->spinBox_HV_b->value());
        ui->label_hv->setText(QString("HV: n=%1, b=%2").arg(sweep_n).arg(ui->spinBox_HV_b->value()));
    }
}

// ── Data export ────────────────────────────────────────────────────────────────

void WM::save_data() {
    if (DATA[0].isEmpty()) {
        QMessageBox::information(this, "Save data", "No data to save.");
        return;
    }

    QString defaultName = "IV-" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".csv";
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save data", defaultName, "CSV (*.csv);;Tab-separated (*.txt);;All files (*)");
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
    out << "# HV n_from=" << ui->spinBox_HV_n_from->value()
        << " n_step="     << ui->spinBox_HV_n_step->value()
        << " n_to="       << ui->spinBox_HV_n_to->value()
        << " b="          << ui->spinBox_HV_b->value() << "\n";
    out << "# Points: " << DATA[0].size() << "\n";
    out << "HV_n\tCurrent_V\tPower_mW\n";

    // n at measurement i = n_from + i × n_step (sweep always advances by fixed step)
    int n    = ui->spinBox_HV_n_from->value();
    int step = ui->spinBox_HV_n_step->value();
    for (int i = 0; i < DATA[0].size(); ++i) {
        out << n << "\t" << DATA[0][i] << "\t" << DATA[1][i] << "\n";
        n += step;
    }

    file.close();
    qInfo() << "Saved" << DATA[0].size() << "points to" << fileName;
}

void WM::clear_data() {
    clearPlotData();
}
