#!/usr/bin/env python3
"""
Standalone test for the PicoScope 2000 (via libps2000.so, called with ctypes).

Opens the unit, prints its identity (driver / hardware / variant / serial), then
does one immediate (untriggered) block capture on Channel A and reports the
signal min/max/mean in millivolts — proving the ADC path works.

Usage:
    python3 picoscope_test.py [--lib /opt/picoscope/lib/libps2000.so] [--range 8]

    --range is a PS2000_RANGE index: 5=500mV 6=1V 7=2V 8=5V 9=10V 10=20V

Exit codes: 0 = pass, 1 = fail, 2 = device absent.
"""
import argparse
import ctypes
import sys
import time

GRN = "\033[1;32m"; RED = "\033[1;31m"; YEL = "\033[1;33m"; OFF = "\033[0m"
def ok(m):   print(f"{GRN}  ok:{OFF} {m}")
def bad(m):  print(f"{RED} fail:{OFF} {m}")
def note(m): print(f"{YEL}  ..:{OFF} {m}")

INFO_LINES = [
    (0, "Driver version"), (1, "USB version"), (2, "Hardware version"),
    (3, "Variant (model)"), (4, "Batch/serial"), (5, "Cal date"),
]
# PS2000_RANGE -> full-scale millivolts
RANGE_MV = {2: 50, 3: 100, 4: 200, 5: 500, 6: 1000, 7: 2000,
            8: 5000, 9: 10000, 10: 20000, 11: 50000}
PS2000_CHANNEL_A = 0
PS2000_CHANNEL_B = 1


def bind(lib):
    lib.ps2000_open_unit.restype = ctypes.c_int16
    lib.ps2000_close_unit.argtypes = [ctypes.c_int16]
    lib.ps2000_get_unit_info.restype = ctypes.c_int16
    lib.ps2000_get_unit_info.argtypes = [ctypes.c_int16, ctypes.c_char_p,
                                        ctypes.c_int16, ctypes.c_int16]
    lib.ps2000_set_channel.argtypes = [ctypes.c_int16] * 5
    lib.ps2000_run_block.argtypes = [ctypes.c_int16, ctypes.c_int32, ctypes.c_int16,
                                    ctypes.c_int16, ctypes.POINTER(ctypes.c_int32)]
    lib.ps2000_ready.restype = ctypes.c_int16
    lib.ps2000_ready.argtypes = [ctypes.c_int16]
    lib.ps2000_get_values.restype = ctypes.c_int32
    lib.ps2000_get_values.argtypes = [
        ctypes.c_int16,
        ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_int16),
        ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_int16),
        ctypes.POINTER(ctypes.c_int16), ctypes.c_int32]
    lib.ps2000_stop.argtypes = [ctypes.c_int16]


def info(lib, handle, line):
    buf = ctypes.create_string_buffer(128)
    lib.ps2000_get_unit_info(handle, buf, 128, line)
    return buf.value.decode(errors="replace")


def capture(lib, handle, rng):
    N = 1024
    lib.ps2000_set_channel(handle, PS2000_CHANNEL_A, 1, 1, rng)  # A on, DC
    lib.ps2000_set_channel(handle, PS2000_CHANNEL_B, 0, 1, rng)  # B off
    tind = ctypes.c_int32(0)
    # timebase 8, oversample 1, no trigger set -> captures immediately
    if lib.ps2000_run_block(handle, N, 8, 1, ctypes.byref(tind)) == 0:
        return None
    t0 = time.time()
    while lib.ps2000_ready(handle) == 0:
        if time.time() - t0 > 2:
            return None
        time.sleep(0.001)
    bufA = (ctypes.c_int16 * N)()
    overflow = ctypes.c_int16(0)
    got = lib.ps2000_get_values(handle, bufA, None, None, None,
                                ctypes.byref(overflow), N)
    lib.ps2000_stop(handle)
    if got <= 0:
        return None
    scale = RANGE_MV.get(rng, 5000) / 32767.0
    vals = [v * scale for v in bufA[:got]]
    return min(vals), max(vals), sum(vals) / len(vals), got


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lib", default="/opt/picoscope/lib/libps2000.so")
    ap.add_argument("--range", type=int, default=8, help="PS2000_RANGE index (8 = 5V)")
    args = ap.parse_args()

    print("== PicoScope 2000 test ==")
    try:
        lib = ctypes.CDLL(args.lib)
    except OSError as e:
        bad(f"cannot load {args.lib}: {e}")
        return 1
    bind(lib)

    handle = lib.ps2000_open_unit()
    if handle == 0:
        note("ps2000_open_unit returned 0 — no PicoScope found (plugged in? powered?)")
        return 2
    if handle < 0:
        bad(f"ps2000_open_unit failed (handle={handle})")
        return 1
    ok(f"opened unit (handle={handle})")

    for line, name in INFO_LINES:
        print(f"    {name:16}: {info(lib, handle, line)}")

    print(f"  capturing 1024 samples on Ch-A (range index {args.range}):")
    res = capture(lib, handle, args.range)
    if res is None:
        bad("block capture failed")
        lib.ps2000_close_unit(handle)
        return 1
    lo, hi, mean, n = res
    ok(f"captured {n} samples: min={lo:.1f} mV  max={hi:.1f} mV  mean={mean:.1f} mV")
    note("(with no signal connected these sit near 0 mV — that's expected)")

    lib.ps2000_close_unit(handle)
    print(f"{GRN}PASS{OFF}: PicoScope opens and acquires")
    return 0


if __name__ == "__main__":
    sys.exit(main())
