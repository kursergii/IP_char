# LaserPulser Commands Reference

Firmware: LaserPulser4_1 V1.1 | Date: 11.11.15

## Syntax

```
command<CR>
command:parameter,...,parameter<CR>
```

- Parameters separated from command name by colon `:`
- Multiple parameters separated by commas `,`
- Commands can be terminated by `<CR>` or semicolon `;`
- Case-insensitive; no spaces allowed
- Prompt `>` appears after each command completes
- Errors: `Cmd?` (unknown), `No Param` (missing parameter), `Param?` (out of range)

---

## Command Table

| Command | Parameters | Description |
|---------|------------|-------------|
| `@` | — | Immediate warm restart |
| `?` | — | List all commands |
| `?I` | — | Programmer address and e-mail |
| `?V` | — | Firmware version and date |
| `?BASCOM` | — | Compiler version used |
| `P` | `w, p` | Set pulse width and repetition rate |
| `?P` | — | Query pulse settings |
| `MS` | `ws, ps` | Internal modulation, synchronous (multiples of pulse period) |
| `MA` | `wa, pa` | Internal modulation, asynchronous (multiples of 50 ns) |
| `ME+` | — | Enable external modulation — High: pulse on |
| `ME-` | — | Enable external modulation — Low: pulse on |
| `?M` | — | Query modulation mode and parameters |
| `DACA` | `n, b` | Set DAC A output |
| `DACB` | `n, b` | Set DAC B output |
| `?DAC` | — | Query DAC settings |
| `HV` | `n, b` | Set pulser high voltage |
| `?HV` | — | Query ADC value for HV monitor on driver module |
| `?NTCE` | — | Query ADC value for external NTC temperature sensor |
| `?NTCP` | — | Query ADC value for NTC sensor on driver module |
| `THRE` | `a, e` | Set temperature thresholds for external NTC |
| `THRP` | `a, e` | Set temperature thresholds for driver module NTC |
| `?THR` | — | Query temperature threshold settings |
| `ON` | — | Switch driver module on |
| `OFF` | — | Switch driver module off |
| `$S` | — | Save all current settings to EEPROM |

---

## Command Details

### `@` — Warm Restart
Immediately resets the microcontroller. Restarts with last saved settings.

```
@
```
Response: `@->Restart` then `>`

---

### `?`, `?V`, `?I`, `?BASCOM` — Info Commands

- `?` — list all remote commands with brief descriptions
- `?V` — firmware name, version, and compilation date
- `?I` — programmer's contact address and e-mail
- `?BASCOM` — BASCOM-AVR compiler version used

---

### `P` — Pulse Width and Repetition Rate

Parameters in multiples of 50 ns. Range: `w` = 1..255, `p` = 2..256. Period must be greater than width.

| Parameter | Description |
|-----------|-------------|
| `w` | Pulse width in 50 ns steps |
| `p` | Pulse period in 50 ns steps |

Default: `w=2, p=20` (100 ns width, 1 µs period)

```
P:3,20        → 150 ns width, 1 µs period
```

---

### `?P` — Query Pulse Parameters

```
?P
```
Response example:
```
Width:  1       (50 ns on)
Period: 5       (250 ns period)
```

---

### `MS` — Internal Modulation, Synchronous

Modulation parameters are multiples of the pulse period. Range: `ws` = 1..65534, `ps` = 1..65535, with `ws ≤ ps`. If `ws == ps`, modulation is off (all pulses pass through).

| Parameter | Description |
|-----------|-------------|
| `ws` | Modulation on-time in multiples of impulse period |
| `ps` | Modulation period in multiples of impulse period |

Default: `ws=50, ps=50` (modulation off, all pulses pass)

```
MS:3,8        → 3 of 8 pulses are on
```

---

### `MA` — Internal Modulation, Asynchronous

Same range as `MS` but parameters are multiples of 50 ns (system clock). May introduce jitter unless modulation period is an integer multiple of pulse period. Parameters should be much larger than pulse parameters (`ws >> w`, `ps >> p`).

| Parameter | Description |
|-----------|-------------|
| `wa` | Modulation on-time in 50 ns steps |
| `pa` | Modulation period in 50 ns steps |

Default: `wa=1000, pa=2000` (50 µs / 100 µs)

```
MA:10000,20000   → 50% duty cycle, 1 kHz modulation
```

---

### `ME+` / `ME-` — External Modulation

Switches to external modulation signal. Internal modulation is disabled immediately.

- `ME+` — High level at modulation input enables pulses
- `ME-` — Low level at modulation input enables pulses

```
ME+
```

---

### `?M` — Query Modulation Settings

```
?M
```
Response example (internal synchronous):
```
Mode: 1  Internal, synchronous
ModWidth:   3
ModPeriode: 15
```
Response example (external):
```
Mode: 3  External, High = On
```

---

### `DACA` / `DACB` — DAC Direct Control

12-bit DACs (0..4095). DAC outputs are connected to XS2 for high-voltage control.
> **Warning:** When using the driver module, prefer the `HV` command over direct DAC control.

| Parameter | Description |
|-----------|-------------|
| `n` | Binary value (0..4095) |
| `b` | Vref multiplier: `0` = output off (500 kΩ pull-down), `1` = ×1, `2` = ×2 |

Formula: `U_DAC = (n / 4096) × Vref × b`

```
DACA:2000,1   → U_DACA = 1.000 V
DACA:1500,2   → U_DACA = 1.500 V
DACA:2500     → U_DACA = 2.500 V  (b keeps last value)
DACA:1234,0   → DAC output off, 500 kΩ to GND
```

---

### `?DAC` — Query DAC Settings

```
?DAC
```
Response example:
```
DACA: 1000   0.5mv/Bit    (n=1000, b=1, U_DACA=0.50 V)
DACB off                  (b=0, Ra=500 kΩ to GND)
```

---

### `HV` — High Voltage Setting

Sets the pulser high voltage via DACB. DACA simultaneously disables the internal potentiometer on the driver module.

| Parameter | Description |
|-----------|-------------|
| `n` | DAC value (0..4095) |
| `b` | Multiplier: `0` = potentiometer on driver module active, `1` = ×1, `2` = ×2 |

Formula:
```
U_HV = 25 × (n / 4096) × Vref × b      (Vref = 2.048 V)
```
Inverse: `n = 80 × U_HV` (b=1) or `n = 40 × U_HV` (b=2)

Default: `n=0, b=0` (potentiometer on driver module active)

```
HV:1234,0    → potentiometer determines voltage
HV:1000,2    → U_HV = 25.0 V
HV:3000,1    → U_HV = 37.5 V
```

---

### `?HV` — Query HV Monitor ADC Value

Returns 10-bit ADC value from the HV monitor pin (XS2, Pin 8).

Conversion: `U_HV ≈ ADC × 100 mV`

If driver module is not connected, measures 0..3.838 V directly: `Ue = ADC × 3.752 mV`

```
?HV
```
Response example:
```
HV: 440       (≈ 44 V)
```

---

### `?NTCE` / `?NTCP` — Query NTC Temperature Sensor ADC Values

Returns raw 10-bit ADC values (0..1023).

- `?NTCE` — external NTC connected to XS4
- `?NTCP` — NTC on driver module

Values < 10 indicate sensor short circuit; values > 1000 indicate open circuit.

```
?NTCE
```
Response:
```
NTC ext: 440
```

---

### `THRE` / `THRP` — Temperature Threshold Settings

Sets shutdown/restart thresholds for NTC sensors. All values are ADC bit values (0..1023); lower values = higher temperature.

- `THRP` — driver module NTC thresholds
- `THRE` — external NTC thresholds

| Parameter | Description |
|-----------|-------------|
| `a` | Shutdown threshold (20..1000, or 0 to disable) |
| `e` | Auto-restart threshold (must be ≥ a + 10, or 0 to disable) |

Default: `THRP:120,190` (≈70°C off, ≈55°C on), `THRE:0,0` (disabled)

```
THRE:0,0          → disable external sensor shutdown
THRP:100,200      → off at 100 (≈77°C), on at 200 (≈55°C)
```

---

### `?THR` — Query Threshold Settings

```
?THR
```
Response example:
```
Thr P OFF: 100
Thr P ON : 200
Thr E OFF: 0, disabled
Thr E ON : 0, disabled
NTC E open!
```

---

### `ON` / `OFF` — Driver Module Power

```
ON    → driver module on, laser diode driven
OFF   → driver module off
```

---

### `$S` — Save Settings to EEPROM

Saves all current settings permanently. Restored automatically on next power-up.
> **Note:** EEPROM has a limited write cycle lifetime (max. 100,000). Do not call this command in a loop.

```
$S
```

---

## Parameter Reference

| Symbol | Description |
|--------|-------------|
| `w` | Pulse width in 50 ns steps (1..255) |
| `p` | Pulse repetition period in 50 ns steps (2..256) |
| `ws` | Modulation on-time in multiples of pulse period |
| `ps` | Modulation period in multiples of pulse period |
| `wa` | Modulation on-time in 50 ns steps |
| `pa` | Modulation period in 50 ns steps |
| `n` | DAC binary value (0..4095) |
| `b` | DAC Vref multiplier (0 = off, 1 = ×1, 2 = ×2) |
| `a` | Temperature shutdown threshold (ADC bits) |
| `e` | Temperature restart threshold (ADC bits) |

## Serial Port Settings

| Setting | Value |
|---------|-------|
| Baudrate | 9600 |
| Databits | 8 |
| Parity | none |
| Stopbits | 1 |
| Handshake | none |
