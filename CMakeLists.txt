cmake_minimum_required(VERSION 3.15)

if(DEFINED TARGET)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/zig-toolchain.cmake" CACHE STRING "zig cross compiler")
endif()

project(startshipper_libretro C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(DEBUG "not strip / remove symbols" OFF)

include(FetchContent)

set(LIBRETRO_DIR "${CMAKE_SOURCE_DIR}/vendor/mono/retroarch")
set(LIBRETRO_DOWNLOAD "https://raw.githubusercontent.com/libretro/RetroArch/ee3eae34c425a1645b923395854dda7f2543df3/libretro-common/include/libretro.h")
if (NOT EXISTS "${LIBRETRO_DIR}/libretro.h")
    file(DOWNLOAD "${LIBRETRO_DOWNLOAD}" "${LIBRETRO_DIR}/libretro.h" SHOW_PROGRESS)
endif()

set(GLAD_DOWNLOAD "https://github.com/gly-engine/archive/archive/refs/heads/glad.tar.gz")
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/vendor/opengl/glad")
if (NOT EXISTS "${GLAD_DIR}/include")
    FetchContent_Populate(glad URL ${GLAD_DOWNLOAD} SOURCE_DIR ${GLAD_DIR})
endif()
set(FONTS_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/gecnd")

set(TAMZEN_DIR "${CMAKE_SOURCE_DIR}/vendor/mono/tamzen-font")
set(TAMZEN_DOWNLOAD "https://github.com/sunaku/tamzen-font/archive/3255e8259bc9b880c60ab8b737ec8aa574e00d75.tar.gz")
if (NOT EXISTS "${TAMZEN_DIR}")
    FetchContent_Populate(tamzen_font URL "${TAMZEN_DOWNLOAD}" SOURCE_DIR ${TAMZEN_DIR})
endif()

if (DEFINED TARGET)
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
    source/backend/loadgl.c
    source/backend/math4.c
    source/backend/text.c
    source/game/main.c

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
)

if(NOT DEBUG)
    set_target_properties(${PROJECT_NAME} PROPERTIES
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
endif()

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m ${CMAKE_DL_LIBS})
endif()
