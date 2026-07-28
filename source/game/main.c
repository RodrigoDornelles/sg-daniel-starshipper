// Full port of source/main.js — ship/hangar economy, planets, particles/fx,
// fighter + voxel-asteroid enemies, waves/score/lives, and a real hangar UI
// (fontstash text). Save is a deliberate stub: game_get_save_data/size
// return NULL/0, credits+best just live in GameState for the session.
#include "game.h"
#include "gl.h"
#include "text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------------
// Tuning constants, ported 1:1 from source/main.js
// ---------------------------------------------------------------------
#define STAR_COUNT 56
#define STAR_SPREAD_X 90.0f
#define STAR_SPREAD_Y 55.0f
#define STAR_Z_NEAR 150.0f
#define STAR_Z_FAR 340.0f
#define MAX_PARTICLES 72
#define MAX_PLANETS 4
#define MAX_BULLETS 40
#define MAX_ENEMIES 10
#define MAX_FX 12
#define BOUNDS_X 6.0f
#define BOUNDS_Y_LOW -2.4f
#define BOUNDS_Y_HIGH 3.4f
#define SHIP_ACCEL 0.16f
#define BASE_SPEED 0.85f
#define VOXEL_GRID 5
#define VOXEL_MAX 16
#define VOXEL_CELL 0.30f
#define VOXEL_DESTROY_AT 0

#define STATE_MENU 0
#define STATE_PLAY 1
#define STATE_OVER 2
#define STATE_HANGAR 3

#define SLOT_WING 0
#define SLOT_ENGINE 1
#define SLOT_CANNON 2
#define SLOT_NOSE 3
#define PART_SLOT_COUNT 4
#define PART_OPTION_COUNT 3

#define UPG_FIRERATE 0
#define UPG_MULTISHOT 1
#define UPG_DAMAGE 2
#define UPG_HULL 3
#define UPGRADE_COUNT 4

#define SHIP_COUNT 3
#define COLOR_COUNT 5

#define HANGAR_TAB_SHIPS 0
#define HANGAR_TAB_PARTS 1
#define HANGAR_TAB_COLORS 2
#define HANGAR_TAB_UPGRADES 3
#define HANGAR_TAB_LAUNCH 4
#define HANGAR_TAB_COUNT 5

#define CAM_X 0.0f
#define CAM_Y 1.4f
#define CAM_Z -9.0f
#define LOOK_X 0.0f
#define LOOK_Y 0.0f
#define LOOK_Z 24.0f

static const float SPACE_COLOR[3] = {6.0f / 255.0f, 8.0f / 255.0f, 26.0f / 255.0f};
static const float C_WHITE[4]  = {0.90f, 0.94f, 1.0f, 1.0f};
static const float C_CYAN[4]   = {0.47f, 0.86f, 1.0f, 1.0f};
static const float C_YELLOW[4] = {1.0f, 0.84f, 0.35f, 1.0f};
static const float C_RED[4]    = {1.0f, 0.35f, 0.35f, 1.0f};
static const float C_GREEN[4]  = {0.47f, 1.0f, 0.59f, 1.0f};
static const float C_DIM[4]    = {0.47f, 0.55f, 0.71f, 1.0f};

// ---------------------------------------------------------------------
// Math helpers
// ---------------------------------------------------------------------
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float randf(float lo, float hi) { return lo + ((float)rand() / (float)RAND_MAX) * (hi - lo); }
static int randi(int lo, int hi) { return lo + (rand() % (hi - lo + 1)); }
static bool edge(bool now, bool prev) { return now && !prev; }
static float apply_deadzone(float v) {
    if (v > -0.22f && v < 0.22f) return 0.0f;
    return clampf(v, -1.0f, 1.0f);
}

// ---------------------------------------------------------------------
// FPS meter — real wall-clock frame time (CLOCK_MONOTONIC), not the nominal
// 60.0 the core reports in retro_get_system_av_info. Smoothed so it reads
// steadily instead of jittering every frame.
// ---------------------------------------------------------------------
static float s_fps = 0.0f;
static struct timespec s_fps_last;
static bool s_fps_has_last = false;

static void update_fps(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (s_fps_has_last) {
        double dt = (double)(now.tv_sec - s_fps_last.tv_sec) + (double)(now.tv_nsec - s_fps_last.tv_nsec) * 1e-9;
        if (dt > 0.0005) {
            float instant = (float)(1.0 / dt);
            s_fps = (s_fps <= 0.0f) ? instant : lerpf(s_fps, instant, 0.1f);
        }
    }
    s_fps_last = now;
    s_fps_has_last = true;
}

// ---------------------------------------------------------------------
// Mesh building primitives — reimplements MeshBuilder.box()/tri()'s baked
// directional shading (source/main.js faceLight).
// ---------------------------------------------------------------------
typedef struct { float x, y, z; } V3;

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

static void append_voxel_face(GlVertex *buf, int *n, float cx, float cy, float cz, float hs, int face,
                               float r, float g, float b, float a) {
    V3 p0, p1, p2, p3;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    switch (face) {
        case 0: // +X
            p0 = (V3){cx + hs, cy - hs, cz - hs}; p1 = (V3){cx + hs, cy + hs, cz - hs};
            p2 = (V3){cx + hs, cy + hs, cz + hs}; p3 = (V3){cx + hs, cy - hs, cz + hs};
            nx = 1.0f; break;
        case 1: // -X
            p0 = (V3){cx - hs, cy + hs, cz - hs}; p1 = (V3){cx - hs, cy - hs, cz - hs};
            p2 = (V3){cx - hs, cy - hs, cz + hs}; p3 = (V3){cx - hs, cy + hs, cz + hs};
            nx = -1.0f; break;
        case 2: // +Y
            p0 = (V3){cx - hs, cy + hs, cz - hs}; p1 = (V3){cx + hs, cy + hs, cz - hs};
            p2 = (V3){cx + hs, cy + hs, cz + hs}; p3 = (V3){cx - hs, cy + hs, cz + hs};
            ny = 1.0f; break;
        case 3: // -Y
            p0 = (V3){cx + hs, cy - hs, cz - hs}; p1 = (V3){cx - hs, cy - hs, cz - hs};
            p2 = (V3){cx - hs, cy - hs, cz + hs}; p3 = (V3){cx + hs, cy - hs, cz + hs};
            ny = -1.0f; break;
        case 4: // +Z
            p0 = (V3){cx - hs, cy - hs, cz + hs}; p1 = (V3){cx + hs, cy - hs, cz + hs};
            p2 = (V3){cx + hs, cy + hs, cz + hs}; p3 = (V3){cx - hs, cy + hs, cz + hs};
            nz = 1.0f; break;
        default: // -Z
            p0 = (V3){cx + hs, cy - hs, cz - hs}; p1 = (V3){cx - hs, cy - hs, cz - hs};
            p2 = (V3){cx - hs, cy + hs, cz - hs}; p3 = (V3){cx + hs, cy + hs, cz - hs};
            nz = -1.0f; break;
    }
    append_quad(buf, n, p0, p1, p2, p3, nx, ny, nz, r, g, b, a, true);
}

// ---------------------------------------------------------------------
// Data tables — SHIPS/PARTS/COLORS/UPGRADES, ported 1:1 from source/main.js
// ---------------------------------------------------------------------
typedef struct { float cx, cy, cz, sx, sy, sz; bool accent; } BoxDef;

static const BoxDef SCOUT_BODY[] = {
    {0.0f, 0.0f, 0.0f, 0.5f, 0.28f, 1.6f, false},
    {0.0f, 0.14f, 0.32f, 0.24f, 0.14f, 0.5f, true},
    {-0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, false},
    {0.66f, 0.14f, -0.35f, 0.07f, 0.4f, 0.42f, false},
    {0.0f, 0.28f, -0.72f, 0.07f, 0.5f, 0.4f, false},
};
static const BoxDef RAPTOR_BODY[] = {
    {0.0f, 0.0f, 0.0f, 0.42f, 0.22f, 1.5f, false},
    {0.0f, 0.12f, 0.28f, 0.2f, 0.12f, 0.45f, true},
    {0.0f, 0.22f, -0.65f, 0.06f, 0.45f, 0.35f, false},
};
static const BoxDef TITAN_BODY[] = {
    {0.0f, 0.0f, 0.0f, 0.62f, 0.34f, 1.75f, false},
    {0.0f, 0.16f, 0.35f, 0.28f, 0.16f, 0.55f, true},
    {-0.72f, 0.12f, -0.2f, 0.08f, 0.35f, 0.5f, false},
    {0.72f, 0.12f, -0.2f, 0.08f, 0.35f, 0.5f, false},
};

typedef struct {
    const char *name;
    int price;
    float cooldown, bullets, spread, damage, lives, speed;
    const BoxDef *body;
    int body_count;
} ShipDef;

static const ShipDef SHIPS[SHIP_COUNT] = {
    {"Scout",  0,   8.0f, 1.0f, 0.0f, 1.0f, 3.0f, 1.0f,  SCOUT_BODY,  5},
    {"Raptor", 120, 6.0f, 1.0f, 0.0f, 1.0f, 2.0f, 1.15f, RAPTOR_BODY, 3},
    {"Titan",  200, 10.0f, 1.0f, 0.0f, 2.0f, 5.0f, 0.82f, TITAN_BODY, 4},
};

static const BoxDef WING_STD_MESH[] = {
    {-1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, false},
    {1.0f, 0.0f, -0.1f, 1.4f, 0.06f, 0.6f, false},
    {-1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, true},
    {1.75f, 0.0f, -0.15f, 0.3f, 0.1f, 0.45f, true},
};
static const BoxDef WING_WIDE_MESH[] = {
    {-1.2f, 0.0f, -0.05f, 1.75f, 0.05f, 0.55f, false},
    {1.2f, 0.0f, -0.05f, 1.75f, 0.05f, 0.55f, false},
};
static const BoxDef WING_DART_MESH[] = {
    {-0.85f, 0.0f, -0.2f, 1.0f, 0.04f, 0.7f, false},
    {0.85f, 0.0f, -0.2f, 1.0f, 0.04f, 0.7f, false},
};
static const BoxDef ENGINE_STD_MESH[]   = {{0.0f, 0.0f, -0.86f, 0.34f, 0.22f, 0.2f, false}};
static const BoxDef ENGINE_TURBO_MESH[] = {{0.0f, 0.0f, -0.95f, 0.42f, 0.28f, 0.28f, true}};
static const BoxDef ENGINE_HEAVY_MESH[] = {{0.0f, 0.0f, -0.88f, 0.48f, 0.3f, 0.32f, false}};
static const BoxDef CANNON_TWIN_MESH[] = {
    {-0.35f, -0.05f, 0.5f, 0.12f, 0.12f, 0.35f, true},
    {0.35f, -0.05f, 0.5f, 0.12f, 0.12f, 0.35f, true},
};
static const BoxDef CANNON_RAIL_MESH[] = {{0.0f, -0.08f, 0.65f, 0.18f, 0.14f, 0.55f, true}};
static const BoxDef NOSE_STD_MESH[] = {
    {0.0f, 0.0f, 1.0f, 0.3f, 0.18f, 0.55f, false},
    {0.0f, -0.02f, 1.35f, 0.14f, 0.1f, 0.3f, true},
};
static const BoxDef NOSE_PROBE_MESH[] = {
    {0.0f, 0.0f, 1.15f, 0.22f, 0.14f, 0.75f, false},
    {0.0f, 0.0f, 1.55f, 0.08f, 0.08f, 0.35f, true},
};
static const BoxDef NOSE_BLUNT_MESH[] = {{0.0f, 0.0f, 0.95f, 0.38f, 0.22f, 0.5f, false}};

typedef struct {
    const char *name;
    int price;
    float mod_cooldown, mod_bullets, mod_spread, mod_damage, mod_lives, mod_speed;
    const BoxDef *mesh;
    int mesh_count;
} PartDef;

static const PartDef PARTS[PART_SLOT_COUNT][PART_OPTION_COUNT] = {
    [SLOT_WING] = {
        {"Std Wings",  0,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,   WING_STD_MESH, 4},
        {"Wide Wings", 80, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.05f, WING_WIDE_MESH, 2},
        {"Dart Wings", 65, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.12f, WING_DART_MESH, 2},
    },
    [SLOT_ENGINE] = {
        {"Std Engine", 0,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,   ENGINE_STD_MESH, 1},
        {"Turbo",      90, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.18f, ENGINE_TURBO_MESH, 1},
        {"Heavy",      70, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.08f, ENGINE_HEAVY_MESH, 1},
    },
    [SLOT_CANNON] = {
        {"Pulse Laser", 0,   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, NULL, 0},
        {"Twin Mount",  100, 0.0f, 1.0f, 0.35f, 0.0f, 0.0f, 0.0f, CANNON_TWIN_MESH, 2},
        {"Rail Gun",    110, 2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, CANNON_RAIL_MESH, 1},
    },
    [SLOT_NOSE] = {
        {"Std Nose",   0,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, NOSE_STD_MESH, 2},
        {"Probe Nose", 55, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, NOSE_PROBE_MESH, 2},
        {"Blunt Nose", 45, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, NOSE_BLUNT_MESH, 1},
    },
};

typedef struct {
    const char *name;
    int price;
    float primary[3];
    float accent[3];
} ColorDef;

static const ColorDef COLORS[COLOR_COUNT] = {
    {"Arwing",  0,  {0.72f, 0.76f, 0.86f}, {0.35f, 0.78f, 0.98f}},
    {"Crimson", 40, {0.82f, 0.22f, 0.18f}, {1.0f, 0.45f, 0.15f}},
    {"Jade",    40, {0.22f, 0.72f, 0.48f}, {0.55f, 1.0f, 0.72f}},
    {"Void",    55, {0.28f, 0.26f, 0.38f}, {0.72f, 0.35f, 0.95f}},
    {"Gold",    80, {0.85f, 0.72f, 0.22f}, {1.0f, 0.92f, 0.45f}},
};

typedef struct { const char *name; int max_level; int base_cost; } UpgradeDef;
static const UpgradeDef UPGRADES[UPGRADE_COUNT] = {
    {"Fire Rate",  5, 45},
    {"Multi-Shot", 3, 70},
    {"Damage",     4, 55},
    {"Hull",       3, 60},
};

typedef struct { float br, bg, bb, hr, hg, hb, x, y, z, sc; } PlanetDef;
static const PlanetDef PLANET_DEFS[MAX_PLANETS] = {
    {0.22f, 0.16f, 0.42f, 0.55f, 0.42f, 0.82f, -34.0f, 3.5f, 165.0f, 2.4f},
    {0.18f, 0.22f, 0.38f, 0.38f, 0.48f, 0.72f, 38.0f, 11.0f, 235.0f, 1.8f},
    {0.32f, 0.14f, 0.12f, 0.72f, 0.38f, 0.22f, 14.0f, 14.5f, 145.0f, 1.4f},
    {0.12f, 0.18f, 0.28f, 0.28f, 0.36f, 0.58f, -22.0f, 7.5f, 205.0f, 1.1f},
};

// ---------------------------------------------------------------------
// Loadout / ownership / stats
// ---------------------------------------------------------------------
typedef struct {
    int ship;
    int parts[PART_SLOT_COUNT];
    int color;
} Loadout;

typedef struct {
    bool owned_ships[SHIP_COUNT];
    bool owned_parts[PART_SLOT_COUNT][PART_OPTION_COUNT];
    bool owned_colors[COLOR_COUNT];
    int upgrade_level[UPGRADE_COUNT];
    int credits;
    int best;
    Loadout loadout;
} Profile;

typedef struct { int cooldown, bullets, damage, lives; float spread, speed; } ShipStats;

static ShipStats compute_stats(const Profile *p) {
    const ShipDef *hull = &SHIPS[p->loadout.ship];
    float cooldown = hull->cooldown, bullets = hull->bullets, spread = hull->spread,
          damage = hull->damage, lives = hull->lives, speed = hull->speed;

    for (int slot = 0; slot < PART_SLOT_COUNT; slot++) {
        const PartDef *pt = &PARTS[slot][p->loadout.parts[slot]];
        cooldown += pt->mod_cooldown;
        bullets += pt->mod_bullets;
        spread += pt->mod_spread;
        damage += pt->mod_damage;
        lives += pt->mod_lives;
        speed += pt->mod_speed;
    }

    cooldown -= (float)p->upgrade_level[UPG_FIRERATE];
    int ms = p->upgrade_level[UPG_MULTISHOT];
    bullets += (float)ms;
    if (ms > 0 && bullets > 1.0f) {
        float want = 0.22f + (float)ms * 0.14f;
        if (want > spread) spread = want;
    }
    damage += (float)p->upgrade_level[UPG_DAMAGE];
    lives += (float)p->upgrade_level[UPG_HULL];

    ShipStats s;
    s.cooldown = (int)(cooldown < 3.0f ? 3.0f : cooldown);
    s.bullets = (int)(bullets < 1.0f ? 1.0f : bullets);
    s.damage = (int)(damage < 1.0f ? 1.0f : damage);
    s.lives = (int)(lives < 1.0f ? 1.0f : lives);
    s.spread = spread;
    s.speed = speed < 0.5f ? 0.5f : speed;
    return s;
}

static int upgrade_cost(int key, int level) { return UPGRADES[key].base_cost + level * 35; }

// ---------------------------------------------------------------------
// Static mesh builders
// ---------------------------------------------------------------------
static GlMesh build_ship_mesh(const Loadout *loadout) {
    int n = 0;
    const ShipDef *hull = &SHIPS[loadout->ship];
    const ColorDef *col = &COLORS[loadout->color];
    for (int i = 0; i < hull->body_count; i++) {
        const BoxDef *b = &hull->body[i];
        const float *c = b->accent ? col->accent : col->primary;
        append_box(s_scratch, &n, b->cx, b->cy, b->cz, b->sx, b->sy, b->sz, c[0], c[1], c[2], 1.0f, true);
    }
    for (int slot = 0; slot < PART_SLOT_COUNT; slot++) {
        const PartDef *pt = &PARTS[slot][loadout->parts[slot]];
        for (int i = 0; i < pt->mesh_count; i++) {
            const BoxDef *b = &pt->mesh[i];
            const float *c = b->accent ? col->accent : col->primary;
            append_box(s_scratch, &n, b->cx, b->cy, b->cz, b->sx, b->sy, b->sz, c[0], c[1], c[2], 1.0f, true);
        }
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

static GlMesh build_enemy_mesh(void) {
    int n = 0;
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.85f, 0.42f, 0.9f, 0.85f, 0.2f, 0.18f, 1.0f, true);
    append_box(s_scratch, &n, 0.0f, 0.0f, -0.5f, 0.32f, 0.32f, 0.45f, 1.0f, 0.45f, 0.15f, 1.0f, true);
    append_box(s_scratch, &n, 0.0f, 0.14f, 0.35f, 0.22f, 0.16f, 0.4f, 0.55f, 0.12f, 0.12f, 1.0f, true);
    append_box(s_scratch, &n, -0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f, true);
    append_box(s_scratch, &n, 0.85f, 0.0f, 0.0f, 0.7f, 0.08f, 0.5f, 0.72f, 0.18f, 0.16f, 1.0f, true);
    append_box(s_scratch, &n, -1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f, true);
    append_box(s_scratch, &n, 1.2f, 0.0f, 0.0f, 0.22f, 0.28f, 0.3f, 0.95f, 0.75f, 0.15f, 1.0f, true);
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_explosion_mesh(void) {
    int n = 0;
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.28f, 0.28f, 0.28f, 1.0f, 0.98f, 0.88f, 1.0f, false);
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.62f, 0.62f, 0.62f, 1.0f, 0.82f, 0.18f, 1.0f, false);
    append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.95f, 0.95f, 0.95f, 0.85f, 0.28f, 0.08f, 1.0f, false);
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_particle_mesh(int kind) {
    int n = 0;
    switch (kind) {
        case 0: // boost
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.35f, 1.0f, 0.72f, 0.18f, 1.0f, false);
            append_box(s_scratch, &n, 0.0f, 0.0f, -0.12f, 0.03f, 0.03f, 0.22f, 1.0f, 0.95f, 0.55f, 1.0f, false);
            break;
        case 1: // spark
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.025f, 0.025f, 0.42f, 1.0f, 0.95f, 0.55f, 1.0f, false);
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.012f, 0.012f, 0.65f, 1.0f, 1.0f, 0.85f, 1.0f, false);
            break;
        case 3: // flash
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.07f, 0.07f, 0.07f, 1.0f, 0.98f, 0.82f, 1.0f, false);
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.03f, 0.03f, 0.28f, 1.0f, 1.0f, 0.92f, 1.0f, false);
            break;
        case 4: // smoke
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.09f, 0.09f, 0.09f, 0.35f, 0.32f, 0.38f, 1.0f, false);
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 0.22f, 0.20f, 0.24f, 1.0f, false);
            break;
        default: // ember
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.06f, 0.06f, 0.06f, 1.0f, 0.62f, 0.18f, 1.0f, false);
            append_box(s_scratch, &n, 0.0f, 0.0f, 0.0f, 0.04f, 0.04f, 0.04f, 0.85f, 0.45f, 0.12f, 1.0f, false);
            break;
    }
    return gl_mesh_create(s_scratch, n);
}

static GlMesh build_planet_dome_mesh(float br, float bg, float bb, float hr, float hg, float hb) {
    int n = 0;
    const int rings = 10, sectors = 18;
    for (int i = 0; i < rings; i++) {
        float lat0 = (float)M_PI * 0.5f * (float)i / (float)rings;
        float lat1 = (float)M_PI * 0.5f * (float)(i + 1) / (float)rings;
        for (int j = 0; j < sectors; j++) {
            float lon0 = (float)M_PI * 2.0f * (float)j / (float)sectors;
            float lon1 = (float)M_PI * 2.0f * (float)(j + 1) / (float)sectors;

            V3 v00 = {cosf(lat0) * cosf(lon0), cosf(lat0) * sinf(lon0), sinf(lat0)};
            V3 v01 = {cosf(lat0) * cosf(lon1), cosf(lat0) * sinf(lon1), sinf(lat0)};
            V3 v10 = {cosf(lat1) * cosf(lon0), cosf(lat1) * sinf(lon0), sinf(lat1)};
            V3 v11 = {cosf(lat1) * cosf(lon1), cosf(lat1) * sinf(lon1), sinf(lat1)};

            float t0 = sinf(lat0), t1 = sinf(lat1);
            float r0 = lerpf(br, hr, t0), g0 = lerpf(bg, hg, t0), b0 = lerpf(bb, hb, t0);
            float r1 = lerpf(br, hr, t1), g1 = lerpf(bg, hg, t1), b1 = lerpf(bb, hb, t1);
            float cr = (r0 + r0 + r1 + r1) * 0.25f, cg = (g0 + g0 + g1 + g1) * 0.25f, cb = (b0 + b0 + b1 + b1) * 0.25f;

            append_tri(s_scratch, &n, v00, v01, v11, 0.0f, 0.0f, 1.0f, cr, cg, cb, 1.0f, false);
            append_tri(s_scratch, &n, v00, v11, v10, 0.0f, 0.0f, 1.0f, cr, cg, cb, 1.0f, false);
        }
    }
    return gl_mesh_create(s_scratch, n);
}

// ---------------------------------------------------------------------
// Game state structs
// ---------------------------------------------------------------------
typedef struct { float x, y, z, par, sc; } Star;
typedef struct { float x, y, z, sc; } Planet;
typedef struct { bool alive; float x, y, z, pz; int damage; } Bullet;
typedef struct { bool alive; int slot_kind; float x, y, z, vx, vy, vz; int life, max_life; float sc, spin; } Particle;
typedef struct { bool alive; float x, y, z; int life, max_life; float base_sc, spin; } Fx;

typedef struct {
    unsigned char grid[VOXEL_GRID * VOXEL_GRID * VOXEL_GRID];
    int count;
    unsigned seed;
} VoxelAsteroid;

typedef struct {
    bool active;
    int type; // 0 fighter, 1 voxel asteroid
    float x, y, z, rx, ry, rz, spin, weave, phase;
    int hp;
    float radius, sc;
    VoxelAsteroid voxel;
    GlMesh voxel_mesh;
} Enemy;

typedef struct {
    int state;
    int score, wave, lives, invuln;
    float shake;
    unsigned frame;
    float shipX, shipY, velX, velY, roll, pitch, speed;
    float menuSpin;
    int fireTimer, fireSide, spawnTimer;
    bool boosting;
    int runEarned;

    Profile profile;
    ShipStats stats;

    int hangTab, hangSel, partSlot, hangNavLock;
    bool hangFocusTabs; // cursor is on the tab row (true) or browsing the list (false)

    Star stars[STAR_COUNT];
    Planet planets[MAX_PLANETS];
    Bullet bullets[MAX_BULLETS];
    Particle particles[MAX_PARTICLES];
    Fx fx[MAX_FX];
    Enemy enemies[MAX_ENEMIES];

    GameInput prev_input;

    bool gl_ready;
    GlMesh ship_mesh, thruster_mesh, star_mesh, bullet_mesh, enemy_mesh, explosion_mesh;
    GlMesh planet_mesh[MAX_PLANETS];
    GlMesh particle_mesh[5];
} GameState;

static GameState g;

static void start_run(void); // forward: hangar_confirm()'s LAUNCH tab starts a run

// ---------------------------------------------------------------------
// Voxel asteroids (enemy type 1) — 5x5x5 grid, grown from a seed, carved on
// hit, face-culled mesh rebuilt whenever the grid changes.
// ---------------------------------------------------------------------
static int voxel_idx(int x, int y, int z) { return x + y * VOXEL_GRID + z * VOXEL_GRID * VOXEL_GRID; }

static int voxel_get(const VoxelAsteroid *v, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= VOXEL_GRID || y >= VOXEL_GRID || z >= VOXEL_GRID) return 0;
    return v->grid[voxel_idx(x, y, z)];
}

static void voxel_set(VoxelAsteroid *v, int x, int y, int z, int val) {
    if (x < 0 || y < 0 || z < 0 || x >= VOXEL_GRID || y >= VOXEL_GRID || z >= VOXEL_GRID) return;
    int i = voxel_idx(x, y, z);
    if (val && !v->grid[i]) v->count++;
    else if (!val && v->grid[i]) v->count--;
    v->grid[i] = val ? 1 : 0;
}

static int voxel_hash(int ix, int iy, int iz, int seed) {
    return ((ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ seed) & 255;
}

static void voxel_color(int ix, int iy, int iz, int seed, float *r, float *g, float *b) {
    float t = (float)voxel_hash(ix, iy, iz, seed) / 255.0f;
    *r = lerpf(0.40f, 0.64f, t);
    *g = lerpf(0.36f, 0.56f, t);
    *b = lerpf(0.32f, 0.50f, t);
}

static V3 voxel_local_from_grid(int ix, int iy, int iz) {
    float half = VOXEL_GRID * VOXEL_CELL * 0.5f;
    return (V3){(ix + 0.5f) * VOXEL_CELL - half, (iy + 0.5f) * VOXEL_CELL - half, (iz + 0.5f) * VOXEL_CELL - half};
}

static void voxel_reset(VoxelAsteroid *v, unsigned seed) {
    memset(v->grid, 0, sizeof(v->grid));
    v->count = 0;
    v->seed = seed;

    int c = VOXEL_GRID / 2;
    int target = 6 + (voxel_hash(c, c, c, (int)seed) % (VOXEL_MAX - 5));
    voxel_set(v, c, c, c, 1);

    static const int dirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    int fails = 0;
    while (v->count < target && fails < 80) {
        int pick = voxel_hash((int)seed, v->count, fails, 0) % v->count;
        bool found = false;
        int ox = 0, oy = 0, oz = 0;
        for (int z = 0; z < VOXEL_GRID && !found; z++) {
            for (int y = 0; y < VOXEL_GRID && !found; y++) {
                for (int x = 0; x < VOXEL_GRID; x++) {
                    if (!voxel_get(v, x, y, z)) continue;
                    if (pick == 0) { ox = x; oy = y; oz = z; found = true; break; }
                    pick--;
                }
            }
        }
        if (!found) break;

        int di = voxel_hash(ox, oy, oz, (int)seed + fails) % 6;
        int nx = ox + dirs[di][0], ny = oy + dirs[di][1], nz = oz + dirs[di][2];
        if (nx < 0 || ny < 0 || nz < 0 || nx >= VOXEL_GRID || ny >= VOXEL_GRID || nz >= VOXEL_GRID || voxel_get(v, nx, ny, nz)) {
            fails++;
            continue;
        }
        voxel_set(v, nx, ny, nz, 1);
        fails = 0;
    }
}

static float voxel_collision_radius(const VoxelAsteroid *v) {
    if (v->count <= 0) return 0.0f;
    float max_dist = 0.0f;
    float corner = VOXEL_CELL * 0.866f;
    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++) {
                if (!voxel_get(v, x, y, z)) continue;
                V3 p = voxel_local_from_grid(x, y, z);
                float d = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z) + corner;
                if (d > max_dist) max_dist = d;
            }
    return max_dist;
}

static int voxel_build_mesh_verts(const VoxelAsteroid *v, GlVertex *buf) {
    int n = 0;
    float hs = VOXEL_CELL * 0.5f;
    for (int z = 0; z < VOXEL_GRID; z++) {
        for (int y = 0; y < VOXEL_GRID; y++) {
            for (int x = 0; x < VOXEL_GRID; x++) {
                if (!voxel_get(v, x, y, z)) continue;
                V3 p = voxel_local_from_grid(x, y, z);
                float r, gc, b;
                voxel_color(x, y, z, (int)v->seed, &r, &gc, &b);
                if (x + 1 >= VOXEL_GRID || !voxel_get(v, x + 1, y, z)) append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 0, r, gc, b, 1.0f);
                if (x - 1 < 0 || !voxel_get(v, x - 1, y, z))          append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 1, r, gc, b, 1.0f);
                if (y + 1 >= VOXEL_GRID || !voxel_get(v, x, y + 1, z)) append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 2, r, gc, b, 1.0f);
                if (y - 1 < 0 || !voxel_get(v, x, y - 1, z))          append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 3, r, gc, b, 1.0f);
                if (z + 1 >= VOXEL_GRID || !voxel_get(v, x, y, z + 1)) append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 4, r, gc, b, 1.0f);
                if (z - 1 < 0 || !voxel_get(v, x, y, z - 1))          append_voxel_face(buf, &n, p.x, p.y, p.z, hs, 5, r, gc, b, 1.0f);
            }
        }
    }
    return n;
}

static void voxel_rebuild_gl_mesh(VoxelAsteroid *v, GlMesh *mesh, bool create) {
    int n = voxel_build_mesh_verts(v, s_scratch);
    if (create) *mesh = gl_mesh_create(s_scratch, n);
    else gl_mesh_update(mesh, s_scratch, n);
}

static V3 rotate_x(V3 v, float a) { float c = cosf(a), s = sinf(a); return (V3){v.x, v.y * c - v.z * s, v.y * s + v.z * c}; }
static V3 rotate_y(V3 v, float a) { float c = cosf(a), s = sinf(a); return (V3){v.x * c + v.z * s, v.y, -v.x * s + v.z * c}; }
static V3 rotate_z(V3 v, float a) { float c = cosf(a), s = sinf(a); return (V3){v.x * c - v.y * s, v.x * s + v.y * c, v.z}; }

static V3 world_to_local(float wx, float wy, float wz, float ex, float ey, float ez, float sc, float rx, float ry, float rz) {
    V3 v = {(wx - ex) / sc, (wy - ey) / sc, (wz - ez) / sc};
    v = rotate_z(v, -rz);
    v = rotate_y(v, -ry);
    v = rotate_x(v, -rx);
    return v;
}

static V3 local_to_world(float lx, float ly, float lz, float ex, float ey, float ez, float sc, float rx, float ry, float rz) {
    V3 v = rotate_x((V3){lx, ly, lz}, rx);
    v = rotate_y(v, ry);
    v = rotate_z(v, rz);
    return (V3){ex + v.x * sc, ey + v.y * sc, ez + v.z * sc};
}

// ---------------------------------------------------------------------
// Particles / explosion fx
// ---------------------------------------------------------------------
static void init_particle_kinds(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        int kind = 0;
        if (i >= 18 && i < 40) kind = 1;
        else if (i >= 40 && i < 54) kind = 2;
        else if (i >= 54 && i < 66) kind = 3;
        else if (i >= 66) kind = 4;
        g.particles[i].slot_kind = kind;
    }
}

static bool spawn_particle(int kind, float x, float y, float z, float vx, float vy, float vz, int life, float sc, float spin) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g.particles[i];
        if (p->alive || p->slot_kind != kind) continue;
        p->alive = true;
        p->x = x; p->y = y; p->z = z;
        p->vx = vx; p->vy = vy; p->vz = vz;
        p->max_life = life; p->life = life;
        p->sc = sc; p->spin = spin;
        return true;
    }
    return false;
}

static V3 burst_dir(float speed) {
    float yaw = randf(0.0f, 6.283185f);
    float pitch = randf(-0.95f, 0.95f);
    float cp = cosf(pitch);
    return (V3){cosf(yaw) * cp * speed, sinf(pitch) * speed, sinf(yaw) * cp * speed};
}

static void spawn_boost_particles(float sx, float sy, float sz, float boost_power) {
    int count = boost_power > 0.5f ? 3 : 1;
    for (int n = 0; n < count; n++) {
        spawn_particle(0,
            sx + randf(-0.12f, 0.12f), sy + randf(-0.08f, 0.08f), sz + randf(-0.05f, 0.05f),
            randf(-0.04f, 0.04f), randf(-0.03f, 0.03f), -randf(0.35f, 0.75f) - boost_power * 0.4f,
            randi(8, 16), randf(0.5f, 1.1f) + boost_power * 0.5f, randf(-0.4f, 0.4f));
    }
}

static void spawn_voxel_debris(float x, float y, float z) {
    float ang = randf(0.0f, 6.28f);
    float spd = randf(0.06f, 0.22f);
    spawn_particle(2,
        x + randf(-0.06f, 0.06f), y + randf(-0.06f, 0.06f), z + randf(-0.06f, 0.06f),
        cosf(ang) * spd, randf(0.02f, 0.14f), sinf(ang) * spd,
        randi(12, 24), randf(0.25f, 0.55f), randf(-0.4f, 0.4f));
}

static void spawn_explosion_particles(float x, float y, float z, bool big) {
    int flash_count = big ? 8 : 4, spark_count = big ? 18 : 10, ember_count = big ? 10 : 6, smoke_count = big ? 6 : 3;

    for (int n = 0; n < flash_count; n++) {
        V3 d = burst_dir(randf(0.04f, big ? 0.18f : 0.12f));
        spawn_particle(3, x + randf(-0.08f, 0.08f), y + randf(-0.08f, 0.08f), z + randf(-0.08f, 0.08f),
            d.x, d.y, d.z, randi(6, 12), randf(0.7f, 1.3f), randf(-0.4f, 0.4f));
    }
    for (int n = 0; n < spark_count; n++) {
        float spd = randf(big ? 0.22f : 0.14f, big ? 0.58f : 0.38f);
        V3 d = burst_dir(spd);
        spawn_particle(1, x + randf(-0.12f, 0.12f), y + randf(-0.12f, 0.12f), z + randf(-0.12f, 0.12f),
            d.x, d.y, d.z, randi(14, big ? 28 : 22), randf(0.45f, 1.0f), randf(-0.4f, 0.4f));
    }
    for (int n = 0; n < ember_count; n++) {
        V3 d = burst_dir(randf(0.08f, big ? 0.32f : 0.22f));
        d.y += randf(0.02f, 0.12f);
        spawn_particle(2, x + randf(-0.16f, 0.16f), y + randf(-0.16f, 0.16f), z + randf(-0.16f, 0.16f),
            d.x, d.y, d.z, randi(18, big ? 36 : 28), randf(0.35f, 0.85f), randf(-0.4f, 0.4f));
    }
    for (int n = 0; n < smoke_count; n++) {
        V3 d = burst_dir(randf(0.03f, big ? 0.14f : 0.09f));
        d.y += randf(0.04f, 0.16f);
        spawn_particle(4, x + randf(-0.1f, 0.1f), y + randf(-0.1f, 0.1f), z + randf(-0.1f, 0.1f),
            d.x, d.y * 0.6f, d.z, randi(22, big ? 40 : 32), randf(0.55f, big ? 1.4f : 1.0f), randf(-0.4f, 0.4f));
    }
}

static void spawn_fx(float x, float y, float z, bool big) {
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &g.fx[i];
        if (f->alive) continue;
        f->alive = true;
        f->x = x; f->y = y; f->z = z;
        f->max_life = big ? 30 : 20;
        f->life = f->max_life;
        f->base_sc = big ? 0.45f : 0.28f;
        f->spin = randf(-0.25f, 0.25f);
        spawn_explosion_particles(x, y, z, big);
        if (big && g.shake < 0.55f) g.shake = 0.55f;
        return;
    }
}

// ---------------------------------------------------------------------
// Collision helpers
// ---------------------------------------------------------------------
static bool hit(float ax, float ay, float az, float ar, float bx, float by, float bz, float br) {
    float dx = ax - bx, dy = ay - by, dz = az - bz;
    float rr = ar + br;
    return (dx * dx + dy * dy + dz * dz) < rr * rr;
}

static float hit_bullet_along_z(float bx, float by, float bz, float prev_z, float br, float ex, float ey, float ez, float er) {
    const int steps = 6;
    for (int s = 0; s <= steps; s++) {
        float z = (s == steps) ? bz : lerpf(prev_z, bz, (float)s / (float)steps);
        if (hit(bx, by, z, br, ex, ey, ez, er)) return z;
    }
    return -1.0f;
}

static int carve_world(Enemy *e, float wx, float wy, float wz, float ex, float ey, float ez, float sc, float rx, float ry, float rz, int dmg) {
    VoxelAsteroid *v = &e->voxel;
    V3 local = world_to_local(wx, wy, wz, ex, ey, ez, sc, rx, ry, rz);
    float hit_slop2 = VOXEL_CELL * VOXEL_CELL * (2.8f + (float)(dmg - 1) * 1.4f);
    int removed = 0, debris = 0;

    int best_x = -1, best_y = -1, best_z = -1;
    float best_d2 = 1e9f;
    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++) {
                if (!voxel_get(v, x, y, z)) continue;
                V3 p = voxel_local_from_grid(x, y, z);
                float dx = p.x - local.x, dy = p.y - local.y, dz = p.z - local.z;
                float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < best_d2) { best_d2 = d2; best_x = x; best_y = y; best_z = z; }
            }
    if (best_x < 0) return 0;

    for (int z = 0; z < VOXEL_GRID; z++)
        for (int y = 0; y < VOXEL_GRID; y++)
            for (int x = 0; x < VOXEL_GRID; x++) {
                if (!voxel_get(v, x, y, z)) continue;
                V3 p = voxel_local_from_grid(x, y, z);
                float dx = p.x - local.x, dy = p.y - local.y, dz = p.z - local.z;
                if (dx * dx + dy * dy + dz * dz > hit_slop2) continue;
                if (debris < 6) {
                    V3 world = local_to_world(p.x, p.y, p.z, ex, ey, ez, sc, rx, ry, rz);
                    spawn_voxel_debris(world.x, world.y, world.z);
                    debris++;
                }
                voxel_set(v, x, y, z, 0);
                removed++;
            }

    if (removed == 0) {
        V3 p = voxel_local_from_grid(best_x, best_y, best_z);
        if (debris < 6) {
            V3 world = local_to_world(p.x, p.y, p.z, ex, ey, ez, sc, rx, ry, rz);
            spawn_voxel_debris(world.x, world.y, world.z);
            debris++;
        }
        voxel_set(v, best_x, best_y, best_z, 0);
        removed++;
    }

    for (int extra = 1; extra < dmg && v->count > VOXEL_DESTROY_AT; extra++) {
        int nx = -1, ny = -1, nz = -1;
        float nd2 = 1e9f;
        for (int z = 0; z < VOXEL_GRID; z++)
            for (int y = 0; y < VOXEL_GRID; y++)
                for (int x = 0; x < VOXEL_GRID; x++) {
                    if (!voxel_get(v, x, y, z)) continue;
                    V3 p = voxel_local_from_grid(x, y, z);
                    float dx = p.x - local.x, dy = p.y - local.y, dz = p.z - local.z;
                    float d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < nd2) { nd2 = d2; nx = x; ny = y; nz = z; }
                }
        if (nx < 0) break;
        V3 p = voxel_local_from_grid(nx, ny, nz);
        if (debris < 6) {
            V3 world = local_to_world(p.x, p.y, p.z, ex, ey, ez, sc, rx, ry, rz);
            spawn_voxel_debris(world.x, world.y, world.z);
            debris++;
        }
        voxel_set(v, nx, ny, nz, 0);
        removed++;
    }

    if (removed > 0) voxel_rebuild_gl_mesh(v, &e->voxel_mesh, false);
    return removed;
}

// ---------------------------------------------------------------------
// Hangar — buy/equip, backed by the SHIPS/PARTS/COLORS/UPGRADES tables.
// ---------------------------------------------------------------------
static bool owns_ship(int id) { return g.profile.owned_ships[id]; }
static bool owns_part(int slot, int id) { return g.profile.owned_parts[slot][id]; }
static bool owns_color(int id) { return g.profile.owned_colors[id]; }

static void rebuild_ship_mesh(void) {
    if (!g.gl_ready) return;
    gl_mesh_destroy(&g.ship_mesh);
    g.ship_mesh = build_ship_mesh(&g.profile.loadout);
}

static void hangar_equip_ship(int id) {
    g.profile.loadout.ship = id;
    rebuild_ship_mesh();
}

static void hangar_buy_ship(int id) {
    if (owns_ship(id)) { hangar_equip_ship(id); return; }
    if (g.profile.credits < SHIPS[id].price) return;
    g.profile.credits -= SHIPS[id].price;
    g.profile.owned_ships[id] = true;
    hangar_equip_ship(id);
}

static void hangar_buy_part(int slot, int id) {
    if (owns_part(slot, id)) { g.profile.loadout.parts[slot] = id; rebuild_ship_mesh(); return; }
    if (g.profile.credits < PARTS[slot][id].price) return;
    g.profile.credits -= PARTS[slot][id].price;
    g.profile.owned_parts[slot][id] = true;
    g.profile.loadout.parts[slot] = id;
    rebuild_ship_mesh();
}

static void hangar_buy_color(int id) {
    if (owns_color(id)) { g.profile.loadout.color = id; rebuild_ship_mesh(); return; }
    if (g.profile.credits < COLORS[id].price) return;
    g.profile.credits -= COLORS[id].price;
    g.profile.owned_colors[id] = true;
    g.profile.loadout.color = id;
    rebuild_ship_mesh();
}

static void hangar_buy_upgrade(int key) {
    int lv = g.profile.upgrade_level[key];
    if (lv >= UPGRADES[key].max_level) return;
    int cost = upgrade_cost(key, lv);
    if (g.profile.credits < cost) return;
    g.profile.credits -= cost;
    g.profile.upgrade_level[key]++;
}

static int hangar_list_count(void) {
    switch (g.hangTab) {
        case HANGAR_TAB_SHIPS: return SHIP_COUNT;
        case HANGAR_TAB_PARTS: return PART_OPTION_COUNT;
        case HANGAR_TAB_COLORS: return COLOR_COUNT;
        case HANGAR_TAB_UPGRADES: return UPGRADE_COUNT;
        default: return 0;
    }
}

static void hangar_confirm(void) {
    switch (g.hangTab) {
        case HANGAR_TAB_SHIPS: hangar_buy_ship(g.hangSel); break;
        case HANGAR_TAB_PARTS: hangar_buy_part(g.partSlot, g.hangSel); break;
        case HANGAR_TAB_COLORS: hangar_buy_color(g.hangSel); break;
        case HANGAR_TAB_UPGRADES: hangar_buy_upgrade(g.hangSel); break;
        case HANGAR_TAB_LAUNCH: start_run(); break;
        default: break;
    }
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------
static void seed_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x = randf(-STAR_SPREAD_X, STAR_SPREAD_X);
        g.stars[i].y = randf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
        g.stars[i].z = randf(STAR_Z_NEAR, STAR_Z_FAR);
        g.stars[i].par = randf(0.04f, 0.16f);
        g.stars[i].sc = randf(0.35f, 0.95f);
    }
}

static void reset_planets(void) {
    for (int i = 0; i < MAX_PLANETS; i++) {
        const PlanetDef *def = &PLANET_DEFS[i];
        g.planets[i].x = def->x;
        g.planets[i].y = def->y;
        g.planets[i].z = def->z;
        g.planets[i].sc = def->sc;
    }
}

void game_init(void) {
    init_light();
    memset(&g, 0, sizeof(g));
    g.state = STATE_MENU;
    g.speed = BASE_SPEED;
    g.fireSide = 1;

    // defaultSave() equivalent: scout / std parts / arwing, all owned.
    g.profile.owned_ships[0] = true;
    for (int slot = 0; slot < PART_SLOT_COUNT; slot++) g.profile.owned_parts[slot][0] = true;
    g.profile.owned_colors[0] = true;
    g.profile.loadout.ship = 0;
    g.profile.loadout.color = 0;
    for (int slot = 0; slot < PART_SLOT_COUNT; slot++) g.profile.loadout.parts[slot] = 0;
    g.stats = compute_stats(&g.profile);

    init_particle_kinds();
    seed_stars();
    reset_planets();
}

void game_shutdown(void) {}

void game_reset(void) {
    game_init();
    if (g.gl_ready) rebuild_ship_mesh();
}

void *game_get_save_data(void) { return NULL; }
unsigned game_get_save_size(void) { return 0; }

void game_gl_ready(void) {
    // A context_reset without a preceding context_destroy means the previous
    // context was lost out from under us — its handles are already dead, so
    // don't try to free them, just rebuild.
    g.ship_mesh = build_ship_mesh(&g.profile.loadout);
    g.thruster_mesh = build_thruster_mesh();
    g.star_mesh = build_star_mesh();
    g.bullet_mesh = build_bullet_mesh();
    g.enemy_mesh = build_enemy_mesh();
    g.explosion_mesh = build_explosion_mesh();
    for (int i = 0; i < MAX_PLANETS; i++) {
        const PlanetDef *def = &PLANET_DEFS[i];
        g.planet_mesh[i] = build_planet_dome_mesh(def->br, def->bg, def->bb, def->hr, def->hg, def->hb);
    }
    for (int k = 0; k < 5; k++) g.particle_mesh[k] = build_particle_mesh(k);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        voxel_reset(&g.enemies[i].voxel, 1000u + (unsigned)i * 7919u);
        voxel_rebuild_gl_mesh(&g.enemies[i].voxel, &g.enemies[i].voxel_mesh, true);
    }
    g.gl_ready = true;
}

void game_gl_shutdown(void) {
    if (!g.gl_ready) return;
    gl_mesh_destroy(&g.ship_mesh);
    gl_mesh_destroy(&g.thruster_mesh);
    gl_mesh_destroy(&g.star_mesh);
    gl_mesh_destroy(&g.bullet_mesh);
    gl_mesh_destroy(&g.enemy_mesh);
    gl_mesh_destroy(&g.explosion_mesh);
    for (int i = 0; i < MAX_PLANETS; i++) gl_mesh_destroy(&g.planet_mesh[i]);
    for (int k = 0; k < 5; k++) gl_mesh_destroy(&g.particle_mesh[k]);
    for (int i = 0; i < MAX_ENEMIES; i++) gl_mesh_destroy(&g.enemies[i].voxel_mesh);
    g.gl_ready = false;
}

static void start_run(void) {
    g.stats = compute_stats(&g.profile);
    g.score = 0;
    g.lives = g.stats.lives;
    g.wave = 1;
    g.fireTimer = 0;
    g.spawnTimer = 45;
    g.invuln = 0;
    g.shake = 0.0f;
    g.shipX = g.shipY = g.velX = g.velY = g.roll = g.pitch = 0.0f;
    g.speed = BASE_SPEED * g.stats.speed;
    g.runEarned = 0;
    g.frame = 0;
    g.boosting = false;

    memset(g.bullets, 0, sizeof(g.bullets));
    for (int i = 0; i < MAX_ENEMIES; i++) g.enemies[i].active = false;
    memset(g.fx, 0, sizeof(g.fx));
    memset(g.particles, 0, sizeof(g.particles));
    init_particle_kinds();

    seed_stars();
    reset_planets();

    g.state = STATE_PLAY;
}

// ---------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------
static void update_world(float scroll_speed) {
    for (int i = 0; i < STAR_COUNT; i++) {
        g.stars[i].x -= scroll_speed * g.stars[i].par;
        if (g.stars[i].x < -STAR_SPREAD_X) {
            g.stars[i].x += STAR_SPREAD_X * 2.0f;
            g.stars[i].y = randf(-STAR_SPREAD_Y, STAR_SPREAD_Y);
            g.stars[i].z = randf(STAR_Z_NEAR, STAR_Z_FAR);
        }
    }
    for (int i = 0; i < MAX_PLANETS; i++) {
        g.planets[i].z -= scroll_speed * 0.35f;
        if (g.planets[i].z < 60.0f) {
            const PlanetDef *def = &PLANET_DEFS[i];
            g.planets[i].z += 260.0f;
            g.planets[i].x = def->x + randf(-8.0f, 8.0f);
            g.planets[i].y = def->y + randf(-2.5f, 2.5f);
        }
    }
}

static void fire_bullet(void) {
    int n = g.stats.bullets;
    float spread = g.stats.spread;
    for (int b = 0; b < n; b++) {
        float t = (n == 1) ? 0.5f : (float)b / (float)(n - 1);
        float off = (t - 0.5f) * spread * 2.0f;
        float wx = (g.fireSide > 0 ? 1.6f : -1.6f) + off;
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (g.bullets[i].alive) continue;
            g.bullets[i].alive = true;
            g.bullets[i].x = g.shipX + wx;
            g.bullets[i].y = g.shipY;
            g.bullets[i].z = 1.2f;
            g.bullets[i].pz = 1.2f;
            g.bullets[i].damage = g.stats.damage;
            break;
        }
    }
    g.fireSide = -g.fireSide;
}

static void spawn_enemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g.enemies[i];
        if (e->active) continue;
        e->active = true;
        e->type = (randi(0, 99) < 32) ? 1 : 0;
        e->x = randf(-BOUNDS_X, BOUNDS_X);
        e->y = randf(BOUNDS_Y_LOW + 0.5f, BOUNDS_Y_HIGH - 0.5f);
        e->z = randf(95.0f, 130.0f);
        e->rx = randf(0.0f, 6.28f);
        e->ry = randf(0.0f, 6.28f);
        e->rz = randf(0.0f, 6.28f);
        e->spin = randf(-0.05f, 0.05f);
        e->weave = randf(0.008f, 0.03f);
        e->phase = randf(0.0f, 6.28f);
        if (e->type == 1) {
            e->sc = randf(0.85f, 1.25f);
            voxel_reset(&e->voxel, (unsigned)randi(1, 999999));
            voxel_rebuild_gl_mesh(&e->voxel, &e->voxel_mesh, false);
            e->radius = voxel_collision_radius(&e->voxel) * e->sc;
            e->hp = 1;
        } else {
            e->hp = 1;
            e->radius = 0.9f;
            e->sc = 1.0f;
        }
        return;
    }
}

static void award_run_credits(void) {
    g.runEarned = (g.score / 10) + g.wave * 25;
    g.profile.credits += g.runEarned;
    if (g.score > g.profile.best) g.profile.best = g.score;
}

static void update_play(const GameInput *input) {
    // D-pad works as a digital fallback/override for the analog stick — full
    // deflection while held, so the ship is flyable without an analog input.
    float ix = apply_deadzone(input->stick_x);
    float iy = apply_deadzone(input->stick_y);
    if (input->left_held) ix = -1.0f;
    else if (input->right_held) ix = 1.0f;
    if (input->up_held) iy = -1.0f;
    else if (input->down_held) iy = 1.0f;

    g.boosting = input->b_held || input->r2_held;
    bool braking = input->l2_held;

    float target_speed = BASE_SPEED * g.stats.speed + (float)g.wave * 0.03f;
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
    bool fire_held = input->a_held || input->r1_held;
    if (fire_held && g.fireTimer == 0) {
        fire_bullet();
        g.fireTimer = g.stats.cooldown;
    }

    update_world(g.speed);

    float boost_power = clampf((g.speed - BASE_SPEED) / (BASE_SPEED * 1.2f), 0.0f, 1.0f);
    if (g.boosting) spawn_boost_particles(g.shipX, g.shipY, -1.35f, boost_power);

    g.spawnTimer--;
    if (g.spawnTimer <= 0) {
        spawn_enemy();
        int t = 42 - g.wave * 2;
        g.spawnTimer = t < 14 ? 14 : t;
    }
    if (g.frame % 900 == 0 && g.frame > 0) g.wave++;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g.bullets[i].alive) continue;
        g.bullets[i].pz = g.bullets[i].z;
        g.bullets[i].z += 3.4f;
        if (g.bullets[i].z > 135.0f) g.bullets[i].alive = false;
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g.enemies[i];
        if (!e->active) continue;

        e->z -= g.speed * (e->type == 1 ? 0.7f : 1.0f) + 0.15f;
        e->phase += e->weave;
        e->x += sinf(e->phase) * 0.04f;
        e->ry += e->spin;
        e->rx += e->spin * 0.5f;

        if (e->z < -8.0f) { e->active = false; continue; }

        if (g.invuln <= 0 && hit(g.shipX, g.shipY, 0.0f, 0.8f, e->x, e->y, e->z, e->radius)) {
            e->active = false;
            g.lives--;
            g.invuln = 90;
            g.shake = 1.0f;
            spawn_fx(g.shipX, g.shipY, 0.5f, true);
            if (g.lives <= 0) {
                award_run_credits();
                g.state = STATE_OVER;
                g.prev_input = *input;
                return;
            }
            continue;
        }

        for (int j = 0; j < MAX_BULLETS; j++) {
            if (!g.bullets[j].alive) continue;
            float bx = g.bullets[j].x, by = g.bullets[j].y, bz = g.bullets[j].z;
            float hitZ = -1.0f;
            if (e->type == 1) {
                float br = 0.55f, er = e->radius + 0.25f;
                hitZ = hit_bullet_along_z(bx, by, bz, g.bullets[j].pz, br, e->x, e->y, e->z, er);
            } else if (hit(bx, by, bz, 0.35f, e->x, e->y, e->z, e->radius)) {
                hitZ = bz;
            }
            if (hitZ < 0.0f) continue;

            bz = hitZ;
            g.bullets[j].alive = false;
            int dmg = g.bullets[j].damage > 0 ? g.bullets[j].damage : 1;

            if (e->type == 1) {
                int removed = carve_world(e, bx, by, bz, e->x, e->y, e->z, e->sc, e->rx, e->ry, e->rz, dmg);
                spawn_fx(bx, by, bz, false);
                g.score += removed * 8;
                e->radius = voxel_collision_radius(&e->voxel) * e->sc;
                if (e->voxel.count <= VOXEL_DESTROY_AT) {
                    e->active = false;
                    spawn_fx(e->x, e->y, e->z, true);
                    g.score += 60;
                }
            } else {
                e->hp -= dmg;
                if (e->hp <= 0) {
                    e->active = false;
                    spawn_fx(e->x, e->y, e->z, false);
                    g.score += 100;
                } else {
                    spawn_fx(bx, by, bz, false);
                }
            }
            break;
        }
    }

    for (int i = 0; i < MAX_FX; i++) {
        if (!g.fx[i].alive) continue;
        g.fx[i].life--;
        if (g.fx[i].life <= 0) g.fx[i].alive = false;
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g.particles[i];
        if (!p->alive) continue;
        p->life--;
        p->x += p->vx; p->y += p->vy; p->z += p->vz;
        switch (p->slot_kind) {
            case 0: p->vz -= 0.02f; p->sc = lerpf(p->sc, 0.15f, 0.12f); break;
            case 1: p->vx *= 0.90f; p->vy *= 0.90f; p->vz *= 0.90f; p->vy -= 0.008f; p->sc = lerpf(p->sc, 0.08f, 0.10f); break;
            case 2: p->vx *= 0.94f; p->vy *= 0.94f; p->vz *= 0.94f; p->vy -= 0.012f; p->sc = lerpf(p->sc, 0.04f, 0.06f); break;
            case 3: p->vx *= 0.86f; p->vy *= 0.86f; p->vz *= 0.86f; p->sc = lerpf(p->sc, 0.02f, 0.18f); break;
            default: p->vx *= 0.96f; p->vy *= 0.96f; p->vz *= 0.96f; p->vy += 0.006f; p->sc = lerpf(p->sc, 1.6f, 0.04f); break;
        }
        if (p->life <= 0) p->alive = false;
    }

    if (g.invuln > 0) g.invuln--;
    g.frame++;
}

// Cursor-style nav: on the tab row, left/right pick a tab and A (or down)
// enters it; in the list, up at the top row climbs back to the tab row.
// No shoulder buttons involved — just the d-pad and A.
static void update_hangar(const GameInput *input) {
    if (g.hangNavLock > 0) g.hangNavLock--;

    bool left_edge = edge(input->left_held, g.prev_input.left_held);
    bool right_edge = edge(input->right_held, g.prev_input.right_held);
    bool up_edge = edge(input->up_held, g.prev_input.up_held);
    bool down_edge = edge(input->down_held, g.prev_input.down_held);
    bool confirm_edge = edge(input->a_held, g.prev_input.a_held);

    if (g.hangFocusTabs) {
        if (g.hangNavLock == 0) {
            if (left_edge) { g.hangTab = (g.hangTab + HANGAR_TAB_COUNT - 1) % HANGAR_TAB_COUNT; g.hangNavLock = 10; }
            if (right_edge) { g.hangTab = (g.hangTab + 1) % HANGAR_TAB_COUNT; g.hangNavLock = 10; }
        }
        if (confirm_edge || down_edge) {
            if (g.hangTab == HANGAR_TAB_LAUNCH) {
                hangar_confirm();
            } else {
                g.hangFocusTabs = false;
                g.hangSel = 0;
                g.hangNavLock = 10;
            }
        }
        return;
    }

    if (g.hangTab == HANGAR_TAB_PARTS && g.hangNavLock == 0) {
        if (left_edge) { g.partSlot = (g.partSlot + PART_SLOT_COUNT - 1) % PART_SLOT_COUNT; g.hangSel = 0; g.hangNavLock = 10; }
        if (right_edge) { g.partSlot = (g.partSlot + 1) % PART_SLOT_COUNT; g.hangSel = 0; g.hangNavLock = 10; }
    }

    int count = hangar_list_count();
    if (g.hangNavLock == 0) {
        if (up_edge) {
            if (count > 0 && g.hangSel > 0) g.hangSel--;
            else g.hangFocusTabs = true;
            g.hangNavLock = 10;
        }
        if (down_edge && count > 0) {
            g.hangSel = (g.hangSel + 1) % count;
            g.hangNavLock = 10;
        }
    }

    if (confirm_edge) hangar_confirm();
}

void game_update(const GameInput *input) {
    bool confirm_edge = edge(input->a_held, g.prev_input.a_held);
    bool start_edge = edge(input->start_held, g.prev_input.start_held);

    if (g.shake > 0.0f) { g.shake -= 0.06f; if (g.shake < 0.0f) g.shake = 0.0f; }

    switch (g.state) {
        case STATE_MENU:
            g.menuSpin += 0.02f;
            update_world(BASE_SPEED * 0.4f);
            if (confirm_edge) {
                start_run();
            } else if (start_edge) {
                g.state = STATE_HANGAR;
                g.hangTab = HANGAR_TAB_SHIPS;
                g.hangSel = 0;
                g.partSlot = 0;
                g.hangFocusTabs = true;
                g.hangNavLock = 0;
            }
            break;
        case STATE_HANGAR:
            g.menuSpin += 0.02f;
            update_world(BASE_SPEED * 0.25f);
            if (start_edge) {
                g.state = STATE_MENU;
            } else {
                update_hangar(input);
            }
            break;
        case STATE_OVER:
            update_world(BASE_SPEED * 0.4f);
            if (confirm_edge) {
                start_run();
            } else if (start_edge) {
                g.state = STATE_HANGAR;
                g.hangTab = HANGAR_TAB_LAUNCH;
                g.hangSel = 0;
                g.hangFocusTabs = true;
                g.hangNavLock = 0;
            }
            break;
        default: // STATE_PLAY
            if (start_edge) {
                g.state = STATE_MENU;
            } else {
                update_play(input);
            }
            break;
    }

    g.prev_input = *input;
}

// ---------------------------------------------------------------------
// Render — 3D scene
// ---------------------------------------------------------------------
static void render_planets(float camX, float camY, float camZ) {
    for (int i = 0; i < MAX_PLANETS; i++) {
        Planet *p = &g.planets[i];
        float dx = p->x - camX, dy = p->y - camY, dz = p->z - camZ;
        float horiz = sqrtf(dx * dx + dz * dz);
        float pitch = 0.0f, yaw = 0.0f;
        if (!(horiz < 0.0001f && fabsf(dy) < 0.0001f)) {
            yaw = atan2f(dx, dz);
            pitch = atan2f(-dy, horiz);
        }
        Mat4 model = mat4_multiply(
            mat4_multiply(mat4_translate(p->x, p->y, p->z), mat4_rotate_xyz(pitch, yaw, 0.0f)),
            mat4_scale(p->sc, p->sc, p->sc));
        gl_mesh_draw(&g.planet_mesh[i], model);
    }
}

static void render_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        Star *s = &g.stars[i];
        Mat4 model = mat4_multiply(mat4_translate(s->x, s->y, s->z), mat4_scale(s->sc, s->sc, s->sc));
        gl_mesh_draw(&g.star_mesh, model);
    }
}

static void render_bullets(void) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g.bullets[i].alive) continue;
        Mat4 model = mat4_translate(g.bullets[i].x, g.bullets[i].y, g.bullets[i].z);
        gl_mesh_draw(&g.bullet_mesh, model);
    }
}

static void render_enemies(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g.enemies[i];
        if (!e->active) continue;
        Mat4 rot = mat4_rotate_xyz(e->rx, e->ry, e->rz);
        if (e->type == 1) {
            Mat4 model = mat4_multiply(mat4_multiply(mat4_translate(e->x, e->y, e->z), rot), mat4_scale(e->sc, e->sc, e->sc));
            gl_mesh_draw(&e->voxel_mesh, model);
        } else {
            Mat4 model = mat4_multiply(mat4_translate(e->x, e->y, e->z), rot);
            gl_mesh_draw(&g.enemy_mesh, model);
        }
    }
}

static void render_fx(void) {
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &g.fx[i];
        if (!f->alive) continue;
        float prog = 1.0f - (float)f->life / (float)f->max_life;
        float pop = sinf(prog * 3.141592f);
        float sc = f->base_sc * (0.35f + pop * 2.4f) * (1.0f - prog * 0.35f);
        Mat4 model = mat4_multiply(
            mat4_multiply(mat4_translate(f->x, f->y, f->z), mat4_rotate_xyz(prog * 0.8f, f->spin + prog * 1.6f, 0.0f)),
            mat4_scale(sc, sc, sc));
        gl_mesh_draw(&g.explosion_mesh, model);
    }
}

static void render_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g.particles[i];
        if (!p->alive) continue;
        float fade = (float)p->life / (float)p->max_life;
        float ease = fade * fade;
        float s = p->sc * ease;
        float age = 1.0f - fade;
        Mat4 rot, scale;
        switch (p->slot_kind) {
            case 0: rot = mat4_rotate_xyz(g.pitch, 0.0f, g.roll); scale = mat4_scale(s * 0.7f, s * 0.7f, s * 1.8f); break;
            case 1: rot = mat4_rotate_xyz(age * 2.1f + p->spin, age * 3.2f, p->spin * 0.5f); scale = mat4_scale(s * 0.35f, s * 0.35f, s * 2.8f); break;
            case 2: rot = mat4_rotate_xyz(age * 1.4f, age * 2.0f + p->spin, p->spin); scale = mat4_scale(s, s, s); break;
            case 3: rot = mat4_rotate_xyz(age * 4.0f, age * 5.0f, age * 2.0f); scale = mat4_scale(s * 1.2f, s * 1.2f, s * 1.2f); break;
            default: rot = mat4_rotate_xyz(age * 0.6f, age * 0.9f + p->spin, 0.0f); scale = mat4_scale(s * 1.1f, s * 1.1f, s * 0.85f); break;
        }
        Mat4 model = mat4_multiply(mat4_multiply(mat4_translate(p->x, p->y, p->z), rot), scale);
        gl_mesh_draw(&g.particle_mesh[p->slot_kind], model);
    }
}

static void render_ship_and_thruster(bool show_ship) {
    if (!show_ship) return;
    Mat4 ship_model;
    if (g.state == STATE_PLAY) {
        ship_model = mat4_multiply(mat4_translate(g.shipX, g.shipY, 0.0f), mat4_rotate_xyz(g.pitch, 0.0f, g.roll));
        float pulse = 0.75f + sinf((float)g.frame * 0.6f) * 0.2f + (g.boosting ? 0.6f : 0.0f);
        Mat4 thruster_model = mat4_multiply(
            mat4_multiply(mat4_translate(g.shipX, g.shipY, -1.15f), mat4_rotate_xyz(g.pitch, 0.0f, g.roll)),
            mat4_scale(1.0f, 1.0f, pulse));
        gl_mesh_draw(&g.thruster_mesh, thruster_model);
    } else {
        ship_model = mat4_rotate_xyz(0.0f, g.menuSpin, 0.0f);
    }
    gl_mesh_draw(&g.ship_mesh, ship_model);
}

// ---------------------------------------------------------------------
// Render — HUD / hangar (fontstash text)
// ---------------------------------------------------------------------
static void draw_backdrop(int width, int height) {
    gl_draw_quad2d(0.0f, 0.0f, (float)width, 28.0f, 0.0f, 0.0f, 0.0f, 0.31f, width, height);
    gl_draw_quad2d(0.0f, (float)height - 28.0f, (float)width, 28.0f, 0.0f, 0.0f, 0.0f, 0.31f, width, height);
}

static void draw_lives(int width, int height) {
    for (int i = 0; i < g.lives; i++) {
        float x = (float)width - 40.0f - (float)i * 26.0f;
        float y = 26.0f;
        gl_draw_triangle2d(x, y + 14.0f, x + 10.0f, y + 14.0f, x + 5.0f, y, C_GREEN[0], C_GREEN[1], C_GREEN[2], C_GREEN[3], width, height);
    }
}

static void draw_boost_bar(int width, int height) {
    float bw = 120.0f, x = 16.0f, y = (float)height - 26.0f;
    gl_draw_quad2d(x, y, bw, 10.0f, 0.12f, 0.16f, 0.24f, 1.0f, width, height);
    float frac = clampf((g.speed - BASE_SPEED * 0.4f) / (BASE_SPEED * 2.1f), 0.0f, 1.0f);
    const float *c = g.boosting ? C_YELLOW : C_CYAN;
    gl_draw_quad2d(x, y, bw * frac, 10.0f, c[0], c[1], c[2], c[3], width, height);
    text_draw(x, y - 22.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], "SPEED", width, height);
}

static const char *HANGAR_TAB_NAMES[HANGAR_TAB_COUNT] = {"SHIPS", "PARTS", "COLORS", "UPGRADES", "LAUNCH"};
static const char *PART_SLOT_NAMES[PART_SLOT_COUNT] = {"WING", "ENGINE", "CANNON", "NOSE"};

static void draw_hangar_ui(int width, int height) {
    float px = (float)width * 0.42f;
    float pw = (float)width - px - 10.0f;
    char buf[64];

    gl_draw_quad2d(px, 30.0f, pw, (float)height - 38.0f, 0.06f, 0.09f, 0.15f, 0.85f, width, height);

    if (g.hangFocusTabs) {
        float hx = px + 6.0f + (float)g.hangTab * 86.0f - 4.0f;
        gl_draw_quad2d(hx, 4.0f, 78.0f, 20.0f, 0.16f, 0.24f, 0.39f, 0.63f, width, height);
    }
    float tx = px + 6.0f;
    for (int t = 0; t < HANGAR_TAB_COUNT; t++) {
        const float *c = (t == g.hangTab) ? C_YELLOW : C_DIM;
        text_draw(tx, 8.0f, 14.0f, c[0], c[1], c[2], c[3], HANGAR_TAB_NAMES[t], width, height);
        tx += 86.0f;
    }

    snprintf(buf, sizeof(buf), "CREDITS %d", g.profile.credits);
    text_draw(px + 8.0f, 38.0f, 16.0f, C_CYAN[0], C_CYAN[1], C_CYAN[2], C_CYAN[3], buf, width, height);
    snprintf(buf, sizeof(buf), "BEST %d", g.profile.best);
    text_draw(px + 8.0f, 58.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], buf, width, height);

    if (g.hangTab == HANGAR_TAB_LAUNCH) {
        ShipStats rs = compute_stats(&g.profile);
        text_draw(px + 8.0f, 90.0f, 16.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], "LAUNCH RUN", width, height);
        snprintf(buf, sizeof(buf), "%s / %s", SHIPS[g.profile.loadout.ship].name, COLORS[g.profile.loadout.color].name);
        text_draw(px + 8.0f, 114.0f, 14.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], buf, width, height);
        snprintf(buf, sizeof(buf), "LIVES %d  DMG %d  SPD X%.2f", rs.lives, rs.damage, rs.speed);
        text_draw(px + 8.0f, 138.0f, 13.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], buf, width, height);
        snprintf(buf, sizeof(buf), "SHOTS %d  RATE %df", rs.bullets, rs.cooldown);
        text_draw(px + 8.0f, 158.0f, 13.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], buf, width, height);
        text_draw(px + 8.0f, 192.0f, 16.0f, C_YELLOW[0], C_YELLOW[1], C_YELLOW[2], C_YELLOW[3], "A = LAUNCH", width, height);
        text_draw(px + 8.0f, 214.0f, 13.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], "ARROWS + A NAVIGATE - START MENU", width, height);
        return;
    }

    float listY = 78.0f, rowH = 22.0f;
    int maxRows = (int)(((float)height - listY - 16.0f) / rowH);

    if (g.hangTab == HANGAR_TAB_PARTS) {
        snprintf(buf, sizeof(buf), "SLOT: %s < >", PART_SLOT_NAMES[g.partSlot]);
        text_draw(px + 8.0f, listY - 18.0f, 14.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], buf, width, height);
    }

    int count = hangar_list_count();
    for (int i = 0; i < maxRows && i < count; i++) {
        float y = listY + (float)i * rowH;
        bool sel = !g.hangFocusTabs && (i == g.hangSel);
        if (sel) gl_draw_quad2d(px + 4.0f, y - 2.0f, pw - 8.0f, rowH - 2.0f, 0.16f, 0.24f, 0.39f, 0.63f, width, height);

        const char *label = "";
        int price = 0;
        bool owned = false, equipped = false;
        char label_buf[48];

        if (g.hangTab == HANGAR_TAB_SHIPS) {
            label = SHIPS[i].name; price = SHIPS[i].price;
            owned = owns_ship(i); equipped = (g.profile.loadout.ship == i);
        } else if (g.hangTab == HANGAR_TAB_PARTS) {
            const PartDef *pt = &PARTS[g.partSlot][i];
            label = pt->name; price = pt->price;
            owned = owns_part(g.partSlot, i); equipped = (g.profile.loadout.parts[g.partSlot] == i);
        } else if (g.hangTab == HANGAR_TAB_COLORS) {
            label = COLORS[i].name; price = COLORS[i].price;
            owned = owns_color(i); equipped = (g.profile.loadout.color == i);
        } else if (g.hangTab == HANGAR_TAB_UPGRADES) {
            int lv = g.profile.upgrade_level[i];
            snprintf(label_buf, sizeof(label_buf), "%s %d/%d", UPGRADES[i].name, lv, UPGRADES[i].max_level);
            label = label_buf;
            price = (lv >= UPGRADES[i].max_level) ? 0 : upgrade_cost(i, lv);
            owned = (lv >= UPGRADES[i].max_level);
        }

        const float *label_col = sel ? C_WHITE : C_DIM;
        text_draw(px + 10.0f, y + 2.0f, 14.0f, label_col[0], label_col[1], label_col[2], label_col[3], label, width, height);

        if (equipped) {
            text_draw(px + pw - 92.0f, y + 2.0f, 13.0f, C_GREEN[0], C_GREEN[1], C_GREEN[2], C_GREEN[3], "EQUIPPED", width, height);
        } else if (g.hangTab == HANGAR_TAB_UPGRADES && owned) {
            text_draw(px + pw - 52.0f, y + 2.0f, 13.0f, C_GREEN[0], C_GREEN[1], C_GREEN[2], C_GREEN[3], "MAX", width, height);
        } else if (owned) {
            text_draw(px + pw - 64.0f, y + 2.0f, 13.0f, C_CYAN[0], C_CYAN[1], C_CYAN[2], C_CYAN[3], "OWNED", width, height);
        } else {
            snprintf(buf, sizeof(buf), "%dC", price);
            const float *pc = (g.profile.credits >= price) ? C_YELLOW : C_RED;
            text_draw(px + pw - 52.0f, y + 2.0f, 13.0f, pc[0], pc[1], pc[2], pc[3], buf, width, height);
        }
    }

    text_draw(px + 8.0f, (float)height - 22.0f, 13.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], "ARROWS + A NAVIGATE - START MENU", width, height);
}

static void draw_hud(int width, int height) {
    char buf[64];
    draw_backdrop(width, height);

    snprintf(buf, sizeof(buf), "SCORE %d", g.score);
    text_draw(16.0f, 14.0f, 18.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], buf, width, height);
    snprintf(buf, sizeof(buf), "WAVE %d", g.wave);
    text_draw(16.0f, 38.0f, 16.0f, C_CYAN[0], C_CYAN[1], C_CYAN[2], C_CYAN[3], buf, width, height);

    if (g.state == STATE_MENU) {
        const char *title = "Sg. Daniel Spaceshipper";
        float title_w = text_width(title, 32.0f);
        text_draw((float)width * 0.5f - title_w * 0.5f, 150.0f, 32.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], title, width, height);
        const char *hint = "PRESS A TO START";
        float hint_w = text_width(hint, 18.0f);
        text_draw((float)width * 0.5f - hint_w * 0.5f, 200.0f, 18.0f, C_YELLOW[0], C_YELLOW[1], C_YELLOW[2], C_YELLOW[3], hint, width, height);
        const char *l1 = "START TO CONFIGURE YOUR SHIP";
        text_draw((float)width * 0.5f - text_width(l1, 14.0f) * 0.5f, 230.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], l1, width, height);
        const char *l2 = "STICK/D-PAD STEER - A FIRE - B BOOST - L2 BRAKE";
        text_draw((float)width * 0.5f - text_width(l2, 14.0f) * 0.5f, 250.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], l2, width, height);
    } else if (g.state == STATE_HANGAR) {
        draw_hangar_ui(width, height);
    } else if (g.state == STATE_OVER) {
        const char *over = "RUN OVER";
        text_draw((float)width * 0.5f - text_width(over, 26.0f) * 0.5f, 140.0f, 26.0f, C_RED[0], C_RED[1], C_RED[2], C_RED[3], over, width, height);
        snprintf(buf, sizeof(buf), "SCORE %d  WAVE %d", g.score, g.wave);
        text_draw((float)width * 0.5f - text_width(buf, 16.0f) * 0.5f, 175.0f, 16.0f, C_WHITE[0], C_WHITE[1], C_WHITE[2], C_WHITE[3], buf, width, height);
        snprintf(buf, sizeof(buf), "+%d CREDITS", g.runEarned);
        text_draw((float)width * 0.5f - text_width(buf, 16.0f) * 0.5f, 200.0f, 16.0f, C_CYAN[0], C_CYAN[1], C_CYAN[2], C_CYAN[3], buf, width, height);
        snprintf(buf, sizeof(buf), "BANK %d", g.profile.credits);
        text_draw((float)width * 0.5f - text_width(buf, 14.0f) * 0.5f, 222.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], buf, width, height);
        const char *cta = "A = PLAY AGAIN";
        text_draw((float)width * 0.5f - text_width(cta, 16.0f) * 0.5f, 250.0f, 16.0f, C_YELLOW[0], C_YELLOW[1], C_YELLOW[2], C_YELLOW[3], cta, width, height);
        const char *cfg = "START = CONFIGURE SHIP";
        text_draw((float)width * 0.5f - text_width(cfg, 13.0f) * 0.5f, 272.0f, 13.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], cfg, width, height);
    } else {
        draw_lives(width, height);
        draw_boost_bar(width, height);
        snprintf(buf, sizeof(buf), "%dC", g.profile.credits);
        text_draw((float)width - 90.0f, 34.0f, 14.0f, C_CYAN[0], C_CYAN[1], C_CYAN[2], C_CYAN[3], buf, width, height);
    }

    snprintf(buf, sizeof(buf), "%d FPS", (int)(s_fps + 0.5f));
    text_draw((float)width - text_width(buf, 14.0f) - 16.0f, 14.0f, 14.0f, C_DIM[0], C_DIM[1], C_DIM[2], C_DIM[3], buf, width, height);
}

void game_render(unsigned int fbo, int width, int height) {
    if (!g.gl_ready) return;

    update_fps();
    gl_begin_frame(fbo, width, height, SPACE_COLOR[0], SPACE_COLOR[1], SPACE_COLOR[2]);

    float sx = 0.0f, sy = 0.0f;
    if (g.shake > 0.0f) { sx = randf(-g.shake, g.shake) * 0.6f; sy = randf(-g.shake, g.shake) * 0.6f; }
    float camX = CAM_X + sx, camY = CAM_Y + sy, camZ = CAM_Z;

    Mat4 view = mat4_look_at(camX, camY, camZ, LOOK_X, LOOK_Y, LOOK_Z, 0.0f, 1.0f, 0.0f);
    Mat4 proj = mat4_perspective(72.0f, (float)width / (float)height, 0.5f, 900.0f);
    gl_set_camera(view, proj);

    render_planets(camX, camY, camZ);
    render_stars();
    render_enemies();
    render_bullets();
    render_fx();
    render_particles();

    bool show_ship = (g.state != STATE_PLAY) || (g.invuln <= 0) || (((g.invuln / 6) % 2) == 0);
    render_ship_and_thruster(show_ship);

    draw_hud(width, height);
}
