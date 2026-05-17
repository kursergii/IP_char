#include "powermeter.h"
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>

Powermeter::Powermeter(const QString &device)
    : device_path(device)
{}

void Powermeter::setWavelength(int nm)
{
    // Must be called before start(); has no effect once the thread is running
    wavelength_nm = nm;
}

void Powermeter::setAveraging(int count)
{
    // Must be called before start(); has no effect once the thread is running.
    // Each sample takes ~3 ms: count=100 → ~300 ms per reading.
    averaging_count = count;
}

bool Powermeter::openDevice()
{
    fd = ::open(device_path.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        qWarning() << "Powermeter: cannot open" << device_path;
        emit connectionResult(false);
        return false;
    }

    // Verify we have a Thorlabs instrument before proceeding
    QString idn = query("*IDN?");
    if (!idn.startsWith("Thorlabs", Qt::CaseInsensitive)) {
        qWarning() << "Powermeter: unexpected IDN:" << idn.trimmed();
        ::close(fd);
        fd = -1;
        emit connectionResult(false);
        return false;
    }
    qWarning() << "Powermeter connected:" << idn.trimmed();

    // Configure device — order matters: RST first, then settings
    sendCmd("*RST");
    QThread::msleep(200);  // RST requires settling time before accepting commands
    sendCmd("SENS:POW:RANG:AUTO ON");
    sendCmd("SENS:POW:UNIT W");
    sendCmd("SENS:CORR:WAV " + QString::number(wavelength_nm));
    sendCmd("SENS:AVER:COUN " + QString::number(averaging_count));

    // Emit success only after full configuration so WM can safely start a sweep
    emit connectionResult(true);
    return true;
}

void Powermeter::running()
{
    running_pm = true;
    while (running_pm) {
        QString response = query("MEAS:POW?");
        bool ok;
        double watts = response.trimmed().toDouble(&ok);
        if (ok)
            emit sendPower(watts);
        else
            qWarning() << "Powermeter: bad response:" << response.trimmed();
        QThread::msleep(500);
    }
    ::close(fd);
    fd = -1;
}

QString Powermeter::query(const QString &cmd)
{
    sendCmd(cmd);
    char buf[128] = {};  // 128 bytes: enough for IDN (~50 chars) and power readings
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    if (n < 0)
        qWarning() << "Powermeter: read error for command:" << cmd;
    return QString::fromLocal8Bit(buf);
}

void Powermeter::sendCmd(const QString &cmd)
{
    QByteArray data = (cmd + "\n").toLocal8Bit();
    ssize_t written = ::write(fd, data.constData(), data.size());
    if (written != data.size())
        qWarning() << "Powermeter: write incomplete for command:" << cmd;
}
