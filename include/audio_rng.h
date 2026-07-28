// Small xorshift32 PRNG shared by the music and SFX generators so every
// procedural variation (riff choice, SFX pitch wobble, drum humanization)
// comes from one deterministic, seedable source.
#ifndef STARTSHIPPER_AUDIO_RNG_H
#define STARTSHIPPER_AUDIO_RNG_H

#include <stdint.h>

void audio_rng_seed(uint32_t seed);
uint32_t audio_rng_next(void);
float audio_randf(void);                       // [0, 1)
float audio_randf_range(float lo, float hi);    // [lo, hi)
int audio_randi_range(int lo, int hi_inclusive);

#endif
