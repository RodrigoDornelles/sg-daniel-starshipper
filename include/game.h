// Game logic interface (source/game/main.c).
#ifndef STARTSHIPPER_GAME_H
#define STARTSHIPPER_GAME_H

#include <stdbool.h>

// Raw per-frame input state — game logic owns edge-detection (justPressed)
// on top of this, since it already keeps per-frame state for everything else.
typedef struct {
    float stick_x; // -1..1, right positive
    float stick_y; // -1..1, down positive (matches source/main.js pad.ly convention)
    bool fire_held;
    bool boost_held;
    bool brake_held;
    bool confirm_held; // CROSS/START
    bool quit_held;    // TRIANGLE
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

#endif
