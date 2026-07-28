#include "audio.h"
#include "audio_rng.h"
#include "mixer.h"

static float g_sample_rate = 44100.0f;

void audio_init(unsigned sample_rate) {
    g_sample_rate = (float)sample_rate;
    audio_rng_seed(0xC0FFEEu);
    mixer_init();
    music_set_track(MUSIC_NONE);
}

void audio_reset(void) {
    music_set_track(MUSIC_NONE);
}

void audio_generate(int16_t *out, unsigned frames) {
    for (unsigned i = 0; i < frames; i++) {
        float sample = mixer_tick(g_sample_rate);
        int16_t pcm = (int16_t)(sample * 32767.0f);
        out[i * 2 + 0] = pcm;
        out[i * 2 + 1] = pcm;
    }
}
