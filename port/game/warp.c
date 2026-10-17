// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — test warp: starts a fresh game in any room, with every item.
//
// Used to test areas without playing up to them. The port layer parses the
// target list (MZM_WARP, see port/src/main.c); this file, which sees the game
// headers, applies it. Each target replaces the current game: the save data
// is reset (no events, no items collected) and the room is loaded from the
// first door it has, like the debug boot menu of the original game does.
#include "types.h"
#include "globals.h"
#include "macros.h"
#include "save_file.h"

#include "data/clipdata_data.h"
#include "data/save_file_data.h"

#include "constants/connection.h"
#include "constants/cutscene.h"
#include "constants/game_state.h"
#include "constants/samus.h"

#include "structs/clipdata.h"
#include "structs/connection.h"
#include "structs/cutscene.h"
#include "structs/game_state.h"
#include "structs/samus.h"
#include "structs/save_file.h"

extern const struct Door* sAreaDoorsPointers[AREA_ENTRY_COUNT];

static s32 sTarget;   // index in the port's target list
static s32 sRoom;     // current room of a whole-area target
static u32 sTimer;    // frames left before the next target
static boolu8 sEquip; // give the items once Samus is initialized

static s32 HighestRoom(u8 area)
{
    const struct Door* pDoor;
    s32 highest = -1;

    for (pDoor = sAreaDoorsPointers[area]; pDoor->type != 0; pDoor++) {
        if (pDoor->sourceRoom > highest)
            highest = pDoor->sourceRoom;
    }
    return highest;
}

// First door of the room, skipping elevator doors (Samus would ride them).
static s32 FindDoor(u8 area, u8 room)
{
    const struct Door* pDoor;
    s32 door;
    s32 found = -1;
    s32 elevator;

    for (pDoor = sAreaDoorsPointers[area], door = 0; pDoor->type != 0; pDoor++, door++) {
        if (pDoor->sourceRoom != room)
            continue;
        if (found < 0)
            found = door;

        for (elevator = 1; elevator < ELEVATOR_ROUTE_COUNT; elevator++) {
            if ((sElevatorRoomPairs[elevator].area1 == area && sElevatorRoomPairs[elevator].room1 == room) ||
                (sElevatorRoomPairs[elevator].area2 == area && sElevatorRoomPairs[elevator].room2 == room))
                break;
        }
        if (elevator == ELEVATOR_ROUTE_COUNT || (pDoor->type & 0xF) >= DOOR_TYPE_OPEN_HATCH)
            return door;
    }
    return found;
}

static boolu32 Warp(u8 area, u8 room)
{
    s32 door = FindDoor(area, room);
    u32 i;

    if (door < 0)
        return FALSE;

    Port_Log("warp: area %d room %d (door %d)", area, room, door);
    unk_7584c(1);
    // SamusInit clears gSamusData with a 32-bit fill, which leaves its last
    // two bytes (the animation frame) from the previous room.
    for (i = 0; i < sizeof(gSamusData); i++)
        ((u8*)&gSamusData)[i] = 0;
    gSectionInfo.sectionIndex = area;
    gSectionInfo.starIndex = area;
    gCurrentArea = area;
    gAreaBeforeTransition = area;
    gCurrentRoom = room;
    gLastDoorUsed = door;
    gMainGameMode = GM_INGAME;
    gSubGameMode1 = 0;
    gSubGameMode2 = 0;
    sEquip = TRUE;
    return TRUE;
}

// Game modes that aren't a room (PORT_WARP_* codes, see port/src/warp.c).
static boolu32 WarpMode(s32 mode, s32 number)
{
    switch (mode) {
    case PORT_WARP_CUTSCENE:
        if (number <= CUTSCENE_NONE || number >= CUTSCENE_COUNT)
            return FALSE;
        Warp(AREA_BRINSTAR, 0); // where the game goes on afterwards
        Port_Log("warp: cutscene %d", number);
        gCurrentCutscene = number;
        gMainGameMode = GM_CUTSCENE;
        return TRUE;

    case PORT_WARP_TOURIAN_ESCAPE:
        Warp(AREA_TOURIAN, 0);
        Port_Log("warp: Tourian escape");
        gTourianEscapeCutsceneStage = 1;
        gMainGameMode = GM_TOURIAN_ESCAPE;
        gSubGameMode2 = GM_INGAME;
        return TRUE;

    case PORT_WARP_CHOZODIA_ESCAPE:
        Warp(AREA_CHOZODIA, 0);
        Port_Log("warp: Chozodia escape");
        gMainGameMode = GM_CHOZODIA_ESCAPE;
        return TRUE;

    case PORT_WARP_CREDITS:
        Warp(AREA_BRINSTAR, 0);
        Port_Log("warp: credits");
        gMainGameMode = GM_CREDITS;
        return TRUE;
    }
    return FALSE;
}

// Moves to the next room of the list. Returns FALSE once it is exhausted.
static boolu32 NextTarget(void)
{
    s32 area;
    s32 room;

    while (Port_WarpTarget(sTarget, &area, &room)) {
        if (area >= PORT_WARP_CUTSCENE) {
            // A mode; "cutscene:*" plays them all.
            if (area == PORT_WARP_CUTSCENE && room < 0) {
                if (sRoom == 0)
                    sRoom = CUTSCENE_NONE + 1;
                if (sRoom < CUTSCENE_COUNT && WarpMode(area, sRoom++))
                    return TRUE;
                sRoom = 0;
                sTarget++;
                continue;
            }
            sTarget++;
            if (WarpMode(area, room))
                return TRUE;
            continue;
        }
        if (area < 0 || area >= AREA_NORMAL_COUNT) {
            sTarget++;
            continue;
        }
        if (room >= 0) {
            sTarget++;
            if (Warp(area, room))
                return TRUE;
            continue;
        }
        // Whole area: one room after the other.
        while (sRoom <= HighestRoom(area)) {
            if (Warp(area, sRoom++))
                return TRUE;
        }
        sRoom = 0;
        sTarget++;
    }
    return FALSE;
}

void Port_WarpUpdate(void)
{
    static boolu8 sDone;

    if (sDone)
        return;
    if (sTimer != 0) {
        sTimer--;
        return;
    }
    if (!NextTarget()) {
        // Only the first call can find an empty list (no warp requested).
        sDone = TRUE;
        return;
    }
    sTimer = Port_WarpInterval();
    if (sTimer == 0)
        sDone = TRUE; // single target: stay there
}

void Port_WarpEquip(void)
{
    if (!sEquip)
        return;
    sEquip = FALSE;

    gSamusData.pose = SPOSE_STANDING;
    gEquipment.maxEnergy = gEquipment.currentEnergy = 1299;
    gEquipment.maxMissiles = gEquipment.currentMissiles = 250;
    gEquipment.maxSuperMissiles = gEquipment.currentSuperMissiles = 30;
    gEquipment.maxPowerBombs = gEquipment.currentPowerBombs = 9;
    gEquipment.beamBombs = gEquipment.beamBombsActivation =
        BBF_LONG_BEAM | BBF_ICE_BEAM | BBF_WAVE_BEAM | BBF_PLASMA_BEAM | BBF_CHARGE_BEAM | BBF_BOMBS;
    gEquipment.suitMisc = gEquipment.suitMiscActivation = SMF_HIGH_JUMP | SMF_SPEEDBOOSTER | SMF_SPACE_JUMP |
                                                          SMF_SCREW_ATTACK | SMF_VARIA_SUIT | SMF_GRAVITY_SUIT |
                                                          SMF_MORPH_BALL | SMF_POWER_GRIP;
    gEquipment.suitType = SUIT_FULLY_POWERED;
    gEquipment.downloadedMapStatus = 0xFF;
}
