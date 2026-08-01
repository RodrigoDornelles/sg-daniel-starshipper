#include "sfx.h"
#include "sfx_laser.h"
#include "sfx_explosion.h"
#include "sfx_jump.h"
#include "sfx_pickup.h"
#include "sfx_impact.h"
#include "sfx_ui.h"
#include "sfx_thrust.h"
#include "sfx_fire.h"

void sfx_init(void) {
    laser_init();
    explosion_init();
    jump_init();
    pickup_init();
    impact_init();
    ui_init();
    thrust_init();
    fire_init();
}

void play_sound(SoundId id) {
    switch (id) {
        case SOUND_LASER:          laser_trigger(); break;
        case SOUND_EXPLOSION_SMALL: explosion_trigger(0); break;
        case SOUND_EXPLOSION_BIG:   explosion_trigger(1); break;
        case SOUND_EXPLOSION_GRAVE: explosion_trigger(2); break;
        case SOUND_JUMP:            jump_trigger(); break;
        case SOUND_PICKUP:          pickup_trigger(); break;
        case SOUND_IMPACT:          impact_trigger(); break;
        case SOUND_UI_BLIP:         ui_trigger(); break;
    }
}

float sfx_tick(float sample_rate) {
    return laser_tick(sample_rate)
         + explosion_tick(sample_rate)
         + jump_tick(sample_rate)
         + pickup_tick(sample_rate)
         + impact_tick(sample_rate)
         + ui_tick(sample_rate)
         + thrust_tick(sample_rate)
         + fire_tick(sample_rate);
}
