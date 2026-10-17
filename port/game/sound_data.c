// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — sSoundDataEntries (sound/sound_entries.s on the GBA).
//
// The table holds pointers to the track headers in the ROM, so it can't be
// copied out of the ROM as is: Port_SoundInit (port/game/audio_engine.c)
// fills it with host pointers once the ROM is loaded. This file is compiled
// with const= (like the generated data wrappers) so the table is writable.
#include "types.h"
#include "data/audio.h"

const struct SoundEntry sSoundDataEntries[SOUND_COUNT] = { { 0 } };

// The rest of the game sees the table as const (data/audio.h), and the
// compiler may drop stores it can see going to a const object: the filling
// code writes through this pointer instead.
struct SoundEntry* Port_SoundEntriesForWriting(void)
{
    return (struct SoundEntry*)sSoundDataEntries;
}
