#!/usr/bin/env python3
"""
Test for the Thorlabs PM100A / PM101A power meter via pyvisa + the pyvisa-py
backend (USBTMC over libusb).

This is the method that actually works for BOTH meters, including the PM101A —
the Linux kernel `usbtmc` driver (/dev/usbtmc0) fails on the PM101A (-110), but
talking to the USBTMC bulk endpoints directly through libusb (as the Thorlabs /
Windows driver does) works. It mirrors the SCPI the app uses.

Requires:
    pip install pyvisa pyvisa-py pyusb
    # raw USB access: run as root, or add a udev rule:
    #   SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", MODE="0666"

Usage:
    python3 powermeter_test.py [--resource VISA_RES] [--wl 1550] [--avg 100] [-n 5]
    python3 powermeter_test.py --list          # list all VISA resources

If a query times out on the PM101A, the kernel usbtmc driver may have grabbed and
wedged the device first — clear it with:  sudo modprobe -r usbtmc  then replug.

Exit codes: 0 = pass, 1 = fail, 2 = device absent.
"""
import argparse
import sys

GRN = "\033[1;32m"; RED = "\033[1;31m"; YEL = "\033[1;33m"; OFF = "\033[0m"
def ok(m):   print(f"{GRN}  ok:{OFF} {m}")
def bad(m):  print(f"{RED} fail:{OFF} {m}")
def note(m): print(f"{YEL}  ..:{OFF} {m}")

THORLABS_VID_DEC = "4883"   # pyvisa-py reports VID/PID in decimal; 4883 = 0x1313


def find_meter(rm):
    """Return the VISA resource string of a Thorlabs USB meter, or None."""
    for r in rm.list_resources():
        if r.startswith("USB") and THORLABS_VID_DEC in r:
            return r
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--resource", help="explicit VISA resource (else auto-detect Thorlabs USB)")
    ap.add_argument("--list", action="store_true", help="list VISA resources and exit")
    ap.add_argument("--wl", type=int, default=1550, help="wavelength correction (nm)")
    ap.add_argument("--avg", type=int, default=100, help="hardware averaging count")
    ap.add_argument("-n", "--count", type=int, default=5, help="power readings to take")
    args = ap.parse_args()

    print("== Power meter test (pyvisa-py / libusb USBTMC) ==")
    try:
        import pyvisa
        import pyvisa_py  # noqa: F401  (ensures the @py backend is available)
    except ImportError as e:
        bad(f"missing dependency: {e}")
        note("install with:  pip install pyvisa pyvisa-py pyusb")
        return 1

    rm = pyvisa.ResourceManager("@py")

    if args.list:
        print("VISA resources:")
        for r in rm.list_resources():
            print("   ", r)
        return 0

    res = args.resource or find_meter(rm)
    if not res:
        note("no Thorlabs (VID 0x1313) USB meter found — plugged in? try --list")
        return 2
    ok(f"resource: {res}")

    try:
        instr = rm.open_resource(res)
    except Exception as e:
        bad(f"cannot open {res}: {e}")
        note("permission? run as root or add the udev rule (see file header)")
        return 1

    instr.timeout = 3000
    instr.read_termination = "\n"
    instr.write_termination = "\n"

    try:
        idn = instr.query("*IDN?").strip()
    except Exception as e:
        bad(f"*IDN? failed: {e}")
        note("if this is the PM101A: sudo modprobe -r usbtmc, then replug and retry")
        instr.close(); return 1

    if not idn.lower().startswith("thorlabs"):
        bad(f"unexpected *IDN?: {idn!r}"); instr.close(); return 1
    ok(f"IDN: {idn}")

    # Configure exactly like the app
    instr.write("SENS:POW:RANG:AUTO ON")
    instr.write("SENS:POW:UNIT W")
    instr.write(f"SENS:CORR:WAV {args.wl}")
    instr.write(f"SENS:AVER:COUN {args.avg}")
    ok(f"configured: auto-range, W, wavelength={args.wl} nm, averaging={args.avg}")

    print(f"  reading {args.count} power samples:")
    good = 0
    for i in range(args.count):
        try:
            watts = float(instr.query("MEAS:POW?").strip())
            print(f"    [{i+1}] {watts*1000:.4f} mW  ({watts:.3e} W)")
            good += 1
        except Exception as e:
            print(f"    [{i+1}] error: {e}")

    instr.close()
    rm.close()
    if good == 0:
        bad("no valid power readings"); return 1
    print(f"{GRN}PASS{OFF}: power meter responds ({good}/{args.count} good reads)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
