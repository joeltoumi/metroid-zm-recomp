// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — headless host: runs the game without a window, as fast as
// possible, for automated tests and CI.
//
//   mzm_headless [--frames N] [--dump-every K] [--out DIR] [--save FILE]
//                [--press FRAME:KEYS:LEN ...]
//
// Frames are written as PPM images (no dependencies). KEYS is a combination
// of the letters A B s(elect) S(tart) R(ight) L(eft) U(p) D(own) r l.
#include "port.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

typedef struct Press {
    uint64_t frame;
    uint64_t length;
    uint16_t keys;
} Press;

static uint64_t sMaxFrames = 600;
static uint64_t sDumpEvery = 60;
static const char* sOutDir = "frames";
static const char* sSavePath = "mzm_headless.sav";
static Press sPresses[64];
static int sPressCount;

static uint16_t ParseKeys(const char* s)
{
    uint16_t keys = 0;
    for (; *s && *s != ':'; s++) {
        switch (*s) {
        case 'A': keys |= PORT_KEY_A; break;
        case 'B': keys |= PORT_KEY_B; break;
        case 's': keys |= PORT_KEY_SELECT; break;
        case 'S': keys |= PORT_KEY_START; break;
        case 'R': keys |= PORT_KEY_RIGHT; break;
        case 'L': keys |= PORT_KEY_LEFT; break;
        case 'U': keys |= PORT_KEY_UP; break;
        case 'D': keys |= PORT_KEY_DOWN; break;
        case 'r': keys |= PORT_KEY_R; break;
        case 'l': keys |= PORT_KEY_L; break;
        default: break;
        }
    }
    return keys;
}

bool Port_HostInit(const PortHostConfig* cfg)
{
    for (int i = 1; i < cfg->argc; i++) {
        const char* a = cfg->argv[i];
        const char* v = (i + 1 < cfg->argc) ? cfg->argv[i + 1] : NULL;
        if (strcmp(a, "--frames") == 0 && v) {
            sMaxFrames = strtoull(v, NULL, 10);
            i++;
        } else if (strcmp(a, "--dump-every") == 0 && v) {
            sDumpEvery = strtoull(v, NULL, 10);
            i++;
        } else if (strcmp(a, "--out") == 0 && v) {
            sOutDir = v;
            i++;
        } else if (strcmp(a, "--save") == 0 && v) {
            sSavePath = v;
            i++;
        } else if (strcmp(a, "--press") == 0 && v && sPressCount < 64) {
            // FRAME:KEYS:LEN
            Press* p = &sPresses[sPressCount++];
            char* end;
            p->frame = strtoull(v, &end, 10);
            p->keys = (*end == ':') ? ParseKeys(end + 1) : 0;
            const char* len = strrchr(v, ':');
            p->length = (len && len != end) ? strtoull(len + 1, NULL, 10) : 1;
            i++;
        } else {
            Port_Log("unknown argument: %s", a);
            return false;
        }
    }

    if (sDumpEvery != 0 && mkdir(sOutDir, 0755) != 0 && errno != EEXIST) {
        Port_Log("can't create %s", sOutDir);
        return false;
    }
    Port_Log("headless: %llu frames, dump every %llu to %s/", (unsigned long long)sMaxFrames,
             (unsigned long long)sDumpEvery, sOutDir);
    return true;
}

void Port_HostShutdown(void)
{
    Port_Log("headless: stopped after %llu frames", (unsigned long long)Port_FrameCount());
}

static void WritePpm(const uint32_t* abgr, int width, int height, uint64_t frame)
{
    char path[512];
    FILE* f;

    snprintf(path, sizeof(path), "%s/frame_%06llu.ppm", sOutDir, (unsigned long long)frame);
    f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        const uint32_t p = abgr[i];
        const unsigned char rgb[3] = { (unsigned char)(p & 0xFF), (unsigned char)((p >> 8) & 0xFF),
                                       (unsigned char)((p >> 16) & 0xFF) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

bool Port_HostPresent(const uint32_t* abgr, int width, int height)
{
    const uint64_t frame = Port_FrameCount();

    if (sDumpEvery != 0 && frame % sDumpEvery == 0)
        WritePpm(abgr, width, height, frame);
    return frame + 1 < sMaxFrames;
}

uint16_t Port_HostPollKeys(void)
{
    const uint64_t frame = Port_FrameCount() + 1;
    uint16_t keys = 0;

    for (int i = 0; i < sPressCount; i++) {
        if (frame >= sPresses[i].frame && frame < sPresses[i].frame + sPresses[i].length)
            keys |= sPresses[i].keys;
    }
    return keys;
}

void Port_HostWaitFrame(void)
{
}

void Port_HostLoadSave(uint8_t* data, size_t size)
{
    FILE* f = fopen(sSavePath, "rb");
    if (!f)
        return;
    if (fread(data, 1, size, f) != size)
        Port_Log("save file %s is short", sSavePath);
    fclose(f);
}

void Port_HostWriteSave(const uint8_t* data, size_t size)
{
    FILE* f = fopen(sSavePath, "wb");
    if (!f)
        return;
    fwrite(data, 1, size, f);
    fclose(f);
}

int main(int argc, char** argv)
{
    return Port_Main(argc, argv);
}
