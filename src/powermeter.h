#ifndef POWERMETER_H
#define POWERMETER_H

#include <QThread>
#include <QString>
#include <atomic>

// Controls a Thorlabs PM100A optical power meter via USBTMC (/dev/usbtmc0).
// Emits sendPower(watts) continuously after each MEAS:POW? query.
class Powermeter : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        if (openDevice())
            running();
    }

public:
    explicit Powermeter(const QString &device = "/dev/usbtmc0");
    void setWavelength(int nm);
    void setAveraging(int count);  // 1 sample ≈ 3 ms; e.g. 100 → ~300 ms per reading

public slots:
    void stop() { running_pm = false; }

signals:
    void sendPower(double watts);
    void connectionResult(bool success);

private:
    bool openDevice();
    void running();
    QString query(const QString &cmd);
    void sendCmd(const QString &cmd);

    QString device_path;
    int fd = -1;
    int wavelength_nm = 1550;   // default operating wavelength for photodiode correction
    int averaging_count = 100;  // hardware averaging: 100 samples × 3 ms ≈ 300 ms per reading
    std::atomic<bool> running_pm{false};
};

#endif // POWERMETER_H
