#include "loadgl.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>

// glad/gl.h only *declares* these (GLAD_API_CALL expands to `extern` [+
// visibility attribute]) — glad's own generated gl.c used to define them.
// Since that file is no longer compiled in, storage for exactly the ones we
// use has to live somewhere; here.
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;

static void *gl_fallback_lib(void) {
    static void *handle = NULL;
    static bool tried = false;
    if (!tried) {
        tried = true;
        handle = dlopen("libGLESv2.so.2", RTLD_LAZY | RTLD_LOCAL);
        if (!handle) handle = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_LOCAL);
        if (!handle) handle = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    }
    return handle;
}

static void *gl_load_symbol(GLADloadfunc load, const char *name) {
    void *p = (void *) load(name);
    if (p) return p;
    void *handle = gl_fallback_lib();
    return handle ? dlsym(handle, name) : NULL;
}

#define GLFN_LOAD(NAME, TYPE) (glad_##NAME = (TYPE) gl_load_symbol(load, #NAME))

void gl_load_functions(GLADloadfunc load) {
    GLFN_LOAD(glActiveTexture, PFNGLACTIVETEXTUREPROC);
    GLFN_LOAD(glAttachShader, PFNGLATTACHSHADERPROC);
    GLFN_LOAD(glBindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC);
    GLFN_LOAD(glBindBuffer, PFNGLBINDBUFFERPROC);
    GLFN_LOAD(glBindFramebuffer, PFNGLBINDFRAMEBUFFERPROC);
    GLFN_LOAD(glBindTexture, PFNGLBINDTEXTUREPROC);
    GLFN_LOAD(glBlendFunc, PFNGLBLENDFUNCPROC);
    GLFN_LOAD(glBufferData, PFNGLBUFFERDATAPROC);
    GLFN_LOAD(glBufferSubData, PFNGLBUFFERSUBDATAPROC);
    GLFN_LOAD(glCheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC);
    GLFN_LOAD(glClear, PFNGLCLEARPROC);
    GLFN_LOAD(glClearColor, PFNGLCLEARCOLORPROC);
    GLFN_LOAD(glClearDepthf, PFNGLCLEARDEPTHFPROC);
    GLFN_LOAD(glCompileShader, PFNGLCOMPILESHADERPROC);
    GLFN_LOAD(glCreateProgram, PFNGLCREATEPROGRAMPROC);
    GLFN_LOAD(glCreateShader, PFNGLCREATESHADERPROC);
    GLFN_LOAD(glDeleteBuffers, PFNGLDELETEBUFFERSPROC);
    GLFN_LOAD(glDeleteProgram, PFNGLDELETEPROGRAMPROC);
    GLFN_LOAD(glDeleteShader, PFNGLDELETESHADERPROC);
    GLFN_LOAD(glDeleteTextures, PFNGLDELETETEXTURESPROC);
    GLFN_LOAD(glDepthFunc, PFNGLDEPTHFUNCPROC);
    GLFN_LOAD(glDepthMask, PFNGLDEPTHMASKPROC);
    GLFN_LOAD(glDisable, PFNGLDISABLEPROC);
    GLFN_LOAD(glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC);
    GLFN_LOAD(glDrawArrays, PFNGLDRAWARRAYSPROC);
    GLFN_LOAD(glEnable, PFNGLENABLEPROC);
    GLFN_LOAD(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC);
    GLFN_LOAD(glGenBuffers, PFNGLGENBUFFERSPROC);
    GLFN_LOAD(glGenTextures, PFNGLGENTEXTURESPROC);
    GLFN_LOAD(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC);
    GLFN_LOAD(glGetProgramiv, PFNGLGETPROGRAMIVPROC);
    GLFN_LOAD(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC);
    GLFN_LOAD(glGetShaderiv, PFNGLGETSHADERIVPROC);
    GLFN_LOAD(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC);
    GLFN_LOAD(glLinkProgram, PFNGLLINKPROGRAMPROC);
    GLFN_LOAD(glPixelStorei, PFNGLPIXELSTOREIPROC);
    GLFN_LOAD(glShaderSource, PFNGLSHADERSOURCEPROC);
    GLFN_LOAD(glTexImage2D, PFNGLTEXIMAGE2DPROC);
    GLFN_LOAD(glTexParameteri, PFNGLTEXPARAMETERIPROC);
    GLFN_LOAD(glUniform1i, PFNGLUNIFORM1IPROC);
    GLFN_LOAD(glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC);
    GLFN_LOAD(glUseProgram, PFNGLUSEPROGRAMPROC);
    GLFN_LOAD(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC);
    GLFN_LOAD(glViewport, PFNGLVIEWPORTPROC);
}
