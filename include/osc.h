// Bare oscillator shapes. `phase` is a 0..1 cycle fraction owned by the
// caller (SynthVoice / one-shot SFX state) and advanced externally, so these
// stay pure and reusable between synth voices and SFX generators.
#ifndef STARTSHIPPER_OSC_H
#define STARTSHIPPER_OSC_H

float osc_sine(float phase);
float osc_square(float phase);
float osc_saw(float phase);
float osc_triangle(float phase);
float osc_noise(void);

// Advances phase by freq_hz/sample_rate, wrapping back into [0, 1).
float phase_advance(float phase, float freq_hz, float sample_rate);

#endif
