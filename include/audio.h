// Public entry point for the procedural audio engine: no WAV/OGG assets —
// music and every SFX are synthesized sample-by-sample and mixed down to
// interleaved PCM16 stereo here. See source/sound/ for the DSP internals.
#ifndef STARTSHIPPER_AUDIO_H
#define STARTSHIPPER_AUDIO_H

#include <stdint.h>

#include "music.h"      // MusicTrack, music_set_track()
#include "sfx.h"        // SoundId, play_sound()
#include "sfx_thrust.h" // thrust_set() - continuous engine rumble

void audio_init(unsigned sample_rate);
void audio_reset(void);

// Fills `frames` interleaved stereo PCM16 samples into `out`
// (out must hold at least frames*2 int16_t's).
void audio_generate(int16_t *out, unsigned frames);

#endif
