#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#ifdef PLATFORM_PC
#include "port/platform.h"
#define PORT_REGION(buf) ((void *)(buf))
#endif

#ifdef PLATFORM_PC
#define EWRAM_BASE PORT_REGION(gPortEwram)
#else
#define EWRAM_BASE (void *)0x02000000
#endif
#define EWRAM_SIZE (256 * 0x400)

#ifdef PLATFORM_PC
#define IWRAM_BASE PORT_REGION(gPortIwram)
#else
#define IWRAM_BASE (void *)0x03000000
#endif
#define IWRAM_SIZE (32 * 0x400)

#ifdef PLATFORM_PC
#define REG_BASE PORT_REGION(gPortIo)
#else
#define REG_BASE (void *)0x04000000
#endif

#ifdef PLATFORM_PC
#define PALRAM_BASE PORT_REGION(gPortPalram)
#else
#define PALRAM_BASE (void *)0x05000000
#endif
#ifdef PLATFORM_PC
#define PALRAM_OBJ PORT_REGION((gPortPalram + 0x200))
#else
#define PALRAM_OBJ (void *)0x05000200
#endif
#define PAL_ROW (16)
#define PAL_ROW_SIZE (PAL_ROW * 2)
#define COLORS_IN_PAL (16 * PAL_ROW)
#define PAL_SIZE (COLORS_IN_PAL * 2)
#define PALRAM_SIZE (PAL_SIZE * 2)

#ifdef PLATFORM_PC
#define VRAM_BASE PORT_REGION(gPortVram)
#else
#define VRAM_BASE (void *)0x06000000
#endif
#ifdef PLATFORM_PC
#define VRAM_OBJ PORT_REGION((gPortVram + 0x10000))
#else
#define VRAM_OBJ (void *)0x06010000
#endif
#define VRAM_SIZE (96 * 0x400)
#define VRAM_BG_SIZE (0x10000)
#define VRAM_OBJ_SIZE (0x8000)

#ifdef PLATFORM_PC
#define OAM_BASE PORT_REGION(gPortOam)
#else
#define OAM_BASE (void *)0x07000000
#endif
#define OAM_SIZE 0x400

#ifdef PLATFORM_PC
#define ROM_BASE PORT_REGION(gPortRomStub)
#else
#define ROM_BASE (void *)0x08000000
#endif
#define ROM_SIZE (32 * 0x100000)

#ifdef PLATFORM_PC
#define SRAM_BASE PORT_REGION(gPortSram)
#else
#define SRAM_BASE (void *)0x0e000000
#endif
#define SRAM_SIZE (32 * 0x400)

#endif /* GBA_MEMORY_H */
