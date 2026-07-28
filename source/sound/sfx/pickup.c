// Pickup/coin: sine, fast ascending pitch, short bright envelope.
#include "sfx_pickup.h"
#include "osc.h"
#include "audio_rng.h"

#include <math.h>

#define PICKUP_VOICES 4

typedef struct {
    int active;
    float t, duration;
    float phase;
    float start_freq, end_freq;
} PickupVoice;

static PickupVoice voices[PICKUP_VOICES];
static int next_voice = 0;

void pickup_init(void) {
    for (int i = 0; i < PICKUP_VOICES; i++) voices[i].active = 0;
}

void pickup_trigger(void) {
    PickupVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % PICKUP_VOICES;
    v->active = 1;
    v->t = 0.0f;
    v->phase = 0.0f;
    v->duration = audio_randf_range(0.12f, 0.18f);
    v->start_freq = audio_randf_range(500.0f, 700.0f);
    v->end_freq = audio_randf_range(1200.0f, 1700.0f);
}

static float pickup_voice_tick(PickupVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    float freq = v->start_freq * powf(v->end_freq / v->start_freq, progress);
    v->phase = phase_advance(v->phase, freq, sample_rate);
    float env = 1.0f - progress;

    v->t += 1.0f / sample_rate;
    return osc_sine(v->phase) * env * 0.4f;
}

float pickup_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < PICKUP_VOICES; i++) sum += pickup_voice_tick(&voices[i], sample_rate);
    return sum;
}
