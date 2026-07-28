#ifndef STARTSHIPPER_LEAD_H
#define STARTSHIPPER_LEAD_H

#include "scales.h"
#include <stdbool.h>

void lead_init(void);
// Triggers one arpeggio/riff step over `chord`. `busy` widens the note
// choice to a full scale run, used during the solo section.
void lead_play_note(const Chord *chord, int step, bool busy);
void lead_silence(void);
float lead_tick(float sample_rate);

#endif
