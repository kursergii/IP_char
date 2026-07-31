# Thorlabs PM100A Commands Reference

Device: Thorlabs PM100A Optical Power Meter | Interface: USBTMC (`/dev/usbtmc0`)

## Syntax

```
COMMAND\n
COMMAND PARAMETER\n
COMMAND:SUBCOMMAND PARAMETER\n
```

- Commands are terminated by `\n` (newline)
- SCPI tree structure: keywords separated by `:` (e.g. `SENS:POW:RANG:AUTO`)
- Upper-case letters indicate the abbreviated form; both short and long forms accepted
- Parameters separated from command by a space
- Query commands end with `?`
- Responses are plain ASCII strings terminated by `\n`
- **Caution:** do not send two queries without reading the first response

---

## Command Table

| Command | Parameters | Description |
|---------|------------|-------------|
| `*IDN?` | — | Read identification string (`THORLABS,MMM,SSS,X.X.X`) |
| `*RST` | — | Reset instrument to defaults |
| `*TST?` | — | Self-test; returns `0` on pass |
| `*CLS` | — | Clear all event registers and error queue |
| `*OPC` | — | Set OPC bit after all operations complete |
| `*OPC?` | — | Place `1` in output queue when all operations done |
| `MEAS:POW?` | — | Trigger and read one power measurement |
| `READ?` | — | Start new measurement and read result |
| `FETC?` | — | Read last measurement result |
| `CONF:POW` | — | Configure for power measurement mode |
| `SENS:AVER:COUN` | `<n>` | Set averaging count (1 sample ≈ 3 ms) |
| `SENS:AVER:COUN?` | — | Query averaging count |
| `SENS:CORR:WAV` | `<nm>` | Set operating wavelength for photodiode correction |
| `SENS:CORR:WAV?` | — | Query operating wavelength |
| `SENS:POW:RANG:AUTO` | `ON\|1\|OFF\|0` | Enable / disable auto-ranging |
| `SENS:POW:RANG:AUTO?` | — | Query auto-range state |
| `SENS:POW:RANG` | `<W>` | Set power range upper limit in watts |
| `SENS:POW:RANG?` | — | Query current power range |
| `SENS:POW:UNIT` | `W\|DBM` | Set power unit |
| `SENS:POW:UNIT?` | — | Query power unit |
| `SENS:POW:REF` | `<W>` | Set delta reference value in watts |
| `SENS:POW:REF:STAT` | `ON\|OFF` | Enable / disable delta mode |
| `SENS:CORR:LOSS:INP:MAGN` | `<dB>` | Set user attenuation factor |
| `SENS:IDN?` | — | Query connected sensor info (name, serial, type, flags) |
| `SYST:ERR?` | — | Read and clear latest error |
| `SYST:BEEP` | — | Issue audible beep |
| `SYST:BEEP:STAT` | `ON\|OFF` | Enable / disable beeper |
| `DISP:BRIT` | `<value>` | Set display brightness |

---

## Key Command Details

### `*IDN?` — Identification

```
*IDN?
```
Response: `THORLABS,PM100A,P1002516,2.3.0`
Fields: manufacturer, model, serial number, firmware version.

---

### `MEAS:POW?` — Power Measurement

Triggers a fresh measurement and returns the result in the configured unit (W or dBm).

```
MEAS:POW?
```
Response example: `1.23456E-03`

---

### `SENS:AVER:COUN` — Averaging

Each sample takes approximately 3 ms. Total measurement time = count × 3 ms.

```
SENS:AVER:COUN 100     → ~300 ms per reading
SENS:AVER:COUN?        → 100
```

---

### `SENS:CORR:WAV` — Wavelength Correction

Sets the wavelength used to apply the photodiode's responsivity correction.

```
SENS:CORR:WAV 1550     → set to 1550 nm
SENS:CORR:WAV?         → 1550
```

---

### `SENS:POW:RANG:AUTO` — Auto-ranging

```
SENS:POW:RANG:AUTO ON   → enable auto-range
SENS:POW:RANG:AUTO OFF  → fix range at current value
SENS:POW:RANG:AUTO?     → 1 (on) or 0 (off)
```

---

### `SENS:POW:UNIT` — Unit Selection

```
SENS:POW:UNIT W      → watts
SENS:POW:UNIT DBM    → dBm
SENS:POW:UNIT?       → W
```

---

## Initialisation Sequence (used in software)

```
*RST
SENS:POW:RANG:AUTO ON
SENS:POW:UNIT W
SENS:CORR:WAV <nm>
SENS:AVER:COUN <n>
```

---

## Serial / USB Settings

| Setting | Value |
|---------|-------|
| Interface | USBTMC (USB Test and Measurement Class) |
| Device node | `/dev/usbtmc0` |
| Terminator | `\n` (newline) |
| Permissions | requires udev rule (vendor `1313`, product `8079`) |

## udev Rule (one-time setup)

```bash
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1313", ATTRS{idProduct}=="8079", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-thorlabs-pm100a.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```
