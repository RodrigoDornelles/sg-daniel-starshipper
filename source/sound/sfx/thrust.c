// Turbine/thruster: filtered noise + a low saw rumble, gain smoothed toward
// its target so boost on/off never clicks.
#include "sfx_thrust.h"
#include "osc.h"
#include "filter.h"

static float target_gain = 0.0f;
static float current_gain = 0.0f;
static float rumble_phase = 0.0f;
static Filter noise_filt;

void thrust_init(void) {
    target_gain = 0.0f;
    current_gain = 0.0f;
    rumble_phase = 0.0f;
    filter_reset(&noise_filt);
}

void thrust_set(int active, float power) {
    if (power < 0.0f) power = 0.0f;
    if (power > 1.0f) power = 1.0f;
    target_gain = active ? (0.15f + 0.25f * power) : 0.0f;
}

float thrust_tick(float sample_rate) {
    float rate = 1.0f / (0.05f * sample_rate); // ~50ms smoothing
    current_gain += (target_gain - current_gain) * rate;
    if (current_gain < 0.0005f && target_gain == 0.0f) return 0.0f;

    float noise = filter_lowpass(&noise_filt, osc_noise(), 900.0f, sample_rate);
    rumble_phase = phase_advance(rumble_phase, 55.0f, sample_rate);
    float rumble = osc_saw(rumble_phase) * 0.4f;

    return (noise * 0.7f + rumble * 0.3f) * current_gain;
}
