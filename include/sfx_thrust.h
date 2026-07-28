#ifndef STARTSHIPPER_SFX_THRUST_H
#define STARTSHIPPER_SFX_THRUST_H

// Continuous turbine rumble, not a one-shot — amplitude tracks the ship's
// boost state instead of being triggered per event.
void thrust_init(void);
void thrust_set(int active, float power); // power 0..1
float thrust_tick(float sample_rate);

#endif
