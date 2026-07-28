// Minimal GL/GLES2 function loader — resolves exactly the ~40 entry points
// gl.c/text.c actually call, nothing more. Replaces glad's generated loader
// (vendor/opengl/glad/src/gl.c, no longer compiled into this target), which
// eagerly resolved every GL 1.0-2.1 + GLES2 entry point whether used or not
// (~24KB of .text for functions this game never calls). glad's headers
// (typedefs, enums, the glad_glFoo globals + `#define glFoo glad_glFoo`
// macros) are still used as-is — only the loading code changes.
#ifndef STARTSHIPPER_LOADGL_H
#define STARTSHIPPER_LOADGL_H

#include "glad/gl.h"

// `load` matches libretro's retro_hw_get_proc_address_t / glad's GLADloadfunc
// (const char *name) -> function pointer, or NULL if unsupported. No version
// detection or extension probing here — if something we call turns out to be
// missing, that surfaces at shader compile/link time (see gl_backend_init).
void gl_load_functions(GLADloadfunc load);

#endif
