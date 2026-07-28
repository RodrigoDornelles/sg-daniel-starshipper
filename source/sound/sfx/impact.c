// Impact/hull hit: noise + lowpass, sharper attack and shorter tail than
// the explosion generator so it reads as a "thud" rather than a "boom".
#include "sfx_impact.h"
#include "osc.h"
#include "filter.h"
#include "audio_rng.h"

#include <math.h>

#define IMPACT_VOICES 4

typedef struct {
    int active;
    float t, duration;
    Filter filt;
    float start_cutoff, end_cutoff;
} ImpactVoice;

static ImpactVoice voices[IMPACT_VOICES];
static int next_voice = 0;

void impact_init(void) {
    for (int i = 0; i < IMPACT_VOICES; i++) {
        voices[i].active = 0;
        filter_reset(&voices[i].filt);
    }
}

void impact_trigger(void) {
    ImpactVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % IMPACT_VOICES;
    v->active = 1;
    v->t = 0.0f;
    filter_reset(&v->filt);
    v->duration = audio_randf_range(0.08f, 0.15f);
    v->start_cutoff = audio_randf_range(1600.0f, 2200.0f);
    v->end_cutoff = 300.0f;
}

static float impact_voice_tick(ImpactVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    float cutoff = v->start_cutoff * powf(v->end_cutoff / v->start_cutoff, progress);
    float filtered = filter_lowpass(&v->filt, osc_noise(), cutoff, sample_rate);
    float env = (1.0f - progress);

    v->t += 1.0f / sample_rate;
    return filtered * env * env * env * 0.8f;
}

float impact_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < IMPACT_VOICES; i++) sum += impact_voice_tick(&voices[i], sample_rate);
    return sum;
}
