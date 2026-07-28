// Classic ADSR. Times are in seconds, `sustain` is a 0..1 level (not a
// duration) — the sustain stage holds until note-off is signalled.
#ifndef STARTSHIPPER_ENVELOPE_H
#define STARTSHIPPER_ENVELOPE_H

typedef struct {
    float attack;
    float decay;
    float sustain;
    float release;
} ADSR;

// `t` is seconds since note-on. `released_at` is the `t` value note-off
// happened at, or a negative number while the note is still held.
float adsr_gain(const ADSR *env, float t, float released_at);

// True once the release tail has fully decayed and the voice can be freed.
int adsr_finished(const ADSR *env, float t, float released_at);

#endif
