// SynthVoice: the one reusable building block behind every instrument
// (bass, lead, pad, kick/snare/hihat) — two detunable oscillators, an ADSR,
// an optional filter and a soft (tanhf) distortion stage. Instruments just
// pick a wave shape, envelope and filter cutoff and drive note_on/note_off.
#ifndef STARTSHIPPER_SYNTH_H
#define STARTSHIPPER_SYNTH_H

#include "envelope.h"
#include "filter.h"

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_NOISE
} WaveShape;

typedef struct {
    WaveShape shape;
    float phase;
    float detune; // multiplier applied to the voice frequency, e.g. 1.003f
} Oscillator;

typedef struct {
    Oscillator osc1;
    Oscillator osc2;
    float osc2_mix; // 0 = osc1 only, 1 = osc2 only

    ADSR env;

    int use_filter;
    int filter_is_highpass;
    float filter_cutoff_hz;
    Filter filter;

    float distortion; // 0 = clean, >0 = tanhf drive amount

    float frequency;
    float volume;

    float t;           // seconds since note_on
    float released_at; // seconds t was at on note_off, or -1 while held
    int active;
} SynthVoice;

void synth_voice_init(SynthVoice *v);
void synth_voice_note_on(SynthVoice *v, float frequency, float volume);
void synth_voice_note_off(SynthVoice *v);
float synth_voice_tick(SynthVoice *v, float sample_rate);
int synth_voice_is_active(const SynthVoice *v);

#endif
