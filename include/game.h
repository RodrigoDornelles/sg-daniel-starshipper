// Game logic interface (source/game/main.c).
#ifndef STARTSHIPPER_GAME_H
#define STARTSHIPPER_GAME_H

#include <stdbool.h>

// Raw per-frame input state — game logic owns edge-detection (justPressed)
// on top of this, since it already keeps per-frame state for everything else
// and buttons mean different things in different states (R1 fires in PLAY,
// switches hangar tabs in HANGAR).
typedef struct {
    float stick_x; // -1..1, right positive
    float stick_y; // -1..1, down positive (matches source/main.js pad.ly convention)
    bool b_held;     // CROSS
    bool start_held;
    bool x_held;     // TRIANGLE
    bool l1_held;
    bool r1_held;
    bool l2_held;
    bool r2_held;
    bool up_held;
    bool down_held;
    bool left_held;
    bool right_held;
} GameInput;

void game_init(void);
void game_shutdown(void);
void game_reset(void);

// Called from the HW render context_reset callback once a GL context is
// live — GL meshes are (re)built here, never in game_init().
void game_gl_ready(void);
// Called from context_destroy, before the GL context goes away.
void game_gl_shutdown(void);

void game_update(const GameInput *input);
void game_render(unsigned int fbo, int width, int height);

// Backing store for RETRO_MEMORY_SAVE_RAM: credits + best score. The
// frontend memcpy's a prior .srm into this buffer right after
// retro_load_game() and memcpy's it back out on unload — no file I/O here.
void *game_get_save_data(void);
unsigned game_get_save_size(void);

#endif
