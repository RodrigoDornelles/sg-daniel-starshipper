// Procedural SFX dispatcher: one call site (sfx_play) for every one-shot
// gameplay sound, backed by the per-effect generators in source/sound/sfx/.
#ifndef STARTSHIPPER_SFX_H
#define STARTSHIPPER_SFX_H

typedef enum {
    SOUND_LASER,
    SOUND_EXPLOSION_SMALL,
    SOUND_EXPLOSION_BIG,
    SOUND_JUMP,
    SOUND_PICKUP,
    SOUND_IMPACT,
    SOUND_UI_BLIP
} SoundId;

void sfx_init(void);
void play_sound(SoundId id);
float sfx_tick(float sample_rate);

#endif
