// Laser: square wave, descending pitch, short envelope. Params vary per
// shot via the shared PRNG so a burst of fire doesn't sound identical.
#include "sfx_laser.h"
#include "osc.h"
#include "audio_rng.h"

#include <math.h>

#define LASER_VOICES 6

typedef struct {
    int active;
    float t, duration;
    float phase;
    float start_freq, end_freq;
} LaserVoice;

static LaserVoice voices[LASER_VOICES];
static int next_voice = 0;

void laser_init(void) {
    for (int i = 0; i < LASER_VOICES; i++) voices[i].active = 0;
}

void laser_trigger(void) {
    LaserVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % LASER_VOICES;
    v->active = 1;
    v->t = 0.0f;
    v->phase = 0.0f;
    v->duration = audio_randf_range(0.15f, 0.25f);
    v->start_freq = audio_randf_range(1400.0f, 1900.0f);
    v->end_freq = audio_randf_range(200.0f, 350.0f);
}

static float laser_voice_tick(LaserVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    float freq = v->start_freq * powf(v->end_freq / v->start_freq, progress);
    v->phase = phase_advance(v->phase, freq, sample_rate);
    float env = 1.0f - progress;

    v->t += 1.0f / sample_rate;
    return osc_square(v->phase) * env * env * 0.5f;
}

float laser_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < LASER_VOICES; i++) sum += laser_voice_tick(&voices[i], sample_rate);
    return sum;
}
