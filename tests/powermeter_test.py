#!/usr/bin/env python3
"""
Standalone test for the Thorlabs PM100A / PM101A power meter.

Two transports, same SCPI:
  * USBTMC (default) — /dev/usbtmc0, as used by src/powermeter.cpp
  * UART   (--uart)  — PM101 DA15 serial interface via a serial adapter

Verifies *IDN? starts with "Thorlabs", configures auto-range + Watt units +
wavelength + averaging, then reads a few power values. All commands are LF
('\\n') terminated per the PM101x manual.

USBTMC:
    python3 powermeter_test.py [--device /dev/usbtmc0] [--wl 1550] [--avg 100] [-n 5]

UART (PM101 DA15: Pin7 TxD, Pin8 RxD, Pin15 GND; RS232 by default, 8N1):
    python3 powermeter_test.py --uart /dev/ttyUSB0 [--baud 115200] [--wl 1550]

Exit codes: 0 = pass, 1 = fail, 2 = device absent.
"""
import argparse
import fcntl
import os
import sys
import time

GRN = "\033[1;32m"; RED = "\033[1;31m"; YEL = "\033[1;33m"; OFF = "\033[0m"
def ok(m):   print(f"{GRN}  ok:{OFF} {m}")
def bad(m):  print(f"{RED} fail:{OFF} {m}")
def note(m): print(f"{YEL}  ..:{OFF} {m}")

USBTMC_IOCTL_CLEAR = (0x5B << 8) | 2   # linux/usb/tmc.h: _IO(USBTMC_IOC_MAGIC=91, 2)


class TmcTransport:
    """SCPI over USBTMC (/dev/usbtmc0)."""
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR)

    def clear(self):
        try:
            fcntl.ioctl(self.fd, USBTMC_IOCTL_CLEAR)
        except OSError:
            pass

    def write(self, cmd):
        os.write(self.fd, (cmd + "\n").encode())

    def query(self, cmd, settle=0.3):
        self.write(cmd)
        time.sleep(settle)
        return os.read(self.fd, 256).decode(errors="replace").strip()

    def close(self):
        os.close(self.fd)


class SerialTransport:
    """SCPI over a serial/UART line (PM101 DA15 interface)."""
    def __init__(self, port, baud):
        import serial  # pyserial
        self.ser = serial.Serial(port, baud, bytesize=serial.EIGHTBITS,
                                parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE,
                                timeout=0.2)
        time.sleep(0.2)

    def clear(self):
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

    def write(self, cmd):
        self.ser.write((cmd + "\n").encode())
        self.ser.flush()

    def query(self, cmd, settle=0.6):
        self.ser.reset_input_buffer()
        self.write(cmd)
        resp = b""
        t0 = time.time()
        while time.time() - t0 < settle:
            chunk = self.ser.read(128)
            if chunk:
                resp += chunk
                if b"\n" in resp:
                    break
        return resp.decode(errors="replace").strip()

    def close(self):
        self.ser.close()


def run(dev, args):
    dev.clear()  # best effort — reset any stale state

    try:
        idn = dev.query("*IDN?")
    except OSError as e:
        bad(f"*IDN? failed: {e}")
        note("USBTMC control/read timeout usually means the device is hung or on a")
        note("marginal USB link. Try another data cable + a direct port, and a")
        note("30 s power cycle. Check: dmesg | grep usbtmc  (look for -110).")
        return 1

    if not idn:
        bad("no response to *IDN? (timeout)")
        if isinstance(dev, SerialTransport):
            note("check wiring (TxD<->RxD swapped?), baud rate, and that the DA15")
            note("switches match your adapter's levels (RS232 default vs TTL UART).")
        return 1
    if not idn.lower().startswith("thorlabs"):
        bad(f"unexpected *IDN?: {idn!r}")
        return 1
    ok(f"IDN: {idn}")

    # Configure exactly like the app does
    dev.write("*CLS")
    dev.write("*RST")
    time.sleep(0.2)
    dev.write("SENS:POW:RANG:AUTO ON")
    dev.write("SENS:POW:UNIT W")
    dev.write(f"SENS:CORR:WAV {args.wl}")
    dev.write(f"SENS:AVER:COUN {args.avg}")
    ok(f"configured: auto-range, W, wavelength={args.wl} nm, averaging={args.avg}")

    unit = dev.query("SENS:POW:UNIT?")
    ok(f"unit readback: {unit!r}")

    print(f"  reading {args.count} power samples:")
    good = 0
    for i in range(args.count):
        resp = dev.query("MEAS:POW?")
        try:
            watts = float(resp)
            print(f"    [{i+1}] {watts*1000:.4f} mW  ({watts:.3e} W)")
            good += 1
        except ValueError:
            print(f"    [{i+1}] bad response: {resp!r}")
        time.sleep(0.2)

    if good == 0:
        bad("no valid power readings")
        return 1
    print(f"{GRN}PASS{OFF}: power meter responds ({good}/{args.count} good reads)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="/dev/usbtmc0", help="USBTMC device node")
    ap.add_argument("--uart", metavar="PORT", help="use serial/UART instead, e.g. /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200, help="UART baud (default 115200)")
    ap.add_argument("--wl", type=int, default=1550, help="wavelength correction (nm)")
    ap.add_argument("--avg", type=int, default=100, help="hardware averaging count")
    ap.add_argument("-n", "--count", type=int, default=5, help="power readings to take")
    args = ap.parse_args()

    if args.uart:
        print(f"== Power meter test over UART on {args.uart} @ {args.baud} 8N1 ==")
        if not os.path.exists(args.uart):
            note(f"{args.uart} not present — serial adapter not plugged in / wired?")
            return 2
        try:
            dev = SerialTransport(args.uart, args.baud)
        except ImportError:
            bad("pyserial not installed  (pip install pyserial)")
            return 1
        except OSError as e:
            bad(f"cannot open {args.uart}: {e}")
            if "Permission" in str(e):
                note("not in 'dialout' group — sudo usermod -aG dialout $USER, then re-login")
            return 1
    else:
        print(f"== Power meter test over USBTMC on {args.device} ==")
        if not os.path.exists(args.device):
            note(f"{args.device} does not exist — meter plugged in / usbtmc bound?")
            return 2
        if not (os.access(args.device, os.R_OK) and os.access(args.device, os.W_OK)):
            bad(f"no read/write permission on {args.device}")
            note("add a udev rule (product 8076 for PM101A):")
            note('  KERNEL=="usbtmc[0-9]*", ATTRS{idVendor}=="1313", '
                 'ATTRS{idProduct}=="8076", MODE="0666"')
            return 1
        try:
            dev = TmcTransport(args.device)
        except OSError as e:
            bad(f"cannot open {args.device}: {e}")
            return 1

    try:
        return run(dev, args)
    finally:
        dev.close()


if __name__ == "__main__":
    sys.exit(main())
