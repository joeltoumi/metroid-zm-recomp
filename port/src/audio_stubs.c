// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — TEMPORARY stand-ins for the sound engine routines that are
// still hand-written ARM assembly upstream (asm/audio_internal.s,
// asm/soundcode.s).
//
// The bulk of MZM's sound engine is C (src/audio.c); these are its inner
// helpers and mixing loops. Until they are translated to C (milestone M3 in
// docs/ROADMAP.md) the game runs silently: UpdateAudio is a no-op, so none of
// the other helpers are ever reached during normal play.
#include "port.h"

struct TrackData;
struct TrackVariables;
struct SoundChannel;
struct PsgSoundData;

unsigned char InitTrack(struct TrackData* pTrack, const uint32_t* pHeader)
{
    (void)pTrack;
    (void)pHeader;
    return 0;
}

void StopMusicOrSound(struct TrackData* pTrack)
{
    (void)pTrack;
}

void ResetTrack(struct TrackData* pTrack)
{
    (void)pTrack;
}

uint32_t unk_4cfc(struct TrackData* pTrack)
{
    (void)pTrack;
    return 0;
}

void unk_4d1c(struct TrackData* pTrack, struct TrackVariables* pVariables)
{
    (void)pTrack;
    (void)pVariables;
}

void UpdateAudio(void)
{
}

void unk_4e10(struct TrackVariables* pVariables)
{
    (void)pVariables;
}

void unk_4eb4(struct TrackVariables* pVariables)
{
    (void)pVariables;
}

void unk_4f10(struct TrackVariables* pVariables)
{
    (void)pVariables;
}

void unk_4f8c(struct SoundChannel* pChannel, struct TrackVariables* pVariables, int32_t param_3)
{
    (void)pChannel;
    (void)pVariables;
    (void)param_3;
}

void AudioCommand_Goto(struct TrackVariables* pVariables)
{
    (void)pVariables;
}

void AudioCommand_PatternPlay(struct TrackVariables* pVariables)
{
    (void)pVariables;
}

void UploadSampleToWaveRam(const uint32_t* pSample)
{
    (void)pSample;
}

void unk_5104(struct PsgSoundData* pSound)
{
    (void)pSound;
}

uint32_t CallGetNoteFrequency(uint32_t frequency, int32_t pitch)
{
    (void)pitch;
    return frequency;
}

// The GBA copies these mixing loops into IWRAM and calls them through
// gSoundCode{A,B,C}Pointer. They are only reached from the mixer, which does
// not run yet; keep them as harmless symbols for the copy in audio_wrappers.c.
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

// Mixing loops (asm/soundcode.s SoundCodeA/B/C), not translated yet.
void Port_SoundCodeA(struct SoundChannel* pChannel, uint32_t* pBuffer, uint8_t size)
{
    (void)pChannel;
    (void)pBuffer;
    (void)size;
}

uint8_t* Port_SoundCodeB(uint32_t* pDst, uint32_t* pSrc1, uint32_t* pSrc2, uint8_t size)
{
    (void)pSrc1;
    (void)pSrc2;
    (void)size;
    return (uint8_t*)pDst;
}

uint8_t* Port_SoundCodeC(uint32_t* pDst, uint32_t* pSrc, uint8_t size)
{
    (void)pSrc;
    (void)size;
    return (uint8_t*)pDst;
}
