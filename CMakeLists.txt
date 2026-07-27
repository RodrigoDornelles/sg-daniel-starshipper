cmake_minimum_required(VERSION 3.15)
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

add_library(${PROJECT_NAME} SHARED
    source/frontend/libretro/libretro_core.c
    source/backend/gl.c
    source/backend/math4.c
    source/game/main.c
    ${GLAD_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LIBRETRO_DIR}
    ${GLAD_DIR}/include
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "startshipper_libretro"
)

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE m ${CMAKE_DL_LIBS})
endif()
