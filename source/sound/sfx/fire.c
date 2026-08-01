// Hull fire: a torch-roar, not TV static — noise banded down to a low
// rushing body (a blowtorch reads as "shhhhh", not a bright treble hiss),
// its amplitude breathing on a slow LFO so it doesn't sit as a flat hum,
// plus a light crackle-pop mixed underneath. Gain smoothed toward target
// the same way thrust.c is — no clicks as severity rises or a run restarts.
#include "sfx_fire.h"
#include "osc.h"
#include "filter.h"

static float target_gain = 0.0f;
static float current_gain = 0.0f;
static Filter roar_lp, roar_hp;
static float lfo_phase = 0.0f;
static float pop_timer = 0.0f;
static float pop_env = 0.0f;

void fire_init(void) {
    target_gain = 0.0f;
    current_gain = 0.0f;
    filter_reset(&roar_lp);
    filter_reset(&roar_hp);
    lfo_phase = 0.0f;
    pop_timer = 0.0f;
    pop_env = 0.0f;
}

void fire_set(float severity) {
    if (severity < 0.0f) severity = 0.0f;
    if (severity > 1.0f) severity = 1.0f;
    target_gain = severity * 0.4f;
}

// Hard clip, not tanh — a guitar amp driven into distortion square-waves
// off the peaks instead of gently rounding them, which is exactly what
// stuffs a signal with the upper harmonics a small/tinny TV speaker can
// still reproduce even when it can't move enough air for the fundamental.
static float hard_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

float fire_tick(float sample_rate) {
    float rate = 1.0f / (0.08f * sample_rate); // ~80ms smoothing
    current_gain += (target_gain - current_gain) * rate;
    if (current_gain < 0.0005f && target_gain == 0.0f) return 0.0f;

    float body = filter_lowpass(&roar_lp, osc_noise(), 550.0f, sample_rate);
    body = filter_highpass(&roar_hp, body, 90.0f, sample_rate); // trims sub-bass so it doesn't read as engine rumble
    body = hard_clip(body * 5.0f); // driven hard — buzzy, harmonic-rich growl instead of a soft rushing hiss

    lfo_phase = phase_advance(lfo_phase, 3.5f, sample_rate);
    float breathe = 0.65f + 0.35f * osc_sine(lfo_phase);

    pop_timer -= 1.0f / sample_rate;
    if (pop_timer <= 0.0f) {
        pop_timer = 0.05f + 0.12f * (osc_noise() * 0.5f + 0.5f); // next pop in 50-170ms
        pop_env = 0.4f + 0.3f * (osc_noise() * 0.5f + 0.5f);
    }
    pop_env *= 0.9996f;
    float pop = hard_clip(osc_noise() * pop_env * 3.0f);

    return (body * breathe * 0.85f + pop * 0.15f) * current_gain;
}
