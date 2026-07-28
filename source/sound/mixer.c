#include "mixer.h"
#include "music.h"
#include "sfx.h"

#include <math.h>

#define MUSIC_GAIN 0.8f
#define SFX_GAIN 0.9f

void mixer_init(void) {
    music_init();
    sfx_init();
}

float mixer_tick(float sample_rate) {
    float sample = music_tick(sample_rate) * MUSIC_GAIN + sfx_tick(sample_rate) * SFX_GAIN;
    return tanhf(sample);
}
