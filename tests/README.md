# Device tests

Standalone Python scripts to verify each instrument independently of the C++
app. Useful for bring-up, permission debugging, and checking a new sensor/cable.

Each script exits: **0 = pass, 1 = fail, 2 = device absent**.

## Requirements
- Python 3
- `pyserial` (for the pulser):  `pip install pyserial`  (or `sudo apt install python3-serial`)
- `libps2000.so` (for the PicoScope), default `/opt/picoscope/lib/libps2000.so`

## Run everything
```bash
python3 tests/run_all.py
```

## Run individually
```bash
python3 tests/pulser_test.py       --port /dev/ttyUSB0
python3 tests/picoscope_test.py    --range 8          # 8 = 5V range
python3 tests/powermeter_test.py   --device /dev/usbtmc0 --wl 1550 --avg 100 -n 5
```

## What each checks
| Script | Device | Verifies |
|--------|--------|----------|
| `pulser_test.py` | LaserPulser4_1 | serial open, `?V`/`?P`/`?HV`, DAC control, laser left OFF |
| `picoscope_test.py` | PicoScope 2000 | `libps2000` open, unit info, one Ch-A block capture (min/max/mean mV) |
| `powermeter_test.py` | PM100A / PM101A | USBTMC open, `*IDN?`, configure, `MEAS:POW?` reads |

## Notes
- `pulser_test.py` does **not** fire the laser unless you pass `--laser`.
- Permission errors are explained inline (dialout group for serial; udev rule
  `idProduct==8076` for the PM101A power meter).
- A USBTMC timeout in `powermeter_test.py` usually means a hung device or
  marginal USB link — try another data cable, a direct port, and a power cycle.
