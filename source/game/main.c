// First vertical slice of the source/main.js -> C port: starfield, the
// scout/std/arwing ship, steering, and shooting. Not ported yet (deliberately
// cut for this pass): hangar/shop economy, save/load, enemies + collisions +
// scoring/waves, voxel asteroids, particle fx, and the bitmap-font HUD
// (score/wave/FPS numbers) — none of those are wired up, this just proves
// the GL2/GLES2 pipeline end-to-end with real game data and feel.
#include "game.h"
#include "gl.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Tuning constants, ported 1:1 from source/main.js
// ---------------------------------------------------------------------------
#define STAR_COUNT 56
#define STAR_SPREAD_X 90.0f
#define STAR_SPREAD_Y 55.0f
#define STAR_Z_NEAR 150.0f
#define STAR_Z_FAR 340.0f
#define MAX_BULLETS 40
#define BOUNDS_X 6.0f
#define BOUNDS_Y_LOW -2.4f
#define BOUNDS_Y_HIGH 3.4f
#define SHIP_ACCEL 0.16f
#define BASE_SPEED 0.85f
#define FIRE_COOLDOWN 8

#define CAM_X 0.0f
#define CAM_Y 1.4f
#define CAM_Z -9.0f
#define LOOK_X 0.0f
#define LOOK_Y 0.0f
#define LOOK_Z 24.0f

#define STATE_MENU 0
#define STATE_PLAY 1

static const float SPACE_COLOR[3] = {6.0f / 255.0f, 8.0f / 255.0f, 26.0f / 255.0f};
static const float PRIMARY_COLOR[3] = {0.72f, 0.76f, 0.86f}; // "arwing" loadout color
static const float ACCENT_COLOR[3] = {0.35f, 0.78f, 0.98f};

// ---------------------------------------------------------------------------
// Mesh building — reimplements MeshBuilder.box()/tri()'s baked directional
// shading (source/main.js faceLight) so unlit boxes (thrusters/stars/bullets)
// and lit ones (the hull) share one code path.
// ---------------------------------------------------------------------------
typedef struct { float x, y, z; } V3;
typedef struct { float cx, cy, cz, sx, sy, sz; bool accent; } BoxDef;

static float LIGHT_X, LIGHT_Y, LIGHT_Z;
static GlVertex s_scratch[4096];

static void init_light(void) {
    float x = 0.35f, y = 0.9f, z = -0.28f;
    float len = sqrtf(x * x + y * y + z * z);
    LIGHT_X = x / len;
    LIGHT_Y = y / len;
    LIGHT_Z = z / len;
}

static float face_light(float nx, float ny, float nz) {
    float d = nx * LIGHT_X + ny * LIGHT_Y + nz * LIGHT_Z;
    if (d < 0.0f) d = 0.0f;
    return 0.4f + 0.6f * d;
}

static void append_vert(GlVertex *buf, int *n, float x, float y, float z, float r, float g, float b, float a) {
    buf[*n].x = x; buf[*n].y = y; buf[*n].z = z;
    buf[*n].r = r; buf[*n].g = g; buf[*n].b = b; buf[*n].a = a;
    (*n)++;
}

static void append_tri(GlVertex *buf, int *n, V3 v0, V3 v1, V3 v2,
                        float nx, float ny, float nz, float r, float g, float b, float a, bool lit) {
    float lr = r, lg = g, lb = b;
    if (lit) {
        float l = face_light(nx, ny, nz);
        lr = r * l; lg = g * l; lb = b * l;
        if (lr > 1.0f) lr = 1.0f;
        if (lg > 1.0f) lg = 1.0f;
        if (lb > 1.0f) lb = 1.0f;
    }
    append_vert(buf, n, v0.x, v0.y, v0.z, lr, lg, lb, a);
    append_vert(buf, n, v1.x, v1.y, v1.z, lr, lg, lb, a);
    append_vert(buf, n, v2.x, v2.y, v2.z, lr, lg, lb, a);
}

static void append_quad(GlVertex *buf, int *n, V3 v0, V3 v1, V3 v2, V3 v3,
                         float nx, float ny, float nz, float r, float g, float b, float a, bool lit) {
    append_tri(buf, n, v0, v1, v2, nx, ny, nz, r, g, b, a, lit);
    append_tri(buf, n, v0, v2, v3, nx, ny, nz, r, g, b, a, lit);
}

static void append_box(GlVertex *buf, int *n, float cx, float cy, float cz,
                        float sx, float sy, float sz, float r, float g, float b, float a, bool lit) {
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    V3 A = {cx - hx, cy - hy, cz - hz}, B = {cx + hx, cy - hy, cz - hz};
    V3 C = {cx + hx, cy + hy, cz - hz}, D = {cx - hx, cy + hy, cz - hz};
    V3 E = {cx - hx, cy - hy, cz + hz}, F = {cx + hx, cy - hy, cz + hz};
    V3 G = {cx + hx, cy + hy, cz + hz}, H = {cx - hx, cy + hy, cz + hz};

    append_quad(buf, n, E, F, G, H, 0.0f, 0.0f, 1.0f, r, g, b, a, lit);
    append_quad(buf, n, B, A, D, C, 0.0f, 0.0f, -1.0f, r, g, b, a, lit);
    append_quad(buf, n, F, B, C, G, 1.0f, 0.0f, 0.0f, r, g, b, a, lit);
    append_quad(buf, n, A, E, H, D, -1.0f, 0.0f, 0.0f, r, g, b, a, lit);
    append_quad(buf, n, H, G, C, D, 0.0f, 1.0f, 0.0f, r, g, b, a, lit);
    append_quad(buf, n, A, B, F, E, 0.0f, -1.0f, 0.0f, r, g, b, a, lit);
}

// SHIPS.scout body + PARTS.wing/engine/nose "std" + COLORS.arwing, hardcoded:
// the hangar/shop system that lets the JS game pick a loadout isn't ported.
static const BoxDef SHIP_BOXES[] = {
    {0.0f, 0.0f, 0.0f, 0.5f, 0.28f, 1.6f, false},
    {0.0f, 0.14f, 0.32f, 0.24f, 0.14f, 0.5f, true},
    {-0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, false},
    {0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, false},
    {0.0f, 0.28f, -0.72f, 0.07f, 0.5f, 0.4f, false},
    {-1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, false},
    {1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, false},
    {-1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, true},
    {1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, true},
    {0.0f, 0.0f, -0.86f, 0.34f, 0.22f, 0.2f, false},
    {0.0f, 0.0f, 1.0f, 0.3f, 0.18f, 0.55f, false},
    {0.0f, -0.02f, 1.35f, 0.14f, 0.1f, 0.3f, true},
};

static GlMesh build_ship_mesh(void) {
    int n = 0;
    int count = (int)(sizeof(SHIP_BOXES) / sizeof(SHIP_BOXES[0]));
    for (int i = 0; i < count; i++) {
        const BoxDef *b = &SHIP_BOXES[i];
        const float *col = b->accent ? ACCENT_COLOR : PRIMARY_COLOR;
        append_box(s_scratch, &n, b->cx, b->cy, b->cz, b->sx, b->sy, b->sz, col[0], col[1], col[2], 1.0f, true);
    }
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_thruster_mesh(void) {
    int n = 0;
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.24f, 0.18f, 0.5f, 1.0f, 0.6f, 0.15f, 1.0f, false);
    append_box(s_scratch, &n, 0.0f, 0.0f, -0.28f, 0.12f, 0.1f, 0.4f, 1.0f, 0.9f, 0.5f, 1.0f, false);
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_star_mesh(void) {
    int n = 0;
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.42f, 0.42f, 0.42f, 1.0f, 1.0f, 1.0f, 1.0f, false);
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_bullet_mesh(void) {
    int n = 0;
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.12f, 0.12f, 1.1f, 0.35f, 1.0f, 0.55f, 1.0f, false);
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.05f, 0.05f, 0.5f, 0.9f, 1.0f, 0.9f, 1.0f, false);
    return gl_mesh_create(s_scratch, n);
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
typedef struct { float x, y, z, par, sc; } Star;
typedef struct { bool alive; float x, y, z; } Bullet;

typedef struct {
    int state;
    float shipX, shipY, velX, velY, roll, pitch, speed;
    float menuSpin;
    int fireTimer, fireSide;
    unsigned frame;
    bool boosting;
    bool prev_confirm, prev_quit;

    Star stars[STAR_COUNT];
    Bullet bullets[MAX_BULLETS];

    bool gl_ready;
    GlMesh ship_mesh, thruster_mesh, star_mesh, bullet_mesh;
} GameState;

static GameState g;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float randf(float lo, float hi) { return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo); }

static float apply_deadzone(float v) {
    if (v > -0.22f && v < 0.22f) return 0.0f;
    return clampf(v, -1.0f, 1.0f);
}

static void seed_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x = randf(-STAR_SPREAD_X, STAR_SPREAD_X);
        g.stars[i].y = randf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
        g.stars[i].z = randf(STAR_Z_NEAR, STAR_Z_FAR);
        g.stars[i].par = randf(0.04f, 0.16f);
        g.stars[i].sc = randf(0.35f, 0.95f);
    }
}

void game_init(void) {
    init_light();
    memset(&g, 0, sizeof(g));
    g.state = STATE_MENU;
    g.speed = BASE_SPEED;
    g.fireSide = 1;
    seed_stars();
}

void game_shutdown(void) {}

void game_reset(void) {
    g.shipX = g.shipY = g.velX = g.velY = g.roll = g.pitch = 0.0f;
    g.speed = BASE_SPEED;
    g.fireTimer = 0;
    g.frame = 0;
    g.boosting = false;
    memset(g.bullets, 0, sizeof(g.bullets));
}

void game_gl_ready(void) {
    // A context_reset without a preceding context_destroy means the previous
    // context was lost out from under us — its handles are already dead, so
    // don't try to free them, just rebuild.
    g.ship_mesh = build_ship_mesh();
    g.thruster_mesh = build_thruster_mesh();
    g.star_mesh = build_star_mesh();
    g.bullet_mesh = build_bullet_mesh();
    g.gl_ready = true;
}

void game_gl_shutdown(void) {
    if (!g.gl_ready) return;
    gl_mesh_destroy(&g.ship_mesh);
    gl_mesh_destroy(&g.thruster_mesh);
    gl_mesh_destroy(&g.star_mesh);
    gl_mesh_destroy(&g.bullet_mesh);
    g.gl_ready = false;
}

static void update_world(float scroll_speed) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x -= scroll_speed * g.stars[i].par;
        if (g.stars[i].x < -STAR_SPREAD_X) {
            g.stars[i].x += STAR_SPREAD_X * 2.0f;
            g.stars[i].y = randf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
            g.stars[i].z = randf(STAR_Z_NEAR, STAR_Z_FAR);
        }
    }
}

static void fire_bullet(void) {
    float wx = (g.fireSide > 0) ? 1.6f : -1.6f;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (g.bullets[i].alive) continue;
        g.bullets[i].alive = true;
        g.bullets[i].x = g.shipX + wx;
        g.bullets[i].y = g.shipY;
        g.bullets[i].z = 1.2f;
        break;
    }
    g.fireSide = -g.fireSide;
}

static void update_play(const GameInput *input) {
    float ix = apply_deadzone(input->stick_x);
    float iy = apply_deadzone(input->stick_y);
    g.boosting = input->boost_held;
    bool braking = input->brake_held;

    float target_speed = BASE_SPEED;
    if (g.boosting) target_speed *= 2.1f;
    else if (braking) target_speed *= 0.45f;
    g.speed = lerpf(g.speed, target_speed, 0.08f);

    g.velX = lerpf(g.velX, -ix * SHIP_ACCEL, 0.35f);
    g.velY = lerpf(g.velY, -iy * SHIP_ACCEL * 0.85f, 0.35f);
    g.shipX = clampf(g.shipX + g.velX, -BOUNDS_X, BOUNDS_X);
    g.shipY = clampf(g.shipY + g.velY, BOUNDS_Y_LOW, BOUNDS_Y_HIGH);

    g.roll = lerpf(g.roll, ix * 0.7f, 0.2f);
    g.pitch = lerpf(g.pitch, -iy * 0.35f, 0.2f);

    if (g.fireTimer > 0) g.fireTimer--;
    if (input->fire_held && g.fireTimer == 0) {
        fire_bullet();
        g.fireTimer = FIRE_COOLDOWN;
    }

    update_world(g.speed);

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g.bullets[i].alive) continue;
        g.bullets[i].z += 3.4f;
        if (g.bullets[i].z > 135.0f) g.bullets[i].alive = false;
    }

    g.frame++;
}

void game_update(const GameInput *input) {
    bool confirm_edge = input->confirm_held && !g.prev_confirm;
    bool quit_edge = input->quit_held && !g.prev_quit;
    g.prev_confirm = input->confirm_held;
    g.prev_quit = input->quit_held;

    if (g.state == STATE_MENU) {
        g.menuSpin += 0.02f;
        update_world(BASE_SPEED * 0.4f);
        if (confirm_edge) {
            game_reset();
            g.state = STATE_PLAY;
        }
        return;
    }

    if (quit_edge) {
        g.state = STATE_MENU;
        return;
    }
    update_play(input);
}

void game_render(unsigned int fbo, int width, int height) {
    if (!g.gl_ready) return;

    gl_begin_frame(fbo, width, height, SPACE_COLOR[0], SPACE_COLOR[1], SPACE_COLOR[2]);

    Mat4 view = mat4_look_at(CAM_X, CAM_Y, CAM_Z, LOOK_X, LOOK_Y, LOOK_Z, 0.0f, 1.0f, 0.0f);
    Mat4 proj = mat4_perspective(72.0f, (float)width / (float)height, 0.5f, 900.0f);
    gl_set_camera(view, proj);

    for (int i = 0; i < STAR_COUNT; i++) {
        Mat4 model = mat4_multiply(
            mat4_translate(g.stars[i].x, g.stars[i].y, g.stars[i].z),
            mat4_scale(g.stars[i].sc, g.stars[i].sc, g.stars[i].sc));
        gl_mesh_draw(&g.star_mesh, model);
    }

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g.bullets[i].alive) continue;
        Mat4 model = mat4_translate(g.bullets[i].x, g.bullets[i].y, g.bullets[i].z);
        gl_mesh_draw(&g.bullet_mesh, model);
    }

    Mat4 ship_model;
    if (g.state == STATE_PLAY) {
        ship_model = mat4_multiply(mat4_translate(g.shipX, g.shipY, 0.0f), mat4_rotate_xyz(g.pitch, 0.0f, g.roll));
    } else {
        ship_model = mat4_rotate_xyz(0.0f, g.menuSpin, 0.0f);
    }
    gl_mesh_draw(&g.ship_mesh, ship_model);

    if (g.state == STATE_PLAY) {
        float pulse = 0.75f + sinf((float)g.frame * 0.6f) * 0.2f + (g.boosting ? 0.6f : 0.0f);
        Mat4 thruster_model = mat4_multiply(
            mat4_translate(g.shipX, g.shipY, -1.15f),
            mat4_multiply(mat4_rotate_xyz(g.pitch, 0.0f, g.roll), mat4_scale(1.0f, 1.0f, pulse)));
        gl_mesh_draw(&g.thruster_mesh, thruster_model);

        float bw = 120.0f, bx = 16.0f, by = (float)height - 26.0f;
        gl_draw_quad2d(bx, by, bw, 10.0f, 0.12f, 0.16f, 0.24f, 1.0f, width, height);
        float frac = clampf((g.speed - BASE_SPEED * 0.4f) / (BASE_SPEED * 2.1f), 0.0f, 1.0f);
        const float *bar_col = g.boosting ? (const float[]){1.0f, 0.84f, 0.35f} : (const float[]){0.47f, 0.86f, 1.0f};
        gl_draw_quad2d(bx, by, bw * frac, 10.0f, bar_col[0], bar_col[1], bar_col[2], 1.0f, width, height);
    }
}
