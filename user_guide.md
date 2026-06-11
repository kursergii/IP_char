# User Guide — IP_char

Software for measuring and plotting **current vs. optical power (I-P)** curves of pulsed laser diodes. The application controls a laser driver, measures laser current with an oscilloscope, and reads optical power with a calibrated power meter — all simultaneously, with a live plot updated every second.

---

## First-time setup

### 1. Permissions for the power meter (run once)

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", ATTRS{idProduct}=="8079", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-thorlabs-pm100a.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug and replug the PM100A.

### 2. Connect hardware

Before starting the software, connect all three instruments:

- **Laser pulser** → USB-to-serial adapter
- **PicoScope 2000** → USB
- **PM100A power meter** → USB

---

## Starting the application

```bash
./bin/WM
```

### Connect devices

Use the buttons in the top bar, in any order:

| Control | Action |
|---------|--------|
| Port dropdown + **↺** | Select the serial port for the pulser. Click **↺** to refresh the list if the device was just plugged in. |
| **connect pulser** | Opens the serial port. The pulser is configured to 150 ns pulses at 10 kHz and the laser is turned OFF. |
| **connect osc** | Opens the PicoScope 2000 and configures triggered acquisition on Channel A. |
| **connect PM** | Opens the PM100A and enables auto-ranging. |

The **Start** button in the right-hand control panel becomes available once all three devices are connected.

---

## Running a measurement

### Set the sweep range

| Control | Meaning |
|---------|---------|
| **n from** | Starting HV DAC value (0–4095). Higher value = higher laser current. |
| **n step** | Step size between measurements. |
| **n to** | Final HV DAC value. Must be greater than *n from*. |
| **b** | Voltage multiplier: **1** = up to ~12.5 V; **2** = up to ~25 V |

The actual high voltage applied is:

```
U_HV = 25 × (n / 4096) × 2.048 × b
```

Examples: `n=1000, b=1` → 12.5 V; `n=2000, b=2` → 25 V

### Run the sweep

1. Press **Start** — the laser turns ON at `n_from`.
2. Every **1 second**, the software:
   - Takes one current reading from the oscilloscope
   - Takes one power reading from the PM100A
   - Plots the point on the I-P graph
   - Advances the HV to the next step
3. At `n_to` the laser turns OFF automatically and the Start button re-enables.

### Controls during a sweep

| Button | Effect |
|--------|--------|
| **Pause** | Stops the timer. The laser and HV stay at the current value. |
| **Resume** | Continues from where it paused. |
| **Stop** | Aborts the sweep and turns the laser OFF. |

The **laser ON** checkbox controls the laser independently — useful for manual checks or keeping the laser warm between sweeps.

---

## Saving data

Click **Save data** at any point during or after a sweep. A file dialog opens to choose the location. The output is a tab-separated text file:

```
# IP characterisation — 2026-05-17T14:32:01
# HV n_from=100 n_step=50 n_to=2000 b=1
# Points: 39
HV_n    Current_V    Power_mW
100     0.00443      1.234
150     0.00512      2.018
...
```

- **HV_n** — the DAC value at each step
- **Current_V** — oscilloscope voltage (divide by the current-sense gain to get amps)
- **Power_mW** — optical power from the PM100A in milliwatts

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Port not in dropdown | Pulser not plugged in | Plug in, click **↺** |
| "Failed to connect" (pulser) | Wrong port selected | Try other `/dev/ttyUSBx` entries |
| "Failed to connect" (PM100A) | Missing udev rule or not plugged in | Run the permission command above and replug |
| Power reads 0 or flat | Laser not firing, or wrong wavelength set | Check laser ON state; verify wavelength correction in source |
| Current reads 0 | Trigger not catching pulses | Check Ch-A wiring; verify laser is actually pulsing |
| Start button stays disabled | One or more devices not connected | All three "connected" buttons must be active |
