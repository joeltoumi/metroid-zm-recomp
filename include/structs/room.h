#ifndef ROOM_STRUCT_H
#define ROOM_STRUCT_H

#include "types.h"

#include "constants/animated_graphics.h"
#include "constants/audio.h"
#include "constants/event.h"
#include "constants/room.h"

#define ROOM_SPRITE_DATA_TERMINATOR UCHAR_MAX, UCHAR_MAX, UCHAR_MAX

struct AnimatedGraphicsEntry {
    AnimatedPaletteId palette;
    u8 tileset;
};

extern struct AnimatedGraphicsEntry gAnimatedGraphicsEntry;

struct TilesetTransparentColor {
    u16 transparentColor;
    u16 field_2;
};

extern struct TilesetTransparentColor gTilesetTransparentColor;

// ---

struct RoomEntry {
    u8 tileset;
    u8 bg0Prop;
    u8 bg1Prop;
    u8 bg2Prop;
    u8 bg3Prop;
    RoomScrollFlags scrollsFlag;
    u8 bg3Scrolling;
    u8 transparency;
    const u8* pEnemyRoomData;
    Event firstSpritesetEvent;
    Event secondSpritesetEvent;
    u8 mapX;
    u8 mapY;
    u8 visualEffect;
    u16 effectY;
    RoomEffect damageEffect;
    Sound musicTrack;
    u8 bg0Size;
    u8 bg3Size;
    u8 bg3FromBottomFlag;
    u8 animatedTileset;
    u8 animatedPalette;
};

struct RoomEntryRom {
    u8 tileset;
    u8 bg0Prop;
    u8 bg1Prop;
    u8 bg2Prop;
    u8 bg3Prop;
    const u8* pBg0Data;
    const u8* pBg1Data;
    const u8* pBg2Data;
    const u8* pClipData;
    const u8* pBg3Data;
    u8 bg3Scrolling;
    u8 transparency;
    const u8* pDefaultSpriteData;
    u8 defaultSpriteset;
    u8 firstSpritesetEvent;
    const u8* pFirstSpriteData;
    u8 firstSpriteset;
    u8 secondSpritesetEvent;
    const u8* pSecondSpriteData;
    u8 secondSpriteset;
    u8 mapX;
    u8 mapY;
    RoomEffect effect;
    u8 effectY;
    Sound musicTrack;
};

struct TilesetEntry {
    const u32* pTileGraphics;
    const u16* pPalette;
    const u32* pBackgroundGraphics;
    const u8* pTilemap;
    u8 animatedTileset;
    u8 animatedPalette;
};

struct BackgroundPositions {
    struct Coordinates bg[4];
    struct Coordinates doorTransition;
};

struct WaterMovement {
    u8 moving;
    u8 stage;
    s8 loopCounter;
    s8 yOffset;
};

struct BG0Movement {
    u8 type;
    u8 counter;
    u16 unused;
    u16 xOffset;
    u16 yOffset;
};

struct BG3Movement {
    u8 active;
    u8 counter;
    u8 undefined; // Needed for correct alignement
    u16 xOffset;
};

struct Unused_3005504 {
    u32 field_0;
    u32 field_4;
};

#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gClipdataCollisionTypes (*(u8 (*)[1024])(EWRAM_BASE + 0x2000))
#else
extern u8 gClipdataCollisionTypes[1024];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gClipdataCollisionTypes_Tilemap (*(u8 (*)[512])(EWRAM_BASE + 0x2400))
#else
extern u8 gClipdataCollisionTypes_Tilemap[512];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gClipdataBehaviorTypes (*(u16 (*)[1024])(EWRAM_BASE + 0x3000))
#else
extern u16 gClipdataBehaviorTypes[1024];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gClipdataBehaviorTypes_Tilemap (*(u16 (*)[512])(EWRAM_BASE + 0x3800))
#else
extern u16 gClipdataBehaviorTypes_Tilemap[512];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gTilemap (*(u16 (*)[1024])(EWRAM_BASE + 0x4000))
#else
extern u16 gTilemap[1024];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gCommonTilemap (*(u16 (*)[1024])(EWRAM_BASE + 0x6000))
#else
extern u16 gCommonTilemap[1024];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gDecompClipdataMap (*(u16 (*)[1024])(EWRAM_BASE + 0x27800))
#else
extern u16 gDecompClipdataMap[1024];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gDecompBg0Map (*(u16 (*)[6144])(EWRAM_BASE + 0x2a800))
#else
extern u16 gDecompBg0Map[6144];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gDecompBg1Map (*(u16 (*)[6144])(EWRAM_BASE + 0x2d800))
#else
extern u16 gDecompBg1Map[6144];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gDecompBg2Map (*(u16 (*)[6144])(EWRAM_BASE + 0x30800))
#else
extern u16 gDecompBg2Map[6144];
#endif
#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gDecompBg3Map (*(u16 (*)[2048])(EWRAM_BASE + 0x7000))
#else
extern u16 gDecompBg3Map[2048];
#endif
extern u8 gSpriteset;
extern u8 gSpritesetEntryUsed;
extern struct Coordinates gWaitingSpacePiratesPosition;
extern struct BG3Movement gBg3Movement;
extern struct BG0Movement gBg0Movement;
extern u16 gRainSoundEffect;
extern struct WaterMovement gWaterMovement;
extern u8 gScrollCounter;
extern struct RoomEntry gCurrentRoomEntry;
extern struct BackgroundPositions gBackgroundPositions;

extern struct Unused_3005504 gUnusedStruct_3005504;

extern u8 gDestinationRoom;

#endif /* ROOM_STRUCT_H */
