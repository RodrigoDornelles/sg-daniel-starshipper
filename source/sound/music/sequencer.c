#include "music.h"
#include "scales.h"
#include "bass.h"
#include "lead.h"
#include "pad.h"
#include "drums.h"
#include "audio_rng.h"

#include <stdbool.h>

#define STEPS_PER_BEAT 2 // eighth notes
#define BEATS_PER_CHORD 4
#define STEPS_PER_CHORD (STEPS_PER_BEAT * BEATS_PER_CHORD)
#define STEPS_PER_SECTION (STEPS_PER_CHORD * RIFF_LENGTH)

typedef enum { SECTION_INTRO, SECTION_RIFF_A, SECTION_RIFF_BC, SECTION_SOLO } Section;

static struct {
    MusicTrack track;
    float beat_dt;
    float step_timer;
    int step;
    Section section;
    const Riff *current_riff;
    int played_intro;
} S;

static const Riff *pick_bc_riff(void) {
    return audio_randi_range(0, 1) == 0 ? &RIFF_B : &RIFF_C;
}

static void enter_section(Section section) {
    S.section = section;
    S.step = 0;
    switch (section) {
        case SECTION_RIFF_BC: S.current_riff = pick_bc_riff(); break;
        default:              S.current_riff = &RIFF_A; break;
    }
}

static void advance_section(void) {
    switch (S.section) {
        case SECTION_INTRO:   enter_section(SECTION_RIFF_A); break;
        case SECTION_RIFF_A:  enter_section(SECTION_RIFF_BC); break;
        case SECTION_RIFF_BC: enter_section(SECTION_SOLO); break;
        case SECTION_SOLO:    enter_section(SECTION_RIFF_A); break;
    }
}

void music_init(void) {
    bass_init();
    lead_init();
    pad_init();
    drums_init();
    S.track = MUSIC_NONE;
    S.beat_dt = 60.0f / 140.0f;
    S.step_timer = 0.0f;
    S.played_intro = 0;
    enter_section(SECTION_INTRO);
}

void music_set_track(MusicTrack track) {
    if (track == S.track) return;
    S.track = track;
    bass_silence();
    lead_silence();
    pad_silence();
    S.step_timer = 0.0f;

    switch (track) {
        case MUSIC_PLAY:
            S.beat_dt = 60.0f / 140.0f;
            enter_section(S.played_intro ? SECTION_RIFF_A : SECTION_INTRO);
            break;
        case MUSIC_MENU:
            S.beat_dt = 60.0f / 76.0f;
            enter_section(SECTION_INTRO);
            break;
        case MUSIC_GAMEOVER:
            S.beat_dt = 60.0f / 50.0f;
            enter_section(SECTION_INTRO);
            break;
        default:
            break;
    }
}

// Full band: drums + bass + lead over the pad, following the song structure.
static void trigger_step_play(void) {
    int beat_in_chord = (S.step / STEPS_PER_BEAT) % BEATS_PER_CHORD;
    int chord_index = (S.step / STEPS_PER_CHORD) % RIFF_LENGTH;
    const Chord *chord = &S.current_riff->chords[chord_index];
    bool on_beat = (S.step % STEPS_PER_BEAT) == 0;

    if (S.step % STEPS_PER_CHORD == 0) {
        pad_play_chord(chord);
        if (S.section != SECTION_INTRO) bass_play_chord(chord);
    }

    if (S.section == SECTION_INTRO) {
        if (S.step == STEPS_PER_SECTION - 1) S.played_intro = 1;
        return; // pad-only intro, no rhythm section yet
    }

    if (on_beat) {
        drum_trigger_kick();
        if (beat_in_chord == 1 || beat_in_chord == 3) drum_trigger_snare();
    }
    drum_trigger_hihat();

    bool busy = (S.section == SECTION_SOLO);
    if (busy || on_beat) {
        lead_play_note(chord, S.step, busy);
    }
}

// MENU / GAMEOVER: pad-only ambience, slow chord changes, no rhythm section.
static void trigger_step_ambient(void) {
    int chord_index = (S.step / STEPS_PER_CHORD) % RIFF_LENGTH;
    if (S.step % STEPS_PER_CHORD == 0) {
        pad_play_chord(&S.current_riff->chords[chord_index]);
    }
}

float music_tick(float sample_rate) {
    if (S.track != MUSIC_NONE) {
        S.step_timer += 1.0f / sample_rate;
        float step_dt = S.beat_dt / STEPS_PER_BEAT;
        while (S.step_timer >= step_dt) {
            S.step_timer -= step_dt;
            if (S.track == MUSIC_PLAY) {
                trigger_step_play();
            } else {
                trigger_step_ambient();
            }
            S.step++;
            if (S.step >= STEPS_PER_SECTION) {
                if (S.track == MUSIC_PLAY) {
                    advance_section();
                } else {
                    S.step = 0; // ambient tracks just loop Riff A's changes forever
                }
            }
        }
    }

    float sample = bass_tick(sample_rate) + lead_tick(sample_rate) + pad_tick(sample_rate);
    if (S.track == MUSIC_PLAY) sample += drums_tick(sample_rate);
    return sample;
}
