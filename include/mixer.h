#ifndef STARTSHIPPER_MIXER_H
#define STARTSHIPPER_MIXER_H

void mixer_init(void);
// Sums the music and SFX buses and soft-clips (tanhf) into [-1, 1].
float mixer_tick(float sample_rate);

#endif
