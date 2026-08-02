// SpaceShipper libretro core.
//
// The "content" retro_load_game() receives is irrelevant — this core carries
// its own game (source/main.js, in the process of being ported to game.c)
// and only needs a HW-rendered GL2/GLES2 context from the frontend. Loading
// a real ROM path is unnecessary; the frontend can point retro_load_game at
// /dev/null and the core still runs.
#include "libretro.h"
#include "gl.h"
#include "text.h"
#include "game.h"
#include "audio.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEFAULT_SCREEN_W 640u
#define DEFAULT_SCREEN_H 480u

// 44100 / 60 == 735 exactly, so a fixed-size buffer needs no fractional
// carry-over between retro_run() calls.
#define AUDIO_SAMPLE_RATE 44100u
#define AUDIO_FRAMES_PER_RUN (AUDIO_SAMPLE_RATE / 60u)

#define FIXED_DT (1.0 / 60.0)
#define MAX_CATCHUP_TICKS 8

static bool s_clock_started = false;
static double s_last_time = 0.0;
static double s_accumulator = 0.0;

#ifdef _WIN32
#include <windows.h>
double monotonic_now(void) {
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}
#else
#include <time.h>
double monotonic_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static struct retro_hw_render_callback hw_render;
static bool audio_enabled = true;
static uint16_t screen_w = DEFAULT_SCREEN_W;
static uint16_t screen_h = DEFAULT_SCREEN_H;
static uint16_t deadscreen_w = 32u;
static uint16_t deadscreen_h = 32u;

static void RETRO_CALLCONV core_context_reset(void) {
    bool is_gles = (hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 ||
                    hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 ||
                    hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION);

    // Gated on the real signal (shader compile/link), not glad's own
    // extension-probe hiccup — see the comment in gl_backend_init(). Skipping
    // game_gl_ready() on failure keeps g.gl_ready false, so game_render()
    // no-ops instead of drawing with program 0 (which is silent on desktop
    // Mesa's fixed-function fallback but draws nothing at all on real GLES2).
    if (!gl_backend_init((gl_proc_address_fn)hw_render.get_proc_address, is_gles)) {
        fprintf(stderr, "[spaceshipper] core_context_reset: gl_backend_init failed, not rendering 3D\n");
        return;
    }
    if (!text_backend_init()) {
        fprintf(stderr, "[spaceshipper] core_context_reset: text_backend_init failed, HUD text will be missing\n");
    }
    game_gl_ready();
}

static void RETRO_CALLCONV core_context_destroy(void) {
    game_gl_shutdown();
    text_backend_shutdown();
    gl_backend_shutdown();
}

RETRO_API void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    bool no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);

    static const struct retro_variable vars[] = {
        { "spaceshipper_audio", "Audio; enabled|disabled" },
        { "spaceshipper_resolution", "Resolution; 640x480|320x240|960x720|1280x960|1920x1080" },
        { "spaceshipper_deadscreen", "Dead zone (TV overscan); 32x32|0x0|48x36|64x48" },
        { "spaceshipper_hud_scale", "HUD Scale; 1x|1.5x|2x|3x" },
        { NULL, NULL },
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);
}

static void poll_audio_option(void) {
    struct retro_variable var = { "spaceshipper_audio", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        audio_enabled = strcmp(var.value, "disabled") != 0;
    }
}

static void poll_resolution_option(void) {
    struct retro_variable var = { "spaceshipper_resolution", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        unsigned w = 0, h = 0;
        if (sscanf(var.value, "%ux%u", &w, &h) == 2 && w > 0 && h > 0 && w <= UINT16_MAX && h <= UINT16_MAX) {
            screen_w = (uint16_t)w;
            screen_h = (uint16_t)h;
        }
    }
}

static void poll_deadscreen_option(void) {
    struct retro_variable var = { "spaceshipper_deadscreen", NULL };
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        unsigned w = 0, h = 0;
        if (sscanf(var.value, "%ux%u", &w, &h) == 2 && w <= UINT16_MAX && h <= UINT16_MAX) {
            deadscreen_w = (uint16_t)w;
            deadscreen_h = (uint16_t)h;
        }
    }
}

static void poll_hud_scale_option(void) {
    struct retro_variable var = { "spaceshipper_hud_scale", NULL };
    float scale = 1.0f;
    if (environ_cb && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        sscanf(var.value, "%f", &scale); // "Nx" - trailing 'x' just stops the scan
    }
    game_set_hud_scale(scale);
}

// libretro.h documents RETRO_ENVIRONMENT_SET_HW_RENDER as "should be called
// in retro_load_game()" — calling it from retro_set_environment() (before the
// frontend's video driver exists) leaves get_current_framebuffer NULL on at
// least RetroArch, even though get_proc_address comes back valid.
static void negotiate_hw_render(void) {
    unsigned preferred = RETRO_HW_CONTEXT_NONE;
    if (!environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred) ||
        preferred == RETRO_HW_CONTEXT_NONE || (int)preferred == INT_MAX) {
        preferred = RETRO_HW_CONTEXT_OPENGLES2;
    }
    // gl.c only ever calls the GL2/GLES2-shared subset — no Vulkan/D3D/GL-core here.
    if (preferred != RETRO_HW_CONTEXT_OPENGL && preferred != RETRO_HW_CONTEXT_OPENGLES2) {
        preferred = RETRO_HW_CONTEXT_OPENGLES2;
    }

    memset(&hw_render, 0, sizeof(hw_render));
    hw_render.context_type = (enum retro_hw_context_type)preferred;
    hw_render.context_reset = core_context_reset;
    hw_render.context_destroy = core_context_destroy;
    hw_render.depth = true;
    hw_render.stencil = false;
    hw_render.bottom_left_origin = true;
    hw_render.cache_context = false;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        hw_render.context_type = (preferred == RETRO_HW_CONTEXT_OPENGLES2)
            ? RETRO_HW_CONTEXT_OPENGL
            : RETRO_HW_CONTEXT_OPENGLES2;
        environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render);
    }
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

RETRO_API void retro_init(void) {}
RETRO_API void retro_deinit(void) {}

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name = "SpaceShipper";
    info->library_version = "0.1";
    info->valid_extensions = NULL;
    info->need_fullpath = false;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = screen_w;
    info->geometry.base_height = screen_h;
    info->geometry.max_width = screen_w;
    info->geometry.max_height = screen_h;
    info->geometry.aspect_ratio = (float)screen_w / (float)screen_h;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}

RETRO_API void retro_reset(void) { game_reset(); }

RETRO_API void retro_run(void) {
    if (input_poll_cb) input_poll_cb();

    GameInput in;
    memset(&in, 0, sizeof(in));
    if (input_state_cb) {
        in.stick_x = (float)input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / 32767.0f;
        in.stick_y = (float)input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / 32767.0f;
        in.a_held     = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0;
        in.b_held     = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0;
        in.start_held = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0;
        in.l1_held    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0;
        in.r1_held    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0;
        in.l2_held    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) != 0;
        in.r2_held    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) != 0;
        in.up_held    = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0;
        in.down_held  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0;
        in.left_held  = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0;
        in.right_held = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0;
    }

    unsigned int fbo = hw_render.get_current_framebuffer ? (unsigned int)hw_render.get_current_framebuffer() : 0u;

    double now = monotonic_now();
    double dt;
    if (!s_clock_started) {
        s_clock_started = true;
        dt = FIXED_DT;
    } else {
        dt = now - s_last_time;
        if (dt < 0.0) dt = 0.0;
        if (dt > FIXED_DT * MAX_CATCHUP_TICKS) dt = FIXED_DT * MAX_CATCHUP_TICKS;
    }
    s_last_time = now;
    s_accumulator += dt;

    int ticks = 0;
    while (s_accumulator >= FIXED_DT && ticks < MAX_CATCHUP_TICKS) {
        game_update(&in);
        s_accumulator -= FIXED_DT;
        ticks++;

        if (audio_enabled && audio_batch_cb) {
            static int16_t audio_buf[AUDIO_FRAMES_PER_RUN * 2];
            audio_generate(audio_buf, AUDIO_FRAMES_PER_RUN);
            audio_batch_cb(audio_buf, AUDIO_FRAMES_PER_RUN);
        }
    }

    game_render(fbo, (int)screen_w, (int)screen_h, (int)deadscreen_w, (int)deadscreen_h);
    if (video_cb) video_cb(RETRO_HW_FRAME_BUFFER_VALID, screen_w, screen_h, 0);
}

RETRO_API bool retro_load_game(const struct retro_game_info *info) {
    (void)info; // content is intentionally ignored — /dev/null works fine.
    negotiate_hw_render();
    poll_audio_option();
    poll_resolution_option();
    poll_deadscreen_option();
    poll_hud_scale_option();
    audio_init(AUDIO_SAMPLE_RATE);
    game_set_screen((int)screen_w, (int)screen_h);
    game_init();
    return true;
}

RETRO_API bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    (void)type; (void)info; (void)num;
    return false;
}

RETRO_API void retro_unload_game(void) { game_shutdown(); }

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }

RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }

RETRO_API void *retro_get_memory_data(unsigned id) {
    return id == RETRO_MEMORY_SAVE_RAM ? game_get_save_data() : NULL;
}
RETRO_API size_t retro_get_memory_size(unsigned id) {
    return id == RETRO_MEMORY_SAVE_RAM ? game_get_save_size() : 0;
}
