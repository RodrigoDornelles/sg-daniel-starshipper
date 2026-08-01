#ifndef STARTSHIPPER_PAD_H
#define STARTSHIPPER_PAD_H

#include "scales.h"

// Space pad: root/third/fifth held on slightly detuned sine voices for a
// wide, ambient "starfield" texture under the intro/riffs.
void pad_init(void);
// off = the usual lush sine pad; on = a drier, distorted saw drone for the
// game-over screen (shorter attack/release, no filter smoothing it out).
void pad_set_gameover(int on);
void pad_play_chord(const Chord *chord);
void pad_silence(void);
float pad_tick(float sample_rate);

#endif
