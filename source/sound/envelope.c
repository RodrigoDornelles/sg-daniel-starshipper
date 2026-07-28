#include "envelope.h"

static float level_at(const ADSR *env, float t) {
    if (t < env->attack) {
        return env->attack > 0.0f ? t / env->attack : 1.0f;
    }
    float decay_t = t - env->attack;
    if (decay_t < env->decay) {
        float dt = env->decay > 0.0f ? decay_t / env->decay : 1.0f;
        return 1.0f + (env->sustain - 1.0f) * dt;
    }
    return env->sustain;
}

float adsr_gain(const ADSR *env, float t, float released_at) {
    float gain;
    if (released_at < 0.0f || t < released_at) {
        gain = level_at(env, t);
    } else {
        float level_at_release = level_at(env, released_at);
        float rt = t - released_at;
        gain = env->release > 0.0f
            ? level_at_release * (1.0f - rt / env->release)
            : 0.0f;
    }
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    return gain;
}

int adsr_finished(const ADSR *env, float t, float released_at) {
    return released_at >= 0.0f && (t - released_at) >= env->release;
}
