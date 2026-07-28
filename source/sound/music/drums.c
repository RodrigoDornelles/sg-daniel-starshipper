#include "drums.h"
#include "osc.h"
#include "filter.h"

#include <math.h>

typedef struct {
    int active;
    float t;
    float phase;
} DrumHit;

static DrumHit kick, snare, hihat;
static Filter snare_hp, snare_lp, hihat_hp;

void drums_init(void) {
    kick.active = 0; kick.t = 0.0f; kick.phase = 0.0f;
    snare.active = 0; snare.t = 0.0f;
    hihat.active = 0; hihat.t = 0.0f;
    filter_reset(&snare_hp);
    filter_reset(&snare_lp);
    filter_reset(&hihat_hp);
}

void drum_trigger_kick(void)  { kick.active = 1;  kick.t = 0.0f; kick.phase = 0.0f; }
void drum_trigger_snare(void) { snare.active = 1; snare.t = 0.0f; }
void drum_trigger_hihat(void) { hihat.active = 1; hihat.t = 0.0f; }

static float kick_tick(float sample_rate) {
    if (!kick.active) return 0.0f;
    const float duration = 0.22f;
    if (kick.t >= duration) { kick.active = 0; return 0.0f; }

    float freq = 150.0f * powf(2.0f, -(kick.t / 0.09f));
    if (freq < 35.0f) freq = 35.0f;
    kick.phase = phase_advance(kick.phase, freq, sample_rate);
    float body = osc_sine(kick.phase);
    float click = kick.t < 0.005f ? osc_noise() * (1.0f - kick.t / 0.005f) : 0.0f;
    float env = 1.0f - kick.t / duration;

    kick.t += 1.0f / sample_rate;
    return (body * 0.9f + click * 0.5f) * env * env;
}

static float snare_tick(float sample_rate) {
    if (!snare.active) return 0.0f;
    const float duration = 0.16f;
    if (snare.t >= duration) { snare.active = 0; return 0.0f; }

    float body = filter_highpass(&snare_hp, osc_noise(), 900.0f, sample_rate);
    body = filter_lowpass(&snare_lp, body, 5500.0f, sample_rate);
    float env = 1.0f - snare.t / duration;

    snare.t += 1.0f / sample_rate;
    return body * env * env * 0.7f;
}

static float hihat_tick(float sample_rate) {
    if (!hihat.active) return 0.0f;
    const float duration = 0.05f;
    if (hihat.t >= duration) { hihat.active = 0; return 0.0f; }

    float body = filter_highpass(&hihat_hp, osc_noise(), 6000.0f, sample_rate);
    float env = 1.0f - hihat.t / duration;

    hihat.t += 1.0f / sample_rate;
    return body * env * 0.35f;
}

float drums_tick(float sample_rate) {
    return kick_tick(sample_rate) + snare_tick(sample_rate) + hihat_tick(sample_rate);
}
