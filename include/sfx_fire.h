#ifndef STARTSHIPPER_SFX_FIRE_H
#define STARTSHIPPER_SFX_FIRE_H

// Continuous hull-fire crackle, not a one-shot — amplitude tracks how badly
// damaged the ship is instead of being triggered per event (see thrust.c,
// same pattern).
void fire_init(void);
void fire_set(float severity); // severity 0..1
float fire_tick(float sample_rate);

#endif
