# Debug & Improvement Notes

_Last updated: 2026-05-17_

---

## 🔴 Must fix before release

**1. Current calibration factor missing**
`ret_A` from the oscilloscope is in **volts**, not amps. The x-axis label says "Current, V" but an I-P curve requires amps. The current-sense circuit conversion factor (`1/R_sense`) must be applied.
- Add a `QDoubleSpinBox` for calibration gain in the UI, or hardcode if R_sense is fixed.
- Apply: `ret_A = max_A * range_values[range_a] * calibration_gain`

**2. Oscilloscope trigger quality unverified**
Distribution logging showed `stddev ≈ mean` (bimodal: real pulses mixed with noise/auto-trigger captures). Must confirm reliable triggering before release, then **remove the diagnostic `qWarning` block** from `run_osc()`.

**3. Ch-B still acquired (wasteful)**
`buffer_b`, `max_B`, `range_b` and Ch-B auto-ranging are still computed every cycle despite Ch-B being unused (PM100A handles power). Disable Ch-B and remove its processing — saves ~15% acquisition time per cycle.

---

## 🟡 Open — important

**~~4. Port list not refreshable~~** ✅ Fixed — `↺` refresh button added next to the comboBox.

**5. `check_val()` oscillation risk**
If the signal amplitude sits exactly at the 5 000 or 25 000 ADU ranging threshold, the auto-ranging loop alternates ±1 indefinitely. Add a maximum iteration count (e.g., 20) to guarantee termination.

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
