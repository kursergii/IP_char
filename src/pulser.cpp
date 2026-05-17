#include "pulser.h"
#include <QDebug>

Pulser::Pulser(const QString &port, QObject *parent)
    : QObject(parent),
      serial(new QSerialPort(this)),
      port_name(port)
{}

Pulser::~Pulser()
{
    close();
}

void Pulser::open(const QString &port)
{
    port_name = port;
    serial->setPortName(port_name);
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        qWarning() << "Pulser: cannot open" << port_name << serial->errorString();
        emit connectionResult(false);
        return;
    }

    qWarning() << "Pulser connected:" << send("?V");

    // Set default pulse parameters:
    //   P:3,200  → width=150 ns (3×50 ns), period=10 µs (200×50 ns) = 100 kHz base rate
    //   MS:1,10  → 1 of every 10 base pulses → effective 10 kHz
    setPulse(3, 200);
    setModulationSync(1, 10);
    qWarning() << "Pulser pulse:" << queryPulse();

    // Switch from potentiometer (b=0) to DAC control (b=1) at zero voltage
    setHV(0, 1);
    laserOff();
    emit connectionResult(true);
}

void Pulser::close()
{
    if (serial->isOpen()) {
        laserOff();
        serial->close();
    }
}

bool Pulser::isOpen() const
{
    return serial->isOpen();
}

void Pulser::laserOn()  { send("ON");  }
void Pulser::laserOff() { send("OFF"); }

void Pulser::setHV(int n, int b)
{
    // U_HV = 25 × (n/4096) × 2.048 × b  (b=1: max ~12.5 V, b=2: max ~25 V)
    send("HV:" + QString::number(n) + "," + QString::number(b));
}

void Pulser::setPulse(int width, int period)
{
    send("P:" + QString::number(width) + "," + QString::number(period));
}

void Pulser::setModulationSync(int ws, int ps)
{
    send("MS:" + QString::number(ws) + "," + QString::number(ps));
}

QString Pulser::queryHV()    { return send("?HV"); }
QString Pulser::queryPulse() { return send("?P");  }
QString Pulser::version()    { return send("?V");  }

void Pulser::saveSettings()
{
    send("$S");  // persists to EEPROM — max 100 000 writes, never call in a loop
}

QString Pulser::send(const QString &cmd)
{
    // Flush stale receive data before sending to prevent command/response misalignment
    serial->clear(QSerialPort::Input);

    QByteArray data = (cmd + "\r").toLocal8Bit();
    serial->write(data);
    // 50 ms is sufficient at 9600 baud: 12-char command ≈ 12 ms to transmit
    serial->waitForBytesWritten(50);

    QByteArray response;
    while (serial->waitForReadyRead(500)) {
        response += serial->readAll();
        if (response.contains('>'))
            break;
    }

    if (response.isEmpty()) {
        qWarning() << "Pulser: no response to:" << cmd;
        return {};
    }

    QString result = QString::fromLocal8Bit(response);

    // Strip echoed command — device echoes with \r\n or just \r
    int crPos = result.indexOf('\r');
    int lfPos = result.indexOf('\n');
    int stripTo = -1;
    if      (crPos >= 0 && lfPos >= 0) stripTo = qMax(crPos, lfPos);
    else if (crPos >= 0)               stripTo = crPos;
    else if (lfPos >= 0)               stripTo = lfPos;
    if (stripTo >= 0)
        result = result.mid(stripTo + 1);

    result.remove('>');
    result = result.trimmed();

    if (result == "Cmd?" || result == "No Param" || result == "Param?")
        qWarning() << "Pulser ERROR on" << cmd << ":" << result;

    return result;
}
