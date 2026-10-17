// SPDX-License-Identifier: GPL-3.0-or-later
// mzm-android — user settings (settings.ini), SDL frontend only.
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#include <SDL3/SDL.h>
#include <stdbool.h>

// Inputs in GBA KEYINPUT bit order, then port-only actions.
enum {
    PORT_INPUT_A,
    PORT_INPUT_B,
    PORT_INPUT_SELECT,
    PORT_INPUT_START,
    PORT_INPUT_RIGHT,
    PORT_INPUT_LEFT,
    PORT_INPUT_UP,
    PORT_INPUT_DOWN,
    PORT_INPUT_R,
    PORT_INPUT_L,
    PORT_INPUT_GBA_COUNT,
    PORT_INPUT_FAST_FORWARD = PORT_INPUT_GBA_COUNT,
    PORT_INPUT_COUNT
};

enum {
    PORT_TOUCH_AUTO, // shown after the screen is touched, hidden by a gamepad
    PORT_TOUCH_ON,
    PORT_TOUCH_OFF,
};

typedef struct PortBinding {
    SDL_Scancode key;
    SDL_GamepadButton button;
    SDL_GamepadAxis axis; // optional analog direction
    int axisSign;         // -1 / +1
} PortBinding;

typedef struct PortConfig {
    // [video]
    int windowScale;
    bool fullscreen;
    bool vsync;
    bool integerScaling;
    bool smoothScaling;
    // [audio]
    int volume; // percent, 0 = muted
    // [input]
    int touchControls;
    int touchOpacity; // percent
    int stickDeadzone; // percent
    int fastForwardSpeed;
    PortBinding bindings[PORT_INPUT_COUNT];
} PortConfig;

void PortConfig_Defaults(PortConfig* cfg);
bool PortConfig_Load(PortConfig* cfg, const char* path);
bool PortConfig_Save(const PortConfig* cfg, const char* path);
const char* PortConfig_InputName(int input);

#endif // PORT_CONFIG_H
