// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — internal interface of the port layer (host side).
//
// Port code is normal hosted C: it may use libc. It must NOT include game
// headers (they redefine NULL and the base types for -nostdinc builds); the
// few game symbols the port needs are declared in port_game.h.
#ifndef PORT_H
#define PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PLATFORM_PC 1
#include "../../include/port/platform.h"

#define GBA_SCREEN_WIDTH 240
#define GBA_SCREEN_HEIGHT 160

// ---- I/O register helpers (gPortIo is the 0x04000000 region) -------------
static inline uint16_t Port_IoRead16(uint32_t off)
{
    uint16_t v;
    memcpy(&v, &gPortIo[off], sizeof(v));
    return v;
}

static inline void Port_IoWrite16(uint32_t off, uint16_t v)
{
    memcpy(&gPortIo[off], &v, sizeof(v));
}

enum {
    IO_DISPCNT = 0x000,
    IO_DISPSTAT = 0x004,
    IO_VCOUNT = 0x006,
    IO_KEYINPUT = 0x130,
    IO_IE = 0x200,
    IO_IF = 0x202,
    IO_IME = 0x208,
};

// GBA KEYINPUT bit layout.
enum {
    PORT_KEY_A = 1 << 0,
    PORT_KEY_B = 1 << 1,
    PORT_KEY_SELECT = 1 << 2,
    PORT_KEY_START = 1 << 3,
    PORT_KEY_RIGHT = 1 << 4,
    PORT_KEY_LEFT = 1 << 5,
    PORT_KEY_UP = 1 << 6,
    PORT_KEY_DOWN = 1 << 7,
    PORT_KEY_R = 1 << 8,
    PORT_KEY_L = 1 << 9,
    PORT_KEY_MASK = 0x3FF,
};

// ---- port subsystems ------------------------------------------------------
void Port_MemReset(void);

void Port_DmaReset(void);
void Port_DmaOnHBlank(void); // run HBlank-timed channels (one scanline)
void Port_DmaOnVBlank(void); // run VBlank-timed channels

// Sound: plays one frame of DirectSound + PSG output (port/src/apu.c).
#define PORT_AUDIO_RATE 48000
void Port_ApuReset(void);
void Port_ApuRunFrame(void);

void Port_PpuInit(void);
const uint32_t* Port_PpuRenderFrame(void); // ABGR8888, 240x160, pitch 240

// One GBA frame: VBlank IRQ, scanline render with HBlank/VCount IRQs,
// present, input, pacing. Called from the game's Halt/VBlankIntrWait.
void Port_RunFrame(void);
uint64_t Port_FrameCount(void);

// ---- host (frontend) interface: host_sdl.c / host_headless.c ---------------
typedef struct PortHostConfig {
    int argc;
    char** argv;
} PortHostConfig;

int Port_Main(int argc, char** argv);

bool Port_HostInit(const PortHostConfig* cfg);
void Port_HostShutdown(void);
// Present a finished frame. Returns false if the user asked to quit.
bool Port_HostPresent(const uint32_t* abgr, int width, int height);
// Pressed keys, as PORT_KEY_* bits (1 = pressed).
uint16_t Port_HostPollKeys(void);
// One frame of sound: interleaved stereo, PORT_AUDIO_RATE Hz.
void Port_HostQueueAudio(const int16_t* samples, int frames);
// Block until the next 1/59.73 s tick (no-op when running unthrottled).
void Port_HostWaitFrame(void);
// The player's ROM. Returns a malloc'd copy of the whole file, or NULL if
// the user gave up (the game then quits). `error` is set on a retry after
// a ROM was rejected, so the host can tell the user why.
uint8_t* Port_HostLoadRom(size_t* size, const char* error);
// Remember a validated ROM for next time (copy it into the app's storage).
void Port_HostRomAccepted(const uint8_t* rom, size_t size);
// Persistent save data (the 64 KiB SRAM/Flash image).
void Port_HostLoadSave(uint8_t* data, size_t size);
void Port_HostWriteSave(const uint8_t* data, size_t size);
// Writes the save through the host if the game changed it.
void Port_FlushSave(void);

// Raises the "quit" request; the frame loop longjmps back to main.
void Port_RequestQuit(void);
bool Port_QuitRequested(void);

void Port_Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
_Noreturn void Port_Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#endif // PORT_H
