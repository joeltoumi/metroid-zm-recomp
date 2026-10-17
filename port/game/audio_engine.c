// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — the parts of MZM's sound engine that are hand-written ARM
// assembly upstream (asm/audio_internal.s, asm/soundcode.s), in C.
//
// Most of the engine is C already (src/audio.c, src/audio_wrappers.c,
// src/music_wrappers.c): it sequences the track scripts, runs the envelopes
// and mixes the DirectSound channels into gMusicInfo.soundRawData, which the
// GBA streams to the FIFOs with DMA 1/2. The port's APU (port/src/apu.c)
// plays that buffer and emulates the PSG channels from the I/O registers.
//
// The sound data itself (track scripts, voicegroups, samples) is assembler
// data with 32-bit pointers between the pieces. The native build uses the
// player's ROM in place (Port_RomPointer) and only rebuilds what has a
// different layout on a 64-bit host:
//   * sSoundDataEntries: pointers to the track headers;
//   * voicegroups: struct Voice is 12 bytes on the GBA, 24 natively.
// Pointers inside the scripts (track list in the header, GOTO/PATT targets)
// are translated where the engine reads them.
//
// Each translation keeps the assembly's exact arithmetic (including its
// quirks), so the music plays as it does on the console.
#include "types.h"
#include "macros.h"
#include "gba.h"
#include "syscalls.h"
#include "audio.h"
#include "audio_wrappers.h"
#include "audio/track_internal.h"
#include "data/audio.h"
#include "structs/audio.h"

#define ROM_ADDRESS 0x08000000u
#define VOICEGROUP_SIZE 128
#define VOICE_SIZE_GBA 12
#define MAX_VOICEGROUPS 256
#define SOUND_ENTRY_SIZE_GBA 8
#define FINE 0xB1 // track script command: end of track

// ---------------------------------------------------------------------------
// ROM access
// ---------------------------------------------------------------------------

static u32 Read32(const u8* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}

static const u8* RomPointer(u32 address)
{
    return Port_RomPointer(address);
}

// Stand-ins for data a corrupt pointer would reference: a script that ends,
// a sample that plays nothing (header + a few zero samples), a silent wave.
static const u8 sSilentScript[4] = { FINE, FINE, FINE, FINE };
static const u8 sSilentHeader[8]; // 0 tracks
static const u32 sSilentSample[8];
static const u32 sSilentWave[4];

static const u8* ScriptPointer(u32 address)
{
    const u8* p = RomPointer(address);
    return p != NULL ? p : sSilentScript;
}

// ---------------------------------------------------------------------------
// Sound tables
// ---------------------------------------------------------------------------

struct SoundEntry* Port_SoundEntriesForWriting(void); // port/game/sound_data.c

// The cache's order is part of the game state (tracks point into it); save
// states keep the addresses and rebuild the copies (Port_SoundRebuildVoicegroups).
static IWRAM_DATA u32 sVoicegroupAddress[MAX_VOICEGROUPS];
static struct Voice sVoicegroups[MAX_VOICEGROUPS][VOICEGROUP_SIZE];
static IWRAM_DATA u32 sVoicegroupCount;
static struct Voice sSilentVoicegroup[VOICEGROUP_SIZE];

static void SetupSilentVoicegroup(void)
{
    u32 i;

    for (i = 0; i < VOICEGROUP_SIZE; i++)
        sSilentVoicegroup[i].pSample = (u32*)sSilentSample;
}

/**
 * @brief Host copy of a voicegroup
 *
 * The GBA indexes voicegroups freely past their end (into the next one), so
 * each copy has the full 128 entries. Keysplit/drum entries (0x40/0x80) point
 * to another voicegroup, translated the same way.
 */
struct Voice* Port_SoundVoicegroup(u32 address)
{
    struct Voice* pGroup;
    struct Voice* pVoice;
    const u8* raw;
    u32 i;
    u32 pointer;
    u8 type;

    for (i = 0; i < sVoicegroupCount; i++)
    {
        if (sVoicegroupAddress[i] == address)
            return sVoicegroups[i];
    }

    if (RomPointer(address) == NULL)
        return sSilentVoicegroup;

    if (sVoicegroupCount == MAX_VOICEGROUPS)
    {
        Port_Log("sound: too many voicegroups (0x%08X)", address);
        return sSilentVoicegroup;
    }

    // Registered before the entries are converted, so a group that refers to
    // itself (directly or not) finds itself in the cache.
    pGroup = sVoicegroups[sVoicegroupCount];
    sVoicegroupAddress[sVoicegroupCount] = address;
    sVoicegroupCount++;

    for (i = 0; i < VOICEGROUP_SIZE; i++)
    {
        pVoice = &pGroup[i];
        raw = RomPointer(address + i * VOICE_SIZE_GBA);
        if (raw == NULL || RomPointer(address + i * VOICE_SIZE_GBA + VOICE_SIZE_GBA - 1) == NULL)
        {
            *pVoice = sSilentVoicegroup[i];
            continue;
        }

        type = raw[0];
        pointer = Read32(raw + 4);
        pVoice->instrumentType = type;
        pVoice->pitch = raw[1];
        pVoice->unk_2 = raw[2];
        pVoice->unk_3 = raw[3];
        pVoice->envelope.attack = raw[8];
        pVoice->envelope.decay = raw[9];
        pVoice->envelope.sustain = raw[10];
        pVoice->envelope.release = raw[11];

        if (type == 0x40 || type == 0x80)
        {
            // Keysplit (the envelope bytes hold the keysplit table address,
            // read by unk_4e10) or "every key" drum kit.
            pVoice->pSample = (u32*)Port_SoundVoicegroup(pointer);
        }
        else if ((type & 7) == 0)
        {
            // DirectSound sample.
            pVoice->pSample = (u32*)RomPointer(pointer);
            if (pVoice->pSample == NULL)
                pVoice->pSample = (u32*)sSilentSample;
        }
        else if ((type & 7) == 3)
        {
            // Programmable wave: 16 bytes of 4-bit samples.
            pVoice->pSample = (u32*)RomPointer(pointer);
            if (pVoice->pSample == NULL)
                pVoice->pSample = (u32*)sSilentWave;
        }
        else
        {
            // Square/noise: the field is a small number (duty cycle, period).
            pVoice->pSample = (u32*)(port_uptr)pointer;
        }
    }

    return pGroup;
}

static boolu32 IsSoundEntry(const u8* rom, u32 size, u32 offset)
{
    const u8* pEntry = rom + offset;
    u32 header = Read32(pEntry) - ROM_ADDRESS;

    if (header >= size - 8 || pEntry[5] != 0 || pEntry[4] > 8 || pEntry[7] != 0 || pEntry[6] > 8)
        return FALSE;

    // Header: track count, unused, priority, flags, voicegroup, tracks...
    if (rom[header] > 16)
        return FALSE;
    if (rom[header] != 0 && Read32(rom + header + 4) - ROM_ADDRESS >= size)
        return FALSE;

    return TRUE;
}

/**
 * @brief Finds sSoundDataEntries in the ROM (its address differs per region)
 *
 * It is the only run of SOUND_COUNT consecutive {header pointer, player,
 * player} entries.
 */
static s32 FindSoundEntries(const u8* rom, u32 size)
{
    u32 offset;
    u32 i;

    if (size < SOUND_COUNT * SOUND_ENTRY_SIZE_GBA)
        return -1;

    for (offset = 0; offset <= size - SOUND_COUNT * SOUND_ENTRY_SIZE_GBA; offset += 4)
    {
        for (i = 0; i < SOUND_COUNT; i++)
        {
            if (!IsSoundEntry(rom, size, offset + i * SOUND_ENTRY_SIZE_GBA))
                break;
        }

        if (i == SOUND_COUNT)
            return offset;
    }

    return -1;
}

void Port_SoundRebuildVoicegroups(void)
{
    u32 addresses[MAX_VOICEGROUPS];
    u32 count;
    u32 i;

    // Same calls in the same order fill the same slots (nested groups
    // included), so pointers into the cache stay valid.
    count = sVoicegroupCount;
    for (i = 0; i < count; i++)
        addresses[i] = sVoicegroupAddress[i];
    sVoicegroupCount = 0;
    for (i = 0; i < count; i++)
        Port_SoundVoicegroup(addresses[i]);
}

void Port_SoundInit(const u8* rom, u32 size)
{
    struct SoundEntry* pEntries = Port_SoundEntriesForWriting();
    const u8* pEntry;
    s32 offset;
    u32 i;

    SetupSilentVoicegroup();
    sVoicegroupCount = 0;

    offset = FindSoundEntries(rom, size);
    if (offset < 0)
    {
        Port_Log("sound: track table not found in the ROM, the game will be silent");
        for (i = 0; i < SOUND_COUNT; i++)
        {
            pEntries[i].pHeader = sSilentHeader;
            pEntries[i].trackGroundNumber = 0;
            pEntries[i].trackGroundNumber_2 = 0;
        }
        return;
    }

    for (i = 0; i < SOUND_COUNT; i++)
    {
        pEntry = rom + offset + i * SOUND_ENTRY_SIZE_GBA;
        pEntries[i].pHeader = RomPointer(Read32(pEntry));
        pEntries[i].trackGroundNumber = pEntry[4] | pEntry[5] << 8;
        pEntries[i].trackGroundNumber_2 = pEntry[6] | pEntry[7] << 8;

        // Convert the voicegroups up front (InitTrack then only looks them up).
        if (pEntries[i].pHeader[0] != 0)
            Port_SoundVoicegroup(Read32(pEntries[i].pHeader + 4));
    }

    Port_Log("sound: %d sounds (table at 0x%08X), %u voicegroups", SOUND_COUNT, ROM_ADDRESS + offset,
        sVoicegroupCount);
}

int Port_SoundFifoBlock(const s8** pFifoA, const s8** pFifoB)
{
    u32 block;

    if (gMusicInfo.unk_E == 0)
        return FALSE;

    block = gMusicInfo.unk_10;
    if (block >= gMusicInfo.unk_E)
        block = 0;

    *pFifoA = (const s8*)&gMusicInfo.soundRawData[block * 16];
    *pFifoB = (const s8*)&gMusicInfo.soundRawData[PCM_DMA_BUF_SIZE + block * 16];
    return TRUE;
}

// ---------------------------------------------------------------------------
// asm/audio_internal.s
// ---------------------------------------------------------------------------

/**
 * @brief 4b50 | cc | Starts playing a track header on a music player
 *
 * @param pTrack Track data pointer
 * @param pHeader Track header
 * @return u8 1 if a sound with a higher priority is already playing
 */
u8 InitTrack(struct TrackData* pTrack, const u32* pHeader)
{
    const u8* header;
    struct TrackVariables* pVariables;
    u32 word;
    u32 i;
    u8 count;

    if (pTrack->occupied)
        return 0;

    pTrack->occupied = TRUE;

    if (!(pTrack->unk_1E & 1))
    {
        header = (const u8*)pHeader;
        word = Read32(header);

        if ((word & 0xFF) == 0)
        {
            ResetTrack(pTrack);
            pTrack->flags = 0;
        }
        else
        {
            if (pTrack->flags & 2)
            {
                if (pTrack->unk_1D != 0 && pTrack->unk_3 > ((word >> 16) & 0xFF))
                {
                    pTrack->occupied = FALSE;
                    return 1;
                }

                ResetTrack(pTrack);
            }

            pTrack->flags = 2;
            pTrack->amountOfTracks = word;
            pTrack->undefined_2 = word >> 8;
            pTrack->unk_3 = word >> 16;
            pTrack->pVoice = Port_SoundVoicegroup(Read32(header + 4));
            pTrack->pHeader = header;
            pTrack->unk_C = 0x100;

            if (word & 0x80000000)
                DoSoundAction(word >> 24);

            count = word;
            pVariables = pTrack->pVariables;
            BitFill(3, 0, pVariables, sizeof(*pVariables) * count, 16);

            for (i = 0; i < count; i++, pVariables++)
            {
                pVariables->unk_0 = 1;
                pVariables->bendRange = 2;
                pVariables->unk_1B = 0xC;
                pVariables->volume = 0;
                pVariables->unk_5 = 0x40;
                pVariables->unk_6 = 0x40;
                pVariables->unk_7 = 0;
                pVariables->pRawData = ScriptPointer(Read32(header + 8 + i * 4));
            }
        }
    }

    pTrack->occupied = FALSE;
    return 0;
}

/**
 * @brief Stops every channel and PSG sound of a track (common part of
 * StopMusicOrSound and ResetTrack)
 *
 * @param pTrack Track data pointer
 */
static void StopTrackChannels(struct TrackData* pTrack)
{
    struct TrackVariables* pVariables;
    struct SoundChannel* pChannel;
    struct SoundChannel* pNext;
    struct PsgSoundData* pSound;
    u32 i;

    pTrack->flags = 1;
    pTrack->currentTrack = 0;

    for (i = 0, pVariables = pTrack->pVariables; i < pTrack->amountOfTracks; i++, pVariables++)
    {
        pSound = pVariables->pSoundPsg;
        if (pSound != NULL)
        {
            ClearRegistersForPsg(pSound, (pVariables->channel & 7) - 1);
            pSound->unk_0 = 0;
            pSound->pVariables = NULL;
        }

        for (pChannel = pVariables->pChannel; pChannel != NULL; pChannel = pNext)
        {
            pNext = pChannel->pChannel2;
            pChannel->unk_0 = 0;
            pChannel->pVariables = NULL;
            pChannel->pChannel2 = NULL;
            pChannel->pChannel1 = NULL;
        }

        pVariables->pSoundPsg = NULL;
        pVariables->pChannel = NULL;
    }
}

/**
 * @brief 4c1c | 78 | Stops a music player
 *
 * @param pTrack Track data pointer
 */
void StopMusicOrSound(struct TrackData* pTrack)
{
    if (pTrack->occupied)
        return;

    pTrack->occupied = TRUE;

    if (!(pTrack->unk_1E & 1) && pTrack->flags & 2)
        StopTrackChannels(pTrack);

    pTrack->occupied = FALSE;
}

/**
 * @brief 4c94 | 68 | Stops a music player (no occupied check)
 *
 * @param pTrack Track data pointer
 */
void ResetTrack(struct TrackData* pTrack)
{
    if (!(pTrack->unk_1E & 1) && pTrack->flags & 2)
        StopTrackChannels(pTrack);
}

/**
 * @brief 4cfc | 20 | Advances the tempo counter
 *
 * @param pTrack Track data pointer
 * @return u32 Number of ticks to process this frame
 */
u32 unk_4cfc(struct TrackData* pTrack)
{
    u32 sum;
    u32 ticks;

    sum = (pTrack->unk_E + pTrack->unk_C) & 0xFFFF;
    ticks = 0;
    if (sum & 0xFF00)
    {
        ticks = sum >> 8;
        sum &= 0xFF;
    }

    pTrack->unk_E = sum;
    return ticks;
}

/**
 * @brief 4d1c | 2c | Processes the audio command TEMPO
 *
 * @param pTrack Track data pointer
 * @param pVariables Track variables pointer
 */
void unk_4d1c(struct TrackData* pTrack, struct TrackVariables* pVariables)
{
    u32 tempo;

    tempo = pVariables->pRawData[1] * 2;
    pVariables->pRawData += 2;

    pTrack->unk_A = tempo;
    if (tempo == 150)
        pTrack->unk_C = 0x100;
    else
        pTrack->unk_C = (0x1B4E * (tempo << 8)) >> 20;
    pTrack->unk_E = 0;
}

static void UpdatePlayer(u32 player)
{
    struct TrackData* pTrack;

    pTrack = sMusicTrackDataRom[player].pTrack;
    if (pTrack != NULL && pTrack->flags & 2)
        UpdateTrack(pTrack);
}

/**
 * @brief 4d48 | c8 | Once per frame: starts queued sounds, updates the music
 * players, the PSG sounds and the mixer
 *
 */
void UpdateAudio(void)
{
    struct TrackData* pTrack;
    u32 i;
    u8 priority;

    for (i = 0; i < ARRAY_SIZE(gSoundQueue); i++)
    {
        if (!gSoundQueue[i].exists)
            continue;

        InitTrack(sMusicTrackDataRom[i].pTrack, (const u32*)gSoundQueue[i].pHeader);
        gSoundQueue[i].exists = FALSE;
        gSoundQueue[i].priority = 0;
        gSoundQueue[i].fadingTimer = 0;
        gSoundQueue[i].pHeader = NULL;
    }

    priority = gMusicInfo.priority;
    if (priority & 0x80)
    {
        if ((priority & 0x7F) != 4)
        {
            for (i = 1; i < ARRAY_SIZE(gSoundQueue); i++)
                UpdatePlayer(i);
        }
    }
    else if (priority == 4)
    {
        pTrack = sMusicTrackDataRom[gMusicInfo.unk_20 ? 1 : 0].pTrack;
        if (pTrack->flags & 2)
            UpdateTrack(pTrack);
    }
    else
    {
        for (i = 0; i < ARRAY_SIZE(gSoundQueue); i++)
            UpdatePlayer(i);
    }

    if (!gMusicInfo.occupied)
    {
        gMusicInfo.occupied = TRUE;
        UpdatePsgSounds();
        UpdateMusic();
    }

    gMusicInfo.occupied = FALSE;
}

/**
 * @brief 4e10 | a4 | Picks the voice of a note in a keysplit/drum voice
 *
 * @param pVariables Track variables pointer
 */
void unk_4e10(struct TrackVariables* pVariables)
{
    struct Voice* pGroup;
    struct Voice* pVoice;
    const u8* pKeysplit;
    u32 keysplit;
    u32 channel;
    u8 key;
    u8 value;

    pGroup = (struct Voice*)pVariables->pSample2;
    if (pGroup == NULL)
        pGroup = sSilentVoicegroup;

    key = pVariables->unk_1;

    if ((pVariables->unk_0 & 0xF0) == 0x80)
    {
        // Drum kit: one voice per key
        pVoice = &pGroup[key];
        pVariables->unk_35 = pVoice->pitch;

        if (pVoice->unk_3 == 0)
            pVariables->unk_6 = 0x40;
        else if (pVoice->unk_3 >= 0x80)
            pVariables->unk_6 = pVoice->unk_3 - 0x80;
    }
    else
    {
        // Keysplit: the table (ROM address in envelope2) maps keys to voices
        keysplit = pVariables->envelope2.attack | pVariables->envelope2.decay << 8 |
            pVariables->envelope2.sustain << 16 | (u32)pVariables->envelope2.release << 24;
        pKeysplit = RomPointer(keysplit + key);
        pVoice = &pGroup[pKeysplit != NULL ? *pKeysplit : 0];
    }

    pVariables->channel = pVoice->instrumentType;
    pVariables->unk_36 = pVoice->unk_2;

    channel = pVoice->instrumentType & 7;
    if (channel == 0)
    {
        pVariables->pSample1 = pVoice->pSample;
    }
    else if (channel <= 2)
    {
        value = pVoice->unk_3;
        if (!(value & 0x80) && value & 0x70)
            pVariables->unk_37 = value;
        else
            pVariables->unk_37 = 8;

        // Only the low byte is written (the PSG code reads only that byte)
        *(u8*)&pVariables->pSample1 = (u32)(port_uptr)pVoice->pSample << 6;
    }
    else if (channel == 3)
    {
        UploadSampleToWaveRam(pVoice->pSample);
    }
    else if (channel == 4)
    {
        *(u8*)&pVariables->pSample1 = (u32)(port_uptr)pVoice->pSample << 3;
    }

    pVariables->envelope1 = pVoice->envelope;
}

/**
 * @brief 4eb4 | 5c | Computes the pitch of a track (key shift, bend, tune,
 * vibrato)
 *
 * @param pVariables Track variables pointer
 */
void unk_4eb4(struct TrackVariables* pVariables)
{
    s32 pitch;
    u32 word;

    pitch = pVariables->pitchBend;
    if (pitch > 0)
        pitch++;

    pitch = pitch * pVariables->bendRange * 4;

    // keyShift, unk_1D, tune, unk_1F read as one word
    word = pVariables->keyShift | pVariables->unk_1D << 8 | pVariables->tune << 16 |
        (u32)pVariables->unk_1F << 24;
    pitch += (s8)word * 256;
    pitch += (s8)(word >> 8) * 256;
    pitch += ((s32)(word << 8) >> 22) * 4;
    pitch += word >> 24;

    if (pVariables->lfoSpeed != 0 && pVariables->modulationDepth != 0 && pVariables->modulationType == 0)
        pitch += pVariables->unk_13 * 4 * 12;

    pVariables->unk_17 = pitch >> 8;
    pVariables->unk_18 = pitch;
}

/**
 * @brief 4f10 | 7c | Computes the left/right volume of a track
 *
 * @param pVariables Track variables pointer
 */
void unk_4f10(struct TrackVariables* pVariables)
{
    s32 volume;
    s32 pan;
    u32 right;
    u32 left;
    u32 result;

    volume = (pVariables->volume * pVariables->unk_5) >> 5;
    if (pVariables->modulationType == 1)
    {
        volume = (volume * (pVariables->unk_13 + 0x41)) >> 6;
        if (volume >= 0xFF)
            volume = 0xFF;
    }

    pan = (s8)pVariables->unk_7 + pVariables->unk_6 - 0x40;
    if (pan >= 0x3F)
        pan = 0x3F;
    else if (pan < -0x40)
        pan = 0x40; // sic

    if (pVariables->modulationType == 2)
    {
        pan += pVariables->unk_13;
        if (pan >= 0x3F)
            pan = 0x3F;
        else if (pan + 0x40 < 0)
            pan = -0x40;
    }

    right = (u32)((pan + 0x40) * volume) >> 7;
    left = (u32)((0x40 - pan) * volume) >> 7;
    result = right | left << 8;
    pVariables->unk_8 = result;
    pVariables->unk_9 = result >> 8;
}

/**
 * @brief 4f8c | a4 | Starts a note on a DirectSound channel
 *
 * @param pChannel Sound channel pointer
 * @param pVariables Track variables pointer
 * @param param_3 Velocity (low byte) and pan bits (second byte)
 */
void unk_4f8c(struct SoundChannel* pChannel, struct TrackVariables* pVariables, s32 param_3)
{
    u32* pSample;
    u32 frequency;
    s32 key;
    u8 note;

    pChannel->unk_13 |= 2;
    unk_20a4(pChannel);

    if (pVariables->pChannel != NULL)
        pVariables->pChannel->pChannel1 = pChannel;
    pChannel->pChannel2 = pVariables->pChannel;
    pChannel->pChannel1 = NULL;
    pChannel->pVariables = pVariables;
    pVariables->pChannel = pChannel;

    pChannel->unk_0 = 1;
    pChannel->unk_1 = pVariables->channel;
    pChannel->unk_2 = param_3;
    pChannel->unk_3 = (u8)(param_3 >> 8) | pVariables->unk_6;

    pChannel->envelope = pVariables->envelope1;
    pChannel->unk_C = pVariables->unk_C;
    pChannel->unk_D = pVariables->unk_D;
    pChannel->unk_E = pVariables->unk_E;
    pChannel->unk_F = pVariables->unk_F;

    pSample = pVariables->pSample1;
    if (pSample == NULL) // no VOICE command yet; the GBA would read the BIOS
        pSample = (u32*)sSilentSample;
    pChannel->pSample = pSample;
    pChannel->pData = (u8*)pSample + 0x10;
    pChannel->pSize = (u32*)((u8*)pSample + 0xC);

    pChannel->unk_6 = pVariables->unk_1;
    if ((pVariables->unk_0 & 0xF0) == 0x80)
        note = pVariables->unk_35;
    else
        note = pVariables->unk_1;
    pChannel->unk_7 = note;

    key = pVariables->unk_17 + note;
    if (key < 0)
        key = 0;
    else if (key > 0x7F)
        key = 0x7F;

    frequency = MidiKey2Freq(pSample, key, pVariables->unk_18);
    if (frequency == gMusicInfo.sampleRate)
        pChannel->unk_1C = 0x4000;
    else
        pChannel->unk_1C = CallGetNoteFrequency(frequency, gMusicInfo.pitch);
}

/**
 * @brief 5030 | 40 | Processes the audio command GOTO
 *
 * @param pVariables Track variables pointer
 */
void AudioCommand_Goto(struct TrackVariables* pVariables)
{
    pVariables->pRawData = ScriptPointer(Read32(pVariables->pRawData + 1));
}

/**
 * @brief 5070 | 60 | Processes the audio command PATT (call a pattern)
 *
 * @param pVariables Track variables pointer
 */
void AudioCommand_PatternPlay(struct TrackVariables* pVariables)
{
    const u8* pReturn;

    pReturn = pVariables->pRawData + 5;
    pVariables->pRawData = ScriptPointer(Read32(pVariables->pRawData + 1));

    if (pVariables->patternStartPointers[0] == NULL)
        pVariables->patternStartPointers[0] = pReturn;
    else if (pVariables->patternStartPointers[1] == NULL)
        pVariables->patternStartPointers[1] = pReturn;
    else if (pVariables->patternStartPointers[2] == NULL)
        pVariables->patternStartPointers[2] = pReturn;
}

/**
 * @brief 50d0 | 34 | Loads a programmable wave into wave RAM
 *
 * @param pSample 16 bytes of 4-bit samples
 */
void UploadSampleToWaveRam(const u32* pSample)
{
    u32 i;

    if (pSample == NULL)
        pSample = sSilentWave;

    WRITE_8(REG_SOUND3CNT_L, 0x40);
    for (i = 0; i < 4; i++)
        WRITE_32(REG_WAVE_RAM0_L + i * 4, pSample[i]);
    WRITE_8(REG_SOUND3CNT_L, 0);
}

/**
 * @brief 5104 | 8c | Writes a PSG sound's registers
 *
 * @param pSound PSG sound data pointer
 */
void unk_5104(struct PsgSoundData* pSound)
{
    u32 frequency;
    u32 control;
    u8 channel;
    u8 bias;

    if (pSound->pVariables == NULL)
        return;

    frequency = pSound->unk_14;
    channel = pSound->pVariables->channel;

    if (channel > 8)
    {
        // "Alt" PSG voices: compensate the bias resolution
        bias = READ_8(REG_SOUNDBIAS + 1) & 0xC0;
        if (bias < 0x40)
            frequency = (frequency + 2) & 0xC7FE;
        else if (bias < 0x80)
            frequency = (frequency + 1) & 0xC7FE;
    }

    control = pSound->unk_11 | pSound->unk_12 << 8;

    switch (channel & 7)
    {
        case 1:
            WRITE_8(REG_SOUND1CNT_L, pSound->unk_10);
            WRITE_16(REG_SOUND1CNT_H, control);
            WRITE_16(REG_SOUND1CNT_X, frequency);
            break;

        case 2:
            WRITE_16(REG_SOUND2CNT_L, control);
            WRITE_16(REG_SOUND2CNT_H, frequency);
            break;

        case 3:
            WRITE_8(REG_SOUND3CNT_L, pSound->unk_10);
            WRITE_16(REG_SOUND3CNT_H, control);
            WRITE_16(REG_SOUND3CNT_X, frequency);
            break;

        case 4:
            WRITE_16(REG_SOUND4CNT_L, control);
            WRITE_16(REG_SOUND4CNT_H, frequency);
            break;
    }

    pSound->unk_14 = frequency & 0x7FFF;
}

// ---------------------------------------------------------------------------
// asm/soundcode.s (on the GBA, copied to IWRAM and called through
// gSoundCode{A,B,C}Pointer)
// ---------------------------------------------------------------------------

/**
 * @brief 4abc | 1c | Sample step of a note: frequency * pitch / 2^18
 *
 */
s32 CallGetNoteFrequency(s32 frequency, u32 pitch)
{
    return (u32)(((u64)(u32)frequency * pitch) >> 18);
}

// One resampled sample (18.14 fixed point position), linear interpolation.
// The unsigned arithmetic matches the ARM code: the error it causes on
// negative slopes is a multiple of 2^18 per unit of volume, which the
// (acc >> 7) & 0x1FF lookup in SoundCodeC discards.
#define MIX_RESAMPLED(position)                                                 \
    do {                                                                        \
        const s8* pAt_ = &pData[(position) >> 14];                              \
        u32 sample_ = pAt_[0] + ((u32)((pAt_[1] - pAt_[0]) * (s32)((position) & 0x3FFF)) >> 14); \
        pBuffer[0] += sample_ * volumeA;                                        \
        pBuffer[1] += sample_ * volumeB;                                        \
        pBuffer += 2;                                                           \
    } while (0)

#define MIX_RAW(index)                                                          \
    do {                                                                        \
        u32 sample_ = pData[(index)];                                           \
        pBuffer[0] += sample_ * volumeA;                                        \
        pBuffer[1] += sample_ * volumeB;                                        \
        pBuffer += 2;                                                           \
    } while (0)

/**
 * @brief 4474 | 63c | Mixes one DirectSound channel into the accumulators
 *
 * @param pChannel Sound channel pointer
 * @param pBuffer Accumulators (FIFO A, FIFO B per sample)
 * @param size Number of groups of 4 samples
 */
void Port_SoundCodeA(struct SoundChannel* pChannel, u32* pBuffer, u8 size)
{
    const u32* pSample;
    const s8* pData;
    u32 position;
    u32 step;
    u32 length;
    u32 loopStart;
    u32 volumeA;
    u32 volumeB;
    u32 groups;
    u32 i;
    boolu32 loop;
    boolu32 wrapped;

    pSample = pChannel->pSample;
    pData = (const s8*)pChannel->pData;
    position = pChannel->unk_18;
    volumeA = pChannel->unk_11;
    volumeB = pChannel->unk_12;
    length = pSample[3];
    loop = ((const u16*)pSample)[1];
    loopStart = pSample[2];
    groups = size != 0 ? size : 256;

    if (pChannel->unk_1 == 0x20)
    {
        // Reversed sample (starts at the end, see UpdateMusic)
        step = -pChannel->unk_1C;
        for (; groups != 0; groups--)
        {
            if ((s32)(position + step * 4) >> 14 > 0)
            {
                for (i = 0; i < 4; i++)
                {
                    MIX_RESAMPLED(position);
                    position += step;
                }
                continue;
            }

            for (i = 0; ; i++)
            {
                MIX_RESAMPLED(position);
                if (i == 3)
                    goto end;
                position += step;
                // The ARM code shifts unsigned; signed stops a position that
                // went below 0 instead of reading far outside the sample.
                if ((s32)position >> 14 <= 0)
                    goto end;
            }
        }
    }
    else if (pChannel->unk_1 == 8)
    {
        // No resampling: one sample per output sample
        for (; groups != 0; groups--)
        {
            if ((s32)(position + 4) < (s32)length)
            {
                for (i = 0; i < 4; i++)
                    MIX_RAW(position++);
                continue;
            }

            for (i = 0; i < 4; i++)
            {
                MIX_RAW(position);
                position++;
                if (position == length)
                {
                    if (!loop)
                        goto end;
                    position = loopStart;
                }
            }
        }
    }
    else
    {
        step = pChannel->unk_1C;
        for (; groups != 0; groups--)
        {
            if ((s32)(length - ((position + step * 4) >> 14)) > 0)
            {
                for (i = 0; i < 4; i++)
                {
                    MIX_RESAMPLED(position);
                    position += step;
                }
                continue;
            }

            if (!loop)
            {
                for (i = 0; ; i++)
                {
                    MIX_RESAMPLED(position);
                    if (i == 3)
                        goto end;
                    position += step;
                    if ((position >> 14) >= length)
                        goto end;
                }
            }

            wrapped = FALSE;
            for (i = 0; i < 4; i++)
            {
                MIX_RESAMPLED(position);
                position += step;
                // The end is crossed within these 4 samples: by the 4th at
                // the latest.
                if (!wrapped && (i == 3 || (position >> 14) >= length))
                {
                    position = (position & 0x3FFF) + (((position >> 14) + loopStart - length) << 14);
                    wrapped = TRUE;
                }
            }
        }
    }

    pChannel->unk_18 = position;
    return;

end:
    pChannel->unk_0 = 0;
}

/**
 * @brief 4320 | 94 | Fills the accumulators with the reverb of the previous
 * output (0 when reverb is off)
 *
 * @param pDst Previous output (FIFO A bytes, FIFO B at +PCM_DMA_BUF_SIZE)
 * @param pSrc1 Same as pDst
 * @param pSrc2 Accumulators to fill
 * @param size Number of groups of 2 samples
 * @return u8* End of the filled accumulators
 */
u8* Port_SoundCodeB(u32* pDst, u32* pSrc1, u32* pSrc2, u8 size)
{
    const s8* pA;
    const s8* pB;
    s32* pOut;
    s32 reverb;
    s32 value;
    u32 count;
    u32 i;

    pA = (const s8*)pDst;
    pB = (const s8*)pSrc1;
    pOut = (s32*)pSrc2;
    reverb = gMusicInfo.reverb;

    for (count = size != 0 ? size : 256; count != 0; count--)
    {
        for (i = 0; i < 2; i++)
        {
            value = pA[i + PCM_DMA_BUF_SIZE] + pA[i] + pB[i] + pB[i + PCM_DMA_BUF_SIZE];
            value = (value * reverb) >> 9;
            if (value < 0)
                value++;
            value = (u32)value << 7;

            pOut[0] = value;
            pOut[1] = value;
            pOut += 2;
        }

        pA += 2;
        pB += 2;
    }

    return (u8*)pOut;
}

/**
 * @brief 43c4 | a0 | Converts the accumulators to 8-bit output samples
 *
 * @param pDst Output (FIFO A bytes, FIFO B at +PCM_DMA_BUF_SIZE)
 * @param pSrc Accumulators
 * @param size Number of groups of 4 samples
 * @return u8* End of the accumulators read
 */
u8* Port_SoundCodeC(u32* pDst, u32* pSrc, u8 size)
{
    u8* pA;
    u8* pB;
    const u32* pIn;
    u32 count;
    u32 i;

    pA = (u8*)pDst;
    pB = pA + PCM_DMA_BUF_SIZE;
    pIn = pSrc;

    for (count = size != 0 ? size : 256; count != 0; count--)
    {
        for (i = 0; i < 4; i++)
        {
            pA[i] = sArray_808cce2[(pIn[0] >> 7) & 0x1FF];
            pB[i] = sArray_808cce2[(pIn[1] >> 7) & 0x1FF];
            pIn += 2;
        }

        pA += 4;
        pB += 4;
    }

    return (u8*)pIn;
}

// The GBA build copies CallSoundCode{A,B,C} to IWRAM; natively the C versions
// above are called directly (audio_wrappers.c). The symbols only need to exist.
void CallSoundCodeA(void)
{
}

void CallSoundCodeB(void)
{
}

void CallSoundCodeC(void)
{
}

// The IRQ dispatcher (asm/intr_main.s) is replaced by the frame loop
// (port/src/frame.c); the symbol is still referenced by init_helpers.c.
void IntrMain(void)
{
}
