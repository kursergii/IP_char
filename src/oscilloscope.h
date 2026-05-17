#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include "ps2000.h"
#include <QThread>
#include <atomic>

// Continuously acquires laser current pulses from a PicoScope 2000 in a background thread.
// Ch-A: triggered peak detection on a negative current pulse (pulsed laser measurement).
// Ch-B: unused — optical power is measured by the PM100A power meter instead.
// Emits sendData(current_V, 0) after each averaged, auto-ranged acquisition cycle.
class Osc : public QThread
{
    Q_OBJECT

    void run() Q_DECL_OVERRIDE {
        running_osc = true;   // set before openDevice so stop() can cancel cleanly
        openDevice();
        if (connected_osc)
            running();
    }

    // ── Hardware state ──────────────────────────────────────────────────────
    int16_t handle = 0;      // PicoScope device handle; 0 = not open

    // Voltage range indices (PS2000_RANGE enum). Start at PS2000_50MV (2);
    // auto-ranging keeps the signal within 5 000–25 000 ADU.
    int16_t range_a = 2;
    int16_t range_b = 2;     // unused — kept to satisfy ps2000_set_channel for Ch-B

    // ── Measurement results (written by acquisition thread) ──────────────────
    double ret_A = 0;        // calibrated Ch-A result (volts); apply 1/R_sense for amps
    double ret_B = 0;        // Ch-B result (volts); not used — PM100A handles power

    // ── Internal acquisition buffers ─────────────────────────────────────────
    static constexpr int NUM_VALUES  = 1024;  // samples per block
    static constexpr int NUM_BLOCKS  = 100;   // blocks averaged per measurement

    short buffer_a[NUM_VALUES];
    short buffer_b[NUM_VALUES];
    double peaks[NUM_BLOCKS];   // reused across calls; stores per-block Ch-A peaks

    short max_A = 0;   // max peak ADU from last run_osc(); used by check_val() for ranging
    short max_B = 0;   // Ch-B mean ADU (unused for power — PM100A handles that)

    int32_t time_indisposed_ms = 100;
    short overflow = 0;

    // Volts per ADU: V_range / 32767, indexed by PS2000_RANGE enum (0=10mV … 11=50V).
    // range_a/range_b are clamped to [2, 10] (PS2000_50MV … PS2000_20V).
    static constexpr double range_values[12] = {
        3.05185094759972E-07,  // PS2000_10MV
        6.10370189519944E-07,  // PS2000_20MV
        1.52592547379986E-06,  // PS2000_50MV   ← minimum used range
        3.05185094759972E-06,  // PS2000_100MV
        6.10370189519944E-06,  // PS2000_200MV
        1.52592547379986E-05,  // PS2000_500MV
        3.05185094759972E-05,  // PS2000_1V
        6.10370189519944E-05,  // PS2000_2V
        0.00015259254738,      // PS2000_5V
        0.00030518509476,      // PS2000_10V
        0.00061037018952,      // PS2000_20V    ← maximum used range
        0.00152592547380,      // PS2000_50V
    };

private:
    std::atomic<bool> running_osc{false};
    bool connected_osc = false;
    void running();
    void openDevice();
    void run_osc();
    void check_val();

public slots:
    void stop() { running_osc = false; }

signals:
    void sendData(double current_V);
    void connectionResult(bool success);
};

#endif // OSCILLOSCOPE_H
