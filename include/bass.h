#ifndef STARTSHIPPER_BASS_H
#define STARTSHIPPER_BASS_H

#include "scales.h"

void bass_init(void);
void bass_play_chord(const Chord *chord);
void bass_silence(void);
float bass_tick(float sample_rate);

#endif
