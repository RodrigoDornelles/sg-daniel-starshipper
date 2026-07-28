cmake_minimum_required(VERSION 3.15)

if(DEFINED TARGET)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/zig-toolchain.cmake" CACHE STRING "zig cross compiler")
endif()

project(startshipper_libretro C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# libretro.h — API header only, no core assumes a frontend already exists.
# ---------------------------------------------------------------------------
set(LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/mono/retroarch")
set(LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df3/libretro-common/include/libretro.h")
if (NOT EXISTS "${LIBRETRO_DIR}/libretro.h")
    file(DOWNLOAD "${LIBRETRO_DOWNLOAD}" "${LIBRETRO_DIR}/libretro.h" SHOW_PROGRESS)
endif()

# ---------------------------------------------------------------------------
# glad — GL2/GLES2 loader used by source/backend/gl.c
# ---------------------------------------------------------------------------
set(GLAD_DOWNLOAD "https://github.com/gly-engine/archive/archive/refs/heads/glad.tar.gz")
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/vendor/opengl/glad")
if (NOT EXISTS "${GLAD_DIR}/include")
    FetchContent_Populate(glad URL ${GLAD_DOWNLOAD} SOURCE_DIR ${GLAD_DIR})
endif()

# gl.c was generated with --merge --api='gl:core=2.1,gles2=2.0': a single
# glad_gl* pointer table shared by both a desktop GL2.1 and a GLES2 context.
# gles2.c/glx.c/wgl.c/egl.c are the legacy standalone loaders — unused here,
# the libretro frontend already owns context creation.
set(GLAD_SOURCES "${GLAD_DIR}/src/gl.c")

# ---------------------------------------------------------------------------
# Tamzen font — HUD/menu text (source/backend/text.c) draws precomputed
# glyph bitmaps, no font-format parsing at runtime at all. tools/fonts.c
# (built below, same host-native pattern as xxd_tool) reads the BDF sources
# for Tamzen5x9 regular+bold at *build* time and bakes each glyph's pixels +
# metrics straight into a C header (see include/bitfont.h) — the shipped
# core never touches BDF/OTB/PCF parsing, just copies bytes into a texture.
# ---------------------------------------------------------------------------
set(FONTS_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/gecnd")

set(TAMZEN_DIR "${CMAKE_SOURCE_DIR}/vendor/mono/tamzen-font")
set(TAMZEN_DOWNLOAD "https://github.com/sunaku/tamzen-font/archive/3255e8259bc9b880c60ab8b737ec8aa574e00d75.tar.gz")
if (NOT EXISTS "${TAMZEN_DIR}")
    FetchContent_Populate(tamzen_font URL "${TAMZEN_DOWNLOAD}" SOURCE_DIR ${TAMZEN_DIR})
endif()

# xxd_tool: generic "xxd -i"-alike — embeds any file as a C byte array.
# Used below for the font and the GLSL sources, so the built .so has nothing
# to locate at runtime (no font file, no shader files).
#
# When cross-compiling (TARGET set -> zig-toolchain.cmake), CMAKE_C_COMPILER
# is the *target* compiler (e.g. armv7) — a binary built with it can't run on
# this machine during the build. zig-toolchain.cmake also drops a host-native
# "zig-cc-host" wrapper next to it (no -target flag, compiles for whatever's
# running cmake); use that to build xxd_tool directly, bypassing
# add_executable, which would always go through CMAKE_C_COMPILER.
if (DEFINED TARGET)
    # "xxd", not "xxd_tool" — same name as the custom target below trips up
    # make's dependency graph (harmless "circular dependency abandoned"
    # warning, but avoidable).
    set(XXD_TOOL_BIN "${CMAKE_BINARY_DIR}/xxd")
    add_custom_command(
        OUTPUT "${XXD_TOOL_BIN}"
        COMMAND "${CMAKE_BINARY_DIR}/zig-cc-host" -O2 -o "${XXD_TOOL_BIN}" "${CMAKE_SOURCE_DIR}/tools/xxd.c"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/xxd.c"
        COMMENT "Building xxd_tool for the host (zig-cc-host)"
        VERBATIM
    )
    add_custom_target(xxd_tool ALL DEPENDS "${XXD_TOOL_BIN}")
    set(XXD_TOOL_CMD "${XXD_TOOL_BIN}")
else()
    add_executable(xxd_tool tools/xxd.c)
    set(XXD_TOOL_CMD "$<TARGET_FILE:xxd_tool>")
endif()

function(embed_file INPUT SYMBOL FILENAME OUTVAR)
    set(_header "${FONTS_INCLUDE_DIR}/${FILENAME}")
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FONTS_INCLUDE_DIR}"
        COMMAND ${XXD_TOOL_CMD} "${INPUT}" "${_header}" "${SYMBOL}"
        DEPENDS xxd_tool "${INPUT}"
        COMMENT "Embedding ${FILENAME}"
        VERBATIM
    )
    set(${OUTVAR} "${_header}" PARENT_SCOPE)
endfunction()

# fonts_tool: BDF -> C header baker (tools/fonts.c). Same host-native /
# cross-compile split as xxd_tool above, for the same reason (it must run
# during the build, on the machine doing the building).
if (DEFINED TARGET)
    set(FONTS_TOOL_BIN "${CMAKE_BINARY_DIR}/fonts")
    add_custom_command(
        OUTPUT "${FONTS_TOOL_BIN}"
        COMMAND "${CMAKE_BINARY_DIR}/zig-cc-host" -O2 -o "${FONTS_TOOL_BIN}" "${CMAKE_SOURCE_DIR}/tools/fonts.c"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/fonts.c"
        COMMENT "Building fonts_tool for the host (zig-cc-host)"
        VERBATIM
    )
    add_custom_target(fonts_tool ALL DEPENDS "${FONTS_TOOL_BIN}")
    set(FONTS_TOOL_CMD "${FONTS_TOOL_BIN}")
else()
    add_executable(fonts_tool tools/fonts.c)
    set(FONTS_TOOL_CMD "$<TARGET_FILE:fonts_tool>")
endif()

function(bake_font INPUT SYMBOL_PREFIX FILENAME OUTVAR)
    set(_header "${FONTS_INCLUDE_DIR}/${FILENAME}")
    add_custom_command(
        OUTPUT "${_header}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${FONTS_INCLUDE_DIR}"
        COMMAND ${FONTS_TOOL_CMD} "${INPUT}" "${_header}" "${SYMBOL_PREFIX}"
        DEPENDS fonts_tool "${INPUT}" "${CMAKE_SOURCE_DIR}/include/bitfont.h"
        COMMENT "Baking ${FILENAME}"
        VERBATIM
    )
    set(${OUTVAR} "${_header}" PARENT_SCOPE)
endfunction()

bake_font("${TAMZEN_DIR}/bdf/Tamzen5x9r.bdf" TAMZEN_5X9_REGULAR "tamzen_5x9_regular.h" TAMZEN_REGULAR_HEADER)
bake_font("${TAMZEN_DIR}/bdf/Tamzen5x9b.bdf" TAMZEN_5X9_BOLD "tamzen_5x9_bold.h" TAMZEN_BOLD_HEADER)
embed_file("${CMAKE_SOURCE_DIR}/source/shaders/basic.vert" SHADER_BASIC_VERT_SRC "shader_basic_vert.h" BASIC_VERT_HEADER)
embed_file("${CMAKE_SOURCE_DIR}/source/shaders/basic.frag" SHADER_BASIC_FRAG_SRC "shader_basic_frag.h" BASIC_FRAG_HEADER)
embed_file("${CMAKE_SOURCE_DIR}/source/shaders/text.vert" SHADER_TEXT_VERT_SRC "shader_text_vert.h" TEXT_VERT_HEADER)
embed_file("${CMAKE_SOURCE_DIR}/source/shaders/text.frag" SHADER_TEXT_FRAG_SRC "shader_text_frag.h" TEXT_FRAG_HEADER)

add_custom_target(generate_headers DEPENDS
    "${TAMZEN_REGULAR_HEADER}" "${TAMZEN_BOLD_HEADER}"
    "${BASIC_VERT_HEADER}" "${BASIC_FRAG_HEADER}" "${TEXT_VERT_HEADER}" "${TEXT_FRAG_HEADER}")

add_library(${PROJECT_NAME} SHARED
    source/frontend/libretro/libretro_core.c
    source/backend/gl.c
    source/backend/math4.c
    source/backend/text.c
    source/game/main.c
    ${GLAD_SOURCES}

    # Procedural audio engine (source/sound/) - no WAV/OGG assets, everything
    # below is generated PCM: oscillators -> synth voices -> music/SFX -> mixer.
    source/sound/audio_rng.c
    source/sound/osc.c
    source/sound/envelope.c
    source/sound/filter.c
    source/sound/synth.c
    source/sound/mixer.c
    source/sound/sfx.c
    source/sound/audio_device.c
    source/sound/music/scales.c
    source/sound/music/bass.c
    source/sound/music/lead.c
    source/sound/music/pad.c
    source/sound/music/drums.c
    source/sound/music/sequencer.c
    source/sound/sfx/laser.c
    source/sound/sfx/explosion.c
    source/sound/sfx/jump.c
    source/sound/sfx/pickup.c
    source/sound/sfx/impact.c
    source/sound/sfx/ui.c
    source/sound/sfx/thrust.c
)
add_dependencies(${PROJECT_NAME} generate_headers)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LIBRETRO_DIR}
    ${GLAD_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "startshipper_libretro"
    C_VISIBILITY_PRESET hidden
    INTERPROCEDURAL_OPTIMIZATION TRUE
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-asynchronous-unwind-tables
)
target_link_options(${PROJECT_NAME} PRIVATE
    -Wl,--gc-sections
    -Wl,--build-id=none
    -s
)

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m ${CMAKE_DL_LIBS})
endif()
