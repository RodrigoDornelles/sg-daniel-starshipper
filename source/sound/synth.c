#include "synth.h"
#include "osc.h"

#include <math.h>

static float osc_tick(Oscillator *osc, float base_freq, float sample_rate) {
    float sample;
    switch (osc->shape) {
        case WAVE_SINE:     sample = osc_sine(osc->phase); break;
        case WAVE_SQUARE:   sample = osc_square(osc->phase); break;
        case WAVE_SAW:      sample = osc_saw(osc->phase); break;
        case WAVE_TRIANGLE: sample = osc_triangle(osc->phase); break;
        case WAVE_NOISE:    return osc_noise();
        default:            sample = 0.0f; break;
    }
    osc->phase = phase_advance(osc->phase, base_freq * osc->detune, sample_rate);
    return sample;
}

void synth_voice_init(SynthVoice *v) {
    v->osc1.shape = WAVE_SINE;
    v->osc1.phase = 0.0f;
    v->osc1.detune = 1.0f;
    v->osc2.shape = WAVE_SINE;
    v->osc2.phase = 0.0f;
    v->osc2.detune = 1.0f;
    v->osc2_mix = 0.0f;
    v->env.attack = 0.01f;
    v->env.decay = 0.05f;
    v->env.sustain = 0.7f;
    v->env.release = 0.1f;
    v->use_filter = 0;
    v->filter_is_highpass = 0;
    v->filter_cutoff_hz = 4000.0f;
    filter_reset(&v->filter);
    v->distortion = 0.0f;
    v->frequency = 440.0f;
    v->volume = 1.0f;
    v->t = 0.0f;
    v->released_at = -1.0f;
    v->active = 0;
}

void synth_voice_note_on(SynthVoice *v, float frequency, float volume) {
    v->frequency = frequency;
    v->volume = volume;
    v->osc1.phase = 0.0f;
    v->osc2.phase = 0.0f;
    v->t = 0.0f;
    v->released_at = -1.0f;
    v->active = 1;
}

void synth_voice_note_off(SynthVoice *v) {
    if (v->active && v->released_at < 0.0f) {
        v->released_at = v->t;
    }
}

float synth_voice_tick(SynthVoice *v, float sample_rate) {
    if (!v->active) return 0.0f;

    float s1 = osc_tick(&v->osc1, v->frequency, sample_rate);
    float s2 = osc_tick(&v->osc2, v->frequency, sample_rate);
    float sample = s1 * (1.0f - v->osc2_mix) + s2 * v->osc2_mix;

    if (v->use_filter) {
        sample = v->filter_is_highpass
            ? filter_highpass(&v->filter, sample, v->filter_cutoff_hz, sample_rate)
            : filter_lowpass(&v->filter, sample, v->filter_cutoff_hz, sample_rate);
    }

    if (v->distortion > 0.0f) {
        sample = tanhf(sample * (1.0f + v->distortion * 4.0f));
    }

    float gain = adsr_gain(&v->env, v->t, v->released_at);
    sample *= gain * v->volume;

    v->t += 1.0f / sample_rate;
    if (adsr_finished(&v->env, v->t, v->released_at)) {
        v->active = 0;
    }
    return sample;
}

int synth_voice_is_active(const SynthVoice *v) {
    return v->active;
}
