// Minimal 1-pole low-pass / high-pass, enough to tame noise-based SFX and
// give instruments a bit of tone shaping without pulling in a biquad lib.
#ifndef STARTSHIPPER_FILTER_H
#define STARTSHIPPER_FILTER_H

typedef struct {
    float lp_z;
    float hp_z;
    float hp_prev_in;
} Filter;

void filter_reset(Filter *f);
float filter_lowpass(Filter *f, float in, float cutoff_hz, float sample_rate);
float filter_highpass(Filter *f, float in, float cutoff_hz, float sample_rate);

#endif
