// Menu navigation/confirm blip: short triangle beep.
#include "sfx_ui.h"
#include "osc.h"
#include "audio_rng.h"

#define UI_VOICES 2

typedef struct {
    int active;
    float t, duration;
    float phase;
    float freq;
} UiVoice;

static UiVoice voices[UI_VOICES];
static int next_voice = 0;

void ui_init(void) {
    for (int i = 0; i < UI_VOICES; i++) voices[i].active = 0;
}

void ui_trigger(void) {
    UiVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % UI_VOICES;
    v->active = 1;
    v->t = 0.0f;
    v->phase = 0.0f;
    v->duration = 0.06f;
    v->freq = audio_randf_range(700.0f, 900.0f);
}

static float ui_voice_tick(UiVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    v->phase = phase_advance(v->phase, v->freq, sample_rate);
    float env = 1.0f - progress;

    v->t += 1.0f / sample_rate;
    return osc_triangle(v->phase) * env * 0.3f;
}

float ui_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < UI_VOICES; i++) sum += ui_voice_tick(&voices[i], sample_rate);
    return sum;
}
