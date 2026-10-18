# mzm-android — native build of Metroid: Zero Mission (decomp) for PC/Android.
#
#   cmake -B build -G Ninja -DMZM_HOST=headless   # or sdl
#   cmake --build build
#
# The GBA ROM build is still the upstream Makefile; this file only drives the
# native port. No ROM is needed to build: game data is read from the player's
# own ROM when the game starts.
cmake_minimum_required(VERSION 3.22)
project(mzm_android C)

set(MZM_REGION "us" CACHE STRING "Game region: us, eu, jp")
set(MZM_HOST "sdl" CACHE STRING "Frontend: sdl or headless")
option(MZM_BUGFIX "Build with the decomp's BUGFIX fixes" ON)
# The decomp is written for agbcc and is very noisy under modern compilers.
set(MZM_GAME_WARNINGS "-w" CACHE STRING "Warning flags for the game code")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Android links everything into libmain.so
set(MZM_SDL3_TAG "release-3.4.18" CACHE STRING "SDL3 version fetched when no SDL3 is installed")
set(MZM_IMGUI_TAG "v1.92.9b" CACHE STRING "Dear ImGui version (options menu)")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

string(TOUPPER "${MZM_REGION}" MZM_REGION_UPPER)

# ---------------------------------------------------------------------------
# Game code (the decomp), compiled natively.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MZM_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")
list(FILTER MZM_GAME_SOURCES EXCLUDE REGEX "/src/data/")

# Game data (src/data/**.c) is never compiled with real ROM contents: each file
# is wrapped so its arrays are zero-filled at build time and registered for
# loading from the player's ROM at startup (tools/port/gen_assets.py).
file(GLOB_RECURSE MZM_DATA_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/data/*.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/database.json" "${CMAKE_SOURCE_DIR}/tools/port/gen_assets.py" ${MZM_DATA_SOURCES})
set(MZM_GEN_DIR "${CMAKE_BINARY_DIR}/gen_assets")
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/port/gen_assets.py"
            --root "${CMAKE_SOURCE_DIR}" --out "${MZM_GEN_DIR}" --region ${MZM_REGION}
            --cc "${CMAKE_C_COMPILER}"
    OUTPUT_VARIABLE MZM_DATA_WRAPPERS
    RESULT_VARIABLE gen_result
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT gen_result EQUAL 0)
    message(FATAL_ERROR "tools/port/gen_assets.py failed")
endif()
string(REPLACE "\n" ";" MZM_DATA_WRAPPERS "${MZM_DATA_WRAPPERS}")

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

add_library(mzm_game STATIC ${MZM_GAME_COMPILE_SOURCES} ${MZM_DATA_WRAPPERS}
    port/game/linker_glue.c
    port/game/audio_engine.c
    port/game/warp.c
    port/game/widescreen.c
    port/game/sound_data.c)
# The zero-filled .inc stubs shadow any real `make extract` output.
target_include_directories(mzm_game PRIVATE "${MZM_GEN_DIR}/stub" "${CMAKE_SOURCE_DIR}/include")
# Data arrays must be writable to receive the ROM contents (they are mostly
# zero-initialized, so they land in .bss and cost nothing in the binary).
set_source_files_properties(${MZM_DATA_WRAPPERS} port/game/sound_data.c PROPERTIES COMPILE_DEFINITIONS "const=")
target_compile_definitions(mzm_game PRIVATE PLATFORM_PC REGION_${MZM_REGION_UPPER})
if(NOT MZM_BUGFIX)
    target_compile_definitions(mzm_game PRIVATE PORT_NO_BUGFIX)
endif()
target_compile_options(mzm_game PRIVATE
    -std=gnu11
    -include port/prototypes.h # no implicit declarations on 64-bit hosts
    -nostdinc                 # the game defines its own NULL/types
    -fno-strict-aliasing      # GBA code type-puns freely
    -fwrapv                   # ...and relies on two's-complement overflow
    -fcommon
    -funsigned-char
    ${MZM_GAME_WARNINGS})
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # A few functions `return;` without a value (callers ignore the result).
    # Implicit function declarations stay errors: see include/port/prototypes.h.
    target_compile_options(mzm_game PRIVATE -Wno-error=return-type)
endif()

# ---------------------------------------------------------------------------
# Port layer.
# ---------------------------------------------------------------------------
add_library(mzm_port STATIC
    port/src/assets.c
    port/src/sha1.c
    port/src/gba_mem.c
    port/src/dma.c
    port/src/bios.c
    port/src/apu.c
    port/src/warp.c
    port/src/widescreen.c
    port/src/frame.c
    port/src/main.c
    port/src/savestate.c
    port/ppu/src/virtuappu.c
    port/ppu/src/mode1.c)
target_include_directories(mzm_port PUBLIC port/include PRIVATE port/ppu/include)
file(READ "${CMAKE_SOURCE_DIR}/mzm_${MZM_REGION}.sha1" MZM_SHA1_LINE)
string(REGEX MATCH "^[0-9a-f]+" MZM_ROM_SHA1 "${MZM_SHA1_LINE}")
target_compile_definitions(mzm_port PRIVATE
    MZM_ROM_SHA1="${MZM_ROM_SHA1}" MZM_ROM_REGION_NAME="${MZM_REGION_UPPER}")
target_compile_options(mzm_port PRIVATE -std=gnu11 -Wall -Wextra -Wno-unused-parameter)
# The PPU renders scanlines in parallel with OpenMP when available. Not used on
# Android (240x160 takes ~1 ms on one core, and it avoids shipping libomp).
if(NOT ANDROID)
    find_package(OpenMP COMPONENTS C)
    if(OpenMP_C_FOUND)
        target_link_libraries(mzm_port PRIVATE OpenMP::OpenMP_C)
    endif()
endif()
target_link_libraries(mzm_port PUBLIC m)
if(ANDROID)
    target_link_libraries(mzm_port PUBLIC log)
endif()

# ---------------------------------------------------------------------------
# Executables.
# ---------------------------------------------------------------------------
if(MZM_HOST STREQUAL "headless")
    add_executable(mzm_headless port/src/host_headless.c)
    target_compile_options(mzm_headless PRIVATE -std=gnu11 -Wall -Wextra)
    target_link_libraries(mzm_headless PRIVATE mzm_port mzm_game mzm_port)
elseif(MZM_HOST STREQUAL "sdl")
    if(NOT ANDROID)
        find_package(SDL3 CONFIG QUIET)
    endif()
    if(NOT TARGET SDL3::SDL3)
        # No system SDL3 (always the case on Android): build it from source.
        include(FetchContent)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG ${MZM_SDL3_TAG}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()
    # Options menu: Dear ImGui with its SDL3 + SDL_Renderer backends.
    enable_language(CXX)
    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG ${MZM_IMGUI_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui)
    add_library(mzm_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(mzm_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_compile_definitions(mzm_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS
                                                IMGUI_DISABLE_DEBUG_TOOLS)
    target_link_libraries(mzm_imgui PUBLIC SDL3::SDL3)

    set(MZM_SDL_SOURCES port/src/host_sdl.c port/src/config.c port/src/menu.cpp)
    if(ANDROID)
        # SDLActivity loads libmain.so and calls SDL_main.
        add_library(main SHARED ${MZM_SDL_SOURCES})
        set(MZM_EXE main)
    else()
        add_executable(mzm ${MZM_SDL_SOURCES})
        set(MZM_EXE mzm)
    endif()
    target_compile_options(${MZM_EXE} PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=gnu11> -Wall -Wextra)
    target_compile_features(${MZM_EXE} PRIVATE cxx_std_17)
    target_link_libraries(${MZM_EXE} PRIVATE mzm_port mzm_game mzm_port mzm_imgui SDL3::SDL3)
else()
    message(FATAL_ERROR "MZM_HOST must be sdl or headless")
endif()
