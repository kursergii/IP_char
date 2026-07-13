#ifndef POWERMETER_H
#define POWERMETER_H

#include <QThread>
#include <QString>
#include <QByteArray>
#include <atomic>
#include <cstdint>

struct libusb_context;
struct libusb_device_handle;

// Controls a Thorlabs PM100A / PM101A optical power meter over USBTMC, spoken
// directly to the device's bulk endpoints via libusb (NOT the kernel usbtmc
// driver). The kernel usbtmc driver fails on the PM101A (-110); libusb — the
// same path the Thorlabs/Windows driver uses — works for both meters.
// Emits sendPower(watts) continuously after each MEAS:POW? query.
class Powermeter : public QThread
{
    Q_OBJECT
    void run() Q_DECL_OVERRIDE {
        if (openDevice())
            running();
        cleanup();
    }

public:
    // Matches any Thorlabs meter by vendor id (0x1313); first one found is used.
    explicit Powermeter(uint16_t vendorId = 0x1313);
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
    void cleanup();

    // USBTMC over libusb bulk transfers
    bool tmcWrite(const QString &cmd);
    QString tmcQuery(const QString &cmd);
    void sendCmd(const QString &cmd) { tmcWrite(cmd); }

    uint16_t vendor_id;
    libusb_context       *ctx    = nullptr;
    libusb_device_handle *handle = nullptr;
    int      iface       = 0;
    uint8_t  ep_out      = 0;
    uint8_t  ep_in       = 0;
    int      max_pkt_in  = 64;   // bulk-IN wMaxPacketSize (read rounding)
    uint8_t  btag        = 0;
    bool     kernel_detached = false;

    int wavelength_nm   = 1550;  // default operating wavelength for correction
    int averaging_count = 100;   // hardware averaging: 100 × 3 ms ≈ 300 ms/reading
    std::atomic<bool> running_pm{false};
};

#endif // POWERMETER_H
