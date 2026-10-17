# mzm-android — native build of Metroid: Zero Mission (decomp) for PC/Android.
#
#   cmake -B build -G Ninja -DMZM_HOST=headless   # or sdl
#   cmake --build build
#
# The GBA ROM build is still the upstream Makefile; this file only drives the
# native port. Game data comes from `make extract` (your own ROM).
cmake_minimum_required(VERSION 3.22)
project(mzm_android C)

set(MZM_REGION "us" CACHE STRING "Game region: us, eu, jp")
set(MZM_HOST "sdl" CACHE STRING "Frontend: sdl or headless")
option(MZM_BUGFIX "Build with the decomp's BUGFIX fixes" ON)
# The decomp is written for agbcc and is very noisy under modern compilers.
set(MZM_GAME_WARNINGS "-w" CACHE STRING "Warning flags for the game code")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

string(TOUPPER "${MZM_REGION}" MZM_REGION_UPPER)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/extracted")
    message(FATAL_ERROR
        "No extracted game data. Put your ROM at mzm_${MZM_REGION}_baserom.gba and run "
        "`python3 tools/extractor.py -r ${MZM_REGION}` (or `make extract`).")
endif()

# ---------------------------------------------------------------------------
# Game code (the decomp), compiled natively.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MZM_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

# A few files carry INCTEXT()/SHIFT_JIS() string macros that tools/preproc.py
# turns into byte arrays; run it on those only.
set(MZM_GAME_COMPILE_SOURCES)
foreach(src IN LISTS MZM_GAME_SOURCES)
    file(STRINGS "${src}" needs_preproc REGEX "INCTEXT\\(|SHIFT_JIS\\(\"" LIMIT_COUNT 1)
    if(needs_preproc)
        file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}" "${src}")
        set(out "${CMAKE_BINARY_DIR}/preproc/${rel}")
        get_filename_component(out_dir "${out}" DIRECTORY)
        add_custom_command(
            OUTPUT "${out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/port/preproc_to_file.py"
                    "${src}" "${CMAKE_SOURCE_DIR}/charmap.txt" "${out}"
            DEPENDS "${src}" "${CMAKE_SOURCE_DIR}/charmap.txt" "${CMAKE_SOURCE_DIR}/tools/preproc.py"
            COMMENT "preproc ${rel}"
            VERBATIM)
        list(APPEND MZM_GAME_COMPILE_SOURCES "${out}")
    else()
        list(APPEND MZM_GAME_COMPILE_SOURCES "${src}")
    endif()
endforeach()

add_library(mzm_game STATIC ${MZM_GAME_COMPILE_SOURCES}
    port/game/linker_glue.c)
target_include_directories(mzm_game PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(mzm_game PRIVATE PLATFORM_PC REGION_${MZM_REGION_UPPER})
if(NOT MZM_BUGFIX)
    target_compile_definitions(mzm_game PRIVATE PORT_NO_BUGFIX)
endif()
target_compile_options(mzm_game PRIVATE
    -std=gnu11
    -nostdinc                 # the game defines its own NULL/types
    -fno-strict-aliasing      # GBA code type-puns freely
    -fwrapv                   # ...and relies on two's-complement overflow
    -fcommon
    -funsigned-char
    ${MZM_GAME_WARNINGS})

# ---------------------------------------------------------------------------
# Port layer.
# ---------------------------------------------------------------------------
add_library(mzm_port STATIC
    port/src/gba_mem.c
    port/src/dma.c
    port/src/bios.c
    port/src/audio_stubs.c
    port/src/frame.c
    port/src/main.c
    port/ppu/src/virtuappu.c
    port/ppu/src/mode1.c)
target_include_directories(mzm_port PUBLIC port/include PRIVATE port/ppu/include)
target_compile_options(mzm_port PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
find_package(OpenMP COMPONENTS C)
if(OpenMP_C_FOUND)
    target_link_libraries(mzm_port PRIVATE OpenMP::OpenMP_C)
endif()
target_link_libraries(mzm_port PUBLIC m)

# ---------------------------------------------------------------------------
# Executables.
# ---------------------------------------------------------------------------
if(MZM_HOST STREQUAL "headless")
    add_executable(mzm_headless port/src/host_headless.c)
    target_compile_options(mzm_headless PRIVATE -std=gnu11 -Wall -Wextra)
    target_link_libraries(mzm_headless PRIVATE mzm_port mzm_game mzm_port)
elseif(MZM_HOST STREQUAL "sdl")
    find_package(SDL3 REQUIRED CONFIG)
    if(ANDROID)
        # SDLActivity loads libmain.so and calls SDL_main.
        add_library(main SHARED port/src/host_sdl.c)
        set(MZM_EXE main)
    else()
        add_executable(mzm port/src/host_sdl.c)
        set(MZM_EXE mzm)
    endif()
    target_compile_options(${MZM_EXE} PRIVATE -std=gnu11 -Wall -Wextra)
    target_link_libraries(${MZM_EXE} PRIVATE mzm_port mzm_game mzm_port SDL3::SDL3)
else()
    message(FATAL_ERROR "MZM_HOST must be sdl or headless")
endif()
