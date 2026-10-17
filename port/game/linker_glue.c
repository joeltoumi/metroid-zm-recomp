// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — game-side definitions for symbols that the GBA build gets
// from linker.ld or from hand-written assembly. Compiled with the game code
// (game headers, -nostdinc), not with the port layer.
#include "types.h"
#include "globals.h"
#include "data/audio.h"
#include "data/cable_link_data.h"

// linker.ld places these at the very end of IWRAM (0x03007FF8/FC), next to
// the BIOS IRQ vector. They are ordinary variables natively.
u16 gInterruptCheckFlag;
u32 (*gIntrCodePointer)[0x80];
Func_T gIntrVector[13];

// End marker of the data block sent over the link cable (Fusion link is not
// supported natively; this only needs to exist).
const u8 sTransferRom_After[1];

// TEMPORARY: sound/sound_entries.s is assembler data (pointers to the track
// headers in sound/tracks/*.s). Until those are converted to C (milestone M3)
// every entry points to an empty header (0 tracks, priority 0): the queueing
// code reads the header, the stubbed sound engine never plays it.
static const u8 sPortSilentTrackHeader[16];

const struct SoundEntry sSoundDataEntries[SOUND_COUNT] = {
    [0 ... SOUND_COUNT - 1] = { .pHeader = sPortSilentTrackHeader },
};
