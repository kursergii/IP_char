# PicoScope 2000 API Reference

Device: PicoScope 2000 series | Driver: libps2000 | Interface: USB

## Overview

The PS2000 driver is a C library (`libps2000.so`). All functions take a `int16_t handle` returned by the open functions. ADC output is always scaled to **16-bit** (±32767), regardless of actual ADC resolution.

```
ADC range: -32767 to +32767
Lost data: -32768
Voltage:   V = ADC × (V_range / 32767)
```

---

## Function Reference

### Opening / Closing

| Function | Returns | Description |
|----------|---------|-------------|
| `ps2000_open_unit()` | handle or 0 | Open unit synchronously |
| `ps2000_open_unit_async()` | handle (preliminary) | Start async open; poll with `ps2000_open_unit_progress()` |
| `ps2000_open_unit_progress(handle*, progress*)` | -1/0/1 | Poll open progress (0–100%); returns 1 on complete |
| `ps2000_close_unit(handle)` | — | Close unit and release handle |

**Async open pattern (used in this project):**
```c
int16_t handle = ps2000_open_unit_async();
int16_t progress = 0, result;
do {
    result = ps2000_open_unit_progress(&handle, &progress);
    sleep_ms(1);
} while (progress < 100);
// result == 1 → success
```

---

### Channel Configuration

```c
ps2000_set_channel(handle, channel, enabled, dc, range)
```

| Parameter | Type | Values |
|-----------|------|--------|
| `channel` | PS2000_CHANNEL | `PS2000_CHANNEL_A` (0), `PS2000_CHANNEL_B` (1) |
| `enabled` | int16_t | 1 = on, 0 = off |
| `dc` | int16_t | 1 = DC coupling, 0 = AC coupling |
| `range` | PS2000_RANGE | see range table below |

**Must be called before every `ps2000_run_block()` if range may have changed.**

---

### Voltage Ranges (`PS2000_RANGE`)

| Enum value | Index | Full-scale | V per ADU |
|------------|-------|-----------|-----------|
| `PS2000_10MV` | 0 | ±10 mV | 3.05E-7 |
| `PS2000_20MV` | 1 | ±20 mV | 6.10E-7 |
| `PS2000_50MV` | 2 | ±50 mV | 1.526E-6 |
| `PS2000_100MV` | 3 | ±100 mV | 3.052E-6 |
| `PS2000_200MV` | 4 | ±200 mV | 6.104E-6 |
| `PS2000_500MV` | 5 | ±500 mV | 1.526E-5 |
| `PS2000_1V` | 6 | ±1 V | 3.052E-5 |
| `PS2000_2V` | 7 | ±2 V | 6.104E-5 |
| `PS2000_5V` | 8 | ±5 V | 1.526E-4 |
| `PS2000_10V` | 9 | ±10 V | 3.052E-4 |
| `PS2000_20V` | 10 | ±20 V | 6.104E-4 |
| `PS2000_50V` | 11 | ±50 V | 1.526E-3 |

**Auto-ranging target in this project:** 5 000–25 000 ADU (15–76% of full scale).

---

### Trigger

```c
ps2000_set_trigger(handle, source, threshold, direction, delay, auto_trigger_ms)
```

| Parameter | Description |
|-----------|-------------|
| `source` | `PS2000_CHANNEL_A`, `PS2000_CHANNEL_B`, or `PS2000_NONE` (disable) |
| `threshold` | ADC value at which trigger fires (−32767 to +32767) |
| `direction` | `PS2000_RISING` or `PS2000_FALLING` |
| `delay` | Trigger position as % of buffer (−100 = start, 0 = centre) |
| `auto_trigger_ms` | Auto-fire after this many ms if no trigger; 0 = wait forever |

**Used in this project:**
```c
ps2000_set_trigger(handle, PS2000_CHANNEL_A, -1000, PS2000_FALLING, 50, 10);
// Trigger on Ch-A falling through -1000 ADU; auto-fire after 10 ms
```

---

### Block Acquisition

```c
// Start acquisition
ps2000_run_block(handle, no_of_values, timebase, oversample, time_indisposed_ms*)

// Poll until ready
while (ps2000_ready(handle) == 0) { sleep_ms(1); }

// Retrieve samples
ps2000_get_values(handle, buffer_a*, buffer_b*, buffer_c*, buffer_d*, overflow*, no_of_values)

// Stop (required before next run)
ps2000_stop(handle)
```

---

### Timebase Table (PS2205, 2-channel mode)

| Timebase | Sample interval | 1024-sample window |
|----------|----------------|--------------------|
| 0 | 10 ns | 10.24 µs |
| 1 | 20 ns | 20.48 µs |
| 2 | 40 ns | 40.96 µs |
| 3 | 80 ns | 81.92 µs |
| 4 | 160 ns | 163.84 µs |
| 5 | 320 ns | 327.68 µs |

**Used in this project:** timebase 1 (20 ns/sample) → 1024 samples covers 20 µs, enough to capture a 150 ns pulse with ~7 samples inside the pulse.

---

### `ps2000_get_values` — Buffer Layout

```c
ps2000_get_values(handle, buffer_a, buffer_b, NULL, NULL, &overflow, no_of_values)
```

- `buffer_a`, `buffer_b`: `int16_t[]` arrays of size `no_of_values`
- `overflow`: bitmask; bit 0 = Ch-A clipped, bit 1 = Ch-B clipped
- Returns: number of values actually written

**In this project, Ch-A carries a negative current pulse — take the minimum:**
```cpp
int16_t peak = *std::min_element(buffer_a, buffer_a + N);
double volts = -peak * range_values[range_a];  // negate → positive amplitude
```

---

## Acquisition Sequence Used in This Project

```
openDevice():
  ps2000_open_unit_async()
  poll ps2000_open_unit_progress() until 100%
  ps2000_set_trigger(Ch-A, -1000 ADU, FALLING, delay=50, auto=10ms)

run_osc() — called per measurement:
  ps2000_set_channel(Ch-A, enabled, DC, range_a)
  ps2000_set_channel(Ch-B, enabled, DC, range_b)
  for each of N blocks:
    ps2000_run_block(1024 samples, timebase=1, oversample=0)
    poll ps2000_ready()
    ps2000_get_values(buffer_a, buffer_b, ...)
    peak_A = -min(buffer_a)         // negative pulse → negate min
    mean_B = mean(buffer_b)
  max_A = mean(peak_A over N blocks)
  max_B = mean(mean_B over N blocks)

check_val() — auto-ranging:
  if max_A > 25000 and range_a < 10: increase range, re-run
  if max_A <  5000 and range_a >  2: decrease range, re-run
  (same for Ch-B)
  ret_A = max_A × range_values[range_a]   // ADU → volts
```

---

## Connection

| Setting | Value |
|---------|-------|
| Interface | USB (libusb) |
| Driver | `/opt/picoscope/lib/libps2000.so` |
| Max simultaneous units | 127 |
| Handle type | `int16_t` (0 = not open) |
