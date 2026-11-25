/* Research-only harness. Includes the unchanged, MIT-licensed upstream DLL.
 * Queue, clock, and recovery models below are experimental abstractions.
 * Build/run from research/pipewire; see EXPERIMENTS.md. */
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <spa/utils/dll.h>
#include <spa/utils/ringbuffer.h>

static void ring_checks(void)
{
    struct spa_ringbuffer r;
    uint32_t index;
    char storage[8] = {0}, readback[6] = {0};
    spa_ringbuffer_init(&r);
    r.readindex = UINT32_MAX - 3u;
    r.writeindex = r.readindex;
    assert(spa_ringbuffer_get_write_index(&r, &index) == 0);
    spa_ringbuffer_write_data(&r, storage, 8, index % 8, "abcdef", 6);
    spa_ringbuffer_write_update(&r, (int32_t)(index + 6u));
    assert(spa_ringbuffer_get_read_index(&r, &index) == 6);
    spa_ringbuffer_read_data(&r, storage, 8, index % 8, readback, 6);
    assert(memcmp(readback, "abcdef", 6) == 0);
    spa_ringbuffer_read_update(&r, (int32_t)(index + 6u));
    assert(spa_ringbuffer_get_read_index(&r, &index) == 0);
}

/* e = delay-target for playback, target-delay for capture.
 * Both ideal plants have e[k+1] = e[k] + Q*(actuator-actual_rate).
 * mode: 0 ideal actuator, 1 native playback rate quantization,
 * 2 native capture rate quantization; no audio filtering is executed. */
static void run(const char *name, double seconds, double ppm,
                int mode, int adaptive, int jitter, int step, int jump)
{
    const double frames = 128.0, sample_rate = 48000.0;
    const double dt = frames / sample_rate, window = sample_rate / frames;
    const uint64_t count = (uint64_t)(seconds / dt);
    struct spa_dll d;
    double e = 0.0, avg = 0.0, var = 0.0, elapsed = 0.0;
    double min_e = DBL_MAX, max_e = -DBL_MAX;
    double tail_min = DBL_MAX, tail_max = -DBL_MAX, tail_sum = 0.0;
    double corr = 1.0, max_ppm = 0.0;
    uint64_t tails = 0, resets = 0;
    spa_dll_init(&d);
    spa_dll_set_bw(&d, SPA_DLL_BW_MAX, (unsigned)frames, (unsigned)sample_rate);
    for (uint64_t k = 0; k < count; ++k) {
        double t = k * dt;
        double actual = 1.0 + ((step && t >= seconds/2.0) ? -ppm : ppm) * 1e-6;
        if (jump && k == count/2) e += 512.0;
        /* Measurement-only deterministic jitter, +/- 16 frames. */
        double err = e + (jitter ? 16.0 * sin(2.0 * M_PI * k / 17.0) : 0.0);
        if (jump && fabs(err) > 160.0) {
            /* Proposed reset-to-target abstraction, not an ALSA emulation. */
            e = 0.0; err = 0.0; ++resets;
            spa_dll_init(&d);
            spa_dll_set_bw(&d, SPA_DLL_BW_MAX, (unsigned)frames, (unsigned)sample_rate);
        }
        if (err > 256.0) err = 256.0;
        if (err < -256.0) err = -256.0;
        corr = spa_dll_update(&d, err);
        assert(isfinite(corr) && corr > 0.0);
        double next_avg = (avg * window + (err - avg)) / (window + 1.0);
        var = (var * window + (err - avg) * (err - next_avg)) / (window + 1.0);
        avg = next_avg;
        elapsed += dt;
        if (adaptive && elapsed > 3.0) {
            double bw = (fabs(avg) + sqrt(fabs(var))) / 1000.0;
            if (bw < 0.001) bw = 0.001;
            if (bw > SPA_DLL_BW_MAX) bw = SPA_DLL_BW_MAX;
            spa_dll_set_bw(&d, bw, (unsigned)frames, (unsigned)sample_rate);
            elapsed = 0.0;
        }
        double actuator = corr;
        if (mode == 1) actuator = sample_rate / floor(sample_rate / corr);
        if (mode == 2) actuator = floor(sample_rate * corr) / sample_rate;
        e += frames * (actuator - actual);
        min_e = fmin(min_e, e); max_e = fmax(max_e, e);
        max_ppm = fmax(max_ppm, fabs((corr - 1.0) * 1e6));
        if (k >= count - (uint64_t)(60.0/dt)) {
            tail_min = fmin(tail_min, e); tail_max = fmax(tail_max, e);
            tail_sum += e; ++tails;
        }
    }
    printf("%s,%.0f,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%.9f,%llu\n",
           name, seconds, mode, adaptive, min_e, max_e, tail_min, tail_max,
           tail_sum / tails, (corr-1.0)*1e6, max_ppm, d.bw,
           (unsigned long long)resets);
    assert(isfinite(e) && fabs(e) < 128.0);
}

int main(void)
{
    ring_checks();
    struct spa_dll d;
    spa_dll_init(&d); spa_dll_set_bw(&d, 0.128, 128, 48000);
    double first = spa_dll_update(&d, 1.0);
    assert(fabs(first - (1.0 - d.w0*d.w0*d.w1*(1.0+d.w2))) < 1e-14);
    puts("scenario,simulated_seconds,actuator_mode,adaptive_bw,min_error_frames,max_error_frames,tail_min_frames,tail_max_frames,tail_mean_frames,final_command_ppm,max_abs_command_ppm,final_bw,resets");
    run("ideal_plus100_6h", 21600, 100, 0, 0, 0, 0, 0);
    run("ideal_minus100", 600, -100, 0, 0, 0, 0, 0);
    run("adaptive_plus100", 600, 100, 0, 1, 0, 0, 0);
    run("native_playback_plus100", 600, 100, 1, 0, 0, 0, 0);
    run("native_capture_plus100", 600, 100, 2, 0, 0, 0, 0);
    run("adaptive_jitter", 600, 100, 0, 1, 1, 0, 0);
    run("fixed_step_plusminus100", 600, 100, 0, 0, 0, 1, 0);
    run("adaptive_step_plusminus100", 600, 100, 0, 1, 0, 1, 0);
    run("proposed_jump_reset", 600, 100, 0, 0, 0, 0, 1);
    return 0;
}
