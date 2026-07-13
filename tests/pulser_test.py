#!/usr/bin/env python3
"""
Standalone test for the LaserPulser4_1 (serial, 9600 8N1, CR-terminated).

Mirrors the protocol used by src/pulser.cpp: flush input, send "<cmd>\\r",
read until the '>' prompt, strip the echoed command. Queries version and pulse
settings, switches to DAC control at 0 V, and leaves the laser OFF.

For safety this test does NOT fire the laser unless you pass --laser.

Usage:
    python3 pulser_test.py [--port /dev/ttyUSB0] [--hv N] [--laser]

Exit codes: 0 = pass, 1 = fail, 2 = device absent.
"""
import argparse
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("FAIL: pyserial not installed  (pip install pyserial)")
    sys.exit(1)

GRN = "\033[1;32m"; RED = "\033[1;31m"; YEL = "\033[1;33m"; OFF = "\033[0m"
def ok(m):   print(f"{GRN}  ok:{OFF} {m}")
def bad(m):  print(f"{RED} fail:{OFF} {m}")
def note(m): print(f"{YEL}  ..:{OFF} {m}")

ERRORS = {"Cmd?", "No Param", "Param?"}


def send(ser, cmd, settle=2.0):
    """Send a command and return the cleaned response (echo + prompt stripped)."""
    ser.reset_input_buffer()
    ser.write((cmd + "\r").encode())
    ser.flush()
    resp = b""
    t0 = time.time()
    while time.time() - t0 < settle:
        chunk = ser.read(64)
        if chunk:
            resp += chunk
            if b">" in resp:
                break
    text = resp.decode(errors="replace")
    # Strip the echoed command (device echoes up to a CR/LF)
    for sep in ("\r\n", "\r", "\n"):
        if sep in text:
            text = text.split(sep, 1)[1]
            break
    return text.replace(">", "").strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0")
    ap.add_argument("--hv", type=int, default=0, help="set HV DAC value n (b=1); laser stays OFF")
    ap.add_argument("--laser", action="store_true", help="briefly pulse laser ON then OFF")
    args = ap.parse_args()

    print(f"== Pulser test on {args.port} ==")
    import os
    if not os.path.exists(args.port):
        note(f"{args.port} not present — pulser/USB-serial adapter not plugged in?")
        return 2
    try:
        ser = serial.Serial(args.port, 9600, bytesize=serial.EIGHTBITS,
                            parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE,
                            timeout=0.2)
    except serial.SerialException as e:
        bad(f"cannot open {args.port}: {e}")
        if "Permission" in str(e):
            note("you are probably not in the 'dialout' group — run:")
            note("  sudo usermod -aG dialout $USER   (then log out and back in)")
        return 1

    time.sleep(0.2)  # let the adapter settle

    version = send(ser, "?V")
    if not version or version in ERRORS:
        bad(f"no valid response to ?V (got {version!r}) — wrong port or not a pulser?")
        ser.close()
        return 1
    ok(f"version:  {version}")

    pulse = send(ser, "?P")
    ok(f"pulse ?P: {pulse}")
    hvq = send(ser, "?HV")
    ok(f"HV ?HV:   {hvq}")

    # Switch to DAC control (b=1) at the requested (default 0) setpoint
    r = send(ser, f"HV:{args.hv},1")
    if r in ERRORS:
        bad(f"HV:{args.hv},1 rejected: {r}")
    else:
        ok(f"set HV:{args.hv},1  -> {r!r} (ack)")

    if args.laser:
        note("pulsing laser ON for 1 s (--laser given)")
        send(ser, "ON")
        time.sleep(1.0)
        send(ser, "OFF")
        ok("laser toggled ON->OFF")
    else:
        send(ser, "OFF")
        ok("laser left OFF")

    ser.close()
    print(f"{GRN}PASS{OFF}: pulser responds correctly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
