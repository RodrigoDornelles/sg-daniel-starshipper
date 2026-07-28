#include "lead.h"
#include "synth.h"
#include "audio_rng.h"

#define LEAD_VOICES 2
static SynthVoice voices[LEAD_VOICES];
static int next_voice = 0;

void lead_init(void) {
    for (int i = 0; i < LEAD_VOICES; i++) {
        synth_voice_init(&voices[i]);
        voices[i].osc1.shape = WAVE_SAW;
        voices[i].osc2.shape = WAVE_SAW;
        voices[i].osc2.detune = 1.006f;
        voices[i].osc2_mix = 0.5f;
        voices[i].env.attack = 0.008f;
        voices[i].env.decay = 0.1f;
        voices[i].env.sustain = 0.5f;
        voices[i].env.release = 0.15f;
        voices[i].use_filter = 1;
        voices[i].filter_cutoff_hz = 2600.0f;
        voices[i].distortion = 0.35f;
        voices[i].volume = 0.28f;
    }
}

void lead_play_note(const Chord *chord, int step, bool busy) {
    float freq;
    if (busy) {
        int degree = step % SCALE_DEGREE_COUNT;
        freq = note_freq(E_NATURAL_MINOR[degree], 1);
    } else {
        int degree = step % 3; // root/third/fifth arpeggio
        freq = chord_tone_freq(chord, degree, 1);
    }
    freq *= audio_randf_range(0.998f, 1.002f); // tiny humanize

    SynthVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % LEAD_VOICES;
    synth_voice_note_on(v, freq, 0.28f);
}

void lead_silence(void) {
    for (int i = 0; i < LEAD_VOICES; i++) synth_voice_note_off(&voices[i]);
}

float lead_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < LEAD_VOICES; i++) sum += synth_voice_tick(&voices[i], sample_rate);
    return sum;
}
