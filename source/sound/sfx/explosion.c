// Explosion: white noise through a lowpass whose cutoff sweeps down,
// giving the classic decreasing-brightness "boom" without a real WAV.
#include "sfx_explosion.h"
#include "osc.h"
#include "filter.h"
#include "audio_rng.h"

#include <math.h>
#include <stdbool.h>

#define EXPLOSION_VOICES 4

typedef struct {
    int active;
    float t, duration;
    Filter filt;
    float start_cutoff, end_cutoff;
    float amp;
    float thump_hz, thump_phase; // 0 = no sub-bass boom; >0 = grave-only deep thump under the noise
} ExplosionVoice;

static ExplosionVoice voices[EXPLOSION_VOICES];
static int next_voice = 0;

void explosion_init(void) {
    for (int i = 0; i < EXPLOSION_VOICES; i++) {
        voices[i].active = 0;
        filter_reset(&voices[i].filt);
    }
}

// big: 0 = small, 1 = big, 2 = grave (ship-death final blow — lower/longer/
// louder than "big", the noise never brightens back up past a deep rumble).
void explosion_trigger(int big) {
    ExplosionVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % EXPLOSION_VOICES;
    v->active = 1;
    v->t = 0.0f;
    filter_reset(&v->filt);
    if (big >= 2) {
        v->duration = audio_randf_range(1.1f, 1.4f);
        v->start_cutoff = audio_randf_range(1400.0f, 1900.0f);
        v->end_cutoff = 55.0f;
        v->amp = 1.0f;
        // Sweeps from ~150Hz down to ~45Hz rather than sitting at one pure
        // sub-bass tone: small speakers (TVs) roll off hard below ~150Hz,
        // so a fixed 40-60Hz sine is often just inaudible on them. Starting
        // higher means real speakers reproduce the front of the hit, and
        // the soft-clip in the tick below adds harmonics of the low end so
        // there's still something to hear once it drops past what the
        // speaker can move.
        v->thump_hz = audio_randf_range(140.0f, 170.0f);
        v->thump_phase = 0.0f;
    } else {
        v->duration = big ? audio_randf_range(0.6f, 0.9f) : audio_randf_range(0.25f, 0.4f);
        v->start_cutoff = big ? audio_randf_range(2200.0f, 3000.0f) : audio_randf_range(3500.0f, 4500.0f);
        v->end_cutoff = big ? 120.0f : 300.0f;
        v->amp = big ? 0.9f : 0.55f;
        v->thump_hz = 0.0f;
    }
}

// Hard clip, not tanh — a guitar amp driven this hard square-waves off the
// peaks instead of gently rounding them, packing the signal with upper
// harmonics that a small/tinny TV speaker can still reproduce even where it
// can't move enough air for the low fundamental underneath.
static float hard_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x;
}

static float explosion_voice_tick(ExplosionVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;
    float progress = v->t / v->duration;
    if (progress >= 1.0f) { v->active = 0; return 0.0f; }

    bool grave = v->thump_hz > 0.0f; // only explosion_trigger(2) sets this
    float cutoff = v->start_cutoff * powf(v->end_cutoff / v->start_cutoff, progress);
    float filtered = filter_lowpass(&v->filt, osc_noise(), cutoff, sample_rate);
    float env = 1.0f - progress;
    float body = filtered * env * env;
    if (grave) body = hard_clip(body * 4.0f); // driven into distortion, not a clean sweep

    float thump = 0.0f;
    if (grave) {
        float freq = v->thump_hz * (1.0f - 0.7f * progress); // ~150Hz -> ~45Hz over the hit
        v->thump_phase = phase_advance(v->thump_phase, freq, sample_rate);
        float raw = osc_sine(v->thump_phase) * env;
        thump = hard_clip(raw * 5.0f); // hard-clipped, not soft — buzzy harmonics of the low end
    }

    v->t += 1.0f / sample_rate;
    return (body + thump * 0.9f) * v->amp;
}

float explosion_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < EXPLOSION_VOICES; i++) sum += explosion_voice_tick(&voices[i], sample_rate);
    return sum;
}
