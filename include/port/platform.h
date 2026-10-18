#ifndef PORT_PLATFORM_H
#define PORT_PLATFORM_H

/*
 * mzm-android: native (PC / Android) build glue for the game code.
 *
 * On the GBA the game talks to hardware through fixed addresses
 * (0x02000000 EWRAM, 0x04000000 I/O, 0x06000000 VRAM, ...). In the native
 * build those regions are plain host buffers owned by the port layer
 * (port/src/gba_mem.c) and the *_BASE macros in include/gba/memory.h point
 * at them instead. The port's software PPU reads the same buffers.
 *
 * This header must stay free of libc includes: game translation units are
 * compiled with -nostdinc and define their own NULL/u8/u32 types.
 */

#ifdef PLATFORM_PC

typedef __UINTPTR_TYPE__ port_uptr;

extern unsigned char gPortEwram[256 * 0x400];
extern unsigned char gPortIwram[32 * 0x400];
extern unsigned char gPortIo[0x400];
extern unsigned char gPortPalram[0x400];
extern unsigned char gPortVram[96 * 0x400];
extern unsigned char gPortOam[0x400];
extern unsigned char gPortSram[64 * 0x400];
/* Nothing reads the cartridge directly in C; ROM_BASE only needs to be a
 * distinct, non-NULL address (soft-reset entry point bookkeeping). */
extern unsigned char gPortRomStub[0x100];

/* DMA: 64-bit pointers don't fit the 32-bit DMA registers, so every DMA
 * request goes through the port, which performs it (or schedules it for
 * HBlank/VBlank) with full-width pointers. */
void Port_DmaSet(unsigned int channel, const void* src, void* dst, unsigned int control);

/* BIOS SWI replacement (Halt, Stop, ...). */
void Port_Syscall(unsigned int number);

/* REG_VCOUNT read from a busy-wait loop: advances a virtual scanline and
 * returns it. */
unsigned short Port_PollVCount(void);

/* Called after the game writes save memory, so the host can persist it. */
void Port_SramWritten(void);

/* The player's ROM stays in memory: the sound engine reads its track scripts,
 * voicegroups and samples in place. Translates a GBA ROM address
 * (0x08xxxxxx) to the host copy, or returns NULL if it is outside the ROM. */
const void* Port_RomPointer(unsigned int gbaAddress);

/* The same bytes as `data` (a ROM data array), but inside the ROM copy. For
 * the few places where the game reads past the end of an array: on the GBA
 * that reads the next data in the ROM, and through this pointer it does too.
 * Returns `data` itself if it isn't ROM data. */
const void* Port_RomMirror(const void* data);
/* Same for any array of `size` bytes: data compiled into the game (not
 * copied from the ROM) is found in the ROM by its contents (it must appear
 * there only once). */
const void* Port_RomFind(const void* data, unsigned int size);

/* Game side of the sound port (port/game/audio_engine.c). */
struct Voice;
/* Builds the sound tables that hold ROM pointers (called once the ROM is
 * loaded). */
void Port_SoundInit(const unsigned char* rom, unsigned int size);
/* Host copy of the voicegroup at a GBA address (never NULL). */
struct Voice* Port_SoundVoicegroup(unsigned int gbaAddress);
/* Converts the cached voicegroups again from their addresses (after a save
 * state restored the cache's address list). */
void Port_SoundRebuildVoicegroups(void);
/* The next 16 samples sound DMA 1/2 would feed to FIFO A/B. Returns 0 when
 * the sound engine hasn't set up its buffers yet. */
int Port_SoundFifoBlock(const signed char** fifoA, const signed char** fifoB);

/* Test warp (MZM_WARP): target `index` of the list, room -1 = every room of
 * the area. Returns 0 past the end. The interval is how many frames each
 * target lasts (0 = stay in the only one). */
int Port_WarpTarget(int index, int* area, int* room);
/* "Areas" of the warp list that start a game mode; room = its number. */
enum {
    PORT_WARP_CUTSCENE = 100,
    PORT_WARP_TOURIAN_ESCAPE,
    PORT_WARP_CHOZODIA_ESCAPE,
    PORT_WARP_CREDITS,
};
unsigned int Port_WarpInterval(void);
/* Game side (port/game/warp.c): called once per frame, and after SamusInit. */
void Port_WarpUpdate(void);
void Port_WarpEquip(void);

/* Widescreen (port/src/widescreen.c). Extra columns the frame has on each
 * side of the GBA screen (0 = option off). */
int Port_WideExtra(void);
/* Display column where GBA x = 0 was drawn in the last wide frame (between 0
 * and 2 * Port_WideExtra()); the game uses it to draw the sprites that are
 * visible in the extra columns. */
int Port_WideOrigin(void);
/* Game side (port/game/widescreen.c), called at the end of every in-game
 * frame: refreshes the extra tilemap columns and reports the frame. */
void Port_WideInGameFrame(int hudEnd, int drewOam);
/* Sprite status for drawing: the GBA status, plus "on screen" when the
 * sprite is in the extra columns of a wide frame. */
struct SpriteData;
unsigned short Port_WideSpriteStatus(struct SpriteData* pSprite);
/* OAM entries [from, to) belong to a sprite with a screen position. */
void Port_WideMarkCenter(int from, int to);
/* Port side: one in-game frame. `oamTags` gives the placement of each of the
 * 128 OAM entries (PORT_WIDE_OAM_*); cameraX is the left edge of the GBA
 * screen and [left, right) the part of the room the camera may show, in
 * pixels of the room. */
enum {
    PORT_WIDE_OAM_SCREEN,    /* drawn like on the GBA, clipped to its screen */
    PORT_WIDE_OAM_WORLD,     /* follows the room, visible in the extra columns */
    PORT_WIDE_OAM_HUD_LEFT,  /* anchored to the left edge of the picture */
    PORT_WIDE_OAM_HUD_RIGHT, /* anchored to the right edge */
    PORT_WIDE_OAM_CENTER,    /* centered on the frame (banners) */
};
void Port_WideSetFrame(const unsigned char* oamTags, int cameraX, int left, int right);

void Port_Log(const char* fmt, ...);

#endif /* PLATFORM_PC */

#endif /* PORT_PLATFORM_H */
