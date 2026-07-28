#include "pad.h"
#include "synth.h"

#define PAD_VOICES 3
static SynthVoice voices[PAD_VOICES];

void pad_init(void) {
    for (int i = 0; i < PAD_VOICES; i++) {
        synth_voice_init(&voices[i]);
        voices[i].osc1.shape = WAVE_SINE;
        voices[i].osc2.shape = WAVE_SINE;
        voices[i].osc1.detune = 1.0f + (float)(i - 1) * 0.003f;
        voices[i].osc2.detune = 1.0f - (float)(i - 1) * 0.004f;
        voices[i].osc2_mix = 0.5f;
        voices[i].env.attack = 1.2f;
        voices[i].env.decay = 0.4f;
        voices[i].env.sustain = 0.8f;
        voices[i].env.release = 1.5f;
        voices[i].use_filter = 1;
        voices[i].filter_cutoff_hz = 1800.0f;
        voices[i].volume = 0.18f;
    }
}

void pad_play_chord(const Chord *chord) {
    for (int i = 0; i < PAD_VOICES; i++) {
        float freq = chord_tone_freq(chord, i, 2);
        synth_voice_note_on(&voices[i], freq, 0.18f);
    }
}

void pad_silence(void) {
    for (int i = 0; i < PAD_VOICES; i++) synth_voice_note_off(&voices[i]);
}

float pad_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < PAD_VOICES; i++) sum += synth_voice_tick(&voices[i], sample_rate);
    return sum;
}
