#ifndef PULSER_H
#define PULSER_H

#include <QObject>
#include <QSerialPort>

// Controls the LaserPulser4_1 via serial port (9600 8N1).
// Runs in its own thread via moveToThread; all command methods are slots
// so they execute in the pulser thread rather than the caller's thread.
class Pulser : public QObject
{
    Q_OBJECT
public:
    explicit Pulser(const QString &port = "/dev/ttyUSB0", QObject *parent = nullptr);
    ~Pulser();

    bool isOpen() const;

    // Query commands — return raw response string
    QString queryHV();
    QString queryPulse();
    QString version();

public slots:
    void open(const QString &port);   // emits connectionResult(true/false) when done
    void close();
    void laserOn();
    void laserOff();

    // HV:n,b — set high voltage. n=0..4095, b=0(pot),1(×1),2(×2)
    void setHV(int n, int b);

    // P:w,p — pulse width and period in 50 ns steps. w=1..255, p=2..256, p>w
    void setPulse(int width, int period);

    // MS:ws,ps — synchronous modulation: ws on-pulses per ps total pulses.
    void setModulationSync(int ws, int ps);

    // Save all settings to EEPROM (max 100k writes — do not call in a loop)
    void saveSettings();

signals:
    void connectionResult(bool success);

private:
    QString send(const QString &cmd);

    QSerialPort *serial;
    QString port_name;
};

#endif // PULSER_H
