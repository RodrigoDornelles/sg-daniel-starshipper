#include "bass.h"
#include "synth.h"

#define BASS_VOICES 2
static SynthVoice voices[BASS_VOICES];
static int next_voice = 0;

void bass_init(void) {
    for (int i = 0; i < BASS_VOICES; i++) {
        synth_voice_init(&voices[i]);
        voices[i].osc1.shape = WAVE_SAW;
        voices[i].osc2.shape = WAVE_SQUARE;
        voices[i].osc2_mix = 0.35f;
        voices[i].env.attack = 0.005f;
        voices[i].env.decay = 0.08f;
        voices[i].env.sustain = 0.85f;
        voices[i].env.release = 0.12f;
        voices[i].use_filter = 1;
        voices[i].filter_cutoff_hz = 900.0f;
        voices[i].volume = 0.55f;
    }
}

void bass_play_chord(const Chord *chord) {
    float freq = chord_tone_freq(chord, 0, 0);
    SynthVoice *v = &voices[next_voice];
    next_voice = (next_voice + 1) % BASS_VOICES;
    synth_voice_note_on(v, freq, 0.55f);
}

void bass_silence(void) {
    for (int i = 0; i < BASS_VOICES; i++) synth_voice_note_off(&voices[i]);
}

float bass_tick(float sample_rate) {
    float sum = 0.0f;
    for (int i = 0; i < BASS_VOICES; i++) sum += synth_voice_tick(&voices[i], sample_rate);
    return sum;
}
