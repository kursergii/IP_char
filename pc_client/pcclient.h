#ifndef PCCLIENT_H
#define PCCLIENT_H

#include <QMainWindow>
#include <QVector>

class PlotWidget;
class Stepper;
class QTcpSocket;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QFrame;

// PC-side GUI client for wm_headless — the Raspberry Pi runs headless
// (see wmcore.h/headless_main.cpp) and this connects to it over TCP
// (Bluetooth PAN once that's wired up; a QTcpSocket either way) using the
// same text command protocol wm_headless's stdin takes. Mirrors WM's layout
// so the two feel like the same app, but talks to the hardware only through
// the socket — a normal keyboard/mouse desktop app, no virtual keyboard/
// numpad (those exist in WM specifically for the Pi's touchscreen).
class PcClient : public QMainWindow
{
    Q_OBJECT
public:
    explicit PcClient(QWidget *parent = nullptr);

private slots:
    void onLinkClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onReadyRead();

    void onRefreshPorts();
    void onConnectDevices();
    void onStartClicked();
    void onStop();
    void onLaserToggled(bool on);
    void onClear();
    void onSave();

private:
    void buildUi();
    void handleLine(const QString &line);
    void sendCmd(const QString &cmd);
    void setPill(QLabel *pill, bool on);
    void updateControls();
    void setLinked(bool linked);

    // ── Link (TCP to wm_headless) ─────────────────────────────────────────
    QTcpSocket  *sock        = nullptr;
    QByteArray   m_recvBuf;
    QLineEdit   *hostEdit    = nullptr;
    QSpinBox    *portSpin    = nullptr;
    QPushButton *linkBtn     = nullptr;
    QLabel      *linkPill    = nullptr;

    // ── Devices (on the Pi) ────────────────────────────────────────────────
    QComboBox   *devicePortCombo = nullptr;
    QPushButton *refreshBtn      = nullptr;
    QPushButton *devConnectBtn   = nullptr;
    QLabel      *pillPulser  = nullptr;
    QLabel      *pillOsc     = nullptr;
    QLabel      *pillPm      = nullptr;
    QPushButton *clearBtn    = nullptr;

    // ── Plot + control panel (mirrors WM) ──────────────────────────────────
    PlotWidget  *plot        = nullptr;
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

    int  m_bValue  = 1;
    bool m_running = false;
    bool m_paused  = false;

    // Local copy of collected points — Save writes this out on the PC, not
    // on the Pi, so the operator never has to touch the headless machine.
    QVector<double> dataCurrent;    // volts
    QVector<double> dataPowerAvg;   // mW
    QVector<double> dataPowerPeak;  // mW
    QVector<int>    dataHv;
};

#endif // PCCLIENT_H
