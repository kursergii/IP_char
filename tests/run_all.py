#!/usr/bin/env python3
"""
Run all device tests and print a summary.

Each test exits 0 = pass, 1 = fail, 2 = device absent. This runner reports a
one-line status per instrument and exits non-zero if any *present* device
failed (absent devices are reported but don't fail the run).

Usage:
    python3 run_all.py
"""
import os
import subprocess
import sys

GRN = "\033[1;32m"; RED = "\033[1;31m"; YEL = "\033[1;33m"; BLU = "\033[1;34m"; OFF = "\033[0m"
HERE = os.path.dirname(os.path.abspath(__file__))

TESTS = [
    ("Pulser",      "pulser_test.py"),
    ("PicoScope",   "picoscope_test.py"),
    ("Power meter", "powermeter_test.py"),
]
STATUS = {0: f"{GRN}PASS{OFF}", 1: f"{RED}FAIL{OFF}", 2: f"{YEL}ABSENT{OFF}"}


def main():
    results = {}
    for name, script in TESTS:
        print(f"\n{BLU}{'='*60}{OFF}\n{BLU}== {name}{OFF}")
        rc = subprocess.run([sys.executable, os.path.join(HERE, script)] + sys.argv[1:]).returncode
        results[name] = rc

    print(f"\n{BLU}{'='*60}{OFF}\n{BLU}== SUMMARY{OFF}")
    for name, _ in TESTS:
        print(f"  {name:14} {STATUS.get(results[name], '?')}")

    # Fail the overall run only if a present device failed
    return 1 if any(rc == 1 for rc in results.values()) else 0


if __name__ == "__main__":
    sys.exit(main())
