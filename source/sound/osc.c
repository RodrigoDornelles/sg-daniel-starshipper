#include "osc.h"
#include "audio_rng.h"

#include <math.h>

#define TWO_PI 6.28318530717958647692f

float osc_sine(float phase) {
    return sinf(phase * TWO_PI);
}

float osc_square(float phase) {
    return phase < 0.5f ? 1.0f : -1.0f;
}

float osc_saw(float phase) {
    return 2.0f * phase - 1.0f;
}

float osc_triangle(float phase) {
    float t = phase < 0.5f ? phase : 1.0f - phase;
    return 4.0f * t - 1.0f;
}

float osc_noise(void) {
    return audio_randf_range(-1.0f, 1.0f);
}

float phase_advance(float phase, float freq_hz, float sample_rate) {
    phase += freq_hz / sample_rate;
    return phase - floorf(phase);
}
