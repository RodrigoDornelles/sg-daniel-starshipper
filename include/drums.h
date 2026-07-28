#ifndef STARTSHIPPER_DRUMS_H
#define STARTSHIPPER_DRUMS_H

// Kick/snare/hihat are one-shot percussive generators, not ADSR SynthVoices
// (the kick needs its own descending-pitch sweep), so they're driven by
// simple trigger + tick pairs instead.
void drums_init(void);
void drum_trigger_kick(void);
void drum_trigger_snare(void);
void drum_trigger_hihat(void);
float drums_tick(float sample_rate);

#endif
