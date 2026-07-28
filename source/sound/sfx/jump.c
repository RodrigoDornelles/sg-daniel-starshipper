#include "sfx_jump.h"
#include "osc.h"
#include "audio_rng.h"

#include <math.h>

#define JUMP_VOICES 2

typedef struct {
    int active;
    float t, duration;
    float phase;
    float start_freq, end_freq;
} JumpVoice;

static JumpVoice voices[JUMP_VOICES];
static int next_voice = 0;

void jump_init(void) {
    for (int i = 0; i < JUMP_VOICES; i++) voices[i].active = 0;
}

void jump_trigger(void) {
    JumpVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % JUMP_VOICES;
    v->active = 1;
    v->t = 0.0f;
    v->phase = 0.0f;
    v->duration = audio_randf_range(0.25f, 0.35f);
    v->start_freq = audio_randf_range(280.0f, 380.0f);
    v->end_freq = audio_randf_range(850.0f, 1100.0f);
}

static float jump_voice_tick(JumpVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    float freq = v->start_freq * powf(v->end_freq / v->start_freq, progress);
    v->phase = phase_advance(v->phase, freq, sample_rate);
    float env = sinf(progress * 3.14159265f); // quick swell then fall

    v->t += 1.0f / sample_rate;
    return osc_sine(v->phase) * env * 0.3f;
}

float jump_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < JUMP_VOICES; i++) sum += jump_voice_tick(&voices[i], sample_rate);
    return sum;
}
