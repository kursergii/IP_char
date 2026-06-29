# IP_char — Laser Diode I-P Characterisation Tool

A Qt6 **QML/Quick** application for measuring and plotting laser diode **current vs. optical power (I-P)** characteristics. It controls a pulsed laser driver over serial, reads laser current via a PicoScope 2000 USB oscilloscope, and measures optical power with a Thorlabs PM100A power meter. The I-P curve is plotted live with **QtGraphs**.

---

## Hardware

| Device | Connection | Purpose |
|--------|-----------|---------|
| LaserPulser4_1 | USB-serial (9600 baud, selectable port) | Sets HV current setpoint, controls laser on/off, sets pulse parameters |
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

- **Channel A** — current-sense output (negative pulse, ~150 ns); triggered on falling edge at −5000 ADU. Peak of the captured waveform is used as the pulse amplitude.

### PM100A setup

- Auto-ranging enabled, units: Watts
- Wavelength correction: 1550 nm default; configurable via `Powermeter::setWavelength(nm)` before `start()`
- Hardware averaging: 100 samples (~300 ms per reading), 500 ms pause between queries

---

## Software Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| Qt 6 | ≥ 6.5 | Core, Gui, Qml, Quick, QuickControls2, SerialPort |
| QtGraphs | ≥ 6.8 | 2D scatter plot (runtime QML plugin) |
| libps2000 | — | PicoScope 2000 driver, installed at `/opt/picoscope/` |
| CMake | ≥ 3.16 | |
| GCC / Clang | C++17 | |

**Install Qt6 on Fedora:**
```bash
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel \
    qt6-qtquickcontrols2-devel qt6-qtserialport-devel qt6-qtgraphs
```

**Install libps2000** from [picotech.com/downloads/linux](https://www.picotech.com/downloads/linux) into `/opt/picoscope/`.

**PM100A USB permissions** (run once, then replug the device):
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

The binary is written to `bin/WM`. To install:

```bash
cmake --install build   # copies binary to install/bin/, docs to install/share/doc/WM/
```

### Build for Raspberry Pi (native)

```bash
# On the Pi (Raspberry Pi OS Bookworm 64-bit):
sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev \
    libqt6serialport6-dev qml6-module-qtquick-controls qml6-module-qtgraphs

# Install libps2000 for ARM from PicoTech repo
curl -L "https://labs.picotech.com/debian/picotech.list" | sudo tee /etc/apt/sources.list.d/picotech.list
sudo apt update && sudo apt install libps2000

# Then build as above
```

---

## Running

```bash
./bin/WM          # normal mode (requires hardware)
./bin/WM -sim     # simulation mode — synthesises a laser L-I curve, no hardware needed
```

In **simulation mode** the title bar shows `[SIMULATION]`, **Connect** marks all
three devices connected instantly, and a sweep generates a realistic
threshold-plus-slope L-I curve (sped up to ~250 ms/point) so the UI, plotting and
CSV export can be exercised without any instruments attached.

Connect all devices at once from the top bar:

1. Select the pulser serial port from the dropdown (use **↺** to refresh if the device was just plugged in)
2. Click **Connect** — opens the pulser (sets default pulse parameters, laser OFF), the PicoScope 2000 (configures trigger), and the PM100A on `/dev/usbtmc0` in one action

Each device reports success independently; a warning dialog appears for any that fail (retry **Connect** to reattempt only the not-yet-connected ones). The control panel and **Start** button enable automatically once all three are connected.

---

## Measurement

1. Set sweep parameters: **n from / step / to** (HV DAC values 0–4095) and bank **b** (1 or 2)
2. Press **Start** — the sweep begins:
   - Laser turns ON, HV set to `n_from`
   - Every 1 second: snapshot current (oscilloscope) + power (PM100A) → plot one point
   - HV advances by `step`; laser stays ON throughout
   - At `n_to`: laser turns OFF, Start re-enables
3. The I-P curve is plotted live (current on X-axis, power in mW on Y-axis)

**Validation:** `n_from` must be less than `n_to`, and `step` must be smaller than the range.

### During a sweep

| Button | Action |
|--------|--------|
| **Start** (becomes **Pause**) | Pause the timer; laser and HV remain unchanged |
| **Pause** (becomes **Resume**) | Resume from where it stopped |
| **Stop** | Abort sweep, turn laser off |

### Laser checkbox

The **laser ON** checkbox controls the laser driver independently of the sweep — useful for manual checks or keeping the laser warm between sweeps.

---

## Saving data

Click **Save data** after a sweep. The file dialog pre-fills a timestamped name like `IV-2026-06-29_143201.csv`. The exported CSV has a comment header:

```
# IP characterisation — 2026-06-29T14:32:01
# Points: 39
Current_V,Power_mW
0.00443,1.234
...
```

---

## Project Structure

```
src/
  main.cpp                — Qt entry point (QQmlApplicationEngine, Fusion style)
  Main.qml                — QML UI: QtGraphs plot, controls, dialogs
  controller.h/cpp        — Controller (QObject): QML↔backend bridge, sweep control,
                            device coordination; exposes Q_PROPERTY / Q_INVOKABLE
  oscilloscope.h/cpp      — Osc (QThread): triggered peak acquisition via PicoScope
  powermeter.h/cpp        — Powermeter (QThread): MEAS:POW? loop via PM100A USBTMC
  pulser.h/cpp            — Pulser (QObject, pulserThread): serial control of laser driver
  CMakeLists.txt
  CMakePresets.json
bin/
  WM                      — compiled binary
build/                    — CMake build artifacts
install/                  — cmake --install output
```

### Threading model

| Component | Thread | Communication |
|-----------|--------|---------------|
| `Controller` + QML UI | Main thread | `Q_PROPERTY`/signals to QML; `pointAdded(x,y)` streams plot data |
| `Osc` | `osc` thread | emits `sendData(current_V)` → `Controller::read_values()` |
| `Powermeter` | `pm` thread | emits `sendPower(W)` → `Controller::read_power()` |
| `Pulser` | `pulserThread` | receives queued signals from Controller; never blocks the UI |
