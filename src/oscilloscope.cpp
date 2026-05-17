#include "oscilloscope.h"
#include <algorithm>
#include <cmath>
#include <QDebug>

void Osc::running() {
    while (running_osc) {
        run_osc();
        check_val();
        emit sendData(ret_A);
    }
}

void Osc::openDevice() {
    int16_t open_value = 0;
    int16_t progress_percent = 0;
    handle = ps2000_open_unit_async();

    // Poll until the unit finishes initialising, with a 5-second timeout
    QElapsedTimer timer;
    timer.start();
    while (progress_percent < 100 && timer.elapsed() < 5000) {
        open_value = ps2000_open_unit_progress(&handle, &progress_percent);
        QThread::msleep(1);
    }

    if (open_value > 0) {
        connected_osc = true;
        // Trigger on Ch-A falling edge at -5000 ADU (~15% of full scale at 50mV range).
        // Threshold chosen to reject HV switching noise (<5 mV) while catching real
        // current pulses (typically >10 mV). auto_trigger_ms=10 fires after 10 ms if
        // no pulse is detected (e.g. laser off); 10 ms = 100 pulse periods at 10 kHz.
        ps2000_set_trigger(handle, PS2000_CHANNEL_A, -5000, PS2000_FALLING, 50, 10);
        emit connectionResult(true);
    } else {
        qWarning() << "Osc: failed to open PicoScope, result=" << open_value;
        emit connectionResult(false);
    }
}

void Osc::run_osc() {
    // Apply current voltage ranges to both channels before every acquisition.
    // This is called after every ranging adjustment in check_val().
    ps2000_set_channel(handle, PS2000_CHANNEL_A, 1, 1, range_a);
    ps2000_set_channel(handle, PS2000_CHANNEL_B, 1, 1, range_b);

    double cA = 0, cB = 0;

    // Timebase 3 (~80 ns/sample on PS2205): 1024 samples = ~82 µs capture window.
    // Triggered on Ch-A falling edge; each block captures one laser current pulse.
    for (int l = 0; l < NUM_BLOCKS; ++l) {
        ps2000_run_block(handle, NUM_VALUES, 3, 0, &time_indisposed_ms);
        while (ps2000_ready(handle) == 0)
            QThread::msleep(1);
        ps2000_get_values(handle, buffer_a, buffer_b, nullptr, nullptr, &overflow, NUM_VALUES);

        // Ch-A: laser current appears as a negative voltage excursion.
        // Scan for the most negative valid sample (PS2000_LOST_DATA = -32768 is excluded
        // to avoid spurious peaks when the ADC drops samples).
        short min_A = 0;
        for (int i = 0; i < NUM_VALUES; ++i)
            if (buffer_a[i] != -32768 && buffer_a[i] < min_A)
                min_A = buffer_a[i];

        // Negate so the peak amplitude is positive; clamp to avoid short overflow
        // if min_A somehow reaches -32768 despite the guard above.
        double peak = static_cast<double>(-min_A);
        peaks[l] = std::min(peak, 32767.0);
        cA += peaks[l];

        // Ch-B: DC photodetector signal (kept for potential future use)
        double X2 = 0;
        for (int i = 0; i < NUM_VALUES; ++i)
            X2 += buffer_b[i];
        cB += X2 / NUM_VALUES;
    }

    // max_A uses the maximum individual block peak rather than the mean, so that
    // even a single clipped capture triggers a range increase (mean would mask it).
    max_A = static_cast<short>(*std::max_element(peaks, peaks + NUM_BLOCKS));
    max_B = static_cast<short>(std::fabs(cB) / NUM_BLOCKS);

    // Store the mean peak for the final calibrated result
    ret_A = (cA / NUM_BLOCKS) * range_values[range_a];
    ret_B = max_B * range_values[range_b];
}

void Osc::check_val() {
    // Auto-range Ch-A: keep max peak in 5 000–25 000 ADU (15–76% of full scale)
    // to maximise dynamic range without clipping.
    // Re-acquire after every range step; loop until stable or at a range limit.
    //
    // Potential improvement: add an oscillation guard — if the signal sits exactly
    // at a range boundary, successive +1/−1 adjustments can loop indefinitely.
    bool rerange;
    do {
        rerange = false;
        if      (max_A > 25000 && range_a < 10) { ++range_a; rerange = true; }
        else if (max_A <  5000 && range_a >  2) { --range_a; rerange = true; }
        if      (max_B > 25000 && range_b < 10) { ++range_b; rerange = true; }
        else if (max_B <  5000 && range_b >  2) { --range_b; rerange = true; }
        if (rerange) run_osc();
    } while (rerange);
}
