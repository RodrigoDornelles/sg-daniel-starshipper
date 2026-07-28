// Music theory tables for the procedural sequencer: E natural minor scale,
// the five triads used by the riffs, and the three riffs from the spec
// (Riff A = Em C G D, Riff B = Em D C B, Riff C = Em G D A). No AI, no
// randomness here — just fixed rule tables the sequencer walks.
#ifndef STARTSHIPPER_SCALES_H
#define STARTSHIPPER_SCALES_H

// Semitone offset from E (E=0 .. D#=11), one octave.
#define SCALE_DEGREE_COUNT 7
extern const int E_NATURAL_MINOR[SCALE_DEGREE_COUNT]; // {0,2,3,5,7,8,10}

typedef struct {
    int root_offset; // semitone offset from E, 0..11
    int is_minor;
} Chord;

extern const Chord CHORD_EM;
extern const Chord CHORD_C;
extern const Chord CHORD_G;
extern const Chord CHORD_D;
extern const Chord CHORD_B;

#define RIFF_LENGTH 4

typedef struct {
    Chord chords[RIFF_LENGTH];
} Riff;

extern const Riff RIFF_A; // Em C G D
extern const Riff RIFF_B; // Em D C B
extern const Riff RIFF_C; // Em G D A

// E2 (low bass E) as the reference pitch; `offset` is semitones from E,
// `octave` 0 = the E2..D#3 octave, 1 = one octave up, etc (may be negative).
float note_freq(int offset, int octave);

// Chord tone `degree` (0=root,1=third,2=fifth) transposed into `octave`.
float chord_tone_freq(const Chord *chord, int degree, int octave);

#endif
