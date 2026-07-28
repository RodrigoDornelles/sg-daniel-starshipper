#include "filter.h"

#include <math.h>

#define TWO_PI 6.28318530717958647692f

void filter_reset(Filter *f) {
    f->lp_z = 0.0f;
    f->hp_z = 0.0f;
    f->hp_prev_in = 0.0f;
}

float filter_lowpass(Filter *f, float in, float cutoff_hz, float sample_rate) {
    float rc = 1.0f / (TWO_PI * cutoff_hz);
    float dt = 1.0f / sample_rate;
    float alpha = dt / (rc + dt);
    f->lp_z += alpha * (in - f->lp_z);
    return f->lp_z;
}

float filter_highpass(Filter *f, float in, float cutoff_hz, float sample_rate) {
    float rc = 1.0f / (TWO_PI * cutoff_hz);
    float dt = 1.0f / sample_rate;
    float alpha = rc / (rc + dt);
    float out = alpha * (f->hp_z + in - f->hp_prev_in);
    f->hp_prev_in = in;
    f->hp_z = out;
    return out;
}
