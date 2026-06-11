# Debug & Improvement Notes

_Last updated: 2026-06-11_

---

## 🔴 Must fix before release

**1. Current calibration factor missing**
`ret_A` from the oscilloscope is in **volts**, not amps. The x-axis label says "Current, V" but an I-P curve requires amps. The current-sense circuit conversion factor (`1/R_sense`) must be applied.
- Add a `QDoubleSpinBox` for calibration gain in the UI, or hardcode if R_sense is fixed.
- Apply: `ret_A = max_A * range_values[range_a] * calibration_gain`

**~~2. Oscilloscope trigger quality unverified~~** ✅ Fixed — diagnostic `qWarning` distribution-logging block removed from `run_osc()`.

**3. Ch-B still acquired (wasteful)**
`buffer_b`, `max_B`, `range_b` and Ch-B auto-ranging are still computed every cycle despite Ch-B being unused (PM100A handles power). Disable Ch-B and remove its processing — saves ~15% acquisition time per cycle.

---

## 🟡 Open — important

**~~4. Port list not refreshable~~** ✅ Fixed — `↺` refresh button added next to the comboBox.

**~~5. `check_val()` oscillation risk~~** ✅ Fixed — `check_val()` now bails out after `MAX_RERANGE_STEPS` (20) range adjustments instead of looping forever if the signal sits on a range boundary.

**6. PM100A wavelength not configurable from UI**
`wavelength_nm` defaults to 1550 nm and can only be changed by calling `setWavelength()` before `start()`. Add a `QSpinBox` so the user can set it without recompiling.

---

## 🟢 Open — nice to have

**~~7. No git repository~~** ✅ Fixed — initial commit `c7ed75f` on branch `master`.

**8. Raspberry Pi build not tested**
Native build on RPi OS Bookworm: install `qt6-base-dev`, `libqt6serialport6-dev`, and the PicoTech ARM package, then build as normal.

**9. PM100A averaging count not configurable from UI**
`averaging_count` defaults to 100 (~300 ms). A `QSpinBox` would let the user trade off noise vs. measurement speed.

**10. Save file column header says `Current_V`**
Once calibration (#1) is applied, rename to `Current_A` and use calibrated values.

**11. Osc shutdown can block up to ~5 s**
If the app is closed while `Osc::openDevice()` is still polling for the PicoScope (up to 5 s timeout), `osc.wait()` in `WM::~WM()` blocks until that polling finishes. Only affects "close immediately after clicking Connect".

**12. `src/wm_ui.py` is a stale artifact**
Tracked PySide6-generated file from an older 1515×969 layout with different widget names than the current `wm.ui` (e.g. `pushButton_open_com`, separate per-device connect buttons). Not used by the CMake build — safe to delete or regenerate.

---

## ✅ Fixed this session

- **`check_val()` oscillation guard** — added `MAX_RERANGE_STEPS` (20) limit (see #5 above).
- **PicoScope handle leak on shutdown** — `Osc::running()` now calls `ps2000_close_unit(handle)` after the acquisition loop exits, mirroring `Powermeter::running()`'s `::close(fd)`.
- **Wrong serial port silently "connected" as pulser** — `Pulser::open()` now requires a non-empty response to `?V` before reporting success; on timeout it closes the port and emits `connectionResult(false)`.
- **"clear data" button wired up** — `pushButton_clear_data` now calls `WM::clear_data()`, sharing a `clearPlotData()` helper with `startMeasurement()`.
- **Three connect buttons merged into one** — `pushButton_open_pulser/osc/pm` replaced by a single `pushButton_connect` → `connectAll()`, which retries only the not-yet-connected devices and shows a per-device warning dialog on failure.
