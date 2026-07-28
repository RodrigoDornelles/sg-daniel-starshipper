// Procedural music sequencer: a fixed rule-based song structure (no AI) —
// Intro (pad only) -> Riff A -> Riff B or C -> Solo -> Riff A -> loop —
// driving the bass/lead/pad/drum instrument modules beat by beat.
#ifndef STARTSHIPPER_MUSIC_H
#define STARTSHIPPER_MUSIC_H

typedef enum {
    MUSIC_NONE,
    MUSIC_MENU,
    MUSIC_PLAY,
    MUSIC_GAMEOVER
} MusicTrack;

void music_init(void);
void music_set_track(MusicTrack track);
float music_tick(float sample_rate);

#endif
