#ifndef STARTSHIPPER_SFX_JUMP_H
#define STARTSHIPPER_SFX_JUMP_H

// Ascending-pitch "whoosh" (spec's SOUND_JUMP) — reused here for the boost
// kick-in cue when the player's engines punch to full power.
void jump_init(void);
void jump_trigger(void);
float jump_tick(float sample_rate);

#endif
