#include "scales.h"

#include <math.h>

#define E2_HZ 82.4069f

const int E_NATURAL_MINOR[SCALE_DEGREE_COUNT] = {0, 2, 3, 5, 7, 8, 10};

const Chord CHORD_EM = {0, 1};
const Chord CHORD_C  = {8, 0};
const Chord CHORD_G  = {3, 0};
const Chord CHORD_D  = {10, 0};
const Chord CHORD_B  = {7, 0};

const Riff RIFF_A = {{{0, 1}, {8, 0}, {3, 0}, {10, 0}}};
const Riff RIFF_B = {{{0, 1}, {10, 0}, {8, 0}, {7, 0}}};
const Riff RIFF_C = {{{0, 1}, {3, 0}, {10, 0}, {5, 0}}};

float note_freq(int offset, int octave) {
    return E2_HZ * powf(2.0f, (float)offset / 12.0f + (float)octave);
}

float chord_tone_freq(const Chord *chord, int degree, int octave) {
    int third = chord->is_minor ? 3 : 4;
    int semis = chord->root_offset;
    switch (degree) {
        case 1: semis += third; break;
        case 2: semis += 7; break;
        default: break; // root
    }
    return note_freq(semis, octave);
}
