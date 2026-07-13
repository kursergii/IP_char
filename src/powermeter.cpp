#include "powermeter.h"

#include <libusb-1.0/libusb.h>
#include <QDebug>

Powermeter::Powermeter(uint16_t vendorId) : vendor_id(vendorId) {}

void Powermeter::setWavelength(int nm)   { wavelength_nm = nm; }      // call before start()
void Powermeter::setAveraging(int count) { averaging_count = count; } // call before start()

// ── Device open: find the Thorlabs USBTMC meter, claim it, configure ─────────────

bool Powermeter::openDevice()
{
    if (libusb_init(&ctx) != 0) {
        qWarning() << "Powermeter: libusb_init failed";
        emit connectionResult(false);
        return false;
    }

    // Find a device with our vendor id that exposes a USBTMC interface
    // (bInterfaceClass 0xFE = Application Specific, bInterfaceSubClass 3 = TMC).
    libusb_device **list = nullptr;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    libusb_device *target = nullptr;

    for (ssize_t i = 0; i < cnt; ++i) {
        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(list[i], &desc) != 0) continue;
        if (desc.idVendor != vendor_id) continue;

        libusb_config_descriptor *cfg = nullptr;
        if (libusb_get_active_config_descriptor(list[i], &cfg) != 0) continue;

        bool found = false;
        for (int ii = 0; ii < cfg->bNumInterfaces && !found; ++ii) {
            const libusb_interface &intf = cfg->interface[ii];
            for (int a = 0; a < intf.num_altsetting; ++a) {
                const libusb_interface_descriptor &id = intf.altsetting[a];
                if (id.bInterfaceClass != 0xFE || id.bInterfaceSubClass != 3)
                    continue;
                iface = id.bInterfaceNumber;
                ep_in = ep_out = 0;
                for (int e = 0; e < id.bNumEndpoints; ++e) {
                    const libusb_endpoint_descriptor &ep = id.endpoint[e];
                    if ((ep.bmAttributes & 0x03) != LIBUSB_TRANSFER_TYPE_BULK)
                        continue;
                    if (ep.bEndpointAddress & 0x80) {
                        ep_in = ep.bEndpointAddress;
                        max_pkt_in = ep.wMaxPacketSize;
                    } else {
                        ep_out = ep.bEndpointAddress;
                    }
                }
                found = true;
                break;
            }
        }
        libusb_free_config_descriptor(cfg);
        if (found && ep_in && ep_out) { target = list[i]; break; }
    }

    if (!target) {
        qWarning() << "Powermeter: no Thorlabs USBTMC device found (vendor"
                   << Qt::hex << vendor_id << ")";
        if (list) libusb_free_device_list(list, 1);
        emit connectionResult(false);
        return false;
    }

    int r = libusb_open(target, &handle);
    libusb_free_device_list(list, 1);
    if (r != 0) {
        qWarning() << "Powermeter: libusb_open failed:" << libusb_error_name(r)
                   << "(permission? add udev rule for vendor 1313)";
        emit connectionResult(false);
        return false;
    }

    // Take the interface away from the kernel usbtmc driver (which fails on the
    // PM101A) so we can drive the bulk endpoints ourselves.
    if (libusb_kernel_driver_active(handle, iface) == 1) {
        if (libusb_detach_kernel_driver(handle, iface) == 0)
            kernel_detached = true;
        else
            qWarning() << "Powermeter: could not detach kernel usbtmc driver";
    }
    if (libusb_claim_interface(handle, iface) != 0) {
        qWarning() << "Powermeter: libusb_claim_interface failed";
        emit connectionResult(false);
        return false;
    }

    QString idn = tmcQuery("*IDN?");
    if (!idn.startsWith("Thorlabs", Qt::CaseInsensitive)) {
        qWarning() << "Powermeter: unexpected IDN:" << idn;
        emit connectionResult(false);
        return false;
    }
    qWarning() << "Powermeter connected:" << idn;

    sendCmd("SENS:POW:RANG:AUTO ON");
    sendCmd("SENS:POW:UNIT W");
    sendCmd("SENS:CORR:WAV " + QString::number(wavelength_nm));
    sendCmd("SENS:AVER:COUN " + QString::number(averaging_count));

    emit connectionResult(true);
    return true;
}

void Powermeter::running()
{
    running_pm = true;
    while (running_pm) {
        QString resp = tmcQuery("MEAS:POW?");
        bool ok = false;
        double watts = resp.toDouble(&ok);
        if (ok)
            emit sendPower(watts);
        else
            qWarning() << "Powermeter: bad response:" << resp;
        QThread::msleep(500);
    }
}

void Powermeter::cleanup()
{
    if (handle) {
        libusb_release_interface(handle, iface);
        if (kernel_detached)
            libusb_attach_kernel_driver(handle, iface);
        libusb_close(handle);
        handle = nullptr;
    }
    if (ctx) {
        libusb_exit(ctx);
        ctx = nullptr;
    }
}

// ── USBTMC bulk protocol over libusb ─────────────────────────────────────────────
// Mirrors the sequence pyvisa-py uses (proven to work with the PM101A):
//   write = DEV_DEP_MSG_OUT bulk transfer (EOM set on the last packet)
//   read  = REQUEST_DEV_DEP_MSG_IN bulk-OUT, then a bulk-IN rounded up to a
//           multiple of the IN endpoint's wMaxPacketSize.

static void put_u32le(QByteArray &b, uint32_t v)
{
    b.append(char(v & 0xff));
    b.append(char((v >> 8) & 0xff));
    b.append(char((v >> 16) & 0xff));
    b.append(char((v >> 24) & 0xff));
}

bool Powermeter::tmcWrite(const QString &cmd)
{
    if (!handle) return false;
    QByteArray msg = cmd.toLatin1();
    msg += '\n';

    btag = (btag % 255) + 1;
    QByteArray pkt;
    pkt.append(char(1));              // MsgID = DEV_DEP_MSG_OUT
    pkt.append(char(btag));
    pkt.append(char(~btag));
    pkt.append(char(0));
    put_u32le(pkt, uint32_t(msg.size()));
    pkt.append(char(0x01));           // bmTransferAttributes: EOM
    pkt.append(char(0));
    pkt.append(char(0));
    pkt.append(char(0));
    pkt += msg;
    while (pkt.size() % 4)            // pad to a 4-byte boundary
        pkt.append(char(0));

    int transferred = 0;
    int r = libusb_bulk_transfer(handle, ep_out,
                                 reinterpret_cast<unsigned char *>(pkt.data()),
                                 int(pkt.size()), &transferred, 1000);
    if (r != 0)
        qWarning() << "Powermeter: bulk-out failed:" << libusb_error_name(r);
    return r == 0;
}

QString Powermeter::tmcQuery(const QString &cmd)
{
    if (!tmcWrite(cmd))
        return QString();

    const uint32_t maxlen = 1024;
    btag = (btag % 255) + 1;
    QByteArray req;
    req.append(char(2));              // MsgID = REQUEST_DEV_DEP_MSG_IN
    req.append(char(btag));
    req.append(char(~btag));
    req.append(char(0));
    put_u32le(req, maxlen);
    req.append(char(0));             // bmTransferAttributes: no term char
    req.append(char(0));             // TermChar
    req.append(char(0));
    req.append(char(0));

    int transferred = 0;
    if (libusb_bulk_transfer(handle, ep_out,
                             reinterpret_cast<unsigned char *>(req.data()),
                             int(req.size()), &transferred, 1000) != 0)
        return QString();

    // Round the read length up to a multiple of wMaxPacketSize (USBTMC 3.3).
    int chunk = ((int(maxlen) + 12) / max_pkt_in + 1) * max_pkt_in;
    QByteArray buf(chunk, char(0));
    int got = 0;
    int r = libusb_bulk_transfer(handle, ep_in,
                                 reinterpret_cast<unsigned char *>(buf.data()),
                                 chunk, &got, 2000);
    if (r != 0 || got < 12) {
        qWarning() << "Powermeter: bulk-in failed:" << libusb_error_name(r) << "got" << got;
        return QString();
    }

    uint32_t tsize = uint8_t(buf[4]) | (uint8_t(buf[5]) << 8) |
                     (uint8_t(buf[6]) << 16) | (uint32_t(uint8_t(buf[7])) << 24);
    if (tsize > uint32_t(got - 12))
        tsize = got - 12;
    return QString::fromLatin1(buf.mid(12, int(tsize))).trimmed();
}
