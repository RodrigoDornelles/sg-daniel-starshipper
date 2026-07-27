// StartShipper libretro core.
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

#include <limits.h>
#include <string.h>

#define SCREEN_W 640u
#define SCREEN_H 480u

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static struct retro_hw_render_callback hw_render;

static void RETRO_CALLCONV core_context_reset(void) {
    bool is_gles = (hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 ||
                    hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 ||
                    hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION);
    gl_backend_init((gl_proc_address_fn)hw_render.get_proc_address, is_gles);
    text_backend_init();
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
    info->library_name = "StartShipper";
    info->library_version = "0.1";
    info->valid_extensions = NULL;
    info->need_fullpath = false;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width = SCREEN_W;
    info->geometry.base_height = SCREEN_H;
    info->geometry.max_width = SCREEN_W;
    info->geometry.max_height = SCREEN_H;
    info->geometry.aspect_ratio = (float)SCREEN_W / (float)SCREEN_H;
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

    game_update(&in);
    game_render(fbo, (int)SCREEN_W, (int)SCREEN_H);

    if (video_cb) video_cb(RETRO_HW_FRAME_BUFFER_VALID, SCREEN_W, SCREEN_H, 0);
}

RETRO_API bool retro_load_game(const struct retro_game_info *info) {
    (void)info; // content is intentionally ignored — /dev/null works fine.
    negotiate_hw_render();
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
