// Explosion: white noise through a lowpass whose cutoff sweeps down,
// giving the classic decreasing-brightness "boom" without a real WAV.
#include "sfx_explosion.h"
#include "osc.h"
#include "filter.h"
#include "audio_rng.h"

#include <math.h>

#define EXPLOSION_VOICES 4

typedef struct {
    int active;
    float t, duration;
    Filter filt;
    float start_cutoff, end_cutoff;
    float amp;
} ExplosionVoice;

static ExplosionVoice voices[EXPLOSION_VOICES];
static int next_voice = 0;

void explosion_init(void) {
    for (int i = 0; i < EXPLOSION_VOICES; i++) {
        voices[i].active = 0;
        filter_reset(&voices[i].filt);
    }
}

void explosion_trigger(int big) {
    ExplosionVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % EXPLOSION_VOICES;
    v->active = 1;
    v->t = 0.0f;
    filter_reset(&v->filt);
    v->duration = big ? audio_randf_range(0.6f, 0.9f) : audio_randf_range(0.25f, 0.4f);
    v->start_cutoff = big ? audio_randf_range(2200.0f, 3000.0f) : audio_randf_range(3500.0f, 4500.0f);
    v->end_cutoff = big ? 120.0f : 300.0f;
    v->amp = big ? 0.9f : 0.55f;
}

static float explosion_voice_tick(ExplosionVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    float cutoff = v->start_cutoff * powf(v->end_cutoff / v->start_cutoff, progress);
    float filtered = filter_lowpass(&v->filt, osc_noise(), cutoff, sample_rate);
    float env = 1.0f - progress;

    v->t += 1.0f / sample_rate;
    return filtered * env * env * v->amp;
}

float explosion_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < EXPLOSION_VOICES; i++) sum += explosion_voice_tick(&voices[i], sample_rate);
    return sum;
}
