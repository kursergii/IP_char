# IP_char — Laser Diode I-P Characterisation Tool

A Qt6 desktop application for measuring and plotting laser diode **current vs. optical power (I-P)** characteristics. It controls a pulsed laser driver over serial, reads laser current via a PicoScope 2000 USB oscilloscope, and measures optical power with a Thorlabs PM100A power meter.

---

## Hardware

| Device | Connection | Purpose |
|--------|-----------|---------|
| LaserPulser4_1 | USB-serial (`/dev/ttyUSB0`, 9600 baud) | Sets HV current setpoint, controls laser on/off, sets pulse parameters |
| PicoScope 2000 | USB | Ch-A: laser current (triggered peak detection on negative pulse) |
| Thorlabs PM100A | USB (`/dev/usbtmc0`) | Optical power measurement via SCPI over USBTMC |

### Laser pulser protocol (ASCII, CR-terminated)

| Command | Description |
|---------|-------------|
| `HV:n,b` | Set high voltage DAC: `n` = 0–4095, `b` = multiplier (1=×1, 2=×2) |
| `P:w,p` | Pulse width `w` and period `p` in 50 ns steps |
| `MS:ws,ps` | Synchronous modulation: `ws` on-pulses per `ps` total |
| `ON` / `OFF` | Enable / disable laser driver |

**Default pulse settings on connect:** 150 ns width, 10 kHz effective rate (via `P:3,200` + `MS:1,10`).

### Oscilloscope wiring

- **Channel A** — current-sense output (negative pulse, ~150 ns); triggered on falling edge at −1000 ADU. Peak of the captured waveform = pulse amplitude.

### PM100A setup

- Auto-ranging enabled, units: Watts
- Wavelength correction: configurable via `Powermeter::setWavelength(nm)`
- Hardware averaging: 100 samples (~300 ms per reading)

---

## Software Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| Qt 6 | ≥ 6.2 | Widgets, SerialPort, PrintSupport |
| libps2000 | — | PicoScope 2000 driver, installed at `/opt/picoscope/` |
| QCustomPlot | 2.1.1 | Bundled in source |
| CMake | ≥ 3.16 | |
| GCC / Clang | C++17 | |

**Install Qt6 on Fedora:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qtserialport-devel
```

**Install libps2000** from [picotech.com/downloads/linux](https://www.picotech.com/downloads/linux) into `/opt/picoscope/`.

**PM100A USB permissions** (run once):
```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", ATTRS{idProduct}=="8079", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-thorlabs-pm100a.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

## Build

```bash
git clone <repo>
cd IP_char
mkdir build && cd build
cmake ../src
cmake --build . -j$(nproc)
```

The binary is written to `bin/WM`.

### Build for Raspberry Pi (native)

```bash
# On the Pi (Raspberry Pi OS Bookworm 64-bit):
sudo apt install cmake g++ qt6-base-dev libqt6serialport6-dev

# Install libps2000 for ARM from PicoTech repo
curl -L "https://labs.picotech.com/debian/picotech.list" | sudo tee /etc/apt/sources.list.d/picotech.list
sudo apt update && sudo apt install libps2000

# Then build as above
```

---

## Running

```bash
./bin/WM
```

Connect devices in any order using the top buttons:

1. **connect pulser** — opens `/dev/ttyUSB0`, sets default pulse parameters, laser OFF
2. **connect osc** — opens PicoScope 2000, configures trigger
3. **connect PM** — opens `/dev/usbtmc0`, configures PM100A

The **Start** button enables automatically once all three devices are connected.

---

## Measurement

1. Set sweep range: **n from / step / to** and bank **b** (1 or 2)
2. Press **Start** — the sweep begins:
   - Laser turns ON, HV set to `n_from`
   - Every 1 second: snapshot current (oscilloscope) + power (PM100A) → plot point
   - HV advances by `step`; laser stays ON throughout
   - At `n_to`: laser turns OFF, Start button re-enables
3. The I-P curve is plotted live (current on X-axis, power in mW on Y-axis)

**Validation:** `n_from` must be less than `n_to`, and `step` must be smaller than the range.

### Laser checkbox

The **laser ON** checkbox controls the laser driver independently of the sweep. Useful for manual operation or keeping the laser warm between sweeps.

---

## Project Structure

```
src/
  main.cpp            — Qt entry point
  wm.h / wm.cpp       — main window: UI logic, sweep control, device coordination
  oscilloscope.h/cpp  — Osc (QThread): continuous triggered peak acquisition via PicoScope
  powermeter.h/cpp    — Powermeter (QThread): continuous MEAS:POW? via PM100A USBTMC
  pulser.h/cpp        — Pulser (QObject, runs in pulserThread): serial control of laser driver
  wm.ui               — Qt Designer layout (1430 × 876)
  qcustomplot.h/cpp   — QCustomPlot 2.1.1 (bundled)
  CMakeLists.txt
bin/
  WM                  — compiled binary
```

### Threading model

| Component | Thread | Communication |
|-----------|--------|---------------|
| `WM` (UI) | Main thread | Qt signals/slots |
| `Osc` | `osc` thread | emits `sendData(A, B)` → `WM::read_values()` |
| `Powermeter` | `pm` thread | emits `sendPower(W)` → `WM::read_power()` |
| `Pulser` | `pulserThread` | receives signals from WM via queued connection |
