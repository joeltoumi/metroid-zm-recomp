// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — SDL3 frontend (desktop and Android).
//
// Video: the 240x160 frame is uploaded to a streaming texture and scaled to
// the window (aspect-correct, optional integer scaling / smooth filter).
// Audio: the port's 48 kHz output goes to an SDL audio stream.
// Input: keyboard, gamepads (the AYN Thor's built-in controls show up as one)
// and, on touch screens, an on-screen pad drawn over the game.
// Everything configurable lives in settings.ini in the app's pref folder.
#include "port.h"
#include "port_config.h"
#include "port_assets.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

#define GBA_FRAME_NS 16742706ull // 280896 cycles / 16.777216 MHz

static SDL_Window* sWindow;
static SDL_Renderer* sRenderer;
static SDL_Texture* sTexture;
static SDL_Gamepad* sGamepad;
static SDL_AudioStream* sAudio;
static char* sPrefPath;
static char sRegionSuffix[8]; // "" for US, "_eu" / "_jp" otherwise
static char sSavePath[1024];
static char sConfigPath[1024];
static const char* sRomArgPath; // mzm [path/to/rom.gba]

static PortConfig sConfig;
static bool sFastForward;
static bool sTouchVisible;
static uint64_t sNextFrameNs;

// ---- touch pad ----------------------------------------------------------------

typedef struct TouchButton {
    uint16_t key;      // PORT_KEY_* bits (diagonals combine two)
    float cx, cy, r;   // center/radius in normalized window coordinates
    bool leftSide;     // anchored to the left or right edge
} TouchButton;

// Layout in units of the window height (so it scales on any aspect ratio).
static const TouchButton sTouchButtons[] = {
    { PORT_KEY_UP, 0.18f, 0.55f, 0.075f, true },
    { PORT_KEY_DOWN, 0.18f, 0.85f, 0.075f, true },
    { PORT_KEY_LEFT, 0.03f, 0.70f, 0.075f, true },
    { PORT_KEY_RIGHT, 0.33f, 0.70f, 0.075f, true },
    { PORT_KEY_A, 0.12f, 0.62f, 0.085f, false },
    { PORT_KEY_B, 0.30f, 0.78f, 0.085f, false },
    { PORT_KEY_L, 0.10f, 0.12f, 0.07f, true },
    { PORT_KEY_R, 0.10f, 0.12f, 0.07f, false },
    { PORT_KEY_SELECT, 0.48f, 0.94f, 0.045f, true },
    { PORT_KEY_START, 0.48f, 0.94f, 0.045f, false },
};

#define MAX_FINGERS 10
static struct {
    SDL_FingerID id;
    bool down;
    float x, y; // window pixels
} sFingers[MAX_FINGERS];

static void TouchButtonCenter(const TouchButton* b, int w, int h, float* x, float* y, float* r)
{
    const float unit = (float)h;
    *r = b->r * unit;
    *y = b->cy * unit;
    if (b->leftSide)
        *x = b->cx * unit + *r;
    else
        *x = (float)w - b->cx * unit - *r;
    if (b->key == PORT_KEY_SELECT)
        *x = (float)w * 0.5f - 1.3f * *r;
    if (b->key == PORT_KEY_START)
        *x = (float)w * 0.5f + 1.3f * *r;
}

static uint16_t TouchKeys(void)
{
    int w, h;
    uint16_t keys = 0;

    if (!sTouchVisible)
        return 0;
    SDL_GetWindowSizeInPixels(sWindow, &w, &h);
    for (int f = 0; f < MAX_FINGERS; f++) {
        if (!sFingers[f].down)
            continue;
        for (size_t i = 0; i < SDL_arraysize(sTouchButtons); i++) {
            float x, y, r;
            TouchButtonCenter(&sTouchButtons[i], w, h, &x, &y, &r);
            const float dx = sFingers[f].x - x, dy = sFingers[f].y - y;
            // Generous hit area so thumbs can roll between buttons.
            if (dx * dx + dy * dy <= (r * 1.35f) * (r * 1.35f))
                keys |= sTouchButtons[i].key;
        }
    }
    return keys;
}

static void DrawCircle(float cx, float cy, float r)
{
    // Filled circle from horizontal spans; cheap enough for ~10 buttons.
    for (float dy = -r; dy <= r; dy += 1.0f) {
        const float dx = SDL_sqrtf(r * r - dy * dy);
        SDL_RenderLine(sRenderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void DrawTouchPad(uint16_t pressed)
{
    int w, h;
    if (!sTouchVisible || sConfig.touchOpacity <= 0)
        return;
    SDL_GetWindowSizeInPixels(sWindow, &w, &h);
    SDL_SetRenderDrawBlendMode(sRenderer, SDL_BLENDMODE_BLEND);
    for (size_t i = 0; i < SDL_arraysize(sTouchButtons); i++) {
        float x, y, r;
        const bool down = (pressed & sTouchButtons[i].key) != 0;
        const Uint8 alpha = (Uint8)(sConfig.touchOpacity * (down ? 2.0f : 1.0f) * 255.0f / 100.0f);
        TouchButtonCenter(&sTouchButtons[i], w, h, &x, &y, &r);
        SDL_SetRenderDrawColor(sRenderer, 255, 255, 255, alpha);
        DrawCircle(x, y, r);
    }
}

// ---- configuration -------------------------------------------------------------

static void ApplyVideoConfig(void)
{
    SDL_SetRenderVSync(sRenderer, sConfig.vsync ? 1 : 0);
    SDL_SetTextureScaleMode(sTexture, sConfig.smoothScaling ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_SetRenderLogicalPresentation(sRenderer, GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT,
                                     sConfig.integerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                                                            : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

// ---- host interface -------------------------------------------------------------

bool Port_HostInit(const PortHostConfig* cfg)
{
    if (cfg->argc > 1)
        sRomArgPath = cfg->argv[1];
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        Port_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }

    sPrefPath = SDL_GetPrefPath("mzm-android", "mzm");
    // Each region keeps its own save and ROM copy: on PC all builds share this
    // folder. The US build keeps the original names.
    const char* suffix = SDL_strcmp(Port_RomRegionName(), "US") == 0 ? "" : Port_RomRegionName();
    SDL_snprintf(sRegionSuffix, sizeof(sRegionSuffix), "%s%s", suffix[0] ? "_" : "", suffix);
    for (char* c = sRegionSuffix; *c; c++)
        *c = (char)SDL_tolower(*c);
    SDL_snprintf(sSavePath, sizeof(sSavePath), "%smzm%s.sav", sPrefPath ? sPrefPath : "", sRegionSuffix);
    SDL_snprintf(sConfigPath, sizeof(sConfigPath), "%ssettings.ini", sPrefPath ? sPrefPath : "");
    PortConfig_Defaults(&sConfig);
    if (!PortConfig_Load(&sConfig, sConfigPath))
        PortConfig_Save(&sConfig, sConfigPath); // first run: write the defaults
    Port_Log("settings: %s", sConfigPath);

    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                  (sConfig.fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
    if (!SDL_CreateWindowAndRenderer("Metroid: Zero Mission", GBA_SCREEN_WIDTH * sConfig.windowScale,
                                     GBA_SCREEN_HEIGHT * sConfig.windowScale, flags, &sWindow, &sRenderer)) {
        Port_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return false;
    }
    sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                 GBA_SCREEN_WIDTH, GBA_SCREEN_HEIGHT);
    if (!sTexture) {
        Port_Log("SDL_CreateTexture: %s", SDL_GetError());
        return false;
    }
    ApplyVideoConfig();

    if (sConfig.volume > 0) {
        const SDL_AudioSpec spec = { SDL_AUDIO_S16, 2, PORT_AUDIO_RATE };
        sAudio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (sAudio) {
            SDL_SetAudioStreamGain(sAudio, sConfig.volume / 100.0f);
            SDL_ResumeAudioStreamDevice(sAudio);
        } else {
            Port_Log("no sound: %s", SDL_GetError()); // the game still runs
        }
    }

#ifdef __ANDROID__
    sTouchVisible = sConfig.touchControls != PORT_TOUCH_OFF;
#else
    sTouchVisible = sConfig.touchControls == PORT_TOUCH_ON;
#endif
    sNextFrameNs = SDL_GetTicksNS();
    return true;
}

void Port_HostShutdown(void)
{
    if (sAudio)
        SDL_DestroyAudioStream(sAudio);
    if (sGamepad)
        SDL_CloseGamepad(sGamepad);
    SDL_DestroyTexture(sTexture);
    SDL_DestroyRenderer(sRenderer);
    SDL_DestroyWindow(sWindow);
    SDL_free(sPrefPath);
    SDL_Quit();
}

static void OpenFirstGamepad(void)
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0 && !sGamepad) {
        sGamepad = SDL_OpenGamepad(ids[0]);
        if (sGamepad)
            Port_Log("gamepad: %s", SDL_GetGamepadName(sGamepad));
    }
    SDL_free(ids);
}

static void HandleFinger(const SDL_TouchFingerEvent* e, bool down)
{
    int w, h;
    int slot = -1;
    SDL_GetWindowSizeInPixels(sWindow, &w, &h);
    for (int i = 0; i < MAX_FINGERS; i++) {
        if (sFingers[i].down && sFingers[i].id == e->fingerID) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (!down)
            return;
        for (int i = 0; i < MAX_FINGERS; i++) {
            if (!sFingers[i].down) {
                slot = i;
                break;
            }
        }
        if (slot < 0)
            return;
    }
    sFingers[slot].id = e->fingerID;
    sFingers[slot].down = down;
    sFingers[slot].x = e->x * (float)w;
    sFingers[slot].y = e->y * (float)h;
    if (sConfig.touchControls == PORT_TOUCH_AUTO)
        sTouchVisible = true;
}

static bool PumpEvents(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_KEY_DOWN:
            if (e.key.key == SDLK_F11 && !e.key.repeat) {
                sConfig.fullscreen = !sConfig.fullscreen;
                SDL_SetWindowFullscreen(sWindow, sConfig.fullscreen);
                PortConfig_Save(&sConfig, sConfigPath);
            }
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            OpenFirstGamepad();
            if (sConfig.touchControls == PORT_TOUCH_AUTO)
                sTouchVisible = false;
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (sGamepad && SDL_GetGamepadID(sGamepad) == e.gdevice.which) {
                SDL_CloseGamepad(sGamepad);
                sGamepad = NULL;
                OpenFirstGamepad();
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (sConfig.touchControls == PORT_TOUCH_AUTO)
                sTouchVisible = false;
            break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
            HandleFinger(&e.tfinger, true);
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            HandleFinger(&e.tfinger, false);
            break;
        default:
            break;
        }
    }
    return true;
}

bool Port_HostPresent(const uint32_t* abgr, int width, int height)
{
    (void)height;
    if (!PumpEvents())
        return false;

    SDL_UpdateTexture(sTexture, NULL, abgr, width * (int)sizeof(uint32_t));
    SDL_SetRenderDrawColor(sRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sRenderer);
    SDL_RenderTexture(sRenderer, sTexture, NULL, NULL);

    // The touch pad is drawn in window pixels, outside the logical GBA canvas.
    SDL_SetRenderLogicalPresentation(sRenderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    DrawTouchPad(TouchKeys());
    ApplyVideoConfig();

    SDL_RenderPresent(sRenderer);
    return true;
}

uint16_t Port_HostPollKeys(void)
{
    const bool* kb = SDL_GetKeyboardState(NULL);
    uint16_t keys = 0;

    for (int k = 0; k < PORT_INPUT_COUNT; k++) {
        const PortBinding* b = &sConfig.bindings[k];
        bool down = false;
        if (b->key != SDL_SCANCODE_UNKNOWN && kb[b->key])
            down = true;
        if (sGamepad && b->button != SDL_GAMEPAD_BUTTON_INVALID && SDL_GetGamepadButton(sGamepad, b->button))
            down = true;
        if (sGamepad && b->axis != SDL_GAMEPAD_AXIS_INVALID) {
            const int v = SDL_GetGamepadAxis(sGamepad, b->axis);
            const int threshold = 32767 * sConfig.stickDeadzone / 100;
            if ((b->axisSign > 0 && v > threshold) || (b->axisSign < 0 && v < -threshold))
                down = true;
        }
        if (!down)
            continue;
        if (k < PORT_INPUT_GBA_COUNT)
            keys |= (uint16_t)(1u << k);
    }

    sFastForward = kb[sConfig.bindings[PORT_INPUT_FAST_FORWARD].key] ||
                   (sGamepad && SDL_GetGamepadButton(sGamepad, sConfig.bindings[PORT_INPUT_FAST_FORWARD].button));

    keys |= TouchKeys();

    // Opposite directions can't be held on a real d-pad; the game misbehaves.
    if ((keys & (PORT_KEY_LEFT | PORT_KEY_RIGHT)) == (PORT_KEY_LEFT | PORT_KEY_RIGHT))
        keys &= ~(PORT_KEY_LEFT | PORT_KEY_RIGHT);
    if ((keys & (PORT_KEY_UP | PORT_KEY_DOWN)) == (PORT_KEY_UP | PORT_KEY_DOWN))
        keys &= ~(PORT_KEY_UP | PORT_KEY_DOWN);
    return keys;
}

void Port_HostQueueAudio(const int16_t* samples, int frames)
{
    // At most ~100 ms queued: in fast-forward (or if the device falls behind)
    // the extra sound is dropped instead of building up a delay.
    const int maxQueued = PORT_AUDIO_RATE / 10 * 4;

    if (!sAudio || frames <= 0 || SDL_GetAudioStreamQueued(sAudio) > maxQueued)
        return;
    SDL_PutAudioStreamData(sAudio, samples, frames * 4);
}

void Port_HostWaitFrame(void)
{
    const uint64_t frameNs = sFastForward ? GBA_FRAME_NS / (uint64_t)sConfig.fastForwardSpeed : GBA_FRAME_NS;
    const uint64_t now = SDL_GetTicksNS();

    sNextFrameNs += frameNs;
    if (sNextFrameNs > now) {
        SDL_DelayPrecise(sNextFrameNs - now);
    } else if (now - sNextFrameNs > 4 * GBA_FRAME_NS) {
        sNextFrameNs = now; // fell far behind (debugger, app suspended): resync
    }
}

// ---- ROM selection ----------------------------------------------------------------

static char sRomCachePath[1024];
static SDL_AtomicInt sDialogState; // 0 idle, 1 open, 2 done
static char sPickedPath[2048];

static void SDLCALL OnRomPicked(void* userdata, const char* const* files, int filter)
{
    (void)userdata;
    (void)filter;
    sPickedPath[0] = '\0';
    if (files && files[0])
        SDL_strlcpy(sPickedPath, files[0], sizeof(sPickedPath));
    SDL_SetAtomicInt(&sDialogState, 2);
}

static uint8_t* LoadWholeFile(const char* path, size_t* size)
{
    size_t len = 0;
    void* data = SDL_LoadFile(path, &len);
    if (!data)
        return NULL;
    // Hand back a malloc'd buffer (the port frees it with free()).
    uint8_t* copy = malloc(len ? len : 1);
    if (copy)
        memcpy(copy, data, len);
    SDL_free(data);
    *size = len;
    return copy;
}

static void DrawRomScreen(const char* error)
{
    float scale;
    int w, h;

    SDL_GetWindowSizeInPixels(sWindow, &w, &h);
    scale = (float)h / 200.0f;
    if (scale < 1.0f)
        scale = 1.0f;
    SDL_SetRenderLogicalPresentation(sRenderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderScale(sRenderer, scale, scale);
    SDL_SetRenderDrawColor(sRenderer, 8, 8, 24, 255);
    SDL_RenderClear(sRenderer);

    SDL_SetRenderDrawColor(sRenderer, 255, 255, 255, 255);
    SDL_RenderDebugText(sRenderer, 8, 8, "Metroid: Zero Mission");
    SDL_SetRenderDrawColor(sRenderer, 180, 180, 200, 255);
    SDL_RenderDebugText(sRenderer, 8, 28, "Choose your own ROM dump:");
    SDL_RenderDebugTextFormat(sRenderer, 8, 40, "Metroid - Zero Mission (%s)", Port_RomRegionName());
    SDL_RenderDebugText(sRenderer, 8, 60, "Press A / Enter or tap to browse.");
    SDL_RenderDebugText(sRenderer, 8, 72, "Press B / Esc to quit.");
    if (error) {
        SDL_SetRenderDrawColor(sRenderer, 255, 120, 120, 255);
        SDL_RenderDebugText(sRenderer, 8, 96, error);
    }
    SDL_SetRenderDrawColor(sRenderer, 120, 120, 140, 255);
    SDL_RenderDebugText(sRenderer, 8, 120, "SHA-1:");
    SDL_RenderDebugText(sRenderer, 8, 132, Port_RomExpectedSha1());

    SDL_SetRenderScale(sRenderer, 1.0f, 1.0f);
    SDL_RenderPresent(sRenderer);
    ApplyVideoConfig();
}

uint8_t* Port_HostLoadRom(size_t* size, const char* error)
{
    static const SDL_DialogFileFilter filters[] = {
        { "GBA ROM", "gba;bin;agb" },
        { "All files", "*" },
    };
    const char* candidates[4];
    char nextToExe[1024];
    int n = 0;

    SDL_snprintf(sRomCachePath, sizeof(sRomCachePath), "%srom%s.gba", sPrefPath ? sPrefPath : "", sRegionSuffix);
    {
        // Upstream's file name for the dump: mzm_us_baserom.gba, next to the exe.
        char region[8];
        SDL_strlcpy(region, Port_RomRegionName(), sizeof(region));
        for (char* c = region; *c; c++)
            *c = (char)SDL_tolower(*c);
        SDL_snprintf(nextToExe, sizeof(nextToExe), "%smzm_%s_baserom.gba",
                     SDL_GetBasePath() ? SDL_GetBasePath() : "", region);
    }

    if (!error) {
        if (sRomArgPath)
            candidates[n++] = sRomArgPath;
        candidates[n++] = sRomCachePath;
        candidates[n++] = nextToExe;
        for (int i = 0; i < n; i++) {
            uint8_t* data = LoadWholeFile(candidates[i], size);
            if (data) {
                Port_Log("ROM: %s", candidates[i]);
                return data;
            }
        }
    }

    // Nothing found (or it was rejected): ask the player.
    for (;;) {
        SDL_Event e;
        bool browse = false;

        DrawRomScreen(error);
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                return NULL;
            if (e.type == SDL_EVENT_GAMEPAD_ADDED)
                OpenFirstGamepad();
            if (SDL_GetAtomicInt(&sDialogState) != 0)
                continue;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE)
                    browse = true;
                else if (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_AC_BACK)
                    return NULL;
            } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                if (e.gbutton.button == sConfig.bindings[PORT_INPUT_A].button ||
                    e.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                    browse = true;
                else if (e.gbutton.button == sConfig.bindings[PORT_INPUT_B].button)
                    return NULL;
            } else if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                browse = true;
            }
        }

        if (browse) {
            SDL_SetAtomicInt(&sDialogState, 1);
            SDL_ShowOpenFileDialog(OnRomPicked, NULL, sWindow, filters, SDL_arraysize(filters), NULL, false);
        }

        if (SDL_GetAtomicInt(&sDialogState) == 2) {
            SDL_SetAtomicInt(&sDialogState, 0);
            if (sPickedPath[0] != '\0') {
                uint8_t* data = LoadWholeFile(sPickedPath, size);
                if (data) {
                    Port_Log("ROM picked: %s", sPickedPath);
                    return data;
                }
                error = "Can't read that file";
            }
        }
        SDL_Delay(16);
    }
}

void Port_HostRomAccepted(const uint8_t* rom, size_t size)
{
    // Keep a private copy so the player only picks the file once (on Android
    // the picked content:// URI is not readable after a restart).
    size_t len = 0;
    void* existing = SDL_LoadFile(sRomCachePath, &len);
    const bool same = existing && len == size && memcmp(existing, rom, size) == 0;
    SDL_free(existing);
    if (!same && !SDL_SaveFile(sRomCachePath, rom, size))
        Port_Log("can't cache the ROM in %s: %s", sRomCachePath, SDL_GetError());
}

void Port_HostLoadSave(uint8_t* data, size_t size)
{
    size_t len = 0;
    void* buf = SDL_LoadFile(sSavePath, &len);
    if (!buf)
        return;
    memcpy(data, buf, len < size ? len : size);
    SDL_free(buf);
    Port_Log("save loaded: %s", sSavePath);
}

void Port_HostWriteSave(const uint8_t* data, size_t size)
{
    if (!SDL_SaveFile(sSavePath, data, size))
        Port_Log("can't write %s: %s", sSavePath, SDL_GetError());
}

int main(int argc, char** argv)
{
    return Port_Main(argc, argv);
}
