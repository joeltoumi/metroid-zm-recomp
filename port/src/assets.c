// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — runtime game data, copied out of the player's own ROM.
//
// The build contains no game data: every array the decomp would fill from the
// ROM at compile time is zero-filled and registered by the generated wrappers
// (tools/port/gen_assets.py). At startup Port_LoadRom() checks the ROM is the
// exact dump this build expects (SHA-1) and copies each block into place.
#include "port.h"
#include "port_assets.h"

#include "../../include/port/assets_glue.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef MZM_ROM_SHA1
#error "MZM_ROM_SHA1 must be defined by the build (expected SHA-1 of the ROM)"
#endif

typedef struct AssetChunk {
    PortAsset* assets;
    unsigned long count;
} AssetChunk;

static AssetChunk* sChunks;
static size_t sChunkCount;
static size_t sChunkCap;

void Port_RegisterAssets(PortAsset* assets, unsigned long count)
{
    if (sChunkCount == sChunkCap) {
        size_t cap = sChunkCap ? sChunkCap * 2 : 512;
        AssetChunk* grown = realloc(sChunks, cap * sizeof(*grown));
        if (!grown)
            abort();
        sChunks = grown;
        sChunkCap = cap;
    }
    sChunks[sChunkCount].assets = assets;
    sChunks[sChunkCount].count = count;
    sChunkCount++;
}

const char* Port_RomRegionName(void)
{
    return MZM_ROM_REGION_NAME;
}

const char* Port_RomExpectedSha1(void)
{
    return MZM_ROM_SHA1;
}

PortRomStatus Port_CheckRom(const uint8_t* rom, size_t size)
{
    char hex[41];
    uint8_t digest[20];

    if (size < 0x200 || size > 32 * 1024 * 1024)
        return PORT_ROM_BAD_SIZE;
    // Development/CI only: boot with any image (e.g. a zero-filled dummy) to
    // smoke-test the engine without game data.
    if (getenv("MZM_DEV_SKIP_ROM_CHECK") != NULL) {
        Port_Log("MZM_DEV_SKIP_ROM_CHECK set: not verifying the ROM");
        return PORT_ROM_OK;
    }
    // Header: game title at 0xA0, game code at 0xAC ("BMXE" for the US version).
    if (memcmp(rom + 0xA0, "ZEROMISSION", 11) != 0)
        return PORT_ROM_NOT_MZM;

    Port_Sha1(rom, size, digest);
    for (int i = 0; i < 20; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (strcmp(hex, MZM_ROM_SHA1) != 0) {
        Port_Log("ROM SHA-1 %s, expected %s (%s)", hex, MZM_ROM_SHA1, MZM_ROM_REGION_NAME);
        return PORT_ROM_WRONG_VERSION;
    }
    return PORT_ROM_OK;
}

PortRomStatus Port_LoadRom(const uint8_t* rom, size_t size)
{
    size_t total = 0;
    unsigned long assets = 0;
    PortRomStatus status = Port_CheckRom(rom, size);

    if (status != PORT_ROM_OK)
        return status;

    for (size_t c = 0; c < sChunkCount; c++) {
        for (unsigned long i = 0; i < sChunks[c].count; i++) {
            const PortAsset* a = &sChunks[c].assets[i];
            if ((size_t)a->romOffset + a->size > size)
                Port_Fatal("asset at 0x%X (+0x%X) is outside the ROM", a->romOffset, a->size);
            memcpy(a->dst, rom + a->romOffset, a->size);
            total += a->size;
            assets++;
        }
    }
    Port_Log("ROM %s OK: %lu assets, %zu KiB copied", MZM_ROM_REGION_NAME, assets, total / 1024);
    return PORT_ROM_OK;
}

const char* Port_RomStatusText(PortRomStatus status)
{
    switch (status) {
    case PORT_ROM_OK:
        return "OK";
    case PORT_ROM_MISSING:
        return "No ROM selected";
    case PORT_ROM_BAD_SIZE:
        return "The file is not a GBA ROM";
    case PORT_ROM_NOT_MZM:
        return "The file is not Metroid: Zero Mission";
    case PORT_ROM_WRONG_VERSION:
        return "Wrong version of Metroid: Zero Mission (this build needs the " MZM_ROM_REGION_NAME " ROM)";
    }
    return "?";
}
