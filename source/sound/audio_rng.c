#include "audio_rng.h"

static uint32_t rng_state = 0x9E3779B9u; // never 0 - xorshift fixed point

void audio_rng_seed(uint32_t seed) {
    rng_state = seed ? seed : 0x9E3779B9u;
}

uint32_t audio_rng_next(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

float audio_randf(void) {
    return (float)(audio_rng_next() >> 8) / (float)(1u << 24);
}

float audio_randf_range(float lo, float hi) {
    return lo + audio_randf() * (hi - lo);
}

int audio_randi_range(int lo, int hi_inclusive) {
    int span = hi_inclusive - lo + 1;
    if (span <= 0) return lo;
    return lo + (int)(audio_rng_next() % (uint32_t)span);
}
