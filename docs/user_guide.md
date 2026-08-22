# User Guide — IP_char

Software for measuring and plotting **current vs. optical power (I-P)** curves of pulsed laser diodes. The application controls a laser driver, measures laser current with an oscilloscope, and reads optical power with a calibrated power meter — all simultaneously, with a live plot updated every second. Designed for a touchscreen kiosk with no physical keyboard: any text or number entry opens an on-screen keyboard or numpad instead.

---

## First-time setup

Run once per machine (installs udev rules, dialout group membership, and — on the Pi — the `uhubctl` permissions `run.sh` needs):

```bash
./build.sh deps
```

See [../README.md](../README.md) for the full dependency/build breakdown.

### Connect hardware

Before starting the software, connect all three instruments:

- **Laser pulser** → USB-to-serial adapter
- **PicoScope 2000** → USB
- **PM101A power meter** → USB

---

## Starting the application

```bash
./bin/WM          # dev box / manual run
./run.sh          # on the Pi kiosk — also power-cycles the meter if it's wedged
./bin/WM -sim     # simulation mode, no hardware needed (also: ./run.sh -sim)
```

### Connect devices

Select the pulser's serial port from the dropdown (use **↺** to refresh the list if it was just plugged in), then click **Connect**. One click attempts all three devices at once; if one fails (e.g. wrong port), fix it and click **Connect** again — it only retries the devices that aren't connected yet.

| Indicator | Meaning |
|-----------|---------|
| **Pulser** pill | Serial port opened; pulser set to 150 ns pulses at 10 kHz, laser OFF |
| **Osc** pill | PicoScope 2000 opened, triggered acquisition configured on Channel A |
| **PM** pill | PM101A opened over USBTMC, auto-ranging enabled |

Each pill turns green as its device connects. The **Start** button in the right-hand control panel enables once all three are green.

---

## Running a measurement

### Set the sweep range

The **from** / **to** / **step** fields under "HV sweep (n)" use big −/+ buttons for fine adjustment — tap the number itself to open an on-screen numeric keypad and type an exact value instead.

| Control | Meaning |
|---------|---------|
| **from** | Starting HV DAC value (1–4095). Higher value = higher laser current. |
| **to** | Final HV DAC value (1–4095). Must be greater than *from*. |
| **step** | Step size between measurements. Must be smaller than (*to* − *from*). |
| **b** | Voltage multiplier: **×1** = up to ~12.5 V; **×2** = up to ~25 V |

The actual high voltage applied is:

```
U_HV = 25 × (n / 4096) × 2.048 × b
```

Examples: `n=1000, b=1` → 12.5 V; `n=2000, b=2` → 25 V

### Run the sweep

1. Press **Start** — the laser turns ON at `from`.
2. Every **1 second**, the software:
   - Takes one current reading from the oscilloscope
   - Takes one power reading from the PM101A
   - Plots the point on the I-P graph
   - Advances the HV by `step`
3. Once HV passes `to`, the laser turns OFF automatically and **Start** re-enables.

### Controls during a sweep

| Button | Effect |
|--------|--------|
| **Start** → **Pause** | Click while running to pause the timer. The laser and HV stay at the current value. |
| **Pause** → **Resume** | Click again to continue from where it paused. |
| **Stop** | Aborts the sweep and turns the laser OFF. |

The **Laser ON** checkbox controls the laser independently of any sweep — useful for manual checks or keeping the laser warm between sweeps.

**Clear data** (top bar) wipes the current plot/points without touching the sweep settings.

---

## Saving data

Click **Save data**. A dialog opens with the filename pre-filled (`IV-<timestamp>.csv`) and an on-screen keyboard — tap the field to edit the name, then **Enter** or **Save**. The file is written as CSV into the app's working directory (the CSV is comma-separated, not tab-separated):

```
# IP characterisation — 2026-05-17T14:32:01
# HV n_from=100 n_step=25 n_to=4095 b=1
# Points: 39
HV_n,Current_V,Power_mW
100,0.00443,1.234
150,0.00512,2.018
...
```

- **HV_n** — the DAC value at each step
- **Current_V** — oscilloscope voltage (divide by the current-sense gain to get amps)
- **Power_mW** — optical power from the PM101A in milliwatts

---

## Remote control from a PC

Instead of the touchscreen GUI, the Pi can run **headless** (no display at all) while a separate GUI on a Linux/Mac/Windows PC controls it remotely — same sweep controls, same live plot, but Save writes the CSV on the PC, not the Pi.

### One-time setup on the Pi

```bash
./build.sh deps          # if not already done
./bluetooth_setup.sh     # makes the Pi discoverable/pairable over Bluetooth,
                          # bridges paired PCs onto its own subnet, and sets
                          # wm_headless to auto-start on every boot
```

After this, `wm_headless` is already running and stays running — nothing to launch by hand. (`bluetooth_setup.sh` is Pi-side system/network configuration, built from the documented BlueZ NAP pattern but not verified against real hardware — if something doesn't come up, check `sudo systemctl status bt-agent bt-pan-nap wm-headless dnsmasq` on the Pi.)

Don't want Bluetooth? Skip `bluetooth_setup.sh` entirely — `wm_headless` listens on every network interface, so as long as the Pi and PC are on the same WiFi/Ethernet network, the PC just needs the Pi's regular IP address (see [Other WiFi during a measurement](#other-wifi-during-a-measurement) below for the tradeoff).

### Pairing the PC over Bluetooth

- **Linux**: Settings → Bluetooth → pair with the Pi. NetworkManager brings up the PAN connection automatically once paired.
- **Windows**: Settings → Bluetooth & devices → Add device → pair with the Pi, then right-click it in the Bluetooth device list → **Join a Personal Area Network**.
- **macOS**: System Settings → Bluetooth → pair with the Pi. It should then appear as a network interface in System Settings → Network.

Once paired, the Pi's Bluetooth PAN address is `10.42.0.1` (see `bluetooth_setup.sh`) unless that's been changed.

### Using pc_client

```bash
./pc_client/build/pc_client
```

1. Enter the Pi's address (Bluetooth PAN: `10.42.0.1`; WiFi/Ethernet: its regular IP) and port (`8765` by default), click **Connect**.
2. Once linked, pick the pulser's serial port from the dropdown (**↺** to refresh) and click **Connect devices** — same three status pills as the GUI app.
3. Sweep controls, readings, and the plot all work exactly as in the touchscreen app. **Save data** opens a normal file dialog and writes the CSV locally on the PC.

### Other WiFi during a measurement

If the Pi and PC talk over WiFi (Pi joined to your existing network, or a WiFi hotspot the Pi itself hosts), whether the PC can *also* use other WiFi at the same time depends on which:

- **Pi joins your existing WiFi/LAN**: no conflict — it's the same network the PC already has internet through.
- **Pi hosts its own WiFi hotspot**: most PCs have one WiFi radio, so connecting to the Pi's hotspot drops whatever other WiFi network you were on.

Bluetooth avoids this entirely — it's a separate radio from WiFi on both ends, so the PC's WiFi stays free for anything else during a measurement.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Port not in dropdown | Pulser not plugged in | Plug in, click **↺** |
| **Connect** fails for the pulser | Wrong port selected | Try other `/dev/ttyUSBx` entries |
| **Connect** fails for the PM101A | Kernel usbtmc driver grabbed it, or meter is wedged | Run `./build.sh deps` once (blacklists the kernel driver); on the Pi, use `./run.sh` instead of `./bin/WM` directly — it power-cycles the meter's hub port first |
| Power reads 0 or flat | Laser not firing, or wrong wavelength set | Check laser ON state; verify wavelength correction in source (`Powermeter::setWavelength`) |
| Current reads 0 | Trigger not catching pulses | Check Ch-A wiring; verify laser is actually pulsing |
| Start button stays disabled | One or more devices not connected | All three status pills (Pulser/Osc/PM) must be green |
| pc_client "Connect" fails / times out | `wm_headless` isn't running on the Pi, wrong address/port, or Bluetooth PAN isn't bridged | On the Pi: `sudo systemctl status wm-headless bt-pan-nap`. Confirm the address (`10.42.0.1` for Bluetooth PAN, or the Pi's normal IP for WiFi/Ethernet) |
